#include <ESPAsyncWebServer.h>
#include <Settings.h>
#include <ArduCAM.h>
#include <MotorControler.h>
#include <CameraController.h>
#include <AudioController.h>
#include <RichHttpServer.h>

#if defined(ESP32)
extern "C" {
  #include "freertos/semphr.h"
}
#endif

#ifndef _HTTP_SERVER_H
#define _HTTP_SERVER_H

using RichHttpConfig = RichHttp::Generics::Configs::AsyncWebServer;
using RequestContext = RichHttpConfig::RequestContextType;

class HttpServer {
public:
  HttpServer(Settings& settings, CameraController& camera, MotorController& motor, AudioController& audio);

  void begin();

private:
  RichHttpServer<RichHttpConfig> server;
  Settings& settings;
  MotorController& motor;
  CameraController& camera;
  AudioController& audio;

  // Settings CRUD
  String getSettingsBody();
  void handleUpdateSettings(RequestContext& request);
  void handleListSettings(RequestContext& request);

  // Camera
  void handleGetCameraStill(RequestContext& request);
  void handleGetCameraStream(RequestContext& request);

  // Motor
  void handlePostMotorCommand(RequestContext& request);

  // Audio
  void handleDeleteSound(RequestContext& request);
  void handleShowSound(RequestContext& request);
  void handlePostAudioCommand(RequestContext& request);

  // General helpers
  void handleListDirectory(const char* dir, RequestContext& request);
  void handleCreateFile(const char* filePrefix, RequestContext& request);
};

#endif