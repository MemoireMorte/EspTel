#pragma once
struct MDNSResponder {
  bool addService(const char*, const char*, int) { return true; }
};
inline MDNSResponder MDNS;
