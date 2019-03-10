#include <SPI.h>
#include <Bleeper.h>
#include <ArduinoJson.h>
#include <HttpServer.h>

#if defined(ESP8266)
#include <Updater.h>
#elif defined(ESP32)
#include <Update.h>
#endif

static const char APPLICATION_JSON[] = "application/json";
static const char TEXT_PLAIN[] = "text/plain";

static const char JPEG_CONTENT_TYPE_HEADER[] PROGMEM = "--frame\r\nContent-Type: image/jpeg\r\n\r\n";

HttpServer::HttpServer(Settings& settings, CameraController& camera, MotorController& motor, AudioController& audio) 
  : audio(audio),
    camera(camera),
    motor(motor),
    server(AsyncWebServer(settings.http.port)),
    settings(settings)
{ }

void HttpServer::begin() {
  on("/settings", HTTP_GET, handleListSettings());
  on("/settings", HTTP_PUT, handleUpdateSettings());

  on("/camera/snapshot.jpg", HTTP_GET, handleGetCameraStill());
  on("/camera/stream.mjpg", HTTP_GET, handleGetCameraStream());

  on("/motor/commands", HTTP_POST, handlePostMotorCommand());

  onUpload("/sounds", HTTP_POST, handleCreateFile(SOUNDS_DIRECTORY));
  onPattern("/sounds/:filename", HTTP_DELETE, handleDeleteSound());
  onPattern("/sounds/:filename", HTTP_GET, handleShowSound());
  // Must go last, or it'll match /sounds/XXX
  on("/sounds", HTTP_GET, handleListDirectory(SOUNDS_DIRECTORY));

  on("/audio/commands", HTTP_POST, handlePostAudioCommand());

  onUpload("/firmware", HTTP_POST, handleOtaSuccess(), handleOtaUpdate());

  server.begin();
}

ArRequestHandlerFunction HttpServer::handleOtaSuccess() {
  return [this](AsyncWebServerRequest* request) {
    request->send_P(200, TEXT_PLAIN, PSTR("Update successful.  Device will now reboot.\n\n"));

    delay(1000);

    ESP.restart();
  };
}

ArUploadHandlerFunction HttpServer::handleOtaUpdate() {
  return [this](
    AsyncWebServerRequest *request,
    const String& filename,
    size_t index,
    uint8_t *data,
    size_t len,
    bool isFinal
  ) {
    if (index == 0) {
      if (request->contentLength() > 0) {
        Update.begin(request->contentLength());
      } else {
        Serial.println(F("OTA Update: ERROR - Content-Length header required, but not present."));
      }
    }

    if (Update.size() > 0) {
      if (Update.write(data, len) != len) {
        Update.printError(Serial);

#if defined(ESP32)
        Update.abort();
#endif
      }

      if (isFinal) {
        if (!Update.end(true)) {
          Update.printError(Serial);
#if defined(ESP32)
          Update.abort();
#endif
        }
      }
    }
  };
}

PatternHandler::TPatternHandlerFn HttpServer::handleShowSound() {
  return [this](const UrlTokenBindings* bindings, AsyncWebServerRequest* request) {
    if (bindings->hasBinding("filename")) {
      const char* filename = bindings->get("filename");
      String path = String(SOUNDS_DIRECTORY) + "/" + filename;

      request->send(SPIFFS, path, "audio/mpeg");
    } else {
      request->send_P(400, TEXT_PLAIN, PSTR("You must provide a filename"));
    }
  };
}

PatternHandler::TPatternHandlerFn HttpServer::handleDeleteSound() {
  return [this](const UrlTokenBindings* bindings, AsyncWebServerRequest* request) {
    if (bindings->hasBinding("filename")) {
      const char* filename = bindings->get("filename");
      String path = String(SOUNDS_DIRECTORY) + "/" + filename;

      if (SPIFFS.exists(path)) {
        if (SPIFFS.remove(path)) {
          request->send_P(200, TEXT_PLAIN, PSTR("success"));
        } else {
          request->send_P(500, TEXT_PLAIN, PSTR("Failed to delete file"));
        }
      } else {
        request->send(404, TEXT_PLAIN);
      }
    } else {
      request->send_P(400, TEXT_PLAIN, PSTR("You must provide a filename"));
    }
  };
}

