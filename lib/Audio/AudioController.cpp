#include <AudioController.h>

#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

AudioController::AudioController(Settings& settings)
  : settings(settings),
    audioGenerator(NULL),
    audioOutput(NULL),
    audioSource(std::make_shared<AudioFileSourceSPIFFS>()),
    mutex(xSemaphoreCreateMutex()),
    requestPending(false)
{
  audioOutput = std::make_shared<AudioOutputI2S>(0, AudioOutputI2S::INTERNAL_DAC);
}

AudioController::~AudioController() {
}

void AudioController::init() {
  pinMode(settings.audio.enable_pin, OUTPUT);
  disable();
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
      audioGenerator = std::make_shared<AudioGeneratorMP3>();
      audioGenerator->begin(audioSource.get(), audioOutput.get());

      xSemaphoreGive(mutex);
    }
  }

  if (audioGenerator && audioGenerator->isRunning()) {
    if (! audioGenerator->loop()) {
      disable();
      audioGenerator->stop();
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