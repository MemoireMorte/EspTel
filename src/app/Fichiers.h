#pragma once

// Fichiers en LittleFS, sur la partition « spiffs » du schéma Minimal SPIFFS
// (190 Ko) : clés SSH (/cles/<nom>) et pages Vidéotex (/vdt/<NOM>.vdt).
// La partition est formatée au premier démarrage.
//
// Le simulateur et l'aperçu du portail les gardent en mémoire.

#include <Arduino.h>

#include <functional>

namespace fichiers {

bool monter();  // au démarrage ; false si la partition est absente
bool monte();
size_t total();
size_t utilise();

bool existe(const char* chemin);
size_t taille(const char* chemin);  // 0 si absent
bool supprimer(const char* chemin);
bool renommer(const char* de, const char* vers);  // remplace vers s'il existe
// Un fichier texte entier (clé SSH), max octets au plus.
bool lire(const char* chemin, String& texte, size_t max);
bool ecrire(const char* chemin, const uint8_t* donnees, size_t n);
// Lecture par morceaux : fn est appelée jusqu'à la fin, ou jusqu'à ce qu'elle renvoie false.
bool parcourir(const char* chemin, const std::function<bool(const uint8_t*, size_t)>& fn);
// Fichiers d'un dossier ("/vdt"), par nom (sans le dossier).
void lister(const char* dossier, const std::function<void(const char* nom, size_t taille)>& fn);

// Écriture en flux (envoi depuis le portail) : un seul fichier à la fois.
// Le fichier n'apparaît sous son nom qu'à terminer(true).
bool commencer(const char* chemin);
bool ajouter(const uint8_t* donnees, size_t n);
bool terminer(bool garder);

}  // namespace fichiers
