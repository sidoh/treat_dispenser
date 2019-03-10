#include <Bleeper.h>
#include <Settings.h>
#include <ArduinoJson.h>
#include <MotorTypes.h>
#include <CameraTypes.h>
#include <ArduCAM.h>

#ifndef _MOTOR_CONTROLLER_H
#define _MOTOR_CONTROLLER_H

class MotorController {
public:
  MotorController(Settings& settings);

  void continuousTurn(float numTurns, MicrostepResolution speed, RotationDirection direction);
  void dispenseTurn();

  bool jsonCommand(const JsonObject& command);

  void disable();
  void enable();

  void init();

private:
  const Settings& settings;
};

#endif