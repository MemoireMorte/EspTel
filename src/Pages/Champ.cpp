#include "Champ.h"

#include "Charte.h"

namespace pages {

void Champ::configurer(int col, int rangee, uint8_t longueur, bool majuscules, uint8_t largeur) {
  _col = col;
  _rangee = rangee;
  _max = longueur > kLongueurMax ? kLongueurMax : longueur;
  _largeur = largeur && largeur < _max ? largeur : _max;
  _majuscules = majuscules;
  _masque = false;
  _clair = false;
  _n = 0;
  _texte[0] = '\0';
}

void Champ::aller(Minitel& m, int i) const { m.newXY(_col + i % _largeur, _rangee + i / _largeur); }

void Champ::dessiner(Minitel& m) const {
  _clair = false;
  // Rangée par rangée : le texte saisi, puis les points restants.
  for (int debut = 0; debut < _max; debut += _largeur) {
    const int fin = debut + _largeur < _max ? debut + _largeur : _max;
    aller(m, debut);
    if (_n > debut) {
      const int k = (_n < fin ? _n : fin) - debut;
      char morceau[kLongueurMax + 1];
      if (_masque) memset(morceau, '*', k);
      else memcpy(morceau, _texte + debut, k);
      morceau[k] = '\0';
      m.attributs(charte::kValeur);
      m.print(morceau);
    }
    const int points = fin - (_n > debut ? (_n < fin ? _n : fin) : debut);
    if (points > 0) {
      m.attributs(charte::kChamp);
      m.print(".");
      if (points > 1) m.repeat(points - 1);
    }
  }
}

void Champ::placerCurseur(Minitel& m) const {
  // Champ plein : le curseur reste sur le dernier caractère.
  aller(m, _n < _max ? _n : _max - 1);
  m.cursor();
}

void Champ::remplir(const char* texte) {
  strlcpy(_texte, texte, static_cast<size_t>(_max) + 1);
  _n = static_cast<uint8_t>(strlen(_texte));
}

void Champ::cacherDernier(Minitel& m) {
  if (!_clair || _n == 0) return;
  _clair = false;
  aller(m, _n - 1);
  m.attributs(charte::kValeur);
  m.print("*");
  placerCurseur(m);
}

void Champ::vider(Minitel* m) {
  _n = 0;
  _clair = false;
  _texte[0] = '\0';
  if (m) {
    dessiner(*m);
    placerCurseur(*m);
  }
}

Champ::Resultat Champ::touche(Minitel& m, const mio::Evenement& evt) {
  if (evt.type == mio::Type::Caractere) {
    uint32_t c = evt.unicode;
    // ASCII imprimable seulement pour l'instant : les codes, adresses et noms de
    // réseau n'ont pas d'accents. Les saisies accentuées viendront avec les fiches.
    if (c < 0x20 || c > 0x7E || c == 0x5E || c == 0x60 || _n >= _max) {
      m.bip();
      return Resultat::Refuse;
    }
    if (_majuscules && c >= 'a' && c <= 'z') c -= 'a' - 'A';
    _texte[_n++] = static_cast<char>(c);
    _texte[_n] = '\0';
    m.attributs(charte::kValeur);
    if (_masque && _clair && _n >= 2) {
      // Le précédent repasse en étoile ; il précède toujours le nouveau à l'écran,
      // sauf en début de rangée.
      aller(m, _n - 2);
      if ((_n - 1) % _largeur == 0) {
        m.print("*");
        aller(m, _n - 1);
      } else {
        m.print("*");
      }
    } else {
      aller(m, _n - 1);
    }
    const char s[2] = {static_cast<char>(c), '\0'};
    m.print(s);
    _clair = _masque;
    placerCurseur(m);
    return Resultat::Modifie;
  }

  if (evt.est(mio::Touche::Correction)) {
    if (_n == 0) {
      m.bip();
      return Resultat::Refuse;
    }
    _texte[--_n] = '\0';
    _clair = false;
    aller(m, _n);
    m.attributs(charte::kChamp);
    m.print(".");
    placerCurseur(m);
    return Resultat::Modifie;
  }

  if (evt.est(mio::Touche::Annulation)) {
    if (_n == 0) return Resultat::Abandon;
    vider(&m);
    return Resultat::Modifie;
  }

  return Resultat::Ignore;
}

}  // namespace pages
