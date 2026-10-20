// Simulateur : le vrai firmware tourne sur PC face à un Minitel 2
// simulé, qui interprète le Vidéotex reçu et répond aux commandes PRO.
// Chaque étape du scénario imprime l'écran obtenu (rangées 0 à 24) et le nombre
// d'octets envoyés, donc le temps d'affichage à 4800 bauds.
//
// Depuis la racine du sketch (Linux ou WSL), avec MINITEL1B le dossier de la
// bibliothèque Minitel1B_Hard :
//   g++ -std=c++17 -DARDUINO=10819 -DSIMULATEUR -Itest/rendu/stubs -I"$MINITEL1B"
//       test/rendu/simulateur.cpp "$MINITEL1B/Minitel1B_Hard.cpp" src/Sessions/Session.cpp
//       src/MinitelIO/MinitelIO.cpp src/Pages/*.cpp src/app/*.cpp -o /tmp/rendu
//   /tmp/rendu        (scénario J4 ; « /tmp/rendu j3 » ou « j2 » pour les précédents)
// (une seule commande g++, coupée ici pour la lecture)

#include <Arduino.h>
#include <WiFi.h>

#include "../../src/Sessions/Session.h"

#include <map>
#include <string>
#include <vector>

uint32_t g_ms = 0;
HardwareSerial Serial;
HardwareSerial Serial2;
EspClass ESP;
WiFiClass WiFi;
int32_t simWifiDelaiMs = 3000;
std::map<std::string, std::string> simNvs;

// Bornes autour du Minitel simulé ; MaBox est celle de secrets.example.h.
std::vector<Borne> simBornes = {
    {"Voisin-5G", -67, WIFI_AUTH_WPA2_PSK, "x"},
    {"MaBox", -48, WIFI_AUTH_WPA2_PSK, "motdepasse"},
    {"FreeWifi", -74, WIFI_AUTH_OPEN, ""},
    {"Atelier", -83, WIFI_AUTH_WPA3_PSK, "x"},
    {"Bureau", -70, WIFI_AUTH_WPA2_ENTERPRISE, "x"},
    {"Caf\xc3\xa9 du Port", -77, WIFI_AUTH_WPA2_PSK, "x"},
    {"MaBox", -71, WIFI_AUTH_WPA2_PSK, "motdepasse"},  // répéteur : même nom
    {"Livebox-A1B2", -79, WIFI_AUTH_WPA_WPA2_PSK, "x"},
    {"SFR_7F3A", -81, WIFI_AUTH_WPA2_PSK, "x"},
    {"Imprimante-HP", -60, WIFI_AUTH_WPA2_PSK, "x"},
    {"Un-nom-de-reseau-tres-tres-long-32", -88, WIFI_AUTH_WPA2_PSK, "x"},
    {"Bbox-99", -90, WIFI_AUTH_WEP, "x"},
};

void setup();
void loop();

// ------------------------------------------------------------------ serveur simulé
// Une session factice : « mort » dans le nom d'hôte fait échouer la connexion,
// go.minipavi.fr raccroche tout seul au bout de 3 secondes, les autres envoient
// une page d'accueil Vidéotex puis l'écho des touches.
int simCoupures = 0;

class SessionSim : public ses::Session {
 public:
  bool connecter(const ses::Cible& c) override {
    hote = c.hote.c_str();
    fprintf(stderr, "[sim] connexion %s://%s:%u%s\n", ses::nomProtocole(c.protocole), hote.c_str(),
            c.portEffectif(), c.chemin.c_str());
    if (hote.find("mort") != std::string::npos) {
      definirErreur("DNS : %s introuvable", hote.c_str());
      return false;
    }
    g_ms += 1200;  // le temps que le serveur réponde
    ouvert = true;
    debut = g_ms;
    accueil = false;
    return true;
  }
  int lire(uint8_t* buf, size_t taille) override {
    if (!ouvert) return -1;
    // coupure.local : la liaison tombe 3 s après chacune des simCoupures premières connexions.
    if (hote == "coupure.local" && simCoupures > 0 && g_ms - debut > 3000) {
      --simCoupures;
      ouvert = false;
      definirErreur("fermée par le serveur");
      return -1;
    }
    if (hote == "go.minipavi.fr" && g_ms - debut > 3000) {
      ouvert = false;
      definirErreur("fermée par le serveur");
      return -1;
    }
    if (!accueil) {
      accueil = true;
      std::string p = "\x0c\x1f\x45\x4b\x1b\x4d" "BIENVENUE\x1f\x48\x45" "serveur " + hote + "\x1f\x4a\x45Tapez une touche :";
      const size_t n = std::min(taille, p.size());
      memcpy(buf, p.data(), n);
      return int(n);
    }
    if (!echo.empty()) {
      const size_t n = std::min(taille, echo.size());
      memcpy(buf, echo.data(), n);
      echo.erase(0, n);
      return int(n);
    }
    return 0;
  }
  bool envoyer(const uint8_t* d, size_t n) override {
    std::string r;
    for (size_t i = 0; i < n; ++i) {
      char h[4];
      snprintf(h, sizeof(h), "%02X ", d[i]);
      r += h;
      if (d[i] >= 0x20 && d[i] < 0x7F) echo += char(d[i]);
    }
    fprintf(stderr, "[sim] serveur reçoit : %s\n", r.c_str());
    return true;
  }
  void fermer() override { ouvert = false; }
  bool ouverte() const override { return ouvert; }

