#include "../config.h"

#include "Hotes.h"

#include <Preferences.h>

namespace hotes {

namespace {

constexpr const char* kEspace = "ssh_hotes";

// Une clé NVS fait 15 caractères au plus : "h" et l'empreinte FNV-1a de
// « hote:port ». La valeur recopie « hote:port » pour écarter une collision.
void cle(const char* nom, char* dest, size_t taille) {
  uint32_t h = 2166136261u;
  for (const char* p = nom; *p; ++p) h = (h ^ static_cast<uint8_t>(tolower(*p))) * 16777619u;
  snprintf(dest, taille, "h%08lx", static_cast<unsigned long>(h));
}

void nommer(const char* hote, uint16_t port, char* dest, size_t taille) {
  snprintf(dest, taille, "%s:%u", hote, port);
  for (char* p = dest; *p; ++p) *p = char(tolower(static_cast<unsigned char>(*p)));
}

}  // namespace

Verdict verifier(const char* hote, uint16_t port, const char* empreinte) {
  char nom[72], k[16], valeur[136];
  nommer(hote, port, nom, sizeof(nom));
  cle(nom, k, sizeof(k));
  Preferences p;
  if (!p.begin(kEspace, true)) return Verdict::Inconnu;  // espace pas encore créé
  const size_t n = p.isKey(k) ? p.getString(k, valeur, sizeof(valeur)) : 0;
  p.end();
  if (n == 0) return Verdict::Inconnu;
  const char* tab = strchr(valeur, '\t');
  if (!tab || size_t(tab - valeur) != strlen(nom) || strncmp(valeur, nom, size_t(tab - valeur)) != 0) {
    return Verdict::Inconnu;
  }
  return strcmp(tab + 1, empreinte) == 0 ? Verdict::Connu : Verdict::Change;
}

void memoriser(const char* hote, uint16_t port, const char* empreinte) {
  char nom[72], k[16], valeur[136];
  nommer(hote, port, nom, sizeof(nom));
  cle(nom, k, sizeof(k));
  snprintf(valeur, sizeof(valeur), "%s\t%s", nom, empreinte);
  Preferences p;
  p.begin(kEspace, false);
  p.putString(k, valeur);
  p.end();
}

void oublier(const char* hote, uint16_t port) {
  char nom[72], k[16];
  nommer(hote, port, nom, sizeof(nom));
  cle(nom, k, sizeof(k));
  Preferences p;
  p.begin(kEspace, false);
  p.remove(k);
  p.end();
}

void toutOublier() {
  Preferences p;
  p.begin(kEspace, false);
  p.clear();
  p.end();
}

}  // namespace hotes
