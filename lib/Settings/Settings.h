#include <ArduCAM.h>
#include <Bleeper.h>
#include <MotorTypes.h>
#include <CameraTypes.h>

#ifndef SOUNDS_DIRECTORY
#define SOUNDS_DIRECTORY "/s"
#endif

#define XQUOTE(x) #x
#define QUOTE(x) XQUOTE(x)

#ifndef FIRMWARE_VARIANT
#define FIRMWARE_VARIANT unknown
#endif

#ifndef TREAT_DISPENSER_VERSION
#define TREAT_DISPENSER_VERSION unknown
#endif

#ifndef _SETTINGS_H
#define _SETTINGS_H

class A4988Settings : public Configuration {
public:
  persistentIntVar(en_pin, 33);
  persistentIntVar(ms1_pin, 17);
  persistentIntVar(ms2_pin, 26);
  persistentIntVar(ms3_pin, 27);
  persistentIntVar(step_pin, 14);
  persistentIntVar(dir_pin, 32);
};

class ArduCamSettings : public Configuration {
public:
  persistentIntVar(i2c_sda_pin, SDA);
  persistentIntVar(i2c_scl_pin, SCL);

  persistentVar(
    CameraResolution,
    camera_resolution,
    CameraResolution::d800x600,
    {
      camera_resolution = CameraTypes::cameraResolutionFromStr(camera_resolutionString);
    },
    {
      camera_resolutionString = CameraTypes::cameraResolutionToStr(camera_resolution);
    }
  );
};

class HttpSettings : public Configuration {
public:
  persistentIntVar(port, 80);
  persistentStringVar(username, "");
  persistentStringVar(password, "");

  bool isAuthenticationEnabled() const;
  const String& getUsername() const;
  const String& getPassword() const;
};

class MotorSettings : public Configuration {
public:
  subconfig(A4988Settings, a4988);

  persistentIntVar(num_microsteps, 200);
  persistentIntVar(microseconds_between_steps, 800);
  persistentFloatVar(num_turns, 0.6);

  persistentIntVar(dispense_jitter_count, 10);
  persistentFloatVar(dispense_jitter_num_turns, 0.1);

  persistentVar(
    bool,
    auto_enable,
    true,
    {
      if (auto_enableString.equalsIgnoreCase("true")) {
        auto_enable = true;
      } else {
        auto_enable = false;
      }
    },
    {
      auto_enableString = auto_enable ? "true" : "false";
    }
  );
  persistentVar(
    MicrostepResolution,
    microstep_resolution,
    MicrostepResolution::FULL,
    {
      microstep_resolution = MotorTypes::microstepResolutionFromStr(microstep_resolutionString);
    },
    {
      microstep_resolutionString = MotorTypes::microstepResolutionToStr(microstep_resolution);
    }
  );
  persistentVar(
    RotationDirection,
    rotation_direction,
    RotationDirection::COUNTERCLOCKWISE,
    {
      rotation_direction = MotorTypes::rotationDirectionFromStr(rotation_directionString);
    },
    {
      rotation_directionString = MotorTypes::rotationDirectionToStr(rotation_direction);
    }
  );

  persistentVar(
    MicrostepResolution,
    jitter_microstep_resolution,
    MicrostepResolution::FULL,
    {
      jitter_microstep_resolution = MotorTypes::microstepResolutionFromStr(jitter_microstep_resolutionString);
    },
    {
      jitter_microstep_resolutionString = MotorTypes::microstepResolutionToStr(jitter_microstep_resolution);
    }
  )
};

class AudioSettings : public Configuration {
public:
  persistentIntVar(enable_pin, 16);
};

class Settings : public RootConfiguration {
public:
  subconfig(MotorSettings, motor);
  subconfig(ArduCamSettings, arducam);
  subconfig(HttpSettings, http);
  subconfig(AudioSettings, audio);
};

#endif