 private:
  std::string hote, echo;
  bool ouvert = false, accueil = false;
  uint32_t debut = 0;
};

SessionSim sessionSim;

// Serveur SSH de nas.local : mot de passe "secret". simCleChangee simule une
// réinstallation (autre clé d'hôte).
bool simCleChangee = false;

class SshSim : public ses::SessionAuth {
 public:
  bool transport(const ses::Cible& c) override {
    fprintf(stderr, "[sim] ssh %s@%s:%u\n", c.utilisateur.c_str(), c.hote.c_str(), c.portEffectif());
    g_ms += 900;
    transportOk = true;
    return true;
  }
  const char* typeCle() const override { return "ssh-ed25519"; }
  const char* empreinte() const override {
    return simCleChangee ? "SHA256:Zz1PqR7tYu2Ii9Oo0Pp3Aa4Ss5Dd6Ff7Gg8Hh9Jj0KkL"
                         : "SHA256:Kx9f2mQ1v8Lr0pTzAa7cYb3NwE5hJd6sUoGiR4kQxyz";
  }
  Auth authentifier(const ses::Cible& c) override {
    fprintf(stderr, "[sim] ssh mot de passe « %s »\n", c.motDePasse.c_str());
    g_ms += 300;
    if (c.motDePasse == "secret") return Auth::Ok;
    definirErreur("mot de passe refusé pour %s", c.utilisateur.c_str());
    return Auth::Refusee;
  }
  // Clé privée : celle générée par le simulateur (Cles.cpp sans libssh) est acceptée.
  Auth authentifierCle(const ses::Cible& c) override {
    fprintf(stderr, "[sim] ssh clé %u octets\n", unsigned(c.clePrivee.length()));
    g_ms += 300;
    if (strstr(c.clePrivee.c_str(), "simulee")) return Auth::Ok;
    definirErreur("clé refusée pour %s", c.utilisateur.c_str());
    return Auth::Refusee;
  }
  bool ouvrirShell(const ses::Cible& c) override {
    fprintf(stderr, "[sim] ssh pty %ux%u\n", c.colonnes, c.lignes);
    ouvert = true;
    envoye = false;
    return true;
  }
  int lire(uint8_t* buf, size_t taille) override {
    if (!ouvert) return -1;
    if (envoye) return 0;
    envoye = true;
    // Ce qu'envoie un vrai shell : titre de fenêtre, couleurs, accents, cadre.
    const std::string p =
        "\x1b]0;moi@nas: ~\x07\x1b[?2004h"
        "\xc3\x89t\xc3\xa9 \xc3\xa0 l'h\xc3\xb4tel, d\xc3\xa9j\xc3\xa0 vu \xc2\xbd \xe2\x94\x8c\xe2\x94\x80\xe2\x94\x90\r\n"
        "\x1b[01;32mmoi@nas\x1b[00m:\x1b[01;34m~\x1b[00m$ \x1b[7minv\x1b[27m \x1b[2K\x1b[3;5H";
    const size_t n = std::min(taille, p.size());
    memcpy(buf, p.data(), n);
    return int(n);
  }
  bool envoyer(const uint8_t* d, size_t n) override {
    std::string r;
    for (size_t i = 0; i < n; ++i) {
      char h[4];
      snprintf(h, sizeof(h), "%02X ", d[i]);
      r += h;
    }
    fprintf(stderr, "[sim] ssh reçoit : %s\n", r.c_str());
    return true;
  }
  void fermer() override { ouvert = transportOk = false; }
  bool ouverte() const override { return ouvert; }

 private:
  bool transportOk = false, ouvert = false, envoye = false;
};

SshSim sshSim;

ses::Session* simCreerSession(ses::Protocole p) {
  if (p == ses::Protocole::Ssh) return &sshSim;
  return &sessionSim;
}

namespace {

// ------------------------------------------------------------------ écran

struct Cellule {
  std::string car = " ";  // UTF-8, pour l'affichage
  bool inverse = false;
  bool clignote = false;
  bool haut = false;      // moitié haute d'un caractère en double hauteur
  bool bas = false;
  int couleur = 7;
};

struct Ecran {
  Cellule c[25][41];
  int r = 1, col = 1;
  int sauveR = 1, sauveCol = 1;  // position avant l'entrée en rangée 0
  bool g1 = false, inverse = false, clignote = false, doubleH = false, curseur = false;
  int couleur = 7;
  std::string dernier = " ";
  bool dernierG1 = false;
  int bips = 0;
  size_t octets = 0;

