#include <CameraController.h>
#include <SPI.h>

#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

CameraController::CameraController(Settings& settings)
  : camera(ArduCAM(OV2640, SS)),
    settings(settings),
    captureStream(CameraStream(camera))
{
  TaskHandle_t xHandle = NULL;
  xTaskCreate(
    &captureStream.taskImpl,
    "ArduCAM_Capture",
    2048,
    (void*)(&captureStream),
    0,
    &xHandle
  );
}

CameraController::CameraStream& CameraController::openCaptureStream() {
  captureStream.open();
  return captureStream;
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

void CameraController::CameraStream::taskImpl(void* _this) {
  ((CameraController::CameraStream*)_this)->taskImpl();
}

void CameraController::CameraStream::taskImpl() {
  TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE;
  TIMERG0.wdt_feed=1;
  TIMERG0.wdt_wprotect=0;
  while (1) {
    if (isOpen && bufferIx >= CAMERA_BUFFER_SIZE && this->bytesRemaining > 0) {
      size_t toRead = std::min((size_t)CAMERA_BUFFER_SIZE, (size_t)this->bytesRemaining);

      SPI.transferBytes(buffer, buffer, toRead);

      bufferLen = toRead;
      bytesRemaining -= toRead;

      if (!bytesRemaining) {
        isOpen = false;
      }

      bufferIx = 0;
    } else {
      vTaskDelay(portTICK_PERIOD_MS / 10);
    }
  }
}

CameraController::CameraStream::CameraStream(ArduCAM& camera)
  : bufferIx(CAMERA_BUFFER_SIZE),
    camera(camera),
    bytesRemaining(0)
{
  cameraLock = xSemaphoreCreateMutex();
}

void CameraController::CameraStream::close() {
  // xSemaphoreGive(cameraLock);
}

void CameraController::CameraStream::open() {
  // xSemaphoreTake(cameraLock, portMAX_DELAY);

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

  isOpen = true;
  bufferIx = CAMERA_BUFFER_SIZE;
}

int CameraController::CameraStream::available() {
  return bufferIx < bufferLen || bytesRemaining > 0;
}

int CameraController::CameraStream::read() {
  while (bufferIx >= bufferLen);

  return buffer[bufferIx++];
}

int CameraController::CameraStream::peek() {
  while (bufferIx >= bufferLen);

  return buffer[bufferIx];
}

void CameraController::CameraStream::flush() {
  // Doesn't do anything
}

size_t CameraController::CameraStream::write(uint8_t byte) {
  // Doesn't do anything
}