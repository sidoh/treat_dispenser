#include <Arduino.h>
#include <MotorTypes.h>

String MotorTypes::microstepResolutionToStr(const MicrostepResolution resolution) {
  switch (resolution) {
    case MicrostepResolution::HALF:
      return "HALF";
    case MicrostepResolution::QUARTER:
      return "QUARTER";
    case MicrostepResolution::EIGHTH:
      return "EIGHTH";
    case MicrostepResolution::SIXTEENTH:
      return "SIXTEENTH";
    default:
      return "FULL";
  }
}

MicrostepResolution MotorTypes::microstepResolutionFromStr(const String& str) {
  if (str.equalsIgnoreCase("HALF")) return MicrostepResolution::HALF;
  else if (str.equalsIgnoreCase("QUARTER")) return MicrostepResolution::QUARTER;
  else if (str.equalsIgnoreCase("EIGHTH")) return MicrostepResolution::EIGHTH;
  else if (str.equalsIgnoreCase("SIXTEENTH")) return MicrostepResolution::SIXTEENTH;
  else return MicrostepResolution::FULL;
}

String MotorTypes::rotationDirectionToStr(const RotationDirection dir) {
  switch (dir) {
    case RotationDirection::CLOCKWISE:
      return "CLOCKWISE";
    default:
      return "COUNTERCLOCKWISE";
  }
}

RotationDirection MotorTypes::rotationDirectionFromStr(const String& str) {
  if (str.equalsIgnoreCase("CLOCKWISE")) return RotationDirection::CLOCKWISE;
  else return RotationDirection::COUNTERCLOCKWISE;
}