  void attributsParDefaut() {
    g1 = inverse = clignote = doubleH = false;
    couleur = 7;
  }
  void effacer() {
    for (int y = 1; y <= 24; ++y)
      for (int x = 1; x <= 40; ++x) c[y][x] = Cellule();
    r = col = 1;
    attributsParDefaut();
  }
  void poser(const std::string& s) {
    if (r >= 0 && r <= 24 && col >= 1 && col <= 40) {
      Cellule& k = c[r][col];
      k.car = s;
      k.inverse = inverse && !g1;
      k.clignote = clignote;
      k.couleur = couleur;
      k.haut = false;
      k.bas = doubleH;
      if (doubleH && r > 1) {
        Cellule& h = c[r - 1][col];
        h = k;
        h.bas = false;
        h.haut = true;
      }
    }
    dernier = s;
    dernierG1 = g1;
    if (++col > 40) {
      col = 1;
      if (r > 0) ++r;
    }
  }
};

Ecran ecran;

std::string mosaique(uint8_t v) {
  if (v == 0x5F) return "#";
  if (v >= 0x40 && v <= 0x5E) return std::string(1, char(v));  // majuscules en G1
  const int k = v - 0x20;
  const bool tl = k & 1, tr = k & 2, ml = k & 4, mr = k & 8, bl = k & 16, br = k & 64;
  const int n = tl + tr + ml + mr + bl + br;
  if (n == 0) return " ";
  if (n == 6) return "#";
  if (tl && tr && n == 2) return "^";
  if (bl && br && n == 2) return "_";
  if (ml && mr && n == 2) return "-";
  return "%";
}

std::string utf8(uint32_t u) {
  std::string s;
  if (u < 0x80) s += char(u);
  else if (u < 0x800) { s += char(0xC0 | (u >> 6)); s += char(0x80 | (u & 0x3F)); }
  else { s += char(0xE0 | (u >> 12)); s += char(0x80 | ((u >> 6) & 0x3F)); s += char(0x80 | (u & 0x3F)); }
  return s;
}

uint32_t compose(uint8_t accent, uint8_t lettre) {
  const char* base = "aeiouc";
  static const uint32_t table[5][6] = {
      {0xE0, 0xE8, 'i', 'o', 0xF9, 'c'},    // grave
      {'a', 0xE9, 'i', 'o', 'u', 'c'},      // aigu
      {0xE2, 0xEA, 0xEE, 0xF4, 0xFB, 'c'},  // circonflexe
      {0xE4, 0xEB, 0xEF, 0xF6, 0xFC, 'c'},  // tréma
      {'a', 'e', 'i', 'o', 'u', 0xE7},      // cédille
  };
  const char* p = strchr(base, lettre);
  int ligne = accent == 0x41 ? 0 : accent == 0x42 ? 1 : accent == 0x43 ? 2 : accent == 0x48 ? 3 : accent == 0x4B ? 4 : -1;
  if (!p || ligne < 0) return lettre;
  return table[ligne][p - base];
}

// ------------------------------------------------------------------ Minitel simulé

std::vector<uint8_t> seq;  // séquence en cours d'analyse

// Mode mixte (80 colonnes) : l'écran simulé n'a que 40 colonnes, on journalise
// donc le flux reçu, séquences comprises.
bool mixte = false;
std::string journal;

void repondre(std::initializer_list<uint8_t> b) {
  for (uint8_t x : b) Serial2.entree.push_back(x);
}

bool simEteint = false;  // Minitel pas encore allumé : il n'entend rien

void recevoir(uint8_t b) {
  if (simEteint) return;
  ++ecran.octets;
  if (mixte) {
    char h[8];
    if (b == 0x1B) journal += "\u238B";
    else if (b == 0x19) journal += "<SS2>";
    else if (b == 0x0D) journal += "<CR>";
    else if (b == 0x0A) journal += "<LF>";
    else if (b >= 0x20 && b < 0x7F) journal += char(b);
    else { snprintf(h, sizeof(h), "<%02X>", b); journal += h; }
  }
  if (!seq.empty()) {
    seq.push_back(b);
    const uint8_t t = seq[0];
    if (t == 0x1B) {
      const uint8_t s = seq[1];
      if (s == 0x39 || s == 0x3A || s == 0x3B) {  // PRO1, PRO2, PRO3
        const size_t lg = 2 + (s - 0x38);
        if (seq.size() < lg) return;
        if (s == 0x39 && seq[2] == 0x74) repondre({0x1B, 0x3A, 0x75, 0x76});            // vitesse : 4800
        if (s == 0x39 && seq[2] == 0x7B) repondre({0x01, 0x42, 0x76, 0x34, 0x04});      // Minitel 2 Philips
        if (s == 0x39 && seq[2] == 0x67) repondre({0x13, 0x59});                         // modem coupé : SEP 5/9
        if (s == 0x3A && seq[2] == 0x32 && seq[3] == 0x7D) { mixte = true; journal.clear(); repondre({0x13, 0x70}); }
        if (s == 0x3A && seq[2] == 0x32 && seq[3] == 0x7E) { mixte = false; repondre({0x13, 0x71}); }
        if (s == 0x3A && seq[2] == 0x6B) repondre({0x1B, 0x3A, 0x75, seq[3]});          // changement de vitesse
        if (s == 0x3A && (seq[2] == 0x69 || seq[2] == 0x6A)) repondre({0x1B, 0x3A, 0x73, 0x41});
        if (s == 0x3B && (seq[2] == 0x60 || seq[2] == 0x61)) repondre({0x1B, 0x3B, 0x63, seq[3], 0x40});
        if (s == 0x3B && (seq[2] == 0x69 || seq[2] == 0x6A)) repondre({0x1B, 0x3B, 0x73, 0x59, 0x41});
        seq.clear();
        return;
      }
      if (s == 0x5B) {  // CSI : jusqu'à la lettre finale
        if (seq.size() < 3 || b < 0x40) return;
        if (b == 'H') {
          int y = 0, x = 0, *p = &y;
          for (size_t i = 2; i + 1 < seq.size(); ++i) {
            if (seq[i] == ';') p = &x;
            else *p = *p * 10 + (seq[i] - '0');
          }
          ecran.r = y;
          ecran.col = x;
        }
        seq.clear();
        return;
      }
      if (s >= 0x40 && s <= 0x47) ecran.couleur = s - 0x40;
      else if (s == 0x48) ecran.clignote = true;
      else if (s == 0x49) ecran.clignote = false;
      else if (s == 0x4C) ecran.doubleH = false;
      else if (s == 0x4D) ecran.doubleH = true;
      else if (s == 0x5C) ecran.inverse = false;
      else if (s == 0x5D) ecran.inverse = true;
      seq.clear();
      return;
    }
    if (t == 0x1F) {  // US rangée colonne
      if (seq.size() < 3) return;
      const int y = seq[1] - 0x40, x = seq[2] - 0x40;
      if (y == 0 && ecran.r != 0) {
        ecran.sauveR = ecran.r;
        ecran.sauveCol = ecran.col;
      }
      ecran.r = y;
      ecran.col = x;
      ecran.attributsParDefaut();
      seq.clear();
      return;
    }
    if (t == 0x12) {  // REP
      const int n = b - 0x40;
      const bool g1 = ecran.g1;
      ecran.g1 = ecran.dernierG1;
      const std::string d = ecran.dernier;
      for (int i = 0; i < n; ++i) ecran.poser(d);
      ecran.g1 = g1;
      seq.clear();
      return;
    }
    if (t == 0x19) {  // SS2
      const uint8_t s = seq[1];
      if (s == 0x41 || s == 0x42 || s == 0x43 || s == 0x48 || s == 0x4B) {
        if (seq.size() < 3) return;
        ecran.poser(utf8(compose(s, seq[2])));
      } else {
        const uint32_t sym = s == 0x2E ? 0x2192 : s == 0x2C ? 0x2190 : s == 0x2F ? 0x2193 : s == 0x23 ? 0xA3 : s == 0x30 ? 0xB0 : '?';
        ecran.poser(utf8(sym));
      }
      seq.clear();
      return;
    }
    seq.clear();
  }

  switch (b) {
    case 0x07: ++ecran.bips; return;
    case 0x08: if (ecran.col > 1) --ecran.col; return;
    case 0x09: ++ecran.col; return;
    case 0x0A:
      if (ecran.r == 0) {
        ecran.r = ecran.sauveR;
        ecran.col = ecran.sauveCol;
      } else {
        ++ecran.r;
      }
      return;
    case 0x0B: --ecran.r; return;
    case 0x0C: ecran.effacer(); return;
    case 0x0D: ecran.col = 1; return;
    case 0x0E: ecran.g1 = true; return;
    case 0x0F: ecran.g1 = false; return;
    case 0x11: ecran.curseur = true; return;
    case 0x14: ecran.curseur = false; return;
    case 0x18:
      for (int x = ecran.col; x <= 40; ++x) {
        Cellule k;
        k.inverse = ecran.inverse;
        if (ecran.r >= 0 && ecran.r <= 24) ecran.c[ecran.r][x] = k;
      }
      return;
    case 0x1E:
      ecran.r = ecran.col = 1;
      ecran.attributsParDefaut();
      return;
    case 0x12: case 0x19: case 0x1B: case 0x1F:
      seq.push_back(b);
      return;
  }
  if (b < 0x20) return;
  ecran.poser(ecran.g1 ? mosaique(b) : std::string(1, char(b)));
}

// ------------------------------------------------------------------ scénario

void tourner(uint32_t ms) {
  const uint32_t fin = g_ms + ms;
  while (g_ms < fin) loop();
}

void touche(std::initializer_list<uint8_t> b) {
  for (uint8_t x : b) Serial2.entree.push_back(x);
  tourner(300);
}

void taper(const char* s) {
  while (*s) touche({uint8_t(*s++)});
}

constexpr uint8_t SEP = 0x13;

void montrer(const char* titre) {
  printf("\n=== %s  (%zu octets, %.1f s à 4800 bauds, %d bip%s, curseur %s en %d,%d)\n", titre, ecran.octets,
         ecran.octets / 480.0, ecran.bips, ecran.bips > 1 ? "s" : "", ecran.curseur ? "visible" : "caché", ecran.r,
         ecran.col);
  printf("    +----------------------------------------+\n");
  std::string inverses;
  for (int y = 0; y <= 24; ++y) {
    printf(" %2d |", y);
    for (int x = 1; x <= 40; ++x) printf("%s", ecran.c[y][x].car.c_str());
    printf("|");
    std::string marques;
    for (int x = 1; x <= 40; ++x) {
      const Cellule& k = ecran.c[y][x];
      if (k.haut) { marques = " (double hauteur)"; }
    }
    printf("%s\n", marques.c_str());
    for (int x = 1; x <= 40; ++x) {
      if (ecran.c[y][x].inverse && (x == 1 || !ecran.c[y][x - 1].inverse)) {
        int f = x;
        while (f < 40 && ecran.c[y][f + 1].inverse) ++f;
        std::string t;
        for (int i = x; i <= f; ++i) t += ecran.c[y][i].car;
        inverses += "[" + t + "] ";
      }
    }
  }
  printf("    +----------------------------------------+\n");
  if (!inverses.empty()) printf("    inversion vidéo : %s\n", inverses.c_str());
  if (!journal.empty()) printf("    flux en mode mixte (80 colonnes) : %s\n", journal.c_str());
  journal.clear();
  ecran.octets = 0;
  ecran.bips = 0;
}

}  // namespace

