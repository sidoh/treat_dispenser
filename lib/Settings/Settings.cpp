#include <Settings.h>

bool HttpSettings::hasAuthSettings() const {
  return username.length() > 0 && password.length() > 0;
}