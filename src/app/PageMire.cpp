#include "../config.h"

#include "../Pages/Charte.h"
#include "Pages.h"
#include "Reglages.h"
#include "Reseau.h"
#include "Vdt.h"

namespace {

constexpr uint32_t kAttenteWifiMaxMs = 25000;  // au plus, si le WiFi cherche encore

// Sans réseau, la mire reste affichée et ENVOI mène au choix d'un réseau (P41).
bool sansReseau() {
  const reseau::Etat e = reseau::etat();
  return e == reseau::Etat::Echec || e == reseau::Etat::NonConfigure;
}

void bord(Minitel& m, int col, int rangee, byte motif, int largeur) {
  m.newXY(col, rangee);
  m.attributs(CARACTERE_JAUNE);
  m.graphicMode();
  m.graphic(motif);
  if (largeur > 1) m.repeat(largeur - 1);
  m.textMode();
}

}  // namespace

void PageMire::entrer(pages::Moteur&) {
  _debut = millis();
  _etatAffiche = -1;
}

void PageMire::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  // Une mire dessinée à l'éditeur (déposée par le portail) remplace celle-ci,
  // sans l'état du réseau : ENVOI et le passage au Sommaire restent.
  _fichier = vdt::afficher(mo.io(), vdt::kMire);
  if (_fichier) return;

  // Cadre des rangées 3 à 8, colonnes 4 à 37, titre en double hauteur au centre.
  bord(m, 4, 3, 0b000011, 34);
  for (int r = 4; r <= 7; ++r) {
    bord(m, 4, r, 0b111111, 1);
    bord(m, 37, r, 0b111111, 1);
  }
  bord(m, 4, 8, 0b110000, 34);
  charte::grand(m, 18, 6, "ESPTEL");

  charte::texte(m, 11, 10, "Terminal réseau WiFi");
  charte::texte(m, 9, 11, "Telnet - WebSocket - SSH", charte::kLibelle);

  const mio::MinitelIO& io = mo.io();
  char t[48];
  snprintf(t, sizeof(t), "%s détecté", io.identite().valide ? io.identite().modele() : "Minitel");
  charte::texte(m, 4, 14, t);
  snprintf(t, sizeof(t), "%d bauds", io.vitesse());
  charte::texte(m, 37 - static_cast<int>(strlen(t)), 14, t);
  _etatAffiche = -1;
  charte::texte(m, 4, 24, "v" FIRMWARE_VERSION, charte::kLibelle);
  dessinerReseau(mo);
}

// Interrogée avant dessiner() : _fichier n'est pas encore à jour.
bool PageMire::rangeeZeroMasquee() const { return vdt::existe(vdt::kMire); }

void PageMire::dessinerReseau(pages::Moteur& mo) {
  if (_fichier) return;
  const reseau::Etat e = reseau::etat();
  if (static_cast<int>(e) == _etatAffiche) return;
  _etatAffiche = static_cast<int>(e);

  Minitel& m = mo.ecran();
  dessinerInvite(m);
  charte::texte(m, 4, 15, "Réseau", charte::kLibelle);
  if (e == reseau::Etat::NonConfigure) {
    charte::texte(m, 12, 15, "non configuré");
    m.cancel();
    return;
  }
  char t[40];
  nomAffiche(reseau::ssid(), t, 21, 20);
  strcat(t, " ");
  charte::effacer(m, 12, 15);  // le statut précédent peut être plus long
  charte::texte(m, 12, 15, t);
  const int pointsDebut = 12 + static_cast<int>(strlen(t));
  const char* statut = e == reseau::Etat::Connecte ? "OK" : e == reseau::Etat::Echec ? "ECHEC" : "...";
  const int statutCol = 38 - static_cast<int>(strlen(statut));
  if (statutCol - 1 > pointsDebut) {
    m.attributs(charte::kChamp);
    m.print(".");
    m.repeat(statutCol - 1 - pointsDebut - 1);
  }
  m.newXY(statutCol, 15);
  if (e == reseau::Etat::Echec) {
    m.attributs(CARACTERE_ROUGE);
    m.attributs(CLIGNOTEMENT);  // le clignotement est réservé aux alertes
  } else {
    m.attributs(CARACTERE_VERT);
  }
  m.print(statut);
  m.attributs(FIXE);

  charte::texte(m, 4, 16, "Adresse", charte::kLibelle);
  const String ip = reseau::ip();
  charte::texte(m, 12, 16, ip.length() ? ip.c_str() : "-");
  m.cancel();
}

void PageMire::dessinerInvite(Minitel& m) {
  const bool choisir = sansReseau();
  m.newXY(4, 22);
  m.attributs(charte::kValeur);
  m.print(choisir ? "Choisir un réseau    → " : "Accès au sommaire    → ");
  charte::touche(m, "ENVOI");
  if (choisir) {
    m.newXY(4, 23);
    m.attributs(charte::kValeur);
    m.print("Aller au sommaire    → ");
    charte::touche(m, "SOMMAIRE");
  } else {
    charte::effacer(m, 1, 23);
  }
}

bool PageMire::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (evt.est(mio::Touche::Envoi)) {
    if (sansReseau()) mo.chemin({&pageWifi, &pageReseaux});
    else mo.sommaire();
    return true;
  }
  return false;
}

void PageMire::tic(pages::Moteur& mo) {
  dessinerReseau(mo);
  if (sansReseau()) return;  // on attend que la personne choisisse
  const uint32_t ecoule = millis() - _debut;
  const bool cherche = reseau::etat() == reseau::Etat::Connexion;
  const uint32_t delai = uint32_t(reglages::lire().mireS) * 1000;  // passage automatique au Sommaire
  if ((ecoule > delai && !cherche) || ecoule > kAttenteWifiMaxMs) mo.sommaire();
}

const char* PageMire::aide() const {
  return "Mire d'accueil.\n"
         "\n"
         "ENVOI ouvre le Sommaire, qui\n"
         "s'ouvre aussi tout seul après\n"
         "quelques secondes.\n"
         "\n"
         "Sans WiFi, la mire reste là :\n"
         "ENVOI mène à la liste des\n"
         "réseaux, SOMMAIRE au Sommaire.";
}