void scenarioJ2() {
  setup();
  tourner(500);
  montrer("Démarrage : P00 Mire, WiFi en cours");
  tourner(3500);
  montrer("P00 Mire, WiFi connecté");
  tourner(6000);
  montrer("P10 Sommaire (passage automatique)");
  taper("5");
  touche({SEP, 0x41});
  montrer("P60 Informations (5 puis ENVOI)");
  touche({SEP, 0x44});
  montrer("P99 Guide de P60 (GUIDE)");
  touche({SEP, 0x42});
  montrer("Retour à P60 (RETOUR)");
  touche({SEP, 0x42});
  montrer("Retour à P10 (RETOUR)");
  taper("teaser");
  montrer("P10, saisie de « teaser »");
  touche({SEP, 0x49});
  montrer("P10, CONNEXION/FIN sur TEASER");
  taper("ab");
  touche({SEP, 0x47});
  montrer("P10, saisie « ab » puis CORRECTION");
  touche({SEP, 0x45});
  touche({SEP, 0x45});
  taper("2");
  touche({SEP, 0x41});
  montrer("P10, 2 puis ENVOI (rubrique J4)");
  tourner(6000);
  montrer("P10, après effacement du message");
  touche({SEP, 0x42});
  montrer("P10, RETOUR au fond de la pile");
  taper("5");
  touche({SEP, 0x41});
  touche({SEP, 0x46});
  montrer("P60 puis SOMMAIRE");
}

