#pragma once
// Bouchon du WiFi : quelques bornes simulées (nom, signal, sécurité, mot de passe).
// Une connexion réussit après simWifiDelaiMs si le mot de passe est bon ; sinon
// le pilote signale la raison du refus comme le vrai (201, 15).

#include "Arduino.h"

#include <functional>
#include <string>
#include <vector>

#define WIFI_STA 1
#define WL_CONNECTED 3
#define WL_DISCONNECTED 6
#define WIFI_SCAN_RUNNING (-1)
#define WIFI_SCAN_FAILED (-2)

typedef enum {
  WIFI_AUTH_OPEN = 0, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK, WIFI_AUTH_WPA_WPA2_PSK,
  WIFI_AUTH_WPA2_ENTERPRISE, WIFI_AUTH_WPA3_PSK, WIFI_AUTH_WPA2_WPA3_PSK, WIFI_AUTH_WAPI_PSK,
} wifi_auth_mode_t;

typedef enum { ARDUINO_EVENT_WIFI_STA_DISCONNECTED = 5 } arduino_event_id_t;
struct arduino_event_info_t {
  struct { uint8_t reason; } wifi_sta_disconnected;
};

struct Borne {
  std::string ssid;
  int rssi;
  wifi_auth_mode_t secu;
  std::string mdp;
};
extern std::vector<Borne> simBornes;
extern int32_t simWifiDelaiMs;

struct IPAddress {
  String toString() const { return String("192.168.1.42"); }
};

struct WiFiClass {
  using Rappel = std::function<void(arduino_event_id_t, arduino_event_info_t)>;
  Rappel rappel;
  uint32_t debut = 0;
  bool lance = false;
  int issue = 0;           // 0 connexion, sinon raison de refus
  int refusEnvoyes = 0;
  const Borne* borne = nullptr;
  uint32_t finScan = 0;
  bool scan = false;

  void persistent(bool) {}
  void mode(int) {}
  void setAutoReconnect(bool) {}
  void onEvent(Rappel r, arduino_event_id_t) { rappel = r; }
  void begin(const char* s, const char* m) {
    debut = g_ms;
    lance = true;
    refusEnvoyes = 0;
    borne = nullptr;
    for (const Borne& b : simBornes)
      if (b.ssid == s) borne = &b;
    if (!borne) issue = 201;
    else if (b_mdp(m) != borne->mdp) issue = 15;
    else issue = 0;
    fprintf(stderr, "[sim] WiFi.begin(%s, %s) -> %s\n", s, m ? m : "(rien)", issue ? "refus" : "ok");
  }
  static std::string b_mdp(const char* m) { return m ? m : ""; }
  bool disconnect(bool = false, bool = false) {
    lance = false;
    return true;
  }
  void refuser(int r) {
    arduino_event_info_t i;
    i.wifi_sta_disconnected.reason = uint8_t(r);
    ++refusEnvoyes;
    if (rappel) rappel(ARDUINO_EVENT_WIFI_STA_DISCONNECTED, i);
  }
  int status() {
    if (!lance) return WL_DISCONNECTED;
    const uint32_t t = g_ms - debut;
    if (issue == 0) return simWifiDelaiMs >= 0 && t > uint32_t(simWifiDelaiMs) ? WL_CONNECTED : WL_DISCONNECTED;
    // Réseau absent : le pilote réessaie et refuse à chaque fois ; mauvais mot de passe : un seul refus.
    if (issue == 201 && t > 2500u * (refusEnvoyes + 1)) refuser(201);
    if (issue == 15 && refusEnvoyes == 0 && t > 2000) refuser(15);
    return WL_DISCONNECTED;
  }
  int RSSI() { return borne ? borne->rssi : -62; }
  IPAddress localIP() { return IPAddress(); }
  String macAddress() { return String("24:6F:28:A1:B2:C3"); }

  int16_t scanNetworks(bool, bool) {
    scan = true;
    finScan = g_ms + 3000;
    return WIFI_SCAN_RUNNING;
  }
  int16_t scanComplete() {
    if (!scan) return WIFI_SCAN_FAILED;
    return g_ms < finScan ? WIFI_SCAN_RUNNING : int16_t(simBornes.size());
  }
  void scanDelete() { scan = false; }
  String SSID(uint8_t i) { return String(simBornes[i].ssid); }
  int32_t RSSI(uint8_t i) { return simBornes[i].rssi; }
  wifi_auth_mode_t encryptionType(uint8_t i) { return simBornes[i].secu; }
};
extern WiFiClass WiFi;
