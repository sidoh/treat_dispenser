#include <Bleeper.h>
#include <MotorTypes.h>
#include <Settings.h>
#include <ArduCAM.h>

#define CAMERA_BUFFER_SIZE 128

#ifndef _CAMERA_CONTROLLER_H
#define _CAMERA_CONTROLLER_H

class CameraController {
public:
  class CameraStream : public Stream {
  public:
    CameraStream(ArduCAM& camera);

    int available();
    int read();
    int peek();
    void flush();
    size_t write(uint8_t byte);

    static void taskImpl(void*);
    void taskImpl();

    void reset();
  private:
    ArduCAM& camera;
    uint8_t buffer[CAMERA_BUFFER_SIZE];

    volatile size_t bufferIx;
    volatile size_t bufferLen;
    volatile size_t bytesRemaining;
    volatile bool isOpen;
  };

  CameraController(Settings& settings, ArduCAM& camera);

  CameraController::CameraStream& openCaptureStream();

private:
  ArduCAM& camera;
  Settings& settings;

  CameraStream captureStream;
};

#endif