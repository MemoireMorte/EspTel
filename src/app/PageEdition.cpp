#include "../config.h"

#include "../Pages/Charte.h"
#include "Cles.h"
#include "Pages.h"

namespace {

enum Champ : int {
  kNom, kCode, kProto, kAdresse, kPort, kChemin,                    // écran 1
  kAffichage, kEcho, kRouleau, kFin, kSousProto, kPing, kReco, kUtil, kMdp, kCle,  // écran 2
};

struct Def {
  const char* libelle;
  uint8_t rangee;
  uint8_t col;
  uint8_t longueur;
  uint8_t largeur;  // 0 : une rangée
  bool majuscules;
  const char* indication;  // à droite du champ, ou nullptr
};

// Rangées et colonnes des deux écrans (maquette P22 du plan).
const Def kDefs[PageEdition::kNbChamps] = {
    {"Nom", 6, 12, 24, 0, false, nullptr},
    {"Code", 7, 12, 8, 0, true, nullptr},
    {"Protocole", 9, 12, 1, 0, true, "1 Telnet 2 WS 3 WSS 4 SSH"},
    {"Adresse", 11, 12, 56, 28, false, nullptr},
    {"Port", 13, 12, 5, 0, false, "vide : par défaut"},
    {"Chemin", 15, 12, 56, 28, false, nullptr},
    {"Affichage", 6, 18, 1, 0, true, nullptr},
    {"Écho local", 9, 18, 1, 0, true, "O/N"},
    {"Mode rouleau", 10, 18, 1, 0, true, "O/N"},
    {"FIN au serveur", 11, 18, 1, 0, true, "O/N, Ctrl+R sort"},
    {"Sous-protocole", 13, 18, 15, 0, false, nullptr},
    {"Ping (s)", 14, 18, 3, 0, false, "0 : aucun"},
    {"Reconnexion", 15, 18, 1, 0, true, "O/N, après coupure"},
    {"Utilisateur", 16, 18, 22, 0, false, nullptr},
    {"Mot de passe", 17, 18, 44, 22, false, nullptr},
    {"Clé SSH", 18, 18, 15, 0, false, "portail"},
};

void entier(char* t, size_t n, unsigned v) {
  if (v) snprintf(t, n, "%u", v);
  else t[0] = '\0';
}

bool lireEntier(const char* t, long max, long& v) {
  if (!t[0]) {
    v = 0;
    return true;
  }
  char* fin;
  v = strtol(t, &fin, 10);
  return *fin == '\0' && v >= 0 && v <= max;
}

}  // namespace

void PageEdition::nouvelle(const annuaire::Fiche* modele) {
  _modele = modele ? *modele : annuaire::Fiche();
  if (!modele) _modele.chemin[0] = '\0';
  _indice = -1;
}

void PageEdition::modifier(int indice) {
  _modele = annuaire::fiche(indice);
  _indice = indice;
}

int PageEdition::ecranDe(int i) const { return i >= kAffichage ? 1 : 0; }

bool PageEdition::visible(int i) const {
  const char p = _champs[kProto].texte()[0];
  const bool ws = p == '2' || p == '3';
  if (i == kChemin || i == kSousProto || i == kPing || i == kReco) return ws;
  if (i == kUtil || i == kMdp || i == kCle) return p == '4';
  return true;
}

void PageEdition::entrer(pages::Moteur&) {
  for (int i = 0; i < kNbChamps; ++i) {
    const Def& d = kDefs[i];
    _champs[i].configurer(d.col, d.rangee, d.longueur, d.majuscules, d.largeur);
  }
  remplir(_modele);
  _actif = kNom;
  _ecran = 0;
  _protoAffiche = _champs[kProto].texte()[0];
}

void PageEdition::remplir(const annuaire::Fiche& f) {
  char t[8];
  _champs[kNom].remplir(f.nom);
  _champs[kCode].remplir(f.code);
  t[0] = char('1' + static_cast<int>(f.proto));
  t[1] = '\0';
  _champs[kProto].remplir(t);
  _champs[kAdresse].remplir(f.hote);
  entier(t, sizeof(t), f.port);
  _champs[kPort].remplir(t);
  _champs[kChemin].remplir(f.chemin);
  t[0] = char('1' + static_cast<int>(f.affichage));
  _champs[kAffichage].remplir(t);
  _champs[kEcho].remplir(f.echo ? "O" : "N");
  _champs[kRouleau].remplir(f.defilement ? "O" : "N");
  _champs[kFin].remplir(f.finTransmise ? "O" : "N");
  _champs[kSousProto].remplir(f.sousProto);
  entier(t, sizeof(t), f.pingS);
  _champs[kPing].remplir(t);
  _champs[kUtil].remplir(f.utilisateur);
  _champs[kMdp].remplir(f.motDePasse);
  _champs[kReco].remplir(f.reconnexion ? "O" : "N");
  _champs[kCle].remplir(f.cle);
}