ArBodyHandlerFunction HttpServer::handlePostMotorCommand() {
  return [this](
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    size_t index,
    size_t total
  ) {
    DynamicJsonBuffer buffer;
    JsonObject& body = buffer.parse(data);

    if (motor.jsonCommand(body)) {
      request->send(200);
    } else {
      request->send_P(400, TEXT_PLAIN, PSTR("Invalid command"));
    }
  };
}

ArBodyHandlerFunction HttpServer::handlePostAudioCommand() {
  return [this](
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    size_t index,
    size_t total
  ) {
    DynamicJsonBuffer buffer;
    JsonObject& body = buffer.parse(data);

    if (audio.handleCommand(body)) {
      request->send(200);
    } else {
      request->send_P(400, TEXT_PLAIN, PSTR("Invalid command"));
    }
  };
}

ArRequestHandlerFunction HttpServer::handleGetCameraStream() {
  return [this](AsyncWebServerRequest* request) {
    CameraController::CameraStream& cameraStream = camera.openCaptureStream();

    auto* response = request->beginChunkedResponse("multipart/x-mixed-replace; boundary=frame", [this, &cameraStream](uint8_t *buffer, size_t maxLen, size_t index) -> size_t { 
      size_t i = 0;

      if (index == 0 || !cameraStream.available()) {
        strcpy_P((char*)buffer, JPEG_CONTENT_TYPE_HEADER);
        i = strlen_P(JPEG_CONTENT_TYPE_HEADER);
      }

      if (!cameraStream.available()) {
        cameraStream.reset();
      }

      for (; i < maxLen && cameraStream.available(); i++) {
        buffer[i] = cameraStream.read();
      }

      return i;
    });

    request->send(response);
  };
}
ArRequestHandlerFunction HttpServer::handleGetCameraStill() {
  return [this](AsyncWebServerRequest* request) {
    Stream& cameraStream = camera.openCaptureStream();

    auto* response = request->beginChunkedResponse("image/jpeg", [this, &cameraStream](uint8_t *buffer, size_t maxLen, size_t index) -> size_t { 

      for (size_t i = 0; i < maxLen; i++) {
        if (cameraStream.available()) {
          buffer[i] = cameraStream.read();
        } else {
          return i;
        }
      }

      return maxLen;
    });

    request->send(response);
  };
}

ArBodyHandlerFunction HttpServer::handleUpdateSettings() {
  return [this](
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    size_t index,
    size_t total
  ) {
    DynamicJsonBuffer buffer;
    JsonObject& req = buffer.parse(data);

    if (! req.success()) {
      request->send_P(400, TEXT_PLAIN, PSTR("Invalid JSON"));
      return;
    }

    ConfigurationDictionary params;

    for (JsonObject::iterator it = req.begin(); it != req.end(); ++it) {
      params[it->key] = it->value.as<String>();
    }

    settings.setFromDictionary(params);
    Bleeper.storage.persist();

    request->send(200, APPLICATION_JSON, getSettingsBody().c_str());
  };
}

ArRequestHandlerFunction HttpServer::handleListSettings() {
  return [this](AsyncWebServerRequest* request) {
    request->send(200, APPLICATION_JSON, getSettingsBody().c_str());
  };
}

String HttpServer::getSettingsBody() {
  DynamicJsonBuffer buffer;
  JsonObject& json = buffer.createObject();

  ConfigurationDictionary params = settings.getAsDictionary(true);
  for (std::map<String, String>::const_iterator it = params.begin(); it != params.end(); ++it) {
    json[it->first] = it->second;
  }

  String strJson;
  json.printTo(strJson);

  return strJson;
}

