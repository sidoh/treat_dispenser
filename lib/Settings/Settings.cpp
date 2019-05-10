#include <Settings.h>

bool HttpSettings::isAuthenticationEnabled() const {
  return username.length() > 0 && password.length() > 0;
}

const String& HttpSettings::getUsername() const {
  return username;
}

const String& HttpSettings::getPassword() const {
  return password;
}