void PageEdition::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::bandeau(m, _indice < 0 ? "NOUVEAU SERVICE" : "FICHE SERVICE", _ecran ? "2/2" : "1/2");

  for (int i = 0; i < kNbChamps; ++i) {
    if (ecranDe(i) != _ecran || !visible(i)) continue;
    const Def& d = kDefs[i];
    charte::texte(m, 2, d.rangee, d.libelle, charte::kLibelle);
    _champs[i].dessiner(m);
    if (d.indication) charte::texte(m, d.col + d.longueur + 1, d.rangee, d.indication, charte::kLibelle);
  }
  if (_ecran == 1) {
    charte::texte(m, 4, 7, "1 Vidéotex  2 Texte 40  3 Texte 80", charte::kLibelle);
    if (visible(kMdp)) charte::texte(m, 2, 19, "Mot de passe vide : demandé à l'appel.", charte::kLibelle);
  } else {
    charte::texte(m, 2, 18, "SUITE depuis le dernier champ :", charte::kLibelle);
    charte::texte(m, 2, 19, "options (affichage, écho...).", charte::kLibelle);
  }

  charte::filet(m, 21);
  m.newXY(2, charte::kRangeeInvite);
  charte::touche(m, "SUITE");
  m.attributs(charte::kValeur);
  m.print(" suivant  ");
  charte::touche(m, "RETOUR");
  m.attributs(charte::kValeur);
  m.print(" précédent");
  m.newXY(2, 23);
  charte::touche(m, "ENVOI");
  m.attributs(charte::kValeur);
  m.print(" enregistrer");
  m.newXY(2, 24);
  charte::touche(m, "ANNULATION");
  m.attributs(charte::kValeur);
  m.print(" sur champ vide : abandon");
}

void PageEdition::replacerCurseur(pages::Moteur& mo) { _champs[_actif].placerCurseur(mo.ecran()); }

// Passe au champ visible suivant (sens +1) ou précédent (-1) à partir de i.
void PageEdition::aller(pages::Moteur& mo, int i, int sens) {
  // Le protocole commande les champs visibles : on redessine s'il a changé.
  const char p = _champs[kProto].texte()[0];
  bool redessiner = false;
  if (p != _protoAffiche) {
    _protoAffiche = p;
    redessiner = true;
    if (_indice < 0 && p >= '1' && p <= '4') {
      const auto proto = static_cast<ses::Protocole>(p - '1');
      char t[2] = {char('1' + static_cast<int>(annuaire::affichageParDefaut(proto, mo.io().avance()))), '\0'};
      _champs[kAffichage].remplir(t);
      _champs[kRouleau].remplir(proto == ses::Protocole::Telnet ? "O" : "N");
    }
  }
  while (i >= 0 && i < kNbChamps && !visible(i)) i += sens;
  if (i < 0 || i >= kNbChamps) {
    mo.ecran().bip();
    if (redessiner) mo.reafficher();
    return;
  }
  _actif = i;
  if (ecranDe(i) != _ecran || redessiner) {
    _ecran = ecranDe(i);
    mo.reafficher();
    return;
  }
  replacerCurseur(mo);
}

bool PageEdition::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (evt.est(mio::Touche::Envoi)) {
    enregistrer(mo);
    return true;
  }
  if (evt.est(mio::Touche::Suite)) {
    aller(mo, _actif + 1, +1);
    return true;
  }
  if (evt.est(mio::Touche::Retour)) {
    if (_actif == kNom) return false;  // premier champ : on remonte d'un niveau
    aller(mo, _actif - 1, -1);
    return true;
  }
  const auto r = _champs[_actif].touche(mo.ecran(), evt);
  if (r == pages::Champ::Resultat::Abandon) {
    quitter(mo);
    return true;
  }
  return r != pages::Champ::Resultat::Ignore;
}

void PageEdition::quitter(pages::Moteur& mo) {
  mo.retour();
  mo.message("Fiche non enregistrée", false);
}

