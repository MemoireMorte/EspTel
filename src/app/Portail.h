#pragma once

// Portail web de configuration (plan §3, V2), ouvert tant que la page P52 est
// affichée sur le Minitel.
//
//   http://<adresse>/ ou http://esptel.local/
//   identifiant « esptel », code à 6 chiffres tiré à chaque ouverture
//
// Il donne l'annuaire (fiches, export et import JSON), les réglages de P50,
// les réseaux WiFi mémorisés, et la mise à jour du firmware : fichier .bin
// envoyé depuis le navigateur, ou téléversement par le réseau depuis l'IDE
// Arduino (ArduinoOTA, même code comme mot de passe).
//
// Tout se passe dans loop() : les pages du Minitel et le portail ne se
// marchent pas dessus.

#include <Arduino.h>

#include "../MinitelIO/MinitelIO.h"

namespace portail {

constexpr const char* kNomHote = "esptel";  // esptel.local, et nom du port réseau de l'IDE
constexpr const char* kIdentifiant = "esptel";
constexpr int kLignesJournal = 4;

bool ouvrir(const mio::MinitelIO& io);  // false sans WiFi
void fermer();
bool ouvert();
void tic();  // à appeler dans loop()

const char* code();
String adresse();  // "http://192.168.1.42"

// Ce que le portail a fait, pour P52 : les lignes les plus récentes en dernier.
// version() change à chaque nouvelle ligne et à chaque étape d'une mise à jour.
uint32_t version();
const char* journal(int i);  // 0 à kLignesJournal - 1, "" si vide

enum class Maj : uint8_t { Aucune, EnCours, Reussie, Echouee };
Maj maj();
int majPourcent();
const char* majSource();  // "le navigateur" ou "l'IDE Arduino"
const char* majErreur();
// Une mise à jour bloque la boucle jusqu'à sa fin : fn est appelée à chaque
// étape pour que P52 redessine sa barre de progression.
void surProgression(void (*fn)());

}  // namespace portail
