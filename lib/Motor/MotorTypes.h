#ifndef _MOTOR_TYPES_H
#define _MOTOR_TYPES_H

enum class MicrostepResolution {
  FULL = 1, HALF = 2, QUARTER = 4, EIGHTH = 8, SIXTEENTH = 16
};

enum class RotationDirection {
  CLOCKWISE, COUNTERCLOCKWISE
};

class MotorTypes {
public:
  static String microstepResolutionToStr(const MicrostepResolution resolution);
  static MicrostepResolution microstepResolutionFromStr(const String& str);

  static String rotationDirectionToStr(const RotationDirection dir);
  static RotationDirection rotationDirectionFromStr(const String& str);
};

#endif