#include "Moteur.h"

#include "Charte.h"

namespace pages {

void Page::replacerCurseur(Moteur& m) { m.ecran().noCursor(); }

void Moteur::demarrer(Page& premiere, Page& sommaire, EtatRepos etat) {
  _sommaire = &sommaire;
  _etat = etat;
  _n = 0;
  aller(premiere);
}

void Moteur::boucle() {
  mio::Evenement evt;
  if (_io.lire(evt) && evt.type != mio::Type::Protocole) {
    if (_guide) {
      toucheGuide(evt);
    } else if (Page* p = courante()) {
      if (!p->touche(*this, evt)) toucheCommune(evt);
    }
  }
  if (!_guide) {
    if (Page* p = courante()) p->tic(*this);
  }
  rangeeZero(false);
}

void Moteur::aller(Page& p) {
  if (_n == kProfondeurMax) {
    // Pile pleine : on oublie la plus ancienne page au-dessus du Sommaire.
    memmove(&_pile[1], &_pile[2], sizeof(_pile[0]) * (kProfondeurMax - 2));
    --_n;
  }
  _pile[_n++] = &p;
  p.entrer(*this);
  afficher();
}

void Moteur::remplacer(Page& p) {
  if (_n == 0) {
    aller(p);
    return;
  }
  _pile[_n - 1] = &p;
  p.entrer(*this);
  afficher();
}

void Moteur::retour() {
  if (_n <= 1) {
    ecran().bip();
    return;
  }
  --_n;
  afficher();
}

void Moteur::retourA(Page& p) {
  for (int i = _n - 1; i >= 0; --i) {
    if (_pile[i] == &p) {
      _n = i + 1;
      afficher();
      return;
    }
  }
  aller(p);
}

void Moteur::chemin(std::initializer_list<Page*> pages) {
  _n = 0;
  _pile[_n++] = _sommaire;
  _sommaire->entrer(*this);
  for (Page* p : pages) {
    if (_n == kProfondeurMax) break;
    _pile[_n++] = p;
    p->entrer(*this);
  }
  afficher();
}

void Moteur::sommaire() {
  for (int i = 0; i < _n; ++i) {
    if (_pile[i] == _sommaire) {
      _n = i + 1;
      _sommaire->entrer(*this);
      afficher();
      return;
    }
  }
  _n = 0;
  aller(*_sommaire);
}

void Moteur::reafficher() {
  if (_guide) afficherGuide();
  else afficher();
}

void Moteur::message(const char* texteUtf8, bool bip) {
  strlcpy(_message, texteUtf8, sizeof(_message));
  _finMessage = millis() + kDureeMessageMs;
  if (bip) ecran().bip();
  rangeeZero(true);
}

void Moteur::afficher() {
  Page* p = courante();
  if (!p) return;
  _guide = false;
  Minitel& m = ecran();
  m.noCursor();
  if (p->rangeeZeroMasquee()) {
    // Effacée avant le dessin : la page peut elle-même écrire en rangée 0.
    _io.ligneService("");
    _ligneAffichee[0] = '\0';
  }
  m.newScreen();
  p->dessiner(*this);
  rangeeZero(true);
}

void Moteur::afficherGuide() {
  Page* p = courante();
  Minitel& m = ecran();
  m.noCursor();
  m.newScreen();
  charte::bandeau(m, "GUIDE", p->code());
  const char* aide = p->aide();
  int rangee = charte::kContenuHaut + 1;
  char ligne[48];
  while (aide && *aide && rangee <= charte::kContenuBas) {
    const char* fin = strchr(aide, '\n');
    const size_t n = fin ? static_cast<size_t>(fin - aide) : strlen(aide);
    const size_t k = n < sizeof(ligne) - 1 ? n : sizeof(ligne) - 1;
    memcpy(ligne, aide, k);
    ligne[k] = '\0';
    if (k) charte::texte(m, 2, rangee, ligne);
    ++rangee;
    aide = fin ? fin + 1 : nullptr;
  }
  charte::filet(m, 21);
  m.newXY(2, charte::kRangeeRappel);
  charte::touche(m, "RETOUR");
  m.attributs(charte::kLibelle);
  m.print(" ou ");
  charte::touche(m, "GUIDE");
  m.attributs(charte::kLibelle);
  m.print(" : revenir");
  rangeeZero(true);
}

bool Moteur::toucheGuide(const mio::Evenement& evt) {
  switch (evt.touche) {
    case mio::Touche::Retour:
    case mio::Touche::Guide:
      afficher();
      return true;
    case mio::Touche::Sommaire:
      sommaire();
      return true;
    case mio::Touche::Repetition:
      afficherGuide();
      return true;
    default:
      ecran().bip();
      return false;
  }
}

void Moteur::toucheCommune(const mio::Evenement& evt) {
  Page* p = courante();
  switch (evt.touche) {
    case mio::Touche::Sommaire:
      if (p->sommaireAutorise()) sommaire();
      else ecran().bip();
      break;
    case mio::Touche::Retour:
      retour();
      break;
    case mio::Touche::Guide:
      if (p->aide()) {
        _guide = true;
        afficherGuide();
      } else {
        message("Pas d'aide pour cette page");
      }
      break;
    case mio::Touche::Repetition:
      afficher();
      break;
    default:
      ecran().bip();
      break;
  }
}

void Moteur::rangeeZero(bool forcer) {
  const uint32_t maintenant = millis();
  const char* texte = nullptr;
  char repos[48];

  if (_message[0] && static_cast<int32_t>(maintenant - _finMessage) < 0) {
    texte = _message;
  } else {
    if (_message[0]) {
      _message[0] = '\0';
      forcer = true;
    }
    if (!forcer && static_cast<int32_t>(maintenant - _prochainEtat) < 0) return;
    _prochainEtat = maintenant + 1000;
    repos[0] = '\0';
    Page* p = courante();
    if (p && !_guide && p->rangeeZeroMasquee()) {
      if (_ligneAffichee[0]) {  // un message vient de finir
        _ligneAffichee[0] = '\0';
        _io.ligneService("");
        p->replacerCurseur(*this);
      }
      return;
    }
    if (!(p && !_guide && p->ligneService(repos, sizeof(repos))) && _etat) _etat(repos, sizeof(repos));
    texte = repos;
  }

  if (!forcer && strcmp(texte, _ligneAffichee) == 0) return;
  strlcpy(_ligneAffichee, texte, sizeof(_ligneAffichee));
  _io.ligneService(texte);
  if (_guide) ecran().noCursor();
  else if (Page* p = courante()) p->replacerCurseur(*this);
}

}  // namespace pages
