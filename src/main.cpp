#include <Arduino.h>
#include <WiFi.h>
#include <ArduCAM.h>
#include <SPI.h>

#include <HttpServer.h>
#include <WiFiManager.h>
#include <MotorControler.h>
#include <CameraController.h>
#include <CameraTypes.h>
#include <MotorTypes.h>
#include <Bleeper.h>
#include <AudioController.h>
#include <Settings.h>

Settings settings;
CameraController cameraController(settings);
MotorController motor(settings);
AudioController audioController(settings);
HttpServer httpServer(settings, cameraController, motor, audioController);
WiFiManager wifiManager;

void setup() {
  Serial.begin(112500);

  Bleeper.verbose()
      .configuration
      .set(&settings)
      .done()
      .storage
      .set(new SPIFFSStorage())
      .done()
      .init();

  SPI.begin();
  SPI.setFrequency(4000000); //4MHz

  audioController.init();
  motor.init();
  cameraController.init();

  wifiManager.autoConnect();

  httpServer.begin();
}

time_t lastS = 0;

void loop() {
  Bleeper.handle();
  audioController.loop();
}