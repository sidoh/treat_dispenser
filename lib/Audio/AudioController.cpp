#include <AudioController.h>

#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

AudioController::AudioController(Settings& settings) 
  : settings(settings),
    audioGenerator(NULL),
    audioOutput(NULL),
    audioSource(new AudioFileSourceSPIFFS()),
    mutex(xSemaphoreCreateMutex()),
    requestPending(false)
{ 
  audioOutput = new AudioOutputI2S(0, AudioOutputI2S::INTERNAL_DAC);
}

AudioController::~AudioController() {
  delete audioOutput;
}

void AudioController::begin() {
  pinMode(settings.audio.enable_pin, OUTPUT);
  // disable();
  enable();
}

void AudioController::enable() {
  digitalWrite(settings.audio.enable_pin, HIGH);
}

void AudioController::disable() {
  digitalWrite(settings.audio.enable_pin, LOW);
}

void AudioController::playMP3FromSpiffs(const String& filename) {
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    requestPending = true;
    filenameToPlay = filename;

    xSemaphoreGive(mutex);
  }
}

void AudioController::loop() {
  if (requestPending) {
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
      requestPending = false;

      enable();

      printf_P(PSTR("Playing filename: %s, free heap: %d\n"), filenameToPlay.c_str(), ESP.getFreeHeap());

      audioSource->open(filenameToPlay.c_str());
      audioGenerator = new AudioGeneratorMP3();
      audioGenerator->begin(audioSource, audioOutput);

      xSemaphoreGive(mutex);
    }
  }

  if (audioGenerator && audioGenerator->isRunning()) {
    if (! audioGenerator->loop()) {
      disable();
      audioGenerator->stop();

      delete audioGenerator;
      audioGenerator = NULL;

      Serial.println(F("Audio not looping"));
    }
  } else {
    disable();
  }
}

bool AudioController::handleCommand(const JsonObject& json) {
  if (!json.containsKey("file")) {
    Serial.println(F("Invalid audio command -- required key `file` does not exist"));
    return false;
  }

  playMP3FromSpiffs(json["file"]);

  return true;
}