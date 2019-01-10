#include <Arduino.h>
#include <WiFi.h>
#include <ArduCAM.h>
#include <SPI.h>
#include <Wire.h>

#include <HttpServer.h>
#include <WiFiManager.h>
#include <MotorControler.h>
#include <CameraController.h>
#include <CameraTypes.h>
#include <MotorTypes.h>
#include <Bleeper.h>
#include <AudioController.h>
#include <Settings.h>

//================
// #include <AudioOutputI2S.h>
// #include <AudioFileSourcePROGMEM.h>
// #include <AudioGeneratorFLAC.h>

// #include <sample.h>

// AudioOutputI2S *out;
// AudioFileSourcePROGMEM *file;
// AudioGeneratorFLAC *flac;

ArduCAM camera(OV2640, SS);
Settings settings;
CameraController cameraController(settings, camera);
MotorController motor(settings);
AudioController audioController(settings);
HttpServer httpServer(settings, cameraController, motor, audioController);
WiFiManager wifiManager;

void initCamera() {
  pinMode(SS, OUTPUT);
  pinMode(settings.arducam.i2c_sda_pin, OUTPUT);
  pinMode(settings.arducam.i2c_scl_pin, OUTPUT);

  digitalWrite(settings.arducam.i2c_scl_pin, LOW);

  Wire.begin(settings.arducam.i2c_sda_pin, settings.arducam.i2c_scl_pin);

  uint8_t vid, pid;

  camera.write_reg(ARDUCHIP_TEST1, 0x55);
  uint8_t temp = camera.read_reg(ARDUCHIP_TEST1);

  if (temp != 0x55) {
    Serial.println(F("SPI1 interface Error!"));
  }

  camera.wrSensorReg8_8(0xff, 0x01);
  camera.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
  camera.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);

  if ((vid != 0x26) && ((pid != 0x41) || (pid != 0x42))) {
    Serial.println(F("Can't find OV2640 module!"));
  } else {
    Serial.println(F("OV2640 detected."));
  }

  camera.set_format(JPEG);
  camera.InitCAM();
  camera.OV2640_set_JPEG_size(static_cast<uint8_t>(settings.arducam.camera_resolution));
  camera.clear_fifo_flag();
  camera.write_reg(ARDUCHIP_FRAMES, 0x00);

  camera.clear_fifo_flag();
  camera.start_capture();

  time_t max_wait_time = 1000;
  time_t start_time = millis();
  bool found_camera = false;

  while (!found_camera && millis() < (start_time + max_wait_time)) {
    found_camera = camera.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK);
  }

  if (found_camera) {
    const size_t cameraBufferLen = camera.read_fifo_length();
    size_t readRemains = cameraBufferLen;

    if (cameraBufferLen >= 0x07ffff){
      Serial.println(F("ERROR: Captured image was too large"));
      return;
    } else if (cameraBufferLen == 0 ){
      Serial.println(F("ERROR: Buffer size was 0"));
      return;
    }

    camera.CS_LOW();
    camera.set_fifo_burst();
    SPI.transfer(0xFF);

    uint8_t buffer[2048];
    SPI.transferBytes(buffer, buffer, 2048);
  }
}

void initMotor() {
  pinMode(settings.motor.a4988.dir_pin, OUTPUT);
  pinMode(settings.motor.a4988.en_pin, OUTPUT);
  pinMode(settings.motor.a4988.ms1_pin, OUTPUT);
  pinMode(settings.motor.a4988.ms2_pin, OUTPUT);
  pinMode(settings.motor.a4988.ms3_pin, OUTPUT);
  pinMode(settings.motor.a4988.step_pin, OUTPUT);

  digitalWrite(settings.motor.a4988.en_pin, HIGH);
}

void initAudio() {
  audioController.begin();
}

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

  initCamera();
  initMotor();
  initAudio();

  wifiManager.autoConnect();

  httpServer.begin();

  // Serial.printf("flac start\n");
  // file = new AudioFileSourcePROGMEM( sample_flac, sample_flac_len );
  // out = new AudioOutputI2S(); // I am connecting the Audio Amp to GPIO25
  // flac = new AudioGeneratorFLAC();
  // flac->begin(file, out);
}

time_t lastS = 0;

void loop() {
  Bleeper.handle();
  audioController.loop();

  // if (flac->isRunning()) {
  //   if (!flac->loop()) flac->stop();
  // } else {
  //   Serial.printf("flac done\n");
  //   delay(1000);
  // }
}

// #include <Arduino.h>
// #include "AudioGeneratorAAC.h"
// #include "AudioOutputI2S.h"
// #include "AudioFileSourcePROGMEM.h"
// #include <AudioFileSourceSPIFFS.h>
// #include <AudioFileSourceID3.h>
// #include <AudioGeneratorMP3.h>
// #include <AudioOutputI2SNoDAC.h>
// #include <sampleaac.h>

//   AudioOutputI2S* audioOutput;
//   AudioGeneratorMP3 *mp3;
//   AudioFileSourceSPIFFS *file;
//   AudioOutputI2S *out;
//   AudioFileSourceID3 *id3;

// void setup()
// {
//   Serial.begin(115200);

//   pinMode(16, OUTPUT);
//   digitalWrite(16, HIGH);
  
//   // in = new AudioFileSourcePROGMEM(sampleaac, sizeof(sampleaac));
//   // aac = new AudioGeneratorAAC();

//   // aac->begin(in, out);

//   file = new AudioFileSourceSPIFFS("/s/Cecilia.mp3");
//   id3 = new AudioFileSourceID3(file);
//   mp3 = new AudioGeneratorMP3();
//   out = new AudioOutputI2S(0, AudioOutputI2S::INTERNAL_DAC);
//   mp3->begin(id3, out);
// }


// void loop()
// {
//   if (mp3->isRunning()) {
//     mp3->loop();
//   } else {
//     digitalWrite(16, LOW);
//     Serial.printf("AAC done\n");
//     delay(1000);
//   }
// }