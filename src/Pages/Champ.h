#pragma once

// Champ de saisie des pages locales, matérialisé par des points (plan, §4 et §5).
//
//   caractère    s'ajoute au champ (bip si le champ est plein ou le caractère refusé)
//   CORRECTION   efface le dernier caractère
//   ANNULATION   vide le champ ; sur un champ déjà vide, signale l'abandon
//
// ENVOI, SUITE, RETOUR et les autres touches restent à la page.
//
// Un champ plus long qu'une rangée (mot de passe WiFi, 64 caractères) passe à
// la ligne tous les `largeur` caractères, à partir de la même colonne.

#include <Arduino.h>
#include <Minitel1B_Hard.h>

#include "../MinitelIO/MinitelIO.h"

namespace pages {

class Champ {
 public:
  static constexpr uint8_t kLongueurMax = 64;

  enum class Resultat : uint8_t {
    Ignore,    // touche qui ne concerne pas le champ
    Modifie,   // contenu changé
    Refuse,    // champ plein ou caractère interdit (le bip est déjà parti)
    Abandon,   // ANNULATION sur champ vide
  };

  // largeur : caractères par rangée, 0 pour tout sur une rangée.
  void configurer(int col, int rangee, uint8_t longueur, bool majuscules = true, uint8_t largeur = 0);

  void dessiner(Minitel& m) const;        // les points et le contenu
  void placerCurseur(Minitel& m) const;   // curseur visible à la position de saisie
  Resultat touche(Minitel& m, const mio::Evenement& evt);

  void vider(Minitel* m = nullptr);       // avec m : redessine le champ vide
  void remplir(const char* texte);        // contenu initial (avant dessiner)
  // Une étoile par caractère (mot de passe SSH). Le dernier tapé reste en clair
  // jusqu'à la touche suivante ou cacherDernier() : on voit sa casse.
  void masquer(bool oui) { _masque = oui; }
  bool dernierEnClair() const { return _clair; }
  void cacherDernier(Minitel& m);
  const char* texte() const { return _texte; }
  uint8_t taille() const { return _n; }
  bool vide() const { return _n == 0; }

 private:
  int _col = 1;
  int _rangee = 22;
  void aller(Minitel& m, int i) const;   // positionne sur le caractère i

  uint8_t _max = 8;
  uint8_t _largeur = 8;
  bool _majuscules = true;
  bool _masque = false;
  mutable bool _clair = false;  // champ masqué : dernier caractère affiché en clair
  char _texte[kLongueurMax + 1] = {0};
  uint8_t _n = 0;
};

}  // namespace pages
