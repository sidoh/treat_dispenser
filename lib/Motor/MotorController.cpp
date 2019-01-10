#include <vector>
#include <MotorControler.h>

MotorController::MotorController(Settings& settings)
  : settings(settings)
{ }

void MotorController::continuousTurn(float numTurns, MicrostepResolution resolution, RotationDirection direction) {
  if (settings.motor.auto_enable) {
    enable();
  }

  digitalWrite(settings.motor.a4988.dir_pin, static_cast<uint8_t>(direction));

  std::vector<uint8_t> speedPinValues;

  switch (resolution) {
    case MicrostepResolution::HALF:
      speedPinValues = {HIGH, LOW, LOW};
      break;
      
    case MicrostepResolution::QUARTER:
      speedPinValues = {LOW, HIGH, LOW};
      break;
      
    case MicrostepResolution::EIGHTH:
      speedPinValues = {HIGH, HIGH, LOW};
      break;
      
    case MicrostepResolution::SIXTEENTH:
      speedPinValues = {HIGH, HIGH, HIGH};
      break;

    case MicrostepResolution::FULL:
    default:
      speedPinValues = {LOW, LOW, LOW};
      break;
  }

  digitalWrite(settings.motor.a4988.ms1_pin, speedPinValues[0]);
  digitalWrite(settings.motor.a4988.ms2_pin, speedPinValues[1]);
  digitalWrite(settings.motor.a4988.ms3_pin, speedPinValues[2]);

  size_t numSteps = ceil(numTurns * settings.motor.num_microsteps * static_cast<uint8_t>(resolution));

  Serial.printf("Steping %d times...\n", numSteps);

  for (size_t i = 0; i < numSteps; ++i) {
    digitalWrite(settings.motor.a4988.step_pin, LOW);
    delayMicroseconds(settings.motor.microseconds_between_steps);
    digitalWrite(settings.motor.a4988.step_pin, HIGH);
    delayMicroseconds(settings.motor.microseconds_between_steps);
  }

  if (settings.motor.auto_enable) {
    disable();
  }
}

void MotorController::disable() {
  Serial.println(F("Disabling stepper"));

  digitalWrite(settings.motor.a4988.en_pin, HIGH);
}

void MotorController::enable() {
  Serial.println(F("Enabling stepper"));

  digitalWrite(settings.motor.a4988.en_pin, LOW);
}

void MotorController::dispenseTurn() {
  // Jitter back and forth a few times to unstick
  for (size_t i = 0; i < settings.motor.dispense_jitter_count; ++i) {
    continuousTurn(settings.motor.dispense_jitter_num_turns, MicrostepResolution::FULL, RotationDirection::CLOCKWISE);
    continuousTurn(settings.motor.dispense_jitter_num_turns, MicrostepResolution::FULL, RotationDirection::COUNTERCLOCKWISE);
  }

  continuousTurn(settings.motor.num_turns, settings.motor.microstep_resolution, settings.motor.rotation_direction);
}

bool MotorController::jsonCommand(const JsonObject& command) {
  if (! command.containsKey("type")) {
    return false;
  }

  const String& type = command["type"];

  if (type.equalsIgnoreCase("simple")) {
    const JsonObject& args = command["args"];

    const RotationDirection dir = MotorTypes::rotationDirectionFromStr(args["direction"]);
    const MicrostepResolution resolution = MotorTypes::microstepResolutionFromStr(args["resolution"]);
    const float numTurns = args["num_turns"];

    Serial.printf("Executing command: %d, %d, %f\n", static_cast<uint8_t>(dir), static_cast<uint8_t>(resolution), numTurns);

    continuousTurn(numTurns, resolution, dir);

    return true;
  } else if (type.equalsIgnoreCase("saved")) {
    continuousTurn(settings.motor.num_turns, settings.motor.microstep_resolution, settings.motor.rotation_direction);
    return true;
  } else if (type.equalsIgnoreCase("dispense")) {
    dispenseTurn();
    return true;
  } else if (type.equalsIgnoreCase("disable")) {
    disable();
    return true;
  } else if (type.equalsIgnoreCase("enable")) {
    enable();
    return true;
  }

  return false;
}