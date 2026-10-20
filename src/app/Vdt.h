#pragma once

// Pages Vidéotex en fichiers (plan §3, V2) : /vdt/<NOM>.vdt en LittleFS,
// déposées par le portail, dessinées avec un éditeur Vidéotex.
//
//   MIRE        remplace la mire d'accueil (P00)
//   autre nom   page locale : le nom tapé au Sommaire l'affiche (P11), comme
//               un code de service, quand aucun service de l'annuaire ne l'a
//
// Le fichier est envoyé tel quel au Minitel, à la suite d'un effacement d'écran.

#include <Arduino.h>

#include <functional>

#include "../MinitelIO/MinitelIO.h"

namespace vdt {

constexpr size_t kTailleNom = 9;  // 8 caractères A-Z 0-9, comme un code de service
constexpr size_t kTailleMax = 16 * 1024;
constexpr const char* kMire = "MIRE";

bool nomValide(const char* nom);
String chemin(const char* nom);  // "/vdt/NOM.vdt"
bool existe(const char* nom);
void lister(const std::function<void(const char* nom, size_t taille)>& fn);
bool afficher(mio::MinitelIO& io, const char* nom);
bool supprimer(const char* nom);
void toutOublier();  // P54

}  // namespace vdt
