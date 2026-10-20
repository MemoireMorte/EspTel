#include "../config.h"

#include "../Pages/Charte.h"
#include "Pages.h"
#include "Reseau.h"

namespace {

constexpr int kColChamp = 4;
constexpr int kRangeeNom = 7;
constexpr int kRangeeMdp = 10;  // et 11 : 64 caractères sur deux rangées de 32
constexpr uint8_t kLargeur = 32;
constexpr size_t kMdpMin = 8;   // WPA ; une clé WEP fait 5 ou 13 caractères

}  // namespace

void PageMotDePasse::preparer(const char* ssid, uint8_t secu, const char* mdpConnu) {
  strlcpy(_ssid, ssid, sizeof(_ssid));
  strlcpy(_mdpConnu, mdpConnu, sizeof(_mdpConnu));
  _secu = secu;
  _cache = false;
}

void PageMotDePasse::preparerCache() {
  _ssid[0] = '\0';
  _mdpConnu[0] = '\0';
  _secu = 0;
  _cache = true;
}

void PageMotDePasse::entrer(pages::Moteur&) {
  _nom.configurer(kColChamp, kRangeeNom, reseau::kTailleSsid - 1, false);
  _mdp.configurer(kColChamp, kRangeeMdp, reseau::kTailleMdp - 1, false, kLargeur);
  _mdp.remplir(_mdpConnu);
  _surNom = _cache;
}

void PageMotDePasse::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::bandeau(m, "MOT DE PASSE");

  if (_cache) {
    charte::texte(m, 2, kRangeeNom - 1, "Nom du réseau caché :");
    _nom.dessiner(m);
    charte::texte(m, 2, kRangeeMdp - 1, "Mot de passe (vide si ouvert) :");
  } else {
    char nom[28];
    nomAffiche(_ssid, nom, sizeof(nom), 26);
    charte::ligneInfo(m, kRangeeNom - 1, "Réseau", nom);
    charte::ligneInfo(m, kRangeeNom, "Sécurité", reseau::nomSecu(_secu));
    charte::texte(m, 2, kRangeeMdp - 1, "Mot de passe :");
  }
  _mdp.dessiner(m);

  charte::texte(m, 2, 13, "Il s'affiche en clair.", charte::kLibelle);
  charte::texte(m, 2, 14, "Majuscule : Maj + lettre.", charte::kLibelle);
  if (_mdpConnu[0]) charte::texte(m, 2, 15, "Mot de passe mémorisé, à garder ou", charte::kLibelle);
  if (_mdpConnu[0]) charte::texte(m, 2, 16, "à corriger.", charte::kLibelle);
  if (_cache) charte::texte(m, 2, 16, "SUITE et RETOUR : changer de champ.", charte::kLibelle);

  charte::filet(m, 21);
  m.newXY(2, charte::kRangeeInvite);
  m.attributs(charte::kValeur);
  m.print("Se connecter    → ");
  charte::touche(m, "ENVOI");
  charte::rappel(m, _cache);
}

void PageMotDePasse::replacerCurseur(pages::Moteur& mo) { actif().placerCurseur(mo.ecran()); }

bool PageMotDePasse::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (evt.est(mio::Touche::Envoi)) {
    valider(mo);
    return true;
  }
  if (_cache && evt.est(mio::Touche::Suite)) {
    if (!_surNom) {
      mo.ecran().bip();
      return true;
    }
    _surNom = false;
    replacerCurseur(mo);
    return true;
  }
  if (_cache && evt.est(mio::Touche::Retour) && !_surNom) {
    _surNom = true;
    replacerCurseur(mo);
    return true;
  }
  // ANNULATION sur un champ vide ne quitte pas la page : RETOUR est là pour ça.
  return actif().touche(mo.ecran(), evt) != pages::Champ::Resultat::Ignore;
}

void PageMotDePasse::valider(pages::Moteur& mo) {
  const char* ssid = _cache ? _nom.texte() : _ssid;
  if (!ssid[0]) {
    _surNom = true;
    mo.message("Tapez le nom du réseau");
    replacerCurseur(mo);
    return;
  }
  const size_t n = _mdp.taille();
  const bool ouvert = _cache ? n == 0 : reseau::secuOuverte(_secu);
  const bool wep = !_cache && strcmp(reseau::nomSecu(_secu), "WEP") == 0;
  if (!ouvert && !wep && n < kMdpMin) {
    _surNom = false;
    mo.message(n ? "8 caractères au moins" : "Tapez le mot de passe");
    replacerCurseur(mo);
    return;
  }
  reseau::connecter(ssid, _mdp.texte());
  mo.retourA(pageWifi);
}

const char* PageMotDePasse::aide() const {
  return "Tapez le mot de passe du réseau\n"
         "puis ENVOI. Le Minitel essaie de\n"
         "se connecter et revient à l'état\n"
         "du WiFi (P40) avec le résultat.\n"
         "\n"
         "Le clavier tape en minuscules ;\n"
         "Maj + lettre pour une majuscule.\n"
         "CORRECTION efface un caractère,\n"
         "ANNULATION tout le champ.\n"
         "\n"
         "Un réseau qui répond est mémorisé\n"
         "avec son mot de passe.";
}