// Sans secrets.h : aucun réseau connu, tout se règle depuis le Minitel.
void scenarioJ3() {
  setup();
  tourner(3000);
  montrer("P00 Mire, aucun réseau mémorisé");
  touche({SEP, 0x41});
  montrer("ENVOI : P41 directement, recherche en cours");
  tourner(3500);
  montrer("P41, réseaux trouvés (page 1)");
  touche({SEP, 0x48});
  montrer("P41, SUITE (page 2)");
  touche({SEP, 0x42});
  taper("4");
  touche({SEP, 0x41});
  montrer("P41, RETOUR (page 1) puis 4 ENVOI : réseau d'entreprise");
  taper("1");
  touche({SEP, 0x41});
  montrer("P42, mot de passe de MaBox");
  taper("mauvais");
  touche({SEP, 0x41});
  montrer("P42, 7 caractères puis ENVOI");
  taper("1");
  touche({SEP, 0x41});
  montrer("P40 après ENVOI, connexion en cours");
  tourner(6500);
  montrer("P40, échec : mot de passe refusé");
  taper("1");
  touche({SEP, 0x41});
  tourner(3500);
  taper("1");
  touche({SEP, 0x41});
  touche({SEP, 0x45});
  taper("motdepasse");
  montrer("P42, bon mot de passe tapé");
  touche({SEP, 0x41});
  tourner(4000);
  montrer("P40, connecté à MaBox");
  taper("1");
  touche({SEP, 0x41});
  tourner(3500);
  montrer("P41, MaBox mémorisée (*) et connectée");
  taper("m");
  touche({SEP, 0x41});
  taper("Cache");
  touche({SEP, 0x48});
  taper("abc");
  touche({SEP, 0x41});
  montrer("P42 réseau caché : nom, SUITE, mot de passe trop court");
  touche({SEP, 0x46});
  montrer("SOMMAIRE : P10, rangée 0 connectée");
  taper("3");
  touche({SEP, 0x41});
  taper("2");
  touche({SEP, 0x41});
  montrer("P43, réseaux mémorisés");
  taper("s1");
  touche({SEP, 0x41});
  montrer("P43, S1 puis ENVOI : MaBox oubliée");
  touche({SEP, 0x42});
  taper("3");
  touche({SEP, 0x41});
  montrer("P40, 3 : rien à reconnecter");
  printf("\nNVS :\n");
  for (auto& kv : simNvs) printf("  %s = %s\n", kv.first.c_str(), kv.second.c_str());
}

// Un réseau mémorisé (MaBox), l'annuaire préréglé d'iodeo, des appels.
void scenarioJ4() {
  simNvs["wifi/n"] = "1";
  simNvs["wifi/s0"] = "MaBox";
  simNvs["wifi/p0"] = "motdepasse";
  simNvs["wifi/init"] = "1";
  setup();
  tourner(12000);
  taper("1");
  touche({SEP, 0x41});
  montrer("P20 Annuaire préréglé (1 puis ENVOI au Sommaire)");
  taper("1");
  touche({SEP, 0x41});
  montrer("P21 Fiche de TEASER");
  touche({SEP, 0x49});
  montrer("P70 Connexion en cours (CONNEXION/FIN sur la fiche)");
  tourner(1500);
  montrer("P80 Session : la page du serveur");
  taper("a");
  tourner(38000);
  montrer("P80 : 40 s plus tard, la session tient");
  touche({SEP, 0x59});  // CONNEXION/FIN seule, comme le Minitel 1B
  montrer("P90 Fin de connexion (CONNEXION/FIN)");
  touche({SEP, 0x41});
  montrer("ENVOI : retour à la fiche, dernière connexion notée");
  touche({SEP, 0x46});
  taper("pavi");
  touche({SEP, 0x49});
  tourner(5000);
  montrer("Sommaire, code PAVI puis CONNEXION/FIN : le serveur raccroche");
  tourner(16000);
  montrer("P90 : retour automatique au Sommaire");
  taper("2");
  touche({SEP, 0x41});
  taper("ws://mort.example/ws");
  montrer("P30 Connexion directe, adresse tapée");
  touche({SEP, 0x49});
  tourner(500);
  montrer("P73 Échec de connexion");
  touche({SEP, 0x42});
  touche({SEP, 0x45});
  taper("bbs.exemple.fr:2323");
  touche({SEP, 0x49});
  tourner(2000);
  touche({SEP, 0x49});
  montrer("P90 après une connexion directe : A propose l'ajout");
  taper("a");
  montrer("P22 Nouveau service, rempli depuis l'adresse");
  for (int i = 0; i < 6; ++i) touche({SEP, 0x48});
  montrer("P22 écran 2 : options (6 fois SUITE)");
  touche({SEP, 0x41});
  montrer("ENVOI : la nouvelle fiche P21");
  touche({SEP, 0x46});
  taper("1");
  touche({SEP, 0x41});
  montrer("P20 avec le nouveau service");
  taper("8");
  touche({SEP, 0x41});
  taper("s");
  touche({SEP, 0x41});
  montrer("P23 Suppression (8, puis S ENVOI)");
  touche({SEP, 0x41});
  montrer("ENVOI : supprimé, retour à P20");
  taper("+");
  touche({SEP, 0x41});
  touche({SEP, 0x41});
  montrer("P22 Nouveau service vide, ENVOI : code manquant");
  printf("\nNVS annuaire :\n");
  for (auto& kv : simNvs)
    if (kv.first.rfind("annuaire/", 0) == 0) printf("  %s = %s\n", kv.first.c_str(), kv.second.c_str());
}

