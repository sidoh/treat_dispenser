#include <SPI.h>
#include <Bleeper.h>
#include <ArduinoJson.h>
#include <HttpServer.h>

#if defined(ESP8266)
#include <Updater.h>
#elif defined(ESP32)
#include <Update.h>
#endif

using namespace std::placeholders;

static const char APPLICATION_JSON[] = "application/json";
static const char TEXT_PLAIN[] = "text/plain";

static const char JPEG_CONTENT_TYPE_HEADER[] PROGMEM = "--frame\r\nContent-Type: image/jpeg\r\n\r\n";

HttpServer::HttpServer(Settings& settings, CameraController& camera, MotorController& motor, AudioController& audio)
  : audio(audio),
    camera(camera),
    motor(motor),
    server(RichHttpServer(settings.http.port)),
    settings(settings)
{ }

void HttpServer::begin() {
  server
    .buildHandler("/settings")
    .on(HTTP_GET, std::bind(&HttpServer::handleListSettings, this, _1))
    .on(
      HTTP_PUT,
      std::bind(&HttpServer::noOpHandler, this, _1),
      std::bind(&HttpServer::handleUpdateSettings, this, _1, _2, _3, _4, _5)
    );

  server
    .buildHandler("/camera")
    .on(HTTP_GET, "/snapshot.jpg", std::bind(&HttpServer::handleGetCameraStill, this, _1))
    .on(HTTP_GET, "/stream.mjpg", std::bind(&HttpServer::handleGetCameraStream, this, _1));

  server
    .buildHandler("/motor/commands")
    .on(
      HTTP_POST,
      std::bind(&HttpServer::noOpHandler, this, _1),
      std::bind(&HttpServer::handlePostMotorCommand, this, _1, _2, _3, _4, _5)
    );

  server
    .buildHandler("/sounds/:filename")
    .on(HTTP_DELETE, std::bind(&HttpServer::handleDeleteSound, this, _1, _2))
    .on(HTTP_GET, std::bind(&HttpServer::handleShowSound, this, _1, _2));

  // Must go last, or it'll match /sounds/XXX
  server
    .buildHandler("/sounds")
    .on(HTTP_GET, std::bind(&HttpServer::handleListDirectory, this, _1, SOUNDS_DIRECTORY))
    .on(
      HTTP_POST,
      std::bind(&HttpServer::staticResponse, this, _1, TEXT_PLAIN, "success"),
      std::bind(&HttpServer::handleCreateFile, this, SOUNDS_DIRECTORY, _1, _2, _3, _4, _5, _6)
    );

  server
    .buildHandler("/firmware")
    .on(
      HTTP_POST,
      std::bind(&HttpServer::handleOtaSuccess, this, _1, _2),
      std::bind(&HttpServer::handleOtaUpdate, this, _1, _2, _3, _4, _5, _6, _7)
    );

  server
    .buildHandler("/audio/commands")
    .on(
      HTTP_POST,
      std::bind(&HttpServer::staticResponse, this, _1, TEXT_PLAIN, "success"),
      std::bind(&HttpServer::handlePostAudioCommand, this, _1, _2, _3, _4, _5)
    );

  server.begin();
}

void HttpServer::staticResponse(AsyncWebServerRequest* request, const char* responseType, const char* response) {
  request->send(200, responseType, response);
}

void HttpServer::noOpHandler(AsyncWebServerRequest* request) { }

void HttpServer::handleOtaSuccess(AsyncWebServerRequest* request, const UrlTokenBindings* bindings) {
  request->send_P(200, TEXT_PLAIN, PSTR("Update successful.  Device will now reboot.\n\n"));

  delay(1000);

  ESP.restart();
}

void HttpServer::handleOtaUpdate(
  AsyncWebServerRequest *request,
  const String &filename,
  size_t index,
  uint8_t *data,
  size_t len,
  bool isFinal,
  const UrlTokenBindings* bindings
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
}

void HttpServer::handleShowSound(AsyncWebServerRequest* request, const UrlTokenBindings* bindings) {
  if (bindings->hasBinding("filename")) {
    const char* filename = bindings->get("filename");
    String path = String(SOUNDS_DIRECTORY) + "/" + filename;

    request->send(SPIFFS, path, "audio/mpeg");
  } else {
    request->send_P(400, TEXT_PLAIN, PSTR("You must provide a filename"));
  }
}

void HttpServer::handleDeleteSound(AsyncWebServerRequest* request, const UrlTokenBindings* bindings) {
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
}

void HttpServer::handlePostMotorCommand(
  AsyncWebServerRequest* request,
  uint8_t* data,
  size_t len,
  size_t index,
  size_t total
){
  DynamicJsonBuffer buffer;
  JsonObject& body = buffer.parse(data);

  if (motor.jsonCommand(body)) {
    request->send(200);
  } else {
    request->send_P(400, TEXT_PLAIN, PSTR("Invalid command"));
  }
}

void HttpServer::handlePostAudioCommand(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  DynamicJsonBuffer buffer;
  JsonObject& body = buffer.parse(data);

  if (audio.handleCommand(body)) {
    request->send(200);
  } else {
    request->send_P(400, TEXT_PLAIN, PSTR("Invalid command"));
  }
}

void HttpServer::handleGetCameraStream(AsyncWebServerRequest* request) {
  CameraController::CameraStream& cameraStream = camera.openCaptureStream();

  auto* response = request->beginChunkedResponse("multipart/x-mixed-replace; boundary=frame", [this, &cameraStream](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    size_t i = 0;

    if (index == 0 || !cameraStream.available()) {
      strcpy_P((char*)buffer, JPEG_CONTENT_TYPE_HEADER);
      i = strlen_P(JPEG_CONTENT_TYPE_HEADER);
    }

    if (!cameraStream.available()) {
      cameraStream.close();
      cameraStream.open();
    }

    for (; i < maxLen && cameraStream.available(); i++) {
      buffer[i] = cameraStream.read();
    }

    return i;
  });

  request->send(response);
}

void HttpServer::handleGetCameraStill(AsyncWebServerRequest* request) {
  CameraController::CameraStream& cameraStream = camera.openCaptureStream();

  auto* response = request->beginChunkedResponse("image/jpeg", [this, &cameraStream](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    for (size_t i = 0; i < maxLen; i++) {
      if (cameraStream.available()) {
        buffer[i] = cameraStream.read();
      } else {
        cameraStream.close();
        return i;
      }
    }

    return maxLen;
  });

  request->send(response);
}

void HttpServer::handleUpdateSettings(
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
}

void HttpServer::handleListSettings(AsyncWebServerRequest* request) {
  request->send(200, APPLICATION_JSON, getSettingsBody().c_str());
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

void HttpServer::handleCreateFile(
  const char* filePrefix,
  AsyncWebServerRequest *request,
  const String& filename,
  size_t index,
  uint8_t *data,
  size_t len,
  bool isFinal//,
  // const UrlTokenBindings* bindings
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
}

void HttpServer::handleListDirectory(AsyncWebServerRequest* request, const char* dirName) {
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
}