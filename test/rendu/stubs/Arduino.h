#pragma once
// Bouchon minimal de l'API Arduino pour faire tourner le firmware sur PC.

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <deque>
#include <string>
#include <algorithm>
#include <cctype>

#ifndef ARDUINO
#define ARDUINO 10819
#endif
#define ESP32 1
#define ARDUINO_RUNNING_CORE 1

using std::min;
using std::max;
typedef uint8_t byte;
typedef bool boolean;
typedef uint16_t word;

#define bitRead(v, b) (((v) >> (b)) & 1)
#define highByte(w) ((uint8_t)((w) >> 8))
#define lowByte(w) ((uint8_t)((w) & 0xff))
#define SERIAL_7E1 0x800001a

// Le temps n'avance qu'avec delay() et à chaque lecture de millis() : les
// boucles d'attente de Minitel1B_Hard finissent ainsi par expirer.
extern uint32_t g_ms;
inline uint32_t millis() { return g_ms++; }
inline void delay(uint32_t d) { g_ms += d; }

inline size_t strlcpy(char* d, const char* s, size_t n) {
  const size_t l = strlen(s);
  if (n) {
    const size_t k = l < n - 1 ? l : n - 1;
    memcpy(d, s, k);
    d[k] = '\0';
  }
  return l;
}

class String {
 public:
  String() {}
  String(const char* c) : s(c ? c : "") {}
  String(const std::string& x) : s(x) {}
  String(char c) : s(1, c) {}
  unsigned int length() const { return s.size(); }
  char charAt(unsigned int i) const { return i < s.size() ? s[i] : 0; }
  int indexOf(char c) const { auto p = s.find(c); return p == std::string::npos ? -1 : int(p); }
  int lastIndexOf(char c) const { auto p = s.rfind(c); return p == std::string::npos ? -1 : int(p); }
  String& operator+=(char c) { s += c; return *this; }
  String& operator+=(const String& o) { s += o.s; return *this; }
  String& operator+=(const char* c) { s += c; return *this; }
  String& operator+=(unsigned int v) { s += std::to_string(v); return *this; }
  String& operator+=(int v) { s += std::to_string(v); return *this; }
  bool operator!=(const String& o) const { return s != o.s; }
  long toInt() const { return strtol(s.c_str(), nullptr, 10); }
  bool operator==(const char* o) const { return s == o; }
  bool operator==(const String& o) const { return s == o.s; }
  String operator+(const String& o) const { return String(s + o.s); }
  String operator+(const char* o) const { return String(s + o); }
  void replace(const char* de, const char* vers) {
    const std::string a(de), b(vers);
    for (size_t p = 0; !a.empty() && (p = s.find(a, p)) != std::string::npos; p += b.size()) s.replace(p, a.size(), b);
  }
  bool reserve(unsigned int n) { s.reserve(n); return true; }
  const char* c_str() const { return s.c_str(); }
  std::string s;
};

// UART : ce qui est écrit part vers write_hook (le Minitel simulé), ce que le
// Minitel envoie arrive dans `entree`.
class HardwareSerial {
 public:
  std::deque<uint8_t> entree;
  std::function<void(uint8_t)> write_hook;
  void begin(unsigned long, uint32_t = 0, int8_t = -1, int8_t = -1) {}
  void end() {}
  void flush(bool = true) {}
  void updateBaudRate(unsigned long) {}
  operator bool() const { return true; }
  int available() { return int(entree.size()); }
  int read() {
    if (entree.empty()) return -1;
    int b = entree.front();
    entree.pop_front();
    return b;
  }
  size_t write(uint8_t b) {
    if (write_hook) write_hook(b);
    return 1;
  }
  size_t write(const uint8_t* d, size_t n) {
    for (size_t i = 0; i < n; ++i) write(d[i]);
    return n;
  }
  void printf(const char* f, ...) {
    va_list a;
    va_start(a, f);
    vfprintf(stderr, f, a);
    va_end(a);
  }
  void println(const char* s) { fprintf(stderr, "%s\n", s); }
};

extern HardwareSerial Serial;
extern HardwareSerial Serial2;

struct EspClass {
  uint32_t getFreeHeap() { return 181234; }
  uint32_t getMaxAllocHeap() { return 110592; }
  uint32_t getMinFreeHeap() { return 160000; }
  uint8_t getChipRevision() { return 3; }
  uint32_t getCpuFreqMHz() { return 240; }
  void restart() { printf("[sim] ESP.restart()\n"); }
};
extern EspClass ESP;

inline void configTzTime(const char*, const char*, const char* = nullptr, const char* = nullptr) {}
