#include <CameraController.h>
#include <SPI.h>

CameraController::CameraController(Settings& settings, ArduCAM& camera)
  : camera(camera),
    settings(settings),
    captureStream(CameraStream(camera))
{ 
  TaskHandle_t xHandle = NULL;
  xTaskCreate(
    &captureStream.taskImpl,
    "ArduCAM_Capture",
    2048,
    (void*)(&captureStream),
    1,
    &xHandle
  );
}

CameraController::CameraStream& CameraController::openCaptureStream() {
  captureStream.reset();
  return captureStream;
}

void CameraController::CameraStream::taskImpl(void* _this) {
  ((CameraController::CameraStream*)_this)->taskImpl();
}

void CameraController::CameraStream::taskImpl() {
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
    }
  }
}

CameraController::CameraStream::CameraStream(ArduCAM& camera) 
  : bufferIx(CAMERA_BUFFER_SIZE),
    camera(camera),
    bytesRemaining(0)
{ }

void CameraController::CameraStream::reset() {
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