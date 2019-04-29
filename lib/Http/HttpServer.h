#include <ESPAsyncWebServer.h>
#include <Settings.h>
#include <ArduCAM.h>
#include <MotorControler.h>
#include <CameraController.h>
#include <AudioController.h>
#include <PathVariableHandler.h>
#include <RichHttpServer.h>

#if defined(ESP32)
extern "C" {
  #include "freertos/semphr.h"
}
#endif

#ifndef _HTTP_SERVER_H
#define _HTTP_SERVER_H

class HttpServer {
public:
  HttpServer(Settings& settings, CameraController& camera, MotorController& motor, AudioController& audio);

  void begin();

private:
  RichHttpServer server;
  Settings& settings;
  MotorController& motor;
  CameraController& camera;
  AudioController& audio;

  void staticResponse(AsyncWebServerRequest*, const char* contentType, const char* response);
  void noOpHandler(AsyncWebServerRequest*);

  // Settings CRUD
  String getSettingsBody();
  void handleUpdateSettings(
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    size_t index,
    size_t total
  );
  void handleListSettings(AsyncWebServerRequest*);

  // Camera
  void handleGetCameraStill(AsyncWebServerRequest*);
  void handleGetCameraStream(AsyncWebServerRequest*);

  // Motor
  void handlePostMotorCommand(
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    size_t index,
    size_t total
  );

  // Audio
  void handleDeleteSound(AsyncWebServerRequest*, const UrlTokenBindings*);
  void handleShowSound(AsyncWebServerRequest*, const UrlTokenBindings*);
  void handlePostAudioCommand(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);

  // OTA updates
  void handleOtaUpdate(
    AsyncWebServerRequest *request,
    const String &filename,
    size_t index,
    uint8_t *data,
    size_t len,
    bool isFinal,
    const UrlTokenBindings* bindings
  );
  void handleOtaSuccess(AsyncWebServerRequest*, const UrlTokenBindings* bindings);

  // General helpers
  void handleListDirectory(AsyncWebServerRequest*, const char* dir);
  void handleCreateFile(
    const char* filePrefix,
    AsyncWebServerRequest *request,
    const String& filename,
    size_t index,
    uint8_t *data,
    size_t len,
    bool isFinal
  );

  // Checks if auth is enabled, and requires appropriate username/password if so
  bool isAuthenticated(AsyncWebServerRequest* request);

  // Support for routes with tokens like a/:id/:id2. Injects auth handling.
  // void onPattern(const String& pattern, const WebRequestMethod method, PathVariableHandler::TPathVariableHandlerFn fn);
  // void onPattern(const String& pattern, const WebRequestMethod method, PathVariableHandler::TPathVariableHandlerBodyFn fn);

  // Injects auth handling
  // void on(const String& pattern, const WebRequestMethod method, ArRequestHandlerFunction fn);
  // void on(const String& pattern, const WebRequestMethod method, ArBodyHandlerFunction fn);
  // void onUpload(const String& pattern, const WebRequestMethod method, ArUploadHandlerFunction uploadFn);
  // void onUpload(const String& pattern, const WebRequestMethod method, ArRequestHandlerFunction onCompleteFn, ArUploadHandlerFunction uploadFn);
};

#endif