#include <SPI.h>
#include <Bleeper.h>
#include <ArduinoJson.h>
#include <HttpServer.h>
#include <AuthProviders.h>

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
  : settings(settings)
  , authProvider(settings.http)
  , server(RichHttpServer<RichHttpConfig>(settings.http.port, authProvider))
  , motor(motor)
  , camera(camera)
  , audio(audio)
{ }

void HttpServer::begin() {
  server
    .buildHandler("/settings")
    .on(HTTP_GET, std::bind(&HttpServer::handleListSettings, this, _1))
    .on(HTTP_PUT, std::bind(&HttpServer::handleUpdateSettings, this, _1));

  server
    .buildHandler("/camera/snapshot.jpg")
    .on(HTTP_GET, std::bind(&HttpServer::handleGetCameraStill, this, _1));
  server
    .buildHandler("/camera/stream.mjpg")
    .on(HTTP_GET, std::bind(&HttpServer::handleGetCameraStream, this, _1));

  server
    .buildHandler("/motor/commands")
    .on(HTTP_POST, std::bind(&HttpServer::handlePostMotorCommand, this, _1));

  server
    .buildHandler("/sounds/:filename")
    .on(HTTP_DELETE, std::bind(&HttpServer::handleDeleteSound, this, _1))
    .on(HTTP_GET, std::bind(&HttpServer::handleShowSound, this, _1));

  // Must go last, or it'll match /sounds/XXX
  server
    .buildHandler("/sounds")
    .on(HTTP_GET, std::bind(&HttpServer::handleListDirectory, this, SOUNDS_DIRECTORY, _1))
    .on(HTTP_POST, std::bind(&HttpServer::handleCreateFile, this, SOUNDS_DIRECTORY, _1));

  server
    .buildHandler("/firmware")
    .handleOTA();

  server
    .buildHandler("/about")
    .on(HTTP_GET, std::bind(&HttpServer::handleAbout, this, _1));

  server
    .buildHandler("/audio/commands")
    .on(HTTP_POST, std::bind(&HttpServer::handlePostAudioCommand, this, _1));

  server.clearBuilders();
  server.begin();
}

void HttpServer::handleShowSound(RequestContext& request) {
  const char* filename = request.pathVariables.get("filename");
  String path = String(SOUNDS_DIRECTORY) + "/" + filename;
  request.rawRequest->send(SPIFFS, path, F("audio/mpeg"));
}

void HttpServer::handleDeleteSound(RequestContext& request) {
  const char* filename = request.pathVariables.get("filename");
  String path = String(SOUNDS_DIRECTORY) + "/" + filename;

  if (SPIFFS.exists(path)) {
    if (SPIFFS.remove(path)) {
      request.response.json["success"] = true;
    } else {
      request.response.setCode(500);
      request.response.json["success"] = false;
      request.response.json["error"] = F("Failed to delete file");
    }
  } else {
    request.response.setCode(404);
    request.response.json["success"] = false;
    request.response.json["error"] = F("File not found");
  }
}

void HttpServer::handlePostMotorCommand(RequestContext& request) {
  JsonObject body = request.getJsonBody().as<JsonObject>();

  if (motor.jsonCommand(body)) {
    request.response.json["success"] = true;
  } else {
    request.response.setCode(400);
    request.response.json["error"] = "Invalid command";
  }
}

void HttpServer::handlePostAudioCommand(RequestContext& request) {
  JsonObject body = request.getJsonBody().as<JsonObject>();

  if (audio.handleCommand(body)) {
    request.response.json["success"] = true;
  } else {
    request.response.json["error"] = F("Invalid command");
    request.response.setCode(400);
  }
}

void HttpServer::handleGetCameraStream(RequestContext& request) {
  CameraController::CameraStream& cameraStream = camera.openCaptureStream();

  auto* response = request.rawRequest->beginChunkedResponse("multipart/x-mixed-replace; boundary=frame", [this, &cameraStream](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
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

  request.rawRequest->send(response);
}

void HttpServer::handleGetCameraStill(RequestContext& request) {
  CameraController::CameraStream& cameraStream = camera.openCaptureStream();

  auto* response = request.rawRequest->beginChunkedResponse("image/jpeg", [this, &cameraStream](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
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

  request.rawRequest->send(response);
}

void HttpServer::handleUpdateSettings(RequestContext& request) {
  JsonObject req = request.getJsonBody().as<JsonObject>();

  if (req.isNull()) {
    request.response.json["error"] = "Invalid JSON";
    request.response.setCode(400);
    return;
  }

  ConfigurationDictionary params;

  for (JsonObject::iterator it = req.begin(); it != req.end(); ++it) {
    params[it->key().c_str()] = it->value().as<String>();
  }

  settings.setFromDictionary(params);
  Bleeper.storage.persist();

  handleListSettings(request);
}

void HttpServer::handleListSettings(RequestContext& request) {
  request.response.sendRaw(200, APPLICATION_JSON, getSettingsBody().c_str());
}

String HttpServer::getSettingsBody() {
  DynamicJsonDocument json(2048);

  ConfigurationDictionary params = settings.getAsDictionary(true);
  for (std::map<String, String>::const_iterator it = params.begin(); it != params.end(); ++it) {
    json[it->first] = it->second;
  }

  String strJson;
  serializeJson(json, strJson);

  return strJson;
}

void HttpServer::handleAbout(RequestContext& request) {
  // Measure before allocating buffers
  uint32_t freeHeap = ESP.getFreeHeap();

  request.response.json["version"] = QUOTE(TREAT_DISPENSER_VERSION);
  request.response.json["variant"] = QUOTE(FIRMWARE_VARIANT);
  request.response.json["free_heap"] = freeHeap;
  request.response.json["sdk_version"] = ESP.getSdkVersion();
}

////////============== Handler wrappers

void HttpServer::handleCreateFile(const char* filePrefix, RequestContext& request) {
  static File updateFile;

  if (request.upload.index == 0) {
    String path = String(filePrefix) + "/" + request.upload.filename;
    updateFile = SPIFFS.open(path, FILE_WRITE);
    Serial.println(F("Writing to file: "));
    Serial.println(path);

    if (!updateFile) {
      request.response.json["error"] = F("Failed to open file");
      request.response.setCode(500);
      return;
    }
  }

  if (!updateFile || updateFile.write(request.upload.data, request.upload.length) != request.upload.length) {
    request.response.json["error"] = F("Failed to write to file");
    request.response.setCode(500);
  }

  if (updateFile && request.upload.isFinal) {
    updateFile.close();
    request.response.json["success"] = true;
  }
}

void HttpServer::handleListDirectory(const char* dirName, RequestContext& request) {
  JsonArray response = request.response.json.to<JsonArray>();

#if defined(ESP8266)
  Dir dir = SPIFFS.openDir(dirName);

  while (dir.next()) {
    JsonObject file = response.createNestedObject();
    file["name"] = dir.fileName();
    file["size"] = dir.fileSize();
  }
#elif defined(ESP32)
  File dir = SPIFFS.open(dirName);

  if (!dir || !dir.isDirectory()) {
    Serial.print(F("Path is not a directory - "));
    Serial.println(dirName);

    request.response.setCode(500);
    request.response.json["error"] = F("Expected path to be a directory, but wasn't");
    return;
  }

  while (File dirFile = dir.openNextFile()) {
    JsonObject file = response.createNestedObject();

    file["name"] = String(dirFile.name());
    file["size"] = dirFile.size();
  }
#endif
}