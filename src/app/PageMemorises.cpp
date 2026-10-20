#include "../config.h"

#include "../Pages/Charte.h"
#include "Pages.h"
#include "Reseau.h"

namespace {

constexpr int kRangeeListe = 6;  // une rangée sur deux, 6 à 14
constexpr int kColChamp = 10;

}  // namespace

void PageMemorises::entrer(pages::Moteur&) { _champ.configurer(kColChamp, charte::kRangeeInvite, 2, true); }

void PageMemorises::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::bandeau(m, "RESEAUX MEMORISES");

  const int n = reseau::nbMemorises();
  const bool connecte = reseau::etat() == reseau::Etat::Connecte;
  for (int i = 0; i < n; ++i) {
    char nom[28];
    nomAffiche(reseau::memorise(i), nom, sizeof(nom), 24);
    const int rangee = kRangeeListe + 2 * i;
    charte::choix(m, rangee, static_cast<char>('1' + i), nom);
    if (connecte && strcmp(reseau::memorise(i), reseau::ssid()) == 0) {
      charte::texte(m, 32, rangee, "connecté", CARACTERE_VERT);
    }
  }
  if (n == 0) {
    charte::texte(m, 2, 8, "Aucun réseau mémorisé.");
    charte::texte(m, 2, 10, "Un réseau s'ajoute ici dès qu'une", charte::kLibelle);
    charte::texte(m, 2, 11, "connexion réussit (recherche, P41).", charte::kLibelle);
  } else {
    charte::texte(m, 2, 17, "N° puis ENVOI : se connecter", charte::kLibelle);
    charte::texte(m, 2, 18, "S et N° (S2...) puis ENVOI : oublier", charte::kLibelle);
  }

  charte::filet(m, 21);
  charte::texte(m, 2, charte::kRangeeInvite, "Choix :");
  _champ.dessiner(m);
  m.newXY(kColChamp + 3, charte::kRangeeInvite);
  m.attributs(charte::kValeur);
  m.print("puis ");
  charte::touche(m, "ENVOI");
  charte::rappel(m);
}

void PageMemorises::replacerCurseur(pages::Moteur& mo) { _champ.placerCurseur(mo.ecran()); }

bool PageMemorises::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (evt.est(mio::Touche::Envoi)) {
    valider(mo);
    return true;
  }
  return _champ.touche(mo.ecran(), evt) != pages::Champ::Resultat::Ignore;
}

void PageMemorises::valider(pages::Moteur& mo) {
  char saisie[4];
  strlcpy(saisie, _champ.texte(), sizeof(saisie));
  _champ.vider(&mo.ecran());

  const bool oubli = saisie[0] == 'S';
  const char* chiffre = oubli ? saisie + 1 : saisie;
  const int i = (chiffre[0] >= '1' && chiffre[0] <= '9' && !chiffre[1]) ? chiffre[0] - '1' : -1;
  if (i < 0 || i >= reseau::nbMemorises()) {
    mo.message(saisie[0] ? "Numéro inconnu" : "Tapez un numéro");
    return;
  }
  if (!oubli) {
    reseau::connecter(reseau::memorise(i), reseau::motDePasse(i));
    mo.retourA(pageWifi);
    return;
  }
  char texte[48];
  char nom[24];
  nomAffiche(reseau::memorise(i), nom, sizeof(nom), 20);
  snprintf(texte, sizeof(texte), "%s oublié", nom);
  reseau::oublier(i);
  mo.reafficher();
  mo.message(texte, false);
}

const char* PageMemorises::aide() const {
  return "Les réseaux qui ont déjà marché,\n"
         "le dernier utilisé en premier\n"
         "(5 au plus). Au démarrage, ils\n"
         "sont essayés dans cet ordre.\n"
         "\n"
         "Numéro puis ENVOI : connexion\n"
         "avec le mot de passe mémorisé.\n"
         "\n"
         "S suivi du numéro (S2 par\n"
         "exemple) puis ENVOI : oublie le\n"
         "réseau et son mot de passe.";
}
