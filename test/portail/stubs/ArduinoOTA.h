#pragma once
#include <Arduino.h>
typedef enum { OTA_AUTH_ERROR, OTA_BEGIN_ERROR, OTA_CONNECT_ERROR, OTA_RECEIVE_ERROR, OTA_END_ERROR } ota_error_t;
struct ArduinoOTAClass {
  ArduinoOTAClass& setHostname(const char*) { return *this; }
  ArduinoOTAClass& setPassword(const char*) { return *this; }
  ArduinoOTAClass& setRebootOnSuccess(bool) { return *this; }
  ArduinoOTAClass& onStart(std::function<void()>) { return *this; }
  ArduinoOTAClass& onEnd(std::function<void()>) { return *this; }
  ArduinoOTAClass& onProgress(std::function<void(unsigned, unsigned)>) { return *this; }
  ArduinoOTAClass& onError(std::function<void(ota_error_t)>) { return *this; }
  void begin() {}
  void end() {}
  void handle() {}
};
inline ArduinoOTAClass ArduinoOTA;
