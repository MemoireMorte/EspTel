#pragma once

// Charte Télétel : les éléments communs à toutes les pages locales (plan, §4).
//
//   rangée 0      ligne de service (écrite par le moteur)
//   rangées 1-3   bandeau : logo en mosaïque, titre en double hauteur, numéro de page
//   rangée 4      filet
//   rangées 5-20  contenu
//   rangée 21     filet
//   rangée 22     invite et champ de saisie
//   rangées 23-24 rappel des touches utiles
//
// Rappel Vidéotex : chaque positionnement (US) remet les attributs à zéro, d'où
// une couleur passée à chaque appel.

#include <Arduino.h>
#include <Minitel1B_Hard.h>

namespace charte {

constexpr int kContenuHaut = 5;
constexpr int kContenuBas = 20;
constexpr int kRangeeInvite = 22;
constexpr int kRangeeRappel = 24;

// Couleurs choisies pour leur gris sur un Minitel monochrome : blanc, jaune et
// cyan ressortent clair, bleu et rouge foncé.
constexpr byte kTitre = CARACTERE_JAUNE;
constexpr byte kLibelle = CARACTERE_CYAN;
constexpr byte kValeur = CARACTERE_BLANC;
constexpr byte kFilet = CARACTERE_BLEU;
constexpr byte kChamp = CARACTERE_CYAN;

// Bandeau des rangées 1 à 3. numero : "1/1", ou nullptr.
void bandeau(Minitel& m, const char* titre, const char* numero = "1/1");

// Ligne de semi-graphiques sur toute la largeur.
void filet(Minitel& m, int rangee);

// Texte simple, positionné.
void texte(Minitel& m, int col, int rangee, const char* utf8, byte couleur = kValeur);

// Texte en double hauteur dont le bas tombe sur rangeeBas (il occupe aussi la
// rangée du dessus).
void grand(Minitel& m, int col, int rangeeBas, const char* utf8, byte couleur = kTitre);

// Choix de menu : numéro en inversion vidéo, puis libellé. "[1] Annuaire".
void choix(Minitel& m, int rangee, char numero, const char* libelle);

// Nom de touche en inversion vidéo, à la position courante.
void touche(Minitel& m, const char* nom);

// Ligne "libellé    valeur" des fiches et des pages d'information.
void ligneInfo(Minitel& m, int rangee, const char* libelle, const char* valeur);

// Rangée 24 : rappel des touches. Avec suite : SUITE RETOUR SOMMAIRE GUIDE.
void rappel(Minitel& m, bool suite = false);

// Barres de signal (0 à max) en mosaïque, à la position courante, complétées
// par des espaces jusqu'à max.
void barres(Minitel& m, int n, int max = 5);

// Efface la fin d'une rangée à partir de col.
void effacer(Minitel& m, int col, int rangee);

}  // namespace charte
