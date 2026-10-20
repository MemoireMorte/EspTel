#include "../config.h"

#include "Reseau.h"

#include <Preferences.h>
#include <WiFi.h>
#include <time.h>

#include <algorithm>

#if __has_include("../secrets.h")
#include "../secrets.h"
#endif

namespace reseau {

namespace {

constexpr uint32_t kDelaiEssaiMs = 15000;  // par réseau essayé
constexpr uint32_t kDelaiPerteMs = 20000;  // liaison perdue : Echec affiché, le WiFi réessaie
constexpr uint32_t kSilenceMs = 4000;      // refus sans nouvel essai du WiFi : échec acquis

// Raisons de déconnexion du pilote (esp_wifi_types.h).
constexpr uint8_t kRaison4WayTimeout = 15;
constexpr uint8_t kRaisonPasDAp = 201;
constexpr uint8_t kRaisonAuthFail = 202;
constexpr uint8_t kRaisonHandshakeTimeout = 204;

struct Memorise {
  char ssid[kTailleSsid];
  char mdp[kTailleMdp];
};

Memorise memo[kMaxMemorises];
int nbMemo = 0;

Etat etatCourant = Etat::NonConfigure;
const char* raisonCourante = "";
bool heureDemandee = false;
bool liaisonPerdue = false;
uint32_t debutPerte = 0;

// Essai en cours.
struct {
  bool actif = false;
  bool manuel = false;  // demandé depuis une page : un seul réseau
  int indice = 0;       // sinon, rang dans les réseaux mémorisés
  char ssid[kTailleSsid] = "";
  char mdp[kTailleMdp] = "";
  uint32_t debut = 0;
} essai;

// Écrits par la tâche des évènements WiFi, lus par loop().
volatile uint8_t refus = 0;
volatile uint8_t derniereRaison = 0;
volatile uint32_t instantRefus = 0;

// Recherche.
enum class Recherche : uint8_t { Aucune, Demandee, EnCours };
Recherche recherche = Recherche::Aucune;
Trouve trouves[kMaxTrouves];
int nbTrouve = 0;

// ------------------------------------------------------------------ NVS

void charger() {
  Preferences p;
  p.begin("wifi", true);
  nbMemo = p.getUChar("n", 0);
  if (nbMemo > kMaxMemorises) nbMemo = kMaxMemorises;
  char cle[8];
  for (int i = 0; i < nbMemo; ++i) {
    snprintf(cle, sizeof(cle), "s%d", i);
    if (!p.getString(cle, memo[i].ssid, sizeof(memo[i].ssid))) memo[i].ssid[0] = '\0';
    snprintf(cle, sizeof(cle), "p%d", i);
    if (!p.getString(cle, memo[i].mdp, sizeof(memo[i].mdp))) memo[i].mdp[0] = '\0';
  }
  const bool amorce = p.getUChar("init", 0);
  p.end();

// Anciens noms (secrets.h d'avant la v0.8.2).
#if !defined(WIFI_SSID) && defined(J1_WIFI_SSID)
#define WIFI_SSID J1_WIFI_SSID
#define WIFI_MDP J1_WIFI_MDP
#endif
#ifdef WIFI_SSID
  // Une seule fois : un réseau oublié depuis le Minitel ne revient pas au démarrage.
  if (!amorce && nbMemo == 0 && WIFI_SSID[0]) {
    strlcpy(memo[0].ssid, WIFI_SSID, sizeof(memo[0].ssid));
    strlcpy(memo[0].mdp, WIFI_MDP, sizeof(memo[0].mdp));
    nbMemo = 1;
  }
#endif
  (void)amorce;
}

void enregistrer() {
  Preferences p;
  p.begin("wifi", false);
  char cle[8];
  for (int i = 0; i < kMaxMemorises; ++i) {
    snprintf(cle, sizeof(cle), "s%d", i);
    if (i < nbMemo) p.putString(cle, memo[i].ssid);
    else p.remove(cle);
    snprintf(cle, sizeof(cle), "p%d", i);
    if (i < nbMemo) p.putString(cle, memo[i].mdp);
    else p.remove(cle);
  }
  p.putUChar("n", nbMemo);
  p.putUChar("init", 1);
  p.end();
}

// Met le réseau en tête de liste (nouveau, ou déjà connu).
void memoriser(const char* s, const char* mdp) {
  int i = indiceMemorise(s);
  if (i < 0) i = nbMemo < kMaxMemorises ? nbMemo++ : kMaxMemorises - 1;
  for (; i > 0; --i) memo[i] = memo[i - 1];
  strlcpy(memo[0].ssid, s, sizeof(memo[0].ssid));
  strlcpy(memo[0].mdp, mdp, sizeof(memo[0].mdp));
  enregistrer();
}

// ------------------------------------------------------------------ connexion

void surDeconnexion(arduino_event_id_t, arduino_event_info_t info) {
  const uint8_t r = info.wifi_sta_disconnected.reason;
  if (r == kRaison4WayTimeout || r == kRaisonPasDAp || r == kRaisonAuthFail || r == kRaisonHandshakeTimeout) {
    derniereRaison = r;
    instantRefus = millis();
    ++refus;
  }
}

void commencer(const char* s, const char* mdp) {
  strlcpy(essai.ssid, s, sizeof(essai.ssid));
  strlcpy(essai.mdp, mdp, sizeof(essai.mdp));
  essai.actif = true;
  essai.debut = millis();
  liaisonPerdue = false;
  WiFi.disconnect();
  refus = 0;
  derniereRaison = 0;
  WiFi.begin(essai.ssid, essai.mdp[0] ? essai.mdp : nullptr);
  etatCourant = Etat::Connexion;
  raisonCourante = "";
  DEBUG_UART.printf("[WiFi] essai de %s\n", essai.ssid);
}

const char* texteRaison(uint8_t r) {
  switch (r) {
    case kRaisonPasDAp: return "réseau introuvable";
    case kRaison4WayTimeout:
    case kRaisonAuthFail:
    case kRaisonHandshakeTimeout: return "mot de passe refusé ?";
    default: return "pas de réponse";
  }
}

void finEssai() {
  raisonCourante = texteRaison(derniereRaison);
  DEBUG_UART.printf("[WiFi] échec de %s : %s (raison %u)\n", essai.ssid, raisonCourante, derniereRaison);
  WiFi.disconnect();
  if (!essai.manuel && essai.indice + 1 < nbMemo) {
    ++essai.indice;
    commencer(memo[essai.indice].ssid, memo[essai.indice].mdp);
    return;
  }
  essai.actif = false;
  etatCourant = Etat::Echec;
}

void ticRecherche() {
  if (recherche == Recherche::Demandee) {
    if (WiFi.status() != WL_CONNECTED) {
      // Le pilote refuse de chercher pendant une tentative : on l'arrête.
      WiFi.disconnect();
      if (essai.actif || etatCourant == Etat::Connexion) {
        essai.actif = false;
        liaisonPerdue = false;
        etatCourant = Etat::Echec;
        raisonCourante = "essai interrompu";
      }
    }
    WiFi.scanNetworks(true, false);
    recherche = Recherche::EnCours;
    return;
  }
  if (recherche != Recherche::EnCours) return;
  const int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  nbTrouve = 0;
  for (int i = 0; i < n; ++i) {
    const String s = WiFi.SSID(i);
    if (!s.length()) continue;  // réseau caché
    const int8_t r = static_cast<int8_t>(WiFi.RSSI(i));
    int k = 0;
    while (k < nbTrouve && strcmp(trouves[k].ssid, s.c_str()) != 0) ++k;
    if (k < nbTrouve) {  // même nom sur plusieurs bornes : la plus forte
      if (r > trouves[k].rssi) trouves[k].rssi = r;
      continue;
    }
    if (nbTrouve == kMaxTrouves) continue;
    Trouve& t = trouves[nbTrouve++];
    strlcpy(t.ssid, s.c_str(), sizeof(t.ssid));
    t.rssi = r;
    t.secu = static_cast<uint8_t>(WiFi.encryptionType(i));
  }
  WiFi.scanDelete();
  std::sort(trouves, trouves + nbTrouve, [](const Trouve& a, const Trouve& b) { return a.rssi > b.rssi; });
  recherche = Recherche::Aucune;
  DEBUG_UART.printf("[WiFi] recherche : %d réseaux (%d bruts)\n", nbTrouve, n);
}

}  // namespace

void demarrer() {
  charger();
  WiFi.persistent(false);  // la liste est en NVS chez nous, pas dans la config du pilote
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(surDeconnexion, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  if (!reconnecter()) etatCourant = Etat::NonConfigure;
}

bool reconnecter() {
  if (nbMemo == 0) return false;
  essai.manuel = false;
  essai.indice = 0;
  commencer(memo[0].ssid, memo[0].mdp);
  return true;
}

void connecter(const char* s, const char* mdp) {
  essai.manuel = true;
  commencer(s, mdp);
}

void tic() {
  ticRecherche();

  if (WiFi.status() == WL_CONNECTED) {
    if (etatCourant != Etat::Connecte) {
      etatCourant = Etat::Connecte;
      raisonCourante = "";
      liaisonPerdue = false;
      if (essai.actif) {
        essai.actif = false;
        memoriser(essai.ssid, essai.mdp);
      }
      DEBUG_UART.printf("[WiFi] connecté à %s, IP %s, %d dBm\n", essai.ssid, WiFi.localIP().toString().c_str(),
                        WiFi.RSSI());
    }
    if (!heureDemandee) {
      // Heure de Paris, changements d'heure compris.
      configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com");
      heureDemandee = true;
    }
    return;
  }

  if (etatCourant == Etat::Connecte) {
    // Liaison perdue : la reconnexion automatique du pilote s'en charge.
    etatCourant = Etat::Connexion;
    liaisonPerdue = true;
    debutPerte = millis();
    return;
  }

  if (essai.actif) {
    // Deux refus, ou un refus que le pilote ne retente pas : inutile d'attendre.
    const bool refuse = refus >= 2 || (refus == 1 && millis() - instantRefus > kSilenceMs);
    if (refuse || millis() - essai.debut > kDelaiEssaiMs) finEssai();
    return;
  }

  if (liaisonPerdue && etatCourant == Etat::Connexion && millis() - debutPerte > kDelaiPerteMs) {
    etatCourant = Etat::Echec;
    raisonCourante = "liaison perdue";
  }
}

Etat etat() { return etatCourant; }
const char* ssid() { return essai.ssid; }
const char* raison() { return raisonCourante; }
int rssi() { return etatCourant == Etat::Connecte ? WiFi.RSSI() : 0; }
String ip() { return etatCourant == Etat::Connecte ? WiFi.localIP().toString() : String(); }

bool heure(char* hhmm, size_t n) {
  const time_t t = time(nullptr);
  if (t < 1700000000) return false;  // pas encore synchronisée
  struct tm local;
  localtime_r(&t, &local);
  strftime(hhmm, n, "%H:%M", &local);
  return true;
}

int nbMemorises() { return nbMemo; }
const char* memorise(int i) { return i >= 0 && i < nbMemo ? memo[i].ssid : ""; }
const char* motDePasse(int i) { return i >= 0 && i < nbMemo ? memo[i].mdp : ""; }

int indiceMemorise(const char* s) {
  for (int i = 0; i < nbMemo; ++i) {
    if (strcmp(memo[i].ssid, s) == 0) return i;
  }
  return -1;
}

bool ajouter(const char* s, const char* mdp) {
  if (!s[0]) return false;
  int i = indiceMemorise(s);
  if (i < 0) {
    if (nbMemo == kMaxMemorises) return false;
    i = nbMemo++;
    strlcpy(memo[i].ssid, s, sizeof(memo[i].ssid));
  }
  strlcpy(memo[i].mdp, mdp, sizeof(memo[i].mdp));
  enregistrer();
  return true;
}

void oublier(int i) {
  if (i < 0 || i >= nbMemo) return;
  for (; i + 1 < nbMemo; ++i) memo[i] = memo[i + 1];
  --nbMemo;
  enregistrer();
}

void lancerRecherche() {
  if (recherche == Recherche::Aucune) recherche = Recherche::Demandee;
}
bool rechercheEnCours() { return recherche != Recherche::Aucune; }
int nbTrouves() { return nbTrouve; }
const Trouve& trouve(int i) { return trouves[i < 0 ? 0 : i >= kMaxTrouves ? kMaxTrouves - 1 : i]; }

const char* nomSecu(uint8_t secu) {
  switch (secu) {
    case WIFI_AUTH_OPEN: return "---";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK:
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "ENT";
    case WIFI_AUTH_WPA3_PSK:
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA3";
    default: return "?";
  }
}

bool secuOuverte(uint8_t secu) { return secu == WIFI_AUTH_OPEN; }
bool secuEntreprise(uint8_t secu) { return secu == WIFI_AUTH_WPA2_ENTERPRISE; }

int barres(int r) {
  if (r == 0) return 0;
  if (r >= -55) return 5;
  if (r >= -65) return 4;
  if (r >= -72) return 3;
  if (r >= -80) return 2;
  return 1;
}

}  // namespace reseau
