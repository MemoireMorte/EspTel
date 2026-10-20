#pragma once
// Serveur web factice : l'aperçu appelle les routes du portail une à une.
#include <Arduino.h>
#include <WiFi.h>

#include <map>
#include <string>
#include <vector>

#define F(x) (x)
#define FPSTR(x) (x)
#ifndef PROGMEM
#define PROGMEM
#endif
template <typename T> T constrain(T v, T a, T b) { return v < a ? a : v > b ? b : v; }
inline uint32_t esp_random() { return 482107; }

enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_POST };
enum HTTPAuthMethod { BASIC_AUTH };
enum HTTPUploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload {
  HTTPUploadStatus status;
  String filename;
  size_t totalSize = 0;
  size_t currentSize = 0;
  uint8_t buf[1436];
};
struct ClientFactice {
  IPAddress remoteIP() { return IPAddress(); }
};

class WebServer {
 public:
  using Fn = std::function<void()>;
  explicit WebServer(int) { instance = this; }
  static inline WebServer* instance = nullptr;
  void on(const char* u, HTTPMethod m, Fn f, Fn up = nullptr) { routes.push_back({u, m, f, up}); }
  void onNotFound(Fn f) { introuvable = f; }
  void begin() {}
  void stop() {}
  void handleClient() {}
  bool authenticate(const char*, const char*) { return true; }
  void requestAuthentication(HTTPAuthMethod, const char* = nullptr, const String& = String()) { code = 401; }
  String arg(const char* n) {
    auto it = args.find(n);
    return it == args.end() ? String() : String(it->second);
  }
  bool hasArg(const char* n) { return args.count(n) > 0; }
  void send(int c, const char* type, const String& body) {
    code = c;
    (void)type;
    corps = body.s;
  }
  void sendHeader(const char* k, const char* v) { entetes += std::string(k) + ": " + v + "\n"; }
  void sendHeader(const char* k, const String& v) { sendHeader(k, v.c_str()); }
  void setContentLength(size_t) {}
  void sendContent(const char* d, size_t n) { corps.append(d, n); }
  HTTPUpload& upload() { return envoi; }
  ClientFactice client() { return {}; }
  int clientContentLength() { return longueur; }

  // Pour l'aperçu.
  struct Route {
    std::string url;
    HTTPMethod m;
    Fn f, up;
  };
  std::vector<Route> routes;
  Fn introuvable;
  std::map<std::string, std::string> args;
  HTTPUpload envoi;
  int longueur = 0;
  String nomFichier = "annuaire.json";
  int code = 0;
  std::string corps, entetes;

  void requete(HTTPMethod m, const std::string& url, const std::map<std::string, std::string>& a,
               const std::string* fichier = nullptr) {
    args = a;
    code = 0;
    corps.clear();
    entetes.clear();
    for (auto& r : routes) {
      if (r.url != url || r.m != m) continue;
      if (fichier && r.up) {
        longueur = int(fichier->size()) + 200;
        envoi.status = UPLOAD_FILE_START;
        envoi.filename = nomFichier;
        envoi.totalSize = 0;
        r.up();
        for (size_t i = 0; i < fichier->size(); i += sizeof(envoi.buf)) {
          envoi.currentSize = std::min(sizeof(envoi.buf), fichier->size() - i);
          memcpy(envoi.buf, fichier->data() + i, envoi.currentSize);
          envoi.status = UPLOAD_FILE_WRITE;
          envoi.totalSize += envoi.currentSize;
          r.up();
        }
        envoi.status = UPLOAD_FILE_END;
        r.up();
      }
      r.f();
      return;
    }
    if (introuvable) introuvable();
  }
};