void scenarioJ6() {
  simNvs["wifi/n"] = "1";
  simNvs["wifi/s0"] = "MaBox";
  simNvs["wifi/p0"] = "motdepasse";
  simNvs["wifi/init"] = "1";
  setup();
  tourner(12000);
  taper("4");
  touche({SEP, 0x41});
  montrer("P50 Paramètres (4 puis ENVOI)");
  taper("2");
  touche({SEP, 0x41});
  taper("4");
  touche({SEP, 0x41});
  taper("4");
  touche({SEP, 0x41});
  montrer("P50 : coût non, retour après la fin : jamais (2, puis 4 deux fois)");
  touche({SEP, 0x44});
  montrer("P99 Guide de P50");
  touche({SEP, 0x42});
  taper("9");
  touche({SEP, 0x41});
  taper("3");
  touche({SEP, 0x41});
  montrer("P54 : 3 puis ENVOI, confirmation demandée");
  touche({SEP, 0x41});
  montrer("P54 ENVOI : clés SSH oubliées");
  taper("4");
  touche({SEP, 0x41});
  touche({SEP, 0x45});
  montrer("P54 : 4 puis ENVOI, puis ANNULATION : rien n'est effacé");
  touche({SEP, 0x46});
  taper("pavi");
  touche({SEP, 0x59});
  tourner(300);
  montrer("P70 : appel avec le bruit de connexion (2 bips)");
  tourner(5000);
  montrer("P90 sans le coût");
  tourner(30000);
  montrer("P90 30 s plus tard : reste affichée (retour : jamais)");
  printf("\nNVS reglages :\n");
  for (auto& kv : simNvs)
    if (kv.first.rfind("reglages/", 0) == 0) printf("  %s = %s\n", kv.first.c_str(), kv.second.c_str());
  touche({SEP, 0x41});
  touche({SEP, 0x46});
  taper("4");
  touche({SEP, 0x41});
  taper("9");
  touche({SEP, 0x41});
  taper("4");
  touche({SEP, 0x41});
  touche({SEP, 0x41});
  printf("\nNVS après « Tout » (seule la marque wifi/init doit rester) :\n");
  for (auto& kv : simNvs) printf("  %s = %s\n", kv.first.c_str(), kv.second.c_str());
}

// Minitel allumé après l'ESP32 : sondé toutes les 5 s, puis réglé et la page redessinée.
void scenarioAllumage() {
  simNvs["wifi/n"] = "1";
  simNvs["wifi/s0"] = "MaBox";
  simNvs["wifi/p0"] = "motdepasse";
  simNvs["wifi/init"] = "1";
  simEteint = true;
  setup();
  tourner(15000);
  simEteint = false;
  tourner(6000);
  montrer("Minitel allumé 15 s après l'ESP32 : détecté, Sommaire redessiné");
  taper("2");
  touche({SEP, 0x41});
  taper("ssh://moi@nas.local");
  montrer("P30 : Texte 80 proposé, le Minitel est reconnu");
}

