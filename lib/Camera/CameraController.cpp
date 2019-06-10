#include <CameraController.h>
#include <SPI.h>

#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

// for some constants...
#include <ESPAsyncWebServer.h>

CameraBuffer::CameraBuffer(const CameraFrame& frame)
  : frame(frame)
  , bufferIx(0)
  , frameRead(false)
{ }

void CameraBuffer::reset() {
  this->bufferIx = 0;
}

size_t CameraBuffer::copy(uint8_t* buffer, size_t maxLen) {
  size_t toCopy = std::min(maxLen, this->frame.length - this->bufferIx);
  memcpy(buffer, this->frame.bytes + this->bufferIx, toCopy);
  this->bufferIx += toCopy;
  return toCopy;
}

bool CameraBuffer::done() {
  return this->bufferIx >= this->frame.length;
}

CameraController::CameraController(Settings& settings)
  : camera(ArduCAM(OV2640, SS))
  , settings(settings)
  , captureStream(CameraStream(camera))
  , cameraFrame(std::make_shared<CameraFrame>())
  , readFrameMtx(xSemaphoreCreateCounting(10, 0))
  , sendFrameMtx(xSemaphoreCreateCounting(1, 0))
  , sendingCount(xSemaphoreCreateCounting(10, 0))
{
  TaskHandle_t copyTask = NULL;
  xTaskCreate(
    &CameraController::readCameraFrame,
    "ArduCAM_Capture",
    2048,
    (void*)(this),
    0,
    &copyTask
  );
}

void CameraController::init() {
  pinMode(SS, OUTPUT);
  pinMode(settings.arducam.i2c_sda_pin, OUTPUT);
  pinMode(settings.arducam.i2c_scl_pin, OUTPUT);

  digitalWrite(settings.arducam.i2c_scl_pin, LOW);

  Wire.begin(settings.arducam.i2c_sda_pin, settings.arducam.i2c_scl_pin);

  uint8_t vid, pid;

  camera.write_reg(ARDUCHIP_TEST1, 0x55);
  uint8_t temp = camera.read_reg(ARDUCHIP_TEST1);

  if (temp != 0x55) {
    Serial.println(F("SPI1 interface Error!"));
  }

  camera.wrSensorReg8_8(0xff, 0x01);
  camera.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
  camera.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);

  if ((vid != 0x26) && ((pid != 0x41) || (pid != 0x42))) {
    Serial.println(F("Can't find OV2640 module!"));
  } else {
    Serial.println(F("OV2640 detected."));
  }

  camera.set_format(JPEG);
  camera.InitCAM();
  camera.OV2640_set_JPEG_size(static_cast<uint8_t>(settings.arducam.camera_resolution));
  camera.clear_fifo_flag();
  camera.write_reg(ARDUCHIP_FRAMES, 0x00);

  camera.clear_fifo_flag();
  camera.start_capture();

  time_t max_wait_time = 1000;
  time_t start_time = millis();
  bool found_camera = false;

  while (!found_camera && millis() < (start_time + max_wait_time)) {
    found_camera = camera.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK);
  }

  if (found_camera) {
    const size_t cameraBufferLen = camera.read_fifo_length();
    size_t readRemains = cameraBufferLen;

    if (cameraBufferLen >= 0x07ffff){
      Serial.println(F("ERROR: Captured image was too large"));
      return;
    } else if (cameraBufferLen == 0 ){
      Serial.println(F("ERROR: Buffer size was 0"));
      return;
    }

    camera.CS_LOW();
    camera.set_fifo_burst();
    SPI.transfer(0xFF);

    uint8_t buffer[2048];
    SPI.transferBytes(buffer, buffer, 2048);
  }
}

size_t CameraController::CameraStream::read(uint8_t* buffer, size_t maxLen) {
  if (isOpen && this->bytesRemaining > 0) {
    size_t toRead = std::min(static_cast<size_t>(maxLen), static_cast<size_t>(this->bytesRemaining));

    SPI.transferBytes(buffer, buffer, toRead);
    this->bytesRemaining -= toRead;

    if (!this->bytesRemaining) {
      isOpen = false;
    }

    return toRead;
  }
  return 0;
}

CameraController::CameraStream::CameraStream(ArduCAM& camera)
  : camera(camera)
  , bytesRemaining(0)
{ }

void CameraController::readCameraFrame(void* _this) {
  static_cast<CameraController*>(_this)->readCameraFrame();
}

void CameraController::readCameraFrame() {
  while (true) {
    if (xSemaphoreTake(readFrameMtx, portMAX_DELAY) == pdTRUE) {
      captureStream.open();
      size_t readBytes = captureStream.read(this->cameraFrame->bytes, MAX_CAMERA_FRAME_SIZE);
      captureStream.close();
      if (readBytes >= MAX_CAMERA_FRAME_SIZE) {
        Serial.println(F("ERROR: read frame was too large!"));
      }
      this->cameraFrame->length = readBytes;

      xSemaphoreGive(sendFrameMtx);
    }
  }
}

void CameraController::CameraStream::close() {
}

const CameraFrame& CameraController::getCameraFrame() {
  return *this->cameraFrame;
}

void CameraController::CameraStream::open() {
  camera.flush_fifo();
  camera.clear_fifo_flag();
  camera.start_capture();

  while (!camera.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));

  this->bytesRemaining = camera.read_fifo_length();

  if (this->bytesRemaining >= 0x07ffff){
    Serial.println(F("ERROR: Captured image was too large"));
    return;
  } else if (this->bytesRemaining == 0 ){
    Serial.println(F("ERROR: Buffer size was 0"));
    return;
  }

  camera.CS_LOW();
  camera.set_fifo_burst();
  SPI.transfer(0x00);

  this->isOpen = true;
}

CameraController::CallbackFn CameraController::chunkedResponseCallback() {
  std::shared_ptr<CameraBuffer> cameraBuffer = std::make_shared<CameraBuffer>(getCameraFrame());

  // Don't request new frame if a send is already in progress.  Instead, just re-send the
  // same frame.
  if (uxSemaphoreGetCount(sendingCount) == 0) {
    if (xSemaphoreGive(readFrameMtx) != pdTRUE) {
      Serial.println("ERROR: could not give read frame mutex");
    }
  } else {
    cameraBuffer->frameRead = true;
  }

  // This is a counter for how many things are reading.  The semaphore will be taken back
  // when the whole frame has been sent.
  //
  // NOTE: seems like this could bug out if a frame is partially sent.  Effect, I believe,
  // would be that the same frame would be sent again and again until reboot.  Will need
  // to be cleverer if this happens often enough.
  xSemaphoreGive(sendingCount);

  return [this, cameraBuffer](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
    if (cameraBuffer->frameRead || xSemaphoreTake(sendFrameMtx, portMAX_DELAY) == pdTRUE) {
      cameraBuffer->frameRead = true;

      size_t readBytes = cameraBuffer->copy(buffer, maxLen);

      if (cameraBuffer->done()) {
        xSemaphoreTake(sendingCount, 0); // don't need to wait
      }

      return readBytes;
    }
  };
}