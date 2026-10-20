#include "Charte.h"

namespace charte {

namespace {

// Logo : le combiné et le clavier de l'annuaire électronique (3611), réduit à
// 12 x 9 points, soit 6 x 3 cellules semi-graphiques, en mosaïque disjointe
// sur fond bleu comme à l'époque. Les bits vont du coin haut gauche au coin bas
// droit de la cellule de 2 x 3 points.
//
//   .....WWWWWWW   W combiné (blanc), C touches (cyan)
//   ...WWWWWWWWW
//   ..WWWW...WWW
//   .WWW........
//   .WWW..C.C.C.
//   WWW.........
//   WWW...C.C.C.
//   WWWW........
//   .WW...C.C.C.
struct Cellule {
  byte motif;
  byte encre;
};
constexpr Cellule kLogo[3][6] = {
    {{0b000000, CARACTERE_BLANC}, {0b000111, CARACTERE_BLANC}, {0b011111, CARACTERE_BLANC},
     {0b111100, CARACTERE_BLANC}, {0b111101, CARACTERE_BLANC}, {0b111111, CARACTERE_BLANC}},
    {{0b010111, CARACTERE_BLANC}, {0b111110, CARACTERE_BLANC}, {0b000000, CARACTERE_BLANC},
     {0b001000, CARACTERE_CYAN}, {0b001000, CARACTERE_CYAN}, {0b001000, CARACTERE_CYAN}},
    {{0b111101, CARACTERE_BLANC}, {0b101110, CARACTERE_BLANC}, {0b000000, CARACTERE_BLANC},
     {0b100010, CARACTERE_CYAN}, {0b100010, CARACTERE_CYAN}, {0b100010, CARACTERE_CYAN}},
};
constexpr int kColLogo = 1;

}  // namespace

void bandeau(Minitel& m, const char* titre, const char* numero) {
  // Titre d'abord : la double hauteur déborde sur la rangée du dessus, mais
  // seulement dans ses propres colonnes, à droite du logo.
  grand(m, 8, 3, titre);
  for (int r = 0; r < 3; ++r) {
    m.newXY(kColLogo, r + 1);
    m.graphicMode();
    // En semi-graphique, fond et lignage (mosaïque disjointe) valent dès la
    // cellule suivante ; l'encre ne change qu'entre combiné et touches.
    m.attributs(FOND_BLEU);
    m.attributs(DEBUT_LIGNAGE);
    byte encre = 0;
    for (int c = 0; c < 6; ++c) {
      if (kLogo[r][c].encre != encre) {
        encre = kLogo[r][c].encre;
        m.attributs(encre);
      }
      m.graphic(kLogo[r][c].motif);
    }
    m.attributs(FIN_LIGNAGE);
    m.attributs(FOND_NOIR);
    m.textMode();
  }
  if (numero) {
    const int col = 41 - static_cast<int>(strlen(numero));
    texte(m, col, 3, numero, kLibelle);
  }
  filet(m, 4);
}

void filet(Minitel& m, int rangee) {
  m.newXY(1, rangee);
  m.attributs(kFilet);
  m.graphicMode();
  m.graphic(0b110000);  // trait fin en haut de la cellule
  m.repeat(39);
  m.textMode();
}

void texte(Minitel& m, int col, int rangee, const char* utf8, byte couleur) {
  m.newXY(col, rangee);
  m.attributs(couleur);
  m.print(utf8);
}

void grand(Minitel& m, int col, int rangeeBas, const char* utf8, byte couleur) {
  // Minitel1B_Hard descend d'une rangée en posant DOUBLE_HAUTEUR (vu sur le
  // Minitel) : on se place donc sur la rangée du haut.
  m.newXY(col, rangeeBas - 1);
  m.attributs(DOUBLE_HAUTEUR);
  m.attributs(couleur);
  m.print(utf8);
  m.attributs(GRANDEUR_NORMALE);
}

void choix(Minitel& m, int rangee, char numero, const char* libelle) {
  m.newXY(3, rangee);
  m.attributs(INVERSION_FOND);
  char n[4] = {' ', numero, ' ', '\0'};
  m.print(n);
  m.attributs(FOND_NORMAL);
  m.attributs(kValeur);
  m.print(" ");
  m.print(libelle);
}

void touche(Minitel& m, const char* nom) {
  m.attributs(INVERSION_FOND);
  m.print(" ");
  m.print(nom);
  m.print(" ");
  m.attributs(FOND_NORMAL);
}

void ligneInfo(Minitel& m, int rangee, const char* libelle, const char* valeur) {
  texte(m, 2, rangee, libelle, kLibelle);
  texte(m, 14, rangee, valeur, kValeur);
  m.cancel();
}

void rappel(Minitel& m, bool suite) {
  m.newXY(2, kRangeeRappel);
  if (suite) {
    touche(m, "SUITE");
    m.print(" ");
  }
  touche(m, "RETOUR");
  m.print(" ");
  touche(m, "SOMMAIRE");
  m.attributs(kLibelle);
  m.print(suite ? " " : "  Aide ");
  touche(m, "GUIDE");
}

void barres(Minitel& m, int n, int max) {
  if (n > 0) {
    m.attributs(CARACTERE_VERT);
    m.graphicMode();
    m.graphic(0b111111);
    if (n > 1) m.repeat(n - 1);
    m.textMode();
  }
  if (max > n) {
    m.print(" ");
    if (max - n > 1) m.repeat(max - n - 1);
  }
}

void effacer(Minitel& m, int col, int rangee) {
  m.newXY(col, rangee);
  m.cancel();
}

}  // namespace charte