void scenarioJ5() {
  simNvs["wifi/n"] = "1";
  simNvs["wifi/s0"] = "MaBox";
  simNvs["wifi/p0"] = "motdepasse";
  simNvs["wifi/init"] = "1";
  // Une fiche SSH en 40 colonnes, avec son mot de passe.
  simNvs["annuaire/n"] = "1";
  simNvs["annuaire/f0"] = "1\tNAS\tMon NAS\t3\tnas.local\t0\t/\t1\t0\t0\t0\t\t0\tmoi\tsecret\t0\t0";
  setup();
  tourner(12000);

  taper("2");
  touche({SEP, 0x41});
  taper("ssh://moi@nas.local");
  montrer("P30 adresse SSH tapée : affichage Texte 80 d'office");
  touche({SEP, 0x48});
  montrer("P30 SUITE : affichage Vidéotex");
  touche({SEP, 0x48});
  touche({SEP, 0x48});
  touche({SEP, 0x59});
  tourner(500);
  montrer("P72 Première connexion SSH : l'empreinte à vérifier");
  touche({SEP, 0x41});
  tourner(300);
  montrer("P71 ENVOI : clé acceptée, mot de passe demandé");
  taper("faux");
  montrer("P71 mot de passe tapé (masqué)");
  touche({SEP, 0x41});
  tourner(300);
  montrer("P71 mot de passe refusé : deuxième essai");
  taper("secret");
  touche({SEP, 0x41});
  tourner(1500);
  montrer("P80 Shell en 80 colonnes (mode mixte)");
  touche({0x19, 0x42, 'e'});  // é tapé au clavier
  touche({SEP, 0x59});
  montrer("P90 Fin de connexion SSH");
  touche({SEP, 0x41});

  touche({SEP, 0x46});
  taper("nas");
  touche({SEP, 0x59});
  tourner(1500);
  montrer("P80 Fiche NAS : clé connue, mot de passe de la fiche, 40 colonnes");
  touche({SEP, 0x59});
  touche({SEP, 0x41});

  simCleChangee = true;
  touche({SEP, 0x46});
  taper("nas");
  touche({SEP, 0x59});
  tourner(1500);
  montrer("P73 La clé de nas.local a changé : appel bloqué");
  taper("o");
  montrer("P73 O : ancienne clé oubliée");
  touche({SEP, 0x43});
  tourner(1500);
  montrer("P72 RÉPÉTITION : la nouvelle clé à vérifier");
  touche({SEP, 0x45});
  tourner(300);
  montrer("P73 ANNULATION : clé refusée");

  printf("\nNVS ssh_hotes :\n");
  for (auto& kv : simNvs)
    if (kv.first.rfind("ssh_hotes/", 0) == 0) printf("  %s = %s\n", kv.first.c_str(), kv.second.c_str());
}

// J7 : P52 portail web (activité, mise à jour simulées) et l'aller-retour JSON de l'annuaire.
namespace portail {
void simNoter(const char* t);
void simMaj(int p);
bool ouvert();
}
#include "../../src/app/Annuaire.h"

void listerAnnuaire() {
  for (int i = 0; i < annuaire::nombre(); ++i) {
    const annuaire::Fiche& f = annuaire::fiche(i);
    printf("  %-8s %-24s %s %s:%u %s aff=%s echo=%d rouleau=%d ping=%u util=%s mdp=%s\n", f.code, f.nom,
           annuaire::cleProtocole(f.proto), f.hote, f.port, f.chemin, annuaire::cleAffichage(f.affichage), f.echo,
           f.defilement, f.pingS, f.utilisateur, f.motDePasse);
  }
}

void scenarioJ7() {
  simNvs["wifi/n"] = "1";
  simNvs["wifi/s0"] = "MaBox";
  simNvs["wifi/p0"] = "motdepasse";
  simNvs["wifi/init"] = "1";
  setup();
  tourner(12000);
  taper("4");
  touche({SEP, 0x41});
  montrer("P50 avec le choix 6");
  taper("6");
  touche({SEP, 0x41});
  montrer("P52 portail ouvert");
  portail::simNoter("Navigateur 192.168.1.20");
  portail::simNoter("Fiche PAVI enregistrée");
  tourner(200);
  montrer("P52 : activité du navigateur");
  touche({SEP, 0x44});
  printf("GUIDE : portail ouvert = %d\n", portail::ouvert());
  montrer("P99 Guide de P52");
  touche({SEP, 0x42});
  portail::simMaj(0);
  portail::simMaj(45);
  tourner(200);
  montrer("P52 : mise à jour à 45 %");
  portail::simMaj(100);
  tourner(200);
  montrer("P52 : mise à jour reçue");
  touche({SEP, 0x42});
  tourner(200);
  printf("Après RETOUR : portail ouvert = %d\n", portail::ouvert());
  montrer("Retour à P50");

  String j;
  annuaire::exporter(j);
  printf("\nExport (%u octets) :\n%.400s...\n", unsigned(j.length()), j.c_str());

  // Réimport de l'export, remplacement complet : rien ne doit changer.
  std::string copie = j.c_str();
  annuaire::BilanImport b;
  bool ok = annuaire::importer(&copie[0], true, b);
  printf("\nRéimport complet : ok=%d ajoutées=%d remplacées=%d refusées=%d\n", ok, b.ajoutees, b.remplacees, b.refusees);
  String j2;
  annuaire::exporter(j2);
  printf("Export identique après réimport : %d\n", j2 == j);

  std::string fichier = R"([
    {"code":"nas","nom":"NAS d'été \"maison\"","protocole":"ssh","hote":"nas.local","port":2222,
     "utilisateur":"ben","affichage":"texte80","futur":{"x":[1,2,{"y":null}]},"echo":true},
    {"code":"PAVI","nom":"MiniPavi 2","protocole":"wss","hote":"go.minipavi.fr","port":8181,"chemin":"/ws"},
    {"code":"5","nom":"rubrique","protocole":"ws","hote":"x"},
    {"code":"MAUVAIS","protocole":"gopher","hote":"x"},
    {"code":"SSHSANS","protocole":"ssh","hote":"x"}
  ])";
  ok = annuaire::importer(&fichier[0], false, b);
  printf("\nImport d'un tableau : ok=%d ajoutées=%d remplacées=%d refusées=%d premier refus=%s\n", ok, b.ajoutees,
         b.remplacees, b.refusees, b.premierRefus);
  listerAnnuaire();

  std::string casse = R"({"services":[{"code":"A","hote":"x"},{"code":"B" "hote":"y"}]})";
  const int avant = annuaire::nombre();
  ok = annuaire::importer(&casse[0], true, b);
  printf("\nJSON cassé : ok=%d erreur=%s position=%u, services avant=%d après=%d\n", ok, b.erreur ? b.erreur : "-",
         unsigned(b.position), avant, annuaire::nombre());
}