// Relit les champs dans f. Sur une erreur : message, curseur sur le champ, false.
bool PageEdition::lire(pages::Moteur& mo, annuaire::Fiche& f) {
  auto refuser = [&](int i, const char* pourquoi) {
    _actif = i;
    if (ecranDe(i) != _ecran) {
      _ecran = ecranDe(i);
      mo.reafficher();
    }
    mo.message(pourquoi);
    return false;
  };
  auto oui = [&](int i, bool& v) {
    const char c = _champs[i].texte()[0];
    if (c && c != 'O' && c != 'N') return false;
    v = c == 'O';
    return true;
  };

  f = _modele;
  const char p = _champs[kProto].texte()[0];
  if (p < '1' || p > '4') return refuser(kProto, "Protocole : 1 à 4");
  f.proto = static_cast<ses::Protocole>(p - '1');

  const char* code = _champs[kCode].texte();
  if (!code[0]) return refuser(kCode, "Tapez un code");
  if (!code[1] && code[0] >= '1' && code[0] <= '9') return refuser(kCode, "Un chiffre seul est une rubrique");
  const int deja = annuaire::chercher(code);
  if (deja >= 0 && deja != _indice) return refuser(kCode, "Code déjà pris");
  strlcpy(f.code, code, sizeof(f.code));
  strlcpy(f.nom, _champs[kNom].vide() ? code : _champs[kNom].texte(), sizeof(f.nom));

  if (_champs[kAdresse].vide()) return refuser(kAdresse, "Tapez l'adresse");
  strlcpy(f.hote, _champs[kAdresse].texte(), sizeof(f.hote));
  long v;
  if (!lireEntier(_champs[kPort].texte(), 65535, v)) return refuser(kPort, "Port : 1 à 65535");
  f.port = uint16_t(v);
  strlcpy(f.chemin, _champs[kChemin].vide() ? "/" : _champs[kChemin].texte(), sizeof(f.chemin));

  const char a = _champs[kAffichage].texte()[0];
  if (a < '1' || a > '3') return refuser(kAffichage, "Affichage : 1 à 3");
  f.affichage = static_cast<annuaire::Affichage>(a - '1');
  if (!oui(kEcho, f.echo)) return refuser(kEcho, "Répondez O ou N");
  if (!oui(kRouleau, f.defilement)) return refuser(kRouleau, "Répondez O ou N");
  if (!oui(kFin, f.finTransmise)) return refuser(kFin, "Répondez O ou N");
  strlcpy(f.sousProto, _champs[kSousProto].texte(), sizeof(f.sousProto));
  if (!lireEntier(_champs[kPing].texte(), 600, v)) return refuser(kPing, "Ping : 0 à 600 s");
  f.pingS = uint16_t(v);
  strlcpy(f.utilisateur, _champs[kUtil].texte(), sizeof(f.utilisateur));
  strlcpy(f.motDePasse, _champs[kMdp].texte(), sizeof(f.motDePasse));
  if (f.proto == ses::Protocole::Ssh && !f.utilisateur[0]) return refuser(kUtil, "Tapez l'utilisateur");
  if (!oui(kReco, f.reconnexion)) return refuser(kReco, "Répondez O ou N");
  strlcpy(f.cle, _champs[kCle].texte(), sizeof(f.cle));
  if (f.cle[0] && !cles::existe(f.cle)) return refuser(kCle, "Clé inconnue : voir le portail");
  return true;
}

void PageEdition::enregistrer(pages::Moteur& mo) {
  annuaire::Fiche f;
  if (!lire(mo, f)) return;
  const int i = annuaire::enregistrer(f, _indice);
  if (i < 0) {
    mo.message("Annuaire plein (30 services)");
    return;
  }
  pageFiche.choisir(i);
  if (_indice < 0) mo.remplacer(pageFiche);  // nouvelle : la fiche prend sa place
  else mo.retour();                           // modification : retour à la fiche
  mo.message("Fiche enregistrée", false);
}

const char* PageEdition::aide() const {
  return "Fiche d'un service, deux écrans.\n"
         "\n"
         "SUITE et RETOUR passent d'un\n"
         "champ à l'autre ; SUITE au dernier\n"
         "champ ouvre les options.\n"
         "ENVOI enregistre depuis n'importe\n"
         "quel champ.\n"
         "\n"
         "Le code (8 caractères) se tape au\n"
         "Sommaire pour appeler le service.\n"
         "Adresse : nom ou IP, sans ws://.\n"
         "Chemin : /ws par exemple (WS).\n"
         "\n"
         "ANNULATION sur un champ vide\n"
         "abandonne sans enregistrer.";
}
