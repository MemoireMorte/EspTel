#pragma once
// Bouchon de la NVS : une table en mémoire, partagée par tout le programme.

#include "Arduino.h"

#include <map>
#include <string>

extern std::map<std::string, std::string> simNvs;

class Preferences {
 public:
  bool begin(const char* nom, bool = false) {
    _ns = nom;
    return true;
  }
  void end() {}
  bool isKey(const char* k) { return simNvs.count(_ns + "/" + k) > 0; }
  bool remove(const char* k) { return simNvs.erase(_ns + "/" + k) > 0; }
  bool clear() {
    const std::string debut = _ns + "/";
    for (auto it = simNvs.begin(); it != simNvs.end();) {
      if (it->first.rfind(debut, 0) == 0) it = simNvs.erase(it);
      else ++it;
    }
    return true;
  }
  size_t putUChar(const char* k, uint8_t v) {
    simNvs[_ns + "/" + k] = std::to_string(v);
    return 1;
  }
  uint8_t getUChar(const char* k, uint8_t d = 0) {
    auto it = simNvs.find(_ns + "/" + k);
    return it == simNvs.end() ? d : uint8_t(std::stoi(it->second));
  }
  size_t putString(const char* k, const char* v) {
    simNvs[_ns + "/" + k] = v;
    return strlen(v);
  }
  size_t getString(const char* k, char* v, size_t n) {
    auto it = simNvs.find(_ns + "/" + k);
    if (it == simNvs.end()) return 0;
    strlcpy(v, it->second.c_str(), n);
    return it->second.size() + 1;
  }

 private:
  std::string _ns;
};