// J8 : mire et page locale en .vdt, clé SSH, reconnexion WebSocket, fiche v1 relue.
#include "../../src/app/Cles.h"
#include "../../src/app/Fichiers.h"

void ecrireFichier(const char* chemin, const std::string& s) {
  fichiers::ecrire(chemin, reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

void scenarioJ8() {
  simNvs["wifi/n"] = "1";
  simNvs["wifi/s0"] = "MaBox";
  simNvs["wifi/p0"] = "motdepasse";
  simNvs["wifi/init"] = "1";
  // Annuaire écrit par la v0.7 (fiches en version 1, 17 champs).
  simNvs["annuaire/n"] = "1";
  simNvs["annuaire/f0"] = "1\tPAVI\tMiniPavi\t1\tgo.minipavi.fr\t8182\t/\t0\t0\t0\t0\t\t0\t\t\t0\t0";
  ecrireFichier("/vdt/MIRE.vdt", "\x0c\x1f\x46\x4a\x1b\x4d\x1b\x43" "MA MIRE A MOI" "\x1f\x4c\x48" "dessinee a l'editeur");
  ecrireFichier("/vdt/METEO.vdt", "\x0c\x1f\x43\x41\x1b\x44" "METEO LOCALE" "\x1f\x45\x41" "Beau temps sur l'atelier.");
  setup();
  tourner(1000);
  montrer("P00 : mire en fichier MIRE.vdt");
  tourner(12000);
  printf("Fiche v1 relue : %d fiche(s), %s reconnexion=%d cle='%s'\n", annuaire::nombre(), annuaire::fiche(0).code,
         annuaire::fiche(0).reconnexion, annuaire::fiche(0).cle);
  taper("meteo");
  touche({SEP, 0x41});
  montrer("P11 : page locale METEO tapée au Sommaire");
  touche({SEP, 0x46});

  const char* e = cles::generer("nas");
  printf("Clé générée : %s\n", e ? e : "ok");
  annuaire::Fiche f;
  strlcpy(f.code, "NAS", sizeof(f.code));
  strlcpy(f.nom, "NAS de l'atelier", sizeof(f.nom));
  f.proto = ses::Protocole::Ssh;
  strlcpy(f.hote, "nas.local", sizeof(f.hote));
  strlcpy(f.utilisateur, "moi", sizeof(f.utilisateur));
  strlcpy(f.cle, "nas", sizeof(f.cle));
  f.affichage = annuaire::Affichage::Texte80;
  annuaire::enregistrer(f, -1);
  simNvs["ssh_hotes/h"] = "";  // rien : la clé d'hôte sera demandée
  taper("1");
  touche({SEP, 0x41});
  taper("2");
  touche({SEP, 0x41});
  montrer("P21 : fiche NAS avec sa clé SSH");
  touche({SEP, 0x46});
  taper("nas");
  touche({SEP, 0x59});
  tourner(2000);
  montrer("P72 : clé d'hôte inconnue");
  taper("o");
  touche({SEP, 0x41});
  tourner(2000);
  montrer("Session SSH ouverte par la clé, sans mot de passe");
  touche({SEP, 0x59});
  tourner(1000);
  touche({SEP, 0x46});

  annuaire::Fiche w;
  strlcpy(w.code, "COUPE", sizeof(w.code));
  strlcpy(w.nom, "Serveur instable", sizeof(w.nom));
  w.proto = ses::Protocole::Ws;
  strlcpy(w.hote, "coupure.local", sizeof(w.hote));
  w.reconnexion = true;
  annuaire::enregistrer(w, -1);
  simCoupures = 1;
  taper("coupe");
  touche({SEP, 0x59});
  tourner(2500);
  montrer("Session WebSocket ouverte");
  tourner(2000);
  montrer("Liaison tombée : reconnexion annoncée en rangée 0");
  tourner(3000);
  montrer("Reconnecté : le serveur renvoie sa page");
  touche({SEP, 0x59});
  tourner(1000);
  montrer("P90 après CONNEXION/FIN");
  printf("\nNVS annuaire :\n");
  for (auto& kv : simNvs)
    if (kv.first.rfind("annuaire/", 0) == 0) printf("  %s = %s\n", kv.first.c_str(), kv.second.c_str());
}

int main(int argc, char** argv) {
  Serial2.write_hook = recevoir;
  const char* s = argc > 1 ? argv[1] : "j4";
  if (strcmp(s, "j2") == 0) scenarioJ2();
  else if (strcmp(s, "j3") == 0) scenarioJ3();
  else if (strcmp(s, "j5") == 0) scenarioJ5();
  else if (strcmp(s, "allumage") == 0) scenarioAllumage();
  else if (strcmp(s, "j6") == 0) scenarioJ6();
  else if (strcmp(s, "j7") == 0) scenarioJ7();
  else if (strcmp(s, "j8") == 0) scenarioJ8();
  else scenarioJ4();
  return 0;
}
