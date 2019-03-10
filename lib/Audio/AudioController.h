#include <Settings.h>

#include <AudioFileSourceSPIFFS.h>
#include <AudioFileSourceID3.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2SNoDAC.h>

#include <ArduinoJson.h>

#if defined(ESP32)
#include <SPIFFS.h>
extern "C" {
  #include "freertos/semphr.h"
}
#endif

#ifndef _AUDIO_CONTROLLER_H
#define _AUDIO_CONTROLLER_H

class AudioController {
public:
  AudioController(Settings& settings);
  ~AudioController();

  void playMP3FromSpiffs(const String& filename);
  void init();
  void loop();
  void enable();
  void disable();

  bool handleCommand(const JsonObject& json);

private:
  Settings& settings;

  AudioOutputI2S* audioOutput;
  AudioGenerator* audioGenerator;
  AudioFileSource* audioSource;

  String filenameToPlay;
  volatile bool requestPending;

  SemaphoreHandle_t mutex;
};

#endif