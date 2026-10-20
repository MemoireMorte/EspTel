#include "../config.h"

#include "Fichiers.h"

#if defined(SIMULATEUR) || defined(FICHIERS_EN_MEMOIRE)

// ------------------------------------------------------------------ en mémoire (PC)

#include <map>
#include <string>

namespace fichiers {

namespace {
std::map<std::string, std::string> contenu;
std::string enCours;
std::string tampon;
bool ouvert = false;
}  // namespace

bool monter() { return true; }
bool monte() { return true; }
size_t total() { return 196608; }
size_t utilise() {
  size_t n = 0;
  for (auto& kv : contenu) n += kv.second.size();
  return n;
}
bool existe(const char* c) { return contenu.count(c) > 0; }
size_t taille(const char* c) { return existe(c) ? contenu[c].size() : 0; }
bool supprimer(const char* c) { return contenu.erase(c) > 0; }
bool renommer(const char* de, const char* vers) {
  if (!existe(de)) return false;
  contenu[vers] = contenu[de];
  contenu.erase(de);
  return true;
}

bool lire(const char* c, String& texte, size_t max) {
  if (!existe(c) || contenu[c].size() > max) return false;
  texte = String(contenu[c]);
  return true;
}

bool ecrire(const char* c, const uint8_t* d, size_t n) {
  contenu[c] = std::string(reinterpret_cast<const char*>(d), n);
  return true;
}

bool parcourir(const char* c, const std::function<bool(const uint8_t*, size_t)>& fn) {
  if (!existe(c)) return false;
  const std::string& s = contenu[c];
  for (size_t i = 0; i < s.size(); i += 256) {
    if (!fn(reinterpret_cast<const uint8_t*>(s.data()) + i, std::min<size_t>(256, s.size() - i))) break;
  }
  return true;
}

void lister(const char* dossier, const std::function<void(const char*, size_t)>& fn) {
  const std::string prefixe = std::string(dossier) + "/";
  for (auto& kv : contenu) {
    if (kv.first.compare(0, prefixe.size(), prefixe) == 0) fn(kv.first.c_str() + prefixe.size(), kv.second.size());
  }
}

bool commencer(const char* c) {
  enCours = c;
  tampon.clear();
  ouvert = true;
  return true;
}

bool ajouter(const uint8_t* d, size_t n) {
  if (!ouvert) return false;
  tampon.append(reinterpret_cast<const char*>(d), n);
  return true;
}

bool terminer(bool garder) {
  if (!ouvert) return false;
  ouvert = false;
  if (garder) contenu[enCours] = tampon;
  tampon.clear();
  return garder;
}

}  // namespace fichiers

#else

// ------------------------------------------------------------------ LittleFS

#include <LittleFS.h>

namespace fichiers {

namespace {

bool estMonte = false;
File ecriture;
String cheminFinal;

String temporaire(const char* chemin) { return String(chemin) + ".tmp"; }

}  // namespace

bool monter() {
  if (estMonte) return true;
  // true : formatée si elle ne se monte pas (premier démarrage, ancienne SPIFFS).
  estMonte = LittleFS.begin(true, "/littlefs", 4, "spiffs");
  if (estMonte) {
    DEBUG_UART.printf("[Fichiers] LittleFS %u Ko, %u Ko utilisés\n", unsigned(LittleFS.totalBytes() / 1024),
                      unsigned(LittleFS.usedBytes() / 1024));
  } else {
    DEBUG_UART.println("[Fichiers] partition spiffs absente : schéma Minimal SPIFFS ?");
  }
  return estMonte;
}

bool monte() { return estMonte; }
size_t total() { return estMonte ? LittleFS.totalBytes() : 0; }
size_t utilise() { return estMonte ? LittleFS.usedBytes() : 0; }

bool existe(const char* chemin) { return estMonte && LittleFS.exists(chemin); }

size_t taille(const char* chemin) {
  if (!existe(chemin)) return 0;
  File f = LittleFS.open(chemin, "r");
  const size_t n = f ? f.size() : 0;
  f.close();
  return n;
}

bool supprimer(const char* chemin) { return existe(chemin) && LittleFS.remove(chemin); }

bool renommer(const char* de, const char* vers) {
  if (!existe(de)) return false;
  LittleFS.remove(vers);
  // rename() ne crée pas le dossier d'arrivée : un fichier vide le fait.
  if (strrchr(vers, '/') != vers) {
    File f = LittleFS.open(vers, "w", true);
    f.close();
    LittleFS.remove(vers);
  }
  return LittleFS.rename(de, vers);
}

bool lire(const char* chemin, String& texte, size_t max) {
  if (!existe(chemin)) return false;
  File f = LittleFS.open(chemin, "r");
  if (!f || f.size() > max) return false;
  texte = "";
  texte.reserve(f.size());
  char t[128];
  size_t n;
  while ((n = f.read(reinterpret_cast<uint8_t*>(t), sizeof(t) - 1)) > 0) {
    t[n] = '\0';
    texte += t;
  }
  f.close();
  return true;
}

bool ecrire(const char* chemin, const uint8_t* donnees, size_t n) {
  if (!commencer(chemin)) return false;
  const bool ok = ajouter(donnees, n);
  return terminer(ok) && ok;
}

bool parcourir(const char* chemin, const std::function<bool(const uint8_t*, size_t)>& fn) {
  if (!existe(chemin)) return false;
  File f = LittleFS.open(chemin, "r");
  if (!f) return false;
  uint8_t t[256];
  size_t n;
  while ((n = f.read(t, sizeof(t))) > 0) {
    if (!fn(t, n)) break;
  }
  f.close();
  return true;
}

void lister(const char* dossier, const std::function<void(const char*, size_t)>& fn) {
  if (!estMonte) return;
  File d = LittleFS.open(dossier);
  if (!d || !d.isDirectory()) return;
  for (File f = d.openNextFile(); f; f = d.openNextFile()) {
    if (!f.isDirectory()) {
      const char* nom = f.name();
      const char* barre = strrchr(nom, '/');
      if (barre) nom = barre + 1;
      if (!strstr(nom, ".tmp")) fn(nom, f.size());
    }
    f.close();
  }
  d.close();
}

bool commencer(const char* chemin) {
  if (!estMonte) return false;
  if (ecriture) ecriture.close();
  cheminFinal = chemin;
  ecriture = LittleFS.open(temporaire(chemin), "w", true);  // true : crée le dossier
  return bool(ecriture);
}

bool ajouter(const uint8_t* donnees, size_t n) {
  if (!ecriture) return false;
  return ecriture.write(donnees, n) == n;
}

bool terminer(bool garder) {
  if (!ecriture) return false;
  ecriture.close();
  const String tmp = temporaire(cheminFinal.c_str());
  if (!garder) {
    LittleFS.remove(tmp);
    return false;
  }
  LittleFS.remove(cheminFinal);
  return LittleFS.rename(tmp, cheminFinal);
}

}  // namespace fichiers

#endif
