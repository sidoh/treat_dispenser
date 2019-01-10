#include <ArduCAM.h>

#ifndef _CAMERA_TYPES_H
#define _CAMERA_TYPES_H

enum class CameraResolution {
  d160x120, d176x144, d320x240, d352x288, d640x480, d800x600,
  d1024x768, d1280x1024, d1600x1200
};

class CameraTypes {
public:
  static String cameraResolutionToStr(const CameraResolution resolution);
  static CameraResolution cameraResolutionFromStr(const String& str);
};

#endif