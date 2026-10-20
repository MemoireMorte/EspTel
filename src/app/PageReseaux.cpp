#include "../config.h"

#include "../Pages/Charte.h"
#include "Pages.h"
#include "Reseau.h"

namespace {

constexpr int kParPage = 8;
constexpr int kRangeeListe = 7;  // rangées 7 à 14
constexpr int kColNom = 6;
constexpr int kColSignal = 26;
constexpr int kColSecu = 34;
constexpr int kColChamp = 7;

}  // namespace

int PageReseaux::nbPages() const {
  const int n = reseau::nbTrouves();
  return n ? (n + kParPage - 1) / kParPage : 1;
}

void PageReseaux::entrer(pages::Moteur&) {
  _champ.configurer(kColChamp, charte::kRangeeInvite, 2, true);
  _page = 0;
  reseau::lancerRecherche();
}

void PageReseaux::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::bandeau(m, "RESEAUX WIFI", nullptr);

  m.newXY(2, 5);
  m.attributs(charte::kLibelle);
  m.print("N°");
  charte::texte(m, kColNom, 5, "RESEAU", charte::kLibelle);
  charte::texte(m, kColSignal, 5, "SIGNAL", charte::kLibelle);
  charte::texte(m, kColSecu, 5, "SECU", charte::kLibelle);
  dessinerListe(mo);

  charte::choix(m, 16, 'R', "Relancer la recherche");
  charte::choix(m, 17, 'M', "Saisir un nom de réseau caché");

  charte::filet(m, 21);
  charte::texte(m, 2, charte::kRangeeInvite, "N° :");
  _champ.dessiner(m);
  m.newXY(kColChamp + 3, charte::kRangeeInvite);
  m.attributs(charte::kValeur);
  m.print("puis ");
  charte::touche(m, "ENVOI");
  charte::rappel(m, true);
}

void PageReseaux::dessinerListe(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  _rechercheAffichee = reseau::rechercheEnCours();
  const int n = reseau::nbTrouves();
  if (_page >= nbPages()) _page = 0;

  // Numéro de page à droite du bandeau (rangée 3).
  char num[16];
  snprintf(num, sizeof(num), "%d/%d", _page + 1, nbPages());
  charte::effacer(m, 35, 3);
  charte::texte(m, 41 - static_cast<int>(strlen(num)), 3, num, charte::kLibelle);

  const bool connecte = reseau::etat() == reseau::Etat::Connecte;
  for (int k = 0; k < kParPage; ++k) {
    const int rangee = kRangeeListe + k;
    const int i = _page * kParPage + k;
    if (_rechercheAffichee || i >= n) {
      charte::effacer(m, 1, rangee);
      continue;
    }
    const reseau::Trouve& t = reseau::trouve(i);
    m.newXY(2, rangee);
    m.attributs(INVERSION_FOND);
    char t3[16];
    snprintf(t3, sizeof(t3), "%2d ", i + 1);
    m.print(t3);
    m.attributs(FOND_NORMAL);
    char nom[24];
    nomAffiche(t.ssid, nom, sizeof(nom), kColSignal - kColNom - 1);
    // Le réseau connecté en vert, les réseaux mémorisés marqués d'une étoile.
    const bool actuel = connecte && strcmp(t.ssid, reseau::ssid()) == 0;
    m.newXY(kColNom, rangee);
    m.attributs(actuel ? CARACTERE_VERT : charte::kValeur);
    m.print(nom);
    if (reseau::indiceMemorise(t.ssid) >= 0) {
      m.attributs(charte::kLibelle);
      m.print("*");
    }
    m.cancel();
    m.newXY(kColSignal, rangee);
    charte::barres(m, reseau::barres(t.rssi));
    charte::texte(m, kColSecu, rangee, reseau::nomSecu(t.secu));
  }
  if (_rechercheAffichee) charte::texte(m, 6, kRangeeListe + 2, "Recherche des réseaux...", charte::kLibelle);
  else if (n == 0) charte::texte(m, 6, kRangeeListe + 2, "Aucun réseau trouvé.", charte::kLibelle);
}

void PageReseaux::tic(pages::Moteur& mo) {
  if (_rechercheAffichee && !reseau::rechercheEnCours()) {
    dessinerListe(mo);
    replacerCurseur(mo);
  }
}

void PageReseaux::replacerCurseur(pages::Moteur& mo) { _champ.placerCurseur(mo.ecran()); }

bool PageReseaux::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (evt.est(mio::Touche::Envoi)) {
    valider(mo);
    return true;
  }
  if (evt.est(mio::Touche::Suite) || (evt.est(mio::Touche::Retour) && _page > 0)) {
    const int p = _page + (evt.est(mio::Touche::Suite) ? 1 : -1);
    if (_rechercheAffichee || p >= nbPages()) {
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

void PageReseaux::valider(pages::Moteur& mo) {
  char saisie[4];
  strlcpy(saisie, _champ.texte(), sizeof(saisie));
  _champ.vider(&mo.ecran());

  if (strcmp(saisie, "R") == 0) {
    reseau::lancerRecherche();
    _page = 0;
    dessinerListe(mo);
    replacerCurseur(mo);
    return;
  }
  if (strcmp(saisie, "M") == 0) {
    pageMotDePasse.preparerCache();
    mo.aller(pageMotDePasse);
    return;
  }

  const int n = atoi(saisie);
  if (_rechercheAffichee || n < 1 || n > reseau::nbTrouves()) {
    mo.message(saisie[0] ? "Numéro inconnu" : "Tapez un numéro");
    return;
  }
  const reseau::Trouve& t = reseau::trouve(n - 1);
  if (reseau::secuEntreprise(t.secu)) {
    mo.message("Réseau d'entreprise : non géré");
    return;
  }
  if (reseau::secuOuverte(t.secu)) {
    reseau::connecter(t.ssid, "");
    mo.retourA(pageWifi);
    return;
  }
  const int k = reseau::indiceMemorise(t.ssid);
  pageMotDePasse.preparer(t.ssid, t.secu, k >= 0 ? reseau::motDePasse(k) : "");
  mo.aller(pageMotDePasse);
}

const char* PageReseaux::aide() const {
  return "Les réseaux WiFi autour, du plus\n"
         "fort au plus faible.\n"
         "\n"
         "Tapez le numéro d'un réseau puis\n"
         "ENVOI : un réseau ouvert (---) se\n"
         "connecte tout de suite, sinon le\n"
         "mot de passe est demandé.\n"
         "\n"
         "* : réseau mémorisé, son mot de\n"
         "passe est déjà rempli.\n"
         "\n"
         "R relance la recherche, M permet\n"
         "de saisir un réseau caché.\n"
         "SUITE et RETOUR changent de page.";
}
