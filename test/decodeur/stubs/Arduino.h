#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <deque>
#include <algorithm>
using std::min;
typedef uint8_t byte; typedef bool boolean;
extern uint32_t g_ms; inline uint32_t millis(){return g_ms;} inline void delay(uint32_t d){g_ms+=d;}
struct String { std::string s; String(const char* c):s(c){} };
struct HardwareSerial { std::deque<uint8_t> in; std::string out;
  int available(){return in.size();} int read(){int b=in.front();in.pop_front();return b;}
  size_t write(uint8_t b){out+=char(b);return 1;} size_t write(const uint8_t*d,size_t n){out.append((const char*)d,n);return n;}
  void updateBaudRate(int){} };
// Journal de débogage (DEBUG_UART) : ignoré.
struct JournalMuet { template <typename... A> void printf(const char*, A...) {} void println(const char*) {} };
inline JournalMuet Serial;