////////============== Handler wrappers

void HttpServer::on(const String& path, const WebRequestMethod method, ArRequestHandlerFunction fn) {
  ArRequestHandlerFunction authedFn = [this, fn](AsyncWebServerRequest* request) {
    if (isAuthenticated(request)) {
      fn(request);
    }
  };

  server.on(path.c_str(), method, authedFn);
}

void HttpServer::on(const String& path, const WebRequestMethod method, ArBodyHandlerFunction fn) {
  ArBodyHandlerFunction authedFn = [this, fn](
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    size_t index,
    size_t total
  ) {
    if (isAuthenticated(request)) {
      fn(request, data, len, index, total);
    }
  };

  server.addHandler(new HttpServer::BodyHandler(path.c_str(), method, authedFn));
}

void HttpServer::onUpload(const String& path, const WebRequestMethod method, ArUploadHandlerFunction fn) {
  ArUploadHandlerFunction authedFn = [this, fn](
    AsyncWebServerRequest *request,
    const String& filename,
    size_t index,
    uint8_t *data,
    size_t len,
    bool isFinal
  ) {
    if (isAuthenticated(request)) {
      fn(request, filename, index, data, len, isFinal);
    }
  };

  server.addHandler(new HttpServer::UploadHandler(path.c_str(), method, authedFn));
}

void HttpServer::onUpload(const String& path, const WebRequestMethod method, ArRequestHandlerFunction onCompleteFn, ArUploadHandlerFunction fn) {
  ArUploadHandlerFunction authedFn = [this, fn](
    AsyncWebServerRequest *request,
    const String& filename,
    size_t index,
    uint8_t *data,
    size_t len,
    bool isFinal
  ) {
    if (isAuthenticated(request)) {
      fn(request, filename, index, data, len, isFinal);
    }
  };

  ArRequestHandlerFunction authedOnCompleteFn = [this, onCompleteFn](AsyncWebServerRequest* request) {
    if (isAuthenticated(request)) {
      onCompleteFn(request);
    }
  };

  server.addHandler(new HttpServer::UploadHandler(path.c_str(), method, authedOnCompleteFn, authedFn));
}

bool HttpServer::isAuthenticated(AsyncWebServerRequest* request) {
  if (settings.http.hasAuthSettings()) {
    if (request->authenticate(settings.http.username.c_str(), settings.http.password.c_str())) {
      return true;
    } else {
      request->send_P(403, TEXT_PLAIN, PSTR("Authentication required"));
      return false;
    }
  } else {
    return true;
  }
}

HttpServer::BodyHandler::BodyHandler(
  const char* uri,
  const WebRequestMethod method,
  ArBodyHandlerFunction handler
) : uri(new char[strlen(uri) + 1]),
    method(method),
    handler(handler)
{
  strcpy(this->uri, uri);
}

HttpServer::BodyHandler::~BodyHandler() {
  delete uri;
}

bool HttpServer::BodyHandler::canHandle(AsyncWebServerRequest *request) {
  if (this->method != HTTP_ANY && this->method != request->method()) {
    return false;
  }

  return request->url() == this->uri;
}

void HttpServer::BodyHandler::handleRequest(AsyncWebServerRequest* request) {
  request->send(200);
}

void HttpServer::BodyHandler::handleBody(
  AsyncWebServerRequest *request,
  uint8_t *data,
  size_t len,
  size_t index,
  size_t total
) {
  handler(request, data, len, index, total);
}

HttpServer::UploadHandler::UploadHandler(
  const char* uri,
  const WebRequestMethod method,
  ArRequestHandlerFunction onCompleteFn,
  ArUploadHandlerFunction handler
) : uri(new char[strlen(uri) + 1]),
    method(method),
    handler(handler),
    onCompleteFn(onCompleteFn)
{
  strcpy(this->uri, uri);
}

