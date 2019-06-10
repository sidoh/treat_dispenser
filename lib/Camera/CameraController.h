#include <Bleeper.h>
#include <MotorTypes.h>
#include <Settings.h>
#include <ArduCAM.h>
#include <Wire.h>

// #define CAMERA_BUFFER_SIZE 128
// 40 KB
// There will be two buffers of this size
#define MAX_CAMERA_FRAME_SIZE 40096

#ifndef _CAMERA_CONTROLLER_H
#define _CAMERA_CONTROLLER_H

struct CameraFrame {
  uint8_t bytes[MAX_CAMERA_FRAME_SIZE];
  size_t length;
};

struct CameraBuffer {
  CameraBuffer(const CameraFrame& frame);

  void reset();
  size_t copy(uint8_t* buffer, size_t maxLen);
  bool done();

  const CameraFrame& frame;
  size_t bufferIx;
  bool frameRead;
};

class CameraController {
public:
  typedef std::function<size_t(uint8_t*, size_t, size_t)> CallbackFn;

  class CameraStream {
  public:
    CameraStream(ArduCAM& camera);

    size_t read(uint8_t* buffer, size_t maxLen);
    void open();
    void close();

  private:
    ArduCAM& camera;

    volatile size_t bytesRemaining;
    volatile bool isOpen;
  };

  CameraController(Settings& settings);

  void init();
  const CameraFrame& getCameraFrame();

  CallbackFn chunkedResponseCallback();

private:
  ArduCAM camera;
  Settings& settings;

  static void readCameraFrame(void*);
  void readCameraFrame();

  std::shared_ptr<CameraFrame> cameraFrame;

  CameraStream captureStream;
  TaskHandle_t copyTask;
  SemaphoreHandle_t readFrameMtx;
  SemaphoreHandle_t sendFrameMtx;
  SemaphoreHandle_t sendingCount;
};

#endif