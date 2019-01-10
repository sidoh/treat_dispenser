#include <CameraTypes.h>

String CameraTypes::cameraResolutionToStr(const CameraResolution v) {
  switch (v) {
    case CameraResolution::d160x120:
      return "d160x120";
    case CameraResolution::d176x144:
      return "d176x144";
    case CameraResolution::d320x240:
      return "d320x240";
    case CameraResolution::d352x288:
      return "d352x288";
    case CameraResolution::d640x480:
      return "d640x480";
    case CameraResolution::d800x600:
      return "d800x600";
    case CameraResolution::d1024x768:
      return "d1024x768";
    case CameraResolution::d1280x1024:
      return "d1280x1024";
    case CameraResolution::d1600x1200:
    default:
      return "d1600x1200";
  }
}

CameraResolution CameraTypes::cameraResolutionFromStr(const String& s) {
  if (s.equalsIgnoreCase("d160x120")) return CameraResolution::d160x120;
  else if (s.equalsIgnoreCase("d176x144")) return CameraResolution::d176x144;
  else if (s.equalsIgnoreCase("d320x240")) return CameraResolution::d320x240;
  else if (s.equalsIgnoreCase("d352x288")) return CameraResolution::d352x288;
  else if (s.equalsIgnoreCase("d640x480")) return CameraResolution::d640x480;
  else if (s.equalsIgnoreCase("d800x600")) return CameraResolution::d800x600;
  else if (s.equalsIgnoreCase("d1024x768")) return CameraResolution::d1024x768;
  else if (s.equalsIgnoreCase("d1280x1024")) return CameraResolution::d1280x1024;
  else return CameraResolution::d1600x1200;
}
