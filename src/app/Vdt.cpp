#include "../config.h"

#include "Vdt.h"

#include "Fichiers.h"

namespace vdt {

namespace {
constexpr const char* kDossier = "/vdt";
}  // namespace

bool nomValide(const char* nom) {
  const size_t n = strlen(nom);
  if (n == 0 || n >= kTailleNom) return false;
  for (const char* c = nom; *c; ++c) {
    if (!((*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9'))) return false;
  }
  // Un chiffre seul est une rubrique du Sommaire.
  return !(n == 1 && nom[0] >= '1' && nom[0] <= '9');
}

String chemin(const char* nom) { return String(kDossier) + "/" + nom + ".vdt"; }

bool existe(const char* nom) { return nomValide(nom) && fichiers::existe(chemin(nom).c_str()); }

void lister(const std::function<void(const char*, size_t)>& fn) {
  fichiers::lister(kDossier, [&](const char* fichier, size_t taille) {
    char nom[kTailleNom + 4];
    strlcpy(nom, fichier, sizeof(nom));
    char* point = strrchr(nom, '.');
    if (!point || strcmp(point, ".vdt") != 0) return;
    *point = '\0';
    if (nomValide(nom)) fn(nom, taille);
  });
}

bool afficher(mio::MinitelIO& io, const char* nom) {
  if (!existe(nom)) return false;
  return fichiers::parcourir(chemin(nom).c_str(), [&](const uint8_t* d, size_t n) {
    io.ecrire(d, n);
    return true;
  });
}

bool supprimer(const char* nom) { return nomValide(nom) && fichiers::supprimer(chemin(nom).c_str()); }

void toutOublier() {
  String noms;
  lister([&](const char* nom, size_t) {
    noms += nom;
    noms += ' ';
  });
  char t[kTailleNom];
  size_t k = 0;
  for (unsigned i = 0; i < noms.length(); ++i) {
    const char c = noms.charAt(i);
    if (c != ' ') {
      if (k + 1 < sizeof(t)) t[k++] = c;
      continue;
    }
    t[k] = '\0';
    supprimer(t);
    k = 0;
  }
}

}  // namespace vdt
