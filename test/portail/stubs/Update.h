#pragma once
#include <Arduino.h>
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
#define U_FLASH 0
struct UpdateClass {
  bool begin(size_t, int) { return true; }
  size_t write(uint8_t*, size_t n) { return n; }
  bool end(bool) { return true; }
  void abort() {}
  const char* errorString() { return "erreur"; }
};
inline UpdateClass Update;
