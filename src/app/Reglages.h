#pragma once

// Réglages de l'appareil (P50), mémorisés en NVS (espace "reglages").

#include <Arduino.h>

namespace reglages {

struct Valeurs {
  bool bruit = true;      // double bip au début d'un appel (P70)
  bool cout = true;       // coût indicatif sur la page de fin (P90)
  uint8_t mireS = 8;      // passage automatique de la mire au Sommaire
  uint8_t finS = 15;      // retour automatique après la fin de connexion, 0 : jamais
  uint16_t vitesse = 0;   // vitesse visée au démarrage, 0 : MINITEL_VITESSE_MAX
};

void charger();  // au démarrage, avant le Minitel
const Valeurs& lire();
void enregistrer(const Valeurs& v);
int vitesseVisee();  // vitesse demandée au Minitel au démarrage

// Réinitialisation (P54) : efface tous les espaces NVS du firmware.
void effacerTout();

}  // namespace reglages
