#include "../config.h"

#include "../Pages/Charte.h"
#include "Pages.h"
#include "Vdt.h"

namespace {

constexpr int kColChamp = 18;
constexpr uint8_t kLongueurCode = 8;  // code service : 8 caractères au plus (plan, §9)

}  // namespace

void PageSommaire::entrer(pages::Moteur&) {
  _champ.configurer(kColChamp, charte::kRangeeInvite, kLongueurCode, true);
}

void PageSommaire::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::bandeau(m, "SOMMAIRE");

  charte::choix(m, 6, '1', "Annuaire des services");
  charte::choix(m, 8, '2', "Connexion directe");
  charte::choix(m, 10, '3', "Réseau WiFi");
  charte::choix(m, 12, '4', "Paramètres");
  charte::choix(m, 14, '5', "Informations");

  charte::texte(m, 2, 17, "Ou tapez le code d'un service", charte::kLibelle);
  m.newXY(2, 18);
  m.attributs(charte::kLibelle);
  m.print("puis ");
  charte::touche(m, "CONNEXION/FIN");

  charte::filet(m, 21);
  charte::texte(m, 2, charte::kRangeeInvite, "Choix ou code :");
  _champ.dessiner(m);
  m.newXY(kColChamp + kLongueurCode + 1, charte::kRangeeInvite);
  m.attributs(charte::kValeur);
  m.print("puis ");
  charte::touche(m, "ENVOI");

  m.newXY(2, charte::kRangeeRappel);
  m.attributs(charte::kLibelle);
  m.print("Aide ");
  charte::touche(m, "GUIDE");
}

void PageSommaire::replacerCurseur(pages::Moteur& mo) { _champ.placerCurseur(mo.ecran()); }

bool PageSommaire::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (evt.est(mio::Touche::Envoi)) {
    valider(mo, false);
    return true;
  }
  if (evt.est(mio::Touche::ConnexionFin)) {
    valider(mo, true);
    return true;
  }
  // ANNULATION sur champ vide : rien à abandonner au Sommaire.
  return _champ.touche(mo.ecran(), evt) != pages::Champ::Resultat::Ignore;
}

void PageSommaire::valider(pages::Moteur& mo, bool connexion) {
  if (_champ.vide()) {
    mo.ecran().bip();
    return;
  }
  const char* saisie = _champ.texte();
  const bool chiffre = _champ.taille() == 1 && saisie[0] >= '1' && saisie[0] <= '5';

  if (chiffre && !connexion) {
    const char rubrique = saisie[0];
    _champ.vider(&mo.ecran());
    switch (rubrique) {
      case '1':
        mo.aller(pageAnnuaire);
        return;
      case '2':
        mo.aller(pageDirecte);
        return;
      case '5':
        mo.aller(pageInfos);
        return;
      case '3':
        mo.aller(pageWifi);
        return;
      default:
        mo.aller(pageParametres);
        return;
    }
  }

  // Un code service, comme sur le Kiosque : on cherche dans l'annuaire.
  const int i = annuaire::chercher(saisie);
  if (i < 0 && vdt::existe(saisie)) {
    // Pas de service à ce nom, mais une page locale (fichier .vdt).
    pageVdt.choisir(saisie);
    _champ.vider(&mo.ecran());
    mo.aller(pageVdt);
    return;
  }
  _champ.vider(&mo.ecran());
  if (i < 0) {
    mo.message("Code inconnu");
    return;
  }
  appeler(mo, annuaire::fiche(i), i);
}

const char* PageSommaire::aide() const {
  return "Tapez le numéro d'une rubrique,\n"
         "puis ENVOI.\n"
         "\n"
         "Ou tapez le code d'un service\n"
         "(TEASER par exemple), puis\n"
         "CONNEXION/FIN : l'appel part.\n"
         "Les codes sont ceux de l'annuaire\n"
         "(rubrique 1), ou ceux des pages\n"
         "locales déposées par le portail.\n"
         "\n"
         "CORRECTION efface un caractère,\n"
         "ANNULATION efface le champ.\n"
         "\n"
         "SOMMAIRE ramène ici depuis\n"
         "n'importe quelle page.";
}
