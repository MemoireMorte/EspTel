#pragma once

// Clés privées SSH de l'usager (plan §3, V2), en LittleFS sous /cles/<nom>.
//
// Déposées par le portail (texte OpenSSH ou PEM, avec ou sans phrase de passe)
// ou générées sur l'ESP32 (Ed25519). Elles sont gardées déchiffrées, au format
// OpenSSH : l'appel ne peut pas demander de phrase de passe. La partition
// n'est pas chiffrée : qui a la carte en main peut lire les clés.
//
// Une fiche SSH nomme sa clé (champ « cle ») ; l'appel l'essaie avant le mot de
// passe.

#include <Arduino.h>

#include <functional>

namespace cles {

constexpr int kMax = 4;
constexpr size_t kTailleNom = 16;    // 15 caractères : a-z 0-9 - _
constexpr size_t kTailleMax = 4096;  // une clé RSA 4096 fait ~3,4 Ko

bool nomValide(const char* nom);
bool existe(const char* nom);
int nombre();
void lister(const std::function<void(const char* nom)>& fn);

// nullptr si la clé est enregistrée, sinon la raison.
const char* importer(const char* nom, const char* texte, const char* phrase);
const char* generer(const char* nom);  // Ed25519

struct Description {
  char type[24] = "";       // "ssh-ed25519"
  char empreinte[64] = "";  // "SHA256:..."
  String publique;          // la ligne à mettre dans authorized_keys
};
bool decrire(const char* nom, Description& d);

bool supprimer(const char* nom);
bool lire(const char* nom, String& texte);  // pour l'appel
void toutOublier();                         // P54

}  // namespace cles
