#include <ESPAsyncWebServer.h>
#include <Settings.h>
#include <ArduCAM.h>
#include <MotorControler.h>
#include <CameraController.h>
#include <AudioController.h>
#include <PatternHandler.h>

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
  AsyncWebServer server;
  Settings& settings;
  MotorController& motor;
  CameraController& camera;
  AudioController& audio;

  // Settings CRUD
  String getSettingsBody();
  ArBodyHandlerFunction    handleUpdateSettings();
  ArRequestHandlerFunction handleListSettings();

  // Camera
  ArRequestHandlerFunction handleGetCameraStill();
  ArRequestHandlerFunction handleGetCameraStream();

  // Motor
  ArBodyHandlerFunction    handlePostMotorCommand();

  // Audio
  PatternHandler::TPatternHandlerFn handleDeleteSound();
  PatternHandler::TPatternHandlerFn handleShowSound();
  ArBodyHandlerFunction             handlePostAudioCommand();

  // OTA updates
  ArUploadHandlerFunction  handleOtaUpdate();
  ArRequestHandlerFunction handleOtaSuccess();

  class BodyHandler : public AsyncWebHandler {
  public:
    BodyHandler(const char* uri, const WebRequestMethod method, ArBodyHandlerFunction handler);
    ~BodyHandler();

    virtual bool isRequestHandlerTrivial() override { return false; }
    virtual bool canHandle(AsyncWebServerRequest* request) override;
    virtual void handleRequest(AsyncWebServerRequest* request) override;
    virtual void handleBody(
      AsyncWebServerRequest *request,
      uint8_t *data,
      size_t len,
      size_t index,
      size_t total
    ) override;

  private:
    char* uri;
    const WebRequestMethod method;
    ArBodyHandlerFunction handler;
  };

  class UploadHandler : public AsyncWebHandler {
  public:
    UploadHandler(const char* uri, const WebRequestMethod method, ArUploadHandlerFunction handler);
    UploadHandler(const char* uri, const WebRequestMethod method, ArRequestHandlerFunction onCompleteFn, ArUploadHandlerFunction handler);
    ~UploadHandler();

    virtual bool isRequestHandlerTrivial() override { return false; }
    virtual bool canHandle(AsyncWebServerRequest* request) override;
    virtual void handleRequest(AsyncWebServerRequest* request) override;
    virtual void handleUpload(
      AsyncWebServerRequest *request,
      const String& filename,
      size_t index,
      uint8_t *data,
      size_t len,
      bool isFinal
    ) override;

  private:
    char* uri;
    const WebRequestMethod method;
    ArUploadHandlerFunction handler;
    ArRequestHandlerFunction onCompleteFn;
  };

  // General helpers
  ArRequestHandlerFunction handleListDirectory(const char* dir);
  ArUploadHandlerFunction  handleCreateFile(const char* filePrefix);

  // Checks if auth is enabled, and requires appropriate username/password if so
  bool isAuthenticated(AsyncWebServerRequest* request);

  // Support for routes with tokens like a/:id/:id2. Injects auth handling.
  void onPattern(const String& pattern, const WebRequestMethod method, PatternHandler::TPatternHandlerFn fn);
  void onPattern(const String& pattern, const WebRequestMethod method, PatternHandler::TPatternHandlerBodyFn fn);

  // Injects auth handling
  void on(const String& pattern, const WebRequestMethod method, ArRequestHandlerFunction fn);
  void on(const String& pattern, const WebRequestMethod method, ArBodyHandlerFunction fn);
  void onUpload(const String& pattern, const WebRequestMethod method, ArUploadHandlerFunction uploadFn);
  void onUpload(const String& pattern, const WebRequestMethod method, ArRequestHandlerFunction onCompleteFn, ArUploadHandlerFunction uploadFn);
};

#endif