HttpServer::UploadHandler::UploadHandler(
  const char* uri,
  const WebRequestMethod method,
  ArUploadHandlerFunction handler
) : UploadHandler(uri, method, NULL, handler)
{ }

HttpServer::UploadHandler::~UploadHandler() {
  delete uri;
}

bool HttpServer::UploadHandler::canHandle(AsyncWebServerRequest *request) {
  if (this->method != HTTP_ANY && this->method != request->method()) {
    return false;
  }

  return request->url() == this->uri;
}

void HttpServer::UploadHandler::handleUpload(
  AsyncWebServerRequest *request,
  const String &filename,
  size_t index,
  uint8_t *data,
  size_t len,
  bool isFinal
) {
  handler(request, filename, index, data, len, isFinal);
}

void HttpServer::UploadHandler::handleRequest(AsyncWebServerRequest* request) {
  if (onCompleteFn == NULL) {
    request->send(200);
  } else {
    onCompleteFn(request);
  }
}

ArUploadHandlerFunction HttpServer::handleCreateFile(const char* filePrefix) {
  return [this, filePrefix](
    AsyncWebServerRequest *request,
    const String& filename,
    size_t index,
    uint8_t *data,
    size_t len,
    bool isFinal
  ) {
    static File updateFile;

    if (index == 0) {
      String path = String(filePrefix) + "/" + filename;
      updateFile = SPIFFS.open(path, FILE_WRITE);
      Serial.println(F("Writing to file: "));
      Serial.println(path);

      if (!updateFile) {
        Serial.println(F("Failed to open file"));
        request->send(500);
        return;
      }
    }

    if (!updateFile || updateFile.write(data, len) != len) {
      Serial.println(F("Failed to write to file"));
      request->send(500);
    }

    if (updateFile && isFinal) {
      updateFile.close();
      request->send(200);
    }
  };
}

ArRequestHandlerFunction HttpServer::handleListDirectory(const char* dirName) {
  return [this, dirName](AsyncWebServerRequest* request) {
    DynamicJsonBuffer buffer;
    JsonArray& responseObj = buffer.createArray();

#if defined(ESP8266)
    Dir dir = SPIFFS.openDir(dirName);

    while (dir.next()) {
      JsonObject& file = buffer.createObject();
      file["name"] = dir.fileName();
      file["size"] = dir.fileSize();
      responseObj.add(file);
    }
#elif defined(ESP32)
    File dir = SPIFFS.open(dirName);

    if (!dir || !dir.isDirectory()) {
      Serial.print(F("Path is not a directory - "));
      Serial.println(dirName);

      request->send_P(500, TEXT_PLAIN, PSTR("Expected path to be a directory, but wasn't"));
      return;
    }

    while (File dirFile = dir.openNextFile()) {
      JsonObject& file = buffer.createObject();

      file["name"] = String(dirFile.name());
      file["size"] = dirFile.size();

      responseObj.add(file);
    }
#endif

    String response;
    responseObj.printTo(response);

    request->send(200, APPLICATION_JSON, response);
  };
}

void HttpServer::onPattern(const String& pattern, const WebRequestMethod method, PatternHandler::TPatternHandlerFn fn) {
  PatternHandler::TPatternHandlerFn authedFn = [this, fn](const UrlTokenBindings* b, AsyncWebServerRequest* request) {
    if (isAuthenticated(request)) {
      fn(b, request);
    }
  };

  server.addHandler(new PatternHandler(pattern.c_str(), method, authedFn, NULL));
}

void HttpServer::onPattern(const String& pattern, const WebRequestMethod method, PatternHandler::TPatternHandlerBodyFn fn) {
  PatternHandler::TPatternHandlerBodyFn authedFn = [this, fn](
    const UrlTokenBindings* bindings,
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    size_t index,
    size_t total
  ) {
    if (isAuthenticated(request)) {
      fn(bindings, request, data, len, index, total);
    }
  };

  server.addHandler(new PatternHandler(pattern.c_str(), method, NULL, authedFn));
}