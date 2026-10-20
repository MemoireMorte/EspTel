#include "../config.h"

#include "../Pages/Charte.h"
#include "Pages.h"

namespace {

constexpr int kParPage = 8;
constexpr int kRangeeListe = 7;  // rangées 7 à 14
constexpr int kColCode = 6;
constexpr int kColNom = 16;
constexpr int kColProto = 35;
constexpr int kColChamp = 7;

}  // namespace

int PageAnnuaire::nbPages() const {
  const int n = annuaire::nombre();
  return n ? (n + kParPage - 1) / kParPage : 1;
}

void PageAnnuaire::entrer(pages::Moteur&) {
  _champ.configurer(kColChamp, charte::kRangeeInvite, 2, true);
  _page = 0;
}

void PageAnnuaire::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::bandeau(m, "ANNUAIRE", nullptr);

  m.newXY(2, 5);
  m.attributs(charte::kLibelle);
  m.print("N°");
  charte::texte(m, kColCode, 5, "CODE", charte::kLibelle);
  charte::texte(m, kColNom, 5, "SERVICE", charte::kLibelle);
  charte::texte(m, kColProto, 5, "PROTO", charte::kLibelle);
  dessinerListe(mo);
  charte::choix(m, 16, '+', "Nouveau service");

  charte::filet(m, 21);
  charte::texte(m, 2, charte::kRangeeInvite, "N° :");
  _champ.dessiner(m);
  m.newXY(kColChamp + 4, charte::kRangeeInvite);
  m.attributs(charte::kValeur);
  m.print("puis ");
  charte::touche(m, "ENVOI");
  m.attributs(charte::kValeur);
  m.print(" : fiche");
  m.newXY(kColChamp + 4, 23);
  m.attributs(charte::kValeur);
  m.print("puis ");
  charte::touche(m, "CONNEXION/FIN");
  m.attributs(charte::kValeur);
  m.print(" : appel");
  charte::rappel(m, true);
}

void PageAnnuaire::dessinerListe(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  if (_page >= nbPages()) _page = nbPages() - 1;
  char num[16];
  snprintf(num, sizeof(num), "%d/%d", _page + 1, nbPages());
  charte::effacer(m, 35, 3);
  charte::texte(m, 41 - static_cast<int>(strlen(num)), 3, num, charte::kLibelle);

  const int n = annuaire::nombre();
  for (int k = 0; k < kParPage; ++k) {
    const int rangee = kRangeeListe + k;
    const int i = _page * kParPage + k;
    if (i >= n) {
      charte::effacer(m, 1, rangee);
      continue;
    }
    const annuaire::Fiche& f = annuaire::fiche(i);
    m.newXY(2, rangee);
    m.attributs(INVERSION_FOND);
    char t[16];
    snprintf(t, sizeof(t), "%2d ", i + 1);
    m.print(t);
    m.attributs(FOND_NORMAL);
    charte::texte(m, kColCode, rangee, f.code);
    char nom[24];
    nomAffiche(f.nom, nom, sizeof(nom), kColProto - kColNom - 1);
    charte::texte(m, kColNom, rangee, nom, charte::kLibelle);
    charte::texte(m, kColProto, rangee, annuaire::abregeProtocole(f.proto));
    m.cancel();
  }
  if (n == 0) charte::texte(m, 6, kRangeeListe + 2, "L'annuaire est vide.", charte::kLibelle);
}

void PageAnnuaire::replacerCurseur(pages::Moteur& mo) { _champ.placerCurseur(mo.ecran()); }

bool PageAnnuaire::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (evt.est(mio::Touche::Envoi) || evt.est(mio::Touche::ConnexionFin)) {
    valider(mo, evt.est(mio::Touche::ConnexionFin));
    return true;
  }
  if (evt.est(mio::Touche::Suite) || (evt.est(mio::Touche::Retour) && _page > 0)) {
    const int p = _page + (evt.est(mio::Touche::Suite) ? 1 : -1);
    if (p >= nbPages()) {
      mo.ecran().bip();
      return true;
    }
    _page = p;
    dessinerListe(mo);
    replacerCurseur(mo);
    return true;
  }
  return _champ.touche(mo.ecran(), evt) != pages::Champ::Resultat::Ignore;
}

void PageAnnuaire::valider(pages::Moteur& mo, bool appel) {
  char saisie[4];
  strlcpy(saisie, _champ.texte(), sizeof(saisie));
  _champ.vider(&mo.ecran());

  if (strcmp(saisie, "+") == 0 && !appel) {
    if (annuaire::nombre() >= annuaire::kMax) {
      mo.message("Annuaire plein (30 services)");
      return;
    }
    pageEdition.nouvelle(nullptr);
    mo.aller(pageEdition);
    return;
  }
  const int n = atoi(saisie);
  if (n < 1 || n > annuaire::nombre()) {
    mo.message(saisie[0] ? "Numéro inconnu" : "Tapez un numéro");
    return;
  }
  if (appel) {
    appeler(mo, annuaire::fiche(n - 1), n - 1);
    return;
  }
  pageFiche.choisir(n - 1);
  mo.aller(pageFiche);
}

const char* PageAnnuaire::aide() const {
  return "Les services enregistrés, avec\n"
         "leur code.\n"
         "\n"
         "N° puis ENVOI : la fiche du\n"
         "service (modifier, supprimer).\n"
         "N° puis CONNEXION/FIN : l'appel.\n"
         "+ puis ENVOI : nouveau service.\n"
         "\n"
         "Au Sommaire, le code d'un service\n"
         "puis CONNEXION/FIN l'appelle\n"
         "directement, comme le 3615.\n"
         "\n"
         "SUITE et RETOUR changent de page.";
}
