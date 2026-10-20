#include "../config.h"

#include "Cles.h"

#include "Fichiers.h"

#if !defined(SIMULATEUR) && !defined(FICHIERS_EN_MEMOIRE)
#include <libssh_esp32.h>
#include <libssh/libssh.h>
#define CLES_LIBSSH 1
#endif

namespace cles {

namespace {

constexpr const char* kDossier = "/cles";

String chemin(const char* nom) { return String(kDossier) + "/" + nom; }

}  // namespace

bool nomValide(const char* nom) {
  const size_t n = strlen(nom);
  if (n == 0 || n >= kTailleNom) return false;
  for (const char* c = nom; *c; ++c) {
    if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '-' || *c == '_')) return false;
  }
  return true;
}

bool existe(const char* nom) { return nomValide(nom) && fichiers::existe(chemin(nom).c_str()); }

int nombre() {
  int n = 0;
  lister([&](const char*) { ++n; });
  return n;
}

void lister(const std::function<void(const char*)>& fn) {
  fichiers::lister(kDossier, [&](const char* nom, size_t) {
    if (nomValide(nom)) fn(nom);
  });
}

bool supprimer(const char* nom) { return nomValide(nom) && fichiers::supprimer(chemin(nom).c_str()); }

bool lire(const char* nom, String& texte) {
  return nomValide(nom) && fichiers::lire(chemin(nom).c_str(), texte, kTailleMax);
}

void toutOublier() {
  char noms[kMax * 2][kTailleNom];
  int n = 0;
  lister([&](const char* nom) {
    if (n < kMax * 2) strlcpy(noms[n++], nom, kTailleNom);
  });
  for (int i = 0; i < n; ++i) supprimer(noms[i]);
}

namespace {

const char* verifierPlace(const char* nom) {
  if (!nomValide(nom)) return "Nom : 1 à 15 caractères a-z, 0-9, - ou _";
  if (!existe(nom) && nombre() >= kMax) return "4 clés au plus : supprimez-en une d'abord";
  if (!fichiers::monte()) return "Stockage LittleFS absent";
  return nullptr;
}

}  // namespace

#ifdef CLES_LIBSSH

namespace {

// Enregistre la clé, déchiffrée, au format OpenSSH.
const char* enregistrer(const char* nom, ssh_key cle) {
  char* b64 = nullptr;
  if (ssh_pki_export_privkey_base64_format(cle, nullptr, nullptr, nullptr, &b64, SSH_FILE_FORMAT_OPENSSH) !=
          SSH_OK ||
      b64 == nullptr) {
    return "Clé impossible à réécrire";
  }
  const bool ok = fichiers::ecrire(chemin(nom).c_str(), reinterpret_cast<const uint8_t*>(b64), strlen(b64));
  memset(b64, 0, strlen(b64));
  ssh_string_free_char(b64);
  return ok ? nullptr : "Écriture impossible (place ?)";
}

}  // namespace

const char* importer(const char* nom, const char* texte, const char* phrase) {
  if (const char* e = verifierPlace(nom)) return e;
  if (strlen(texte) > kTailleMax) return "Clé trop longue (4 Ko au plus)";
  if (strstr(texte, "PRIVATE KEY") == nullptr) return "Ce n'est pas une clé privée (BEGIN ... PRIVATE KEY)";
  ssh_key cle = nullptr;
  const int rc = ssh_pki_import_privkey_base64(texte, phrase && phrase[0] ? phrase : nullptr, nullptr, nullptr, &cle);
  if (rc != SSH_OK || cle == nullptr) {
    return phrase && phrase[0] ? "Clé illisible ou phrase de passe fausse"
                               : "Clé illisible, ou protégée par une phrase de passe";
  }
  const char* e = enregistrer(nom, cle);
  ssh_key_free(cle);
  return e;
}

const char* generer(const char* nom) {
  if (const char* e = verifierPlace(nom)) return e;
  if (existe(nom)) return "Ce nom est déjà pris";
  ssh_key cle = nullptr;
  if (ssh_pki_generate(SSH_KEYTYPE_ED25519, 0, &cle) != SSH_OK || cle == nullptr) return "Génération impossible";
  const char* e = enregistrer(nom, cle);
  ssh_key_free(cle);
  return e;
}

bool decrire(const char* nom, Description& d) {
  d = Description();
  String texte;
  if (!lire(nom, texte)) return false;
  ssh_key cle = nullptr;
  if (ssh_pki_import_privkey_base64(texte.c_str(), nullptr, nullptr, nullptr, &cle) != SSH_OK) return false;
  snprintf(d.type, sizeof(d.type), "%s", ssh_key_type_to_char(ssh_key_type(cle)));
  ssh_key pub = nullptr;
  if (ssh_pki_export_privkey_to_pubkey(cle, &pub) == SSH_OK) {
    char* b64 = nullptr;
    if (ssh_pki_export_pubkey_base64(pub, &b64) == SSH_OK && b64) {
      d.publique = String(d.type) + " " + b64 + " esptel-" + nom;
      ssh_string_free_char(b64);
    }
    unsigned char* hash = nullptr;
    size_t longueur = 0;
    if (ssh_get_publickey_hash(pub, SSH_PUBLICKEY_HASH_SHA256, &hash, &longueur) == 0) {
      char* t = ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256, hash, longueur);
      if (t) {
        snprintf(d.empreinte, sizeof(d.empreinte), "%s", t);
        ssh_string_free_char(t);
      }
      ssh_clean_pubkey_hash(&hash);
    }
    ssh_key_free(pub);
  }
  ssh_key_free(cle);
  return true;
}

#else  // PC : pas de libssh, la clé est gardée telle quelle.

const char* importer(const char* nom, const char* texte, const char*) {
  if (const char* e = verifierPlace(nom)) return e;
  if (strlen(texte) > kTailleMax) return "Clé trop longue (4 Ko au plus)";
  if (strstr(texte, "PRIVATE KEY") == nullptr) return "Ce n'est pas une clé privée (BEGIN ... PRIVATE KEY)";
  fichiers::ecrire(chemin(nom).c_str(), reinterpret_cast<const uint8_t*>(texte), strlen(texte));
  return nullptr;
}

const char* generer(const char* nom) {
  if (const char* e = verifierPlace(nom)) return e;
  if (existe(nom)) return "Ce nom est déjà pris";
  const char* t = "-----BEGIN OPENSSH PRIVATE KEY-----\nsimulee\n-----END OPENSSH PRIVATE KEY-----\n";
  fichiers::ecrire(chemin(nom).c_str(), reinterpret_cast<const uint8_t*>(t), strlen(t));
  return nullptr;
}

bool decrire(const char* nom, Description& d) {
  d = Description();
  if (!existe(nom)) return false;
  strlcpy(d.type, "ssh-ed25519", sizeof(d.type));
  strlcpy(d.empreinte, "SHA256:simuleesimuleesimuleesimuleesimulee0", sizeof(d.empreinte));
  d.publique = String("ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIsimulee esptel-") + nom;
  return true;
}

#endif

}  // namespace cles
