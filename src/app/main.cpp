// EspTel : le firmware.
//
// Mire (P00), Sommaire (P10), Annuaire (P20 à P23), Connexion directe (P30),
// Réseau WiFi (P40 à P43), Paramètres (P50, P54), portail web et mise à jour
// (P52), Informations (P60), et l'appel d'un service en Telnet, WebSocket ou
// SSH (P70 à P73, P90 ; la session est P80).

#include "../config.h"

#include <Arduino.h>

#include "../MinitelIO/MinitelIO.h"
#include "../Pages/Moteur.h"
#include "Annuaire.h"
#include "Appel.h"
#include "Fichiers.h"
#include "Pages.h"
#include "Portail.h"
#include "Reglages.h"
#include "Reseau.h"

#ifndef SIMULATEUR
// La boucle déchiffre les clés SSH déposées par le portail (libssh, mbedTLS) :
// 16 Ko de pile au lieu de 8.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);
#endif

mio::MinitelIO minitel(MINITEL_UART, MINITEL_RX_PIN, MINITEL_TX_PIN);
pages::Moteur moteur(minitel);

PageMire pageMire;
PageSommaire pageSommaire;
PageInfos pageInfos;
PageWifi pageWifi;
PageReseaux pageReseaux;
PageMotDePasse pageMotDePasse;
PageMemorises pageMemorises;
PageAnnuaire pageAnnuaire;
PageFiche pageFiche;
PageEdition pageEdition;
PageSuppression pageSuppression;
PageDirecte pageDirecte;
PageAppel pageAppel;
PageEchec pageEchec;
PageIdentification pageIdentification;
PageCle pageCle;
PageFin pageFin;
PageParametres pageParametres;
PageReinitialisation pageReinitialisation;
PagePortail pagePortail;
PageVdt pageVdt;

namespace {

// Nombre de colonnes occupées par un texte UTF-8 (un accent = une colonne).
int colonnes(const char* s) {
  int n = 0;
  for (; *s; ++s) {
    if ((static_cast<uint8_t>(*s) & 0xC0) != 0x80) ++n;
  }
  return n;
}

// Rangée 0 au repos : « WiFi Connecté  -62dBm » à gauche, l'heure à droite.
void etatRepos(char* dest, size_t taille) {
  switch (reseau::etat()) {
    case reseau::Etat::NonConfigure:
      snprintf(dest, taille, "WiFi non configuré");
      return;
    case reseau::Etat::Connexion:
      snprintf(dest, taille, "WiFi : connexion...");
      return;
    case reseau::Etat::Echec:
      snprintf(dest, taille, "WiFi : échec");
      return;
    case reseau::Etat::Connecte:
      break;
  }
  // La rangée 0 se voit se réécrire : le RSSI n'est relu qu'au changement de
  // minute, pour que la ligne ne change qu'une fois par minute au plus.
  static char minuteLue[8] = "";
  static int rssi = 0;
  char hhmm[8] = "";
  const bool aHeure = reseau::heure(hhmm, sizeof(hhmm));
  if (rssi == 0 || strcmp(hhmm, minuteLue) != 0) {
    strlcpy(minuteLue, hhmm, sizeof(minuteLue));
    rssi = reseau::rssi();
  }
  // « Connecté » plutôt que le nom du réseau : la longueur de la ligne est fixe.
  snprintf(dest, taille, "WiFi Connecté  %ddBm", rssi);
  if (aHeure) {
    // L'heure finit en colonne 36 : le Minitel garde les dernières colonnes pour
    // sa lettre d'état (F) et une marge (vu sur le Minitel).
    const size_t n = strlen(dest);
    const int pad = 31 - colonnes(dest);
    if (pad > 0 && n + pad + strlen(hhmm) < taille) {
      memset(dest + n, ' ', pad);
      strcpy(dest + n + pad, hhmm);
    }
  }
}

// Minitel allumé après l'ESP32 : sans réponse au démarrage, il est resté à 1200
// bauds sans réglage (40 colonnes, écho local, majuscules). On le cherche à 1200
// bauds, sa vitesse de mise sous tension, tant qu'il ne répond pas.
constexpr uint32_t kSondageMs = 5000;
constexpr uint32_t kDelaiSondageMs = 250;
uint32_t dernierSondage = 0;

void chercherMinitel() {
  if (minitel.repond() || appel::minitelCede() || millis() - dernierSondage < kSondageMs) return;
  dernierSondage = millis();
  if (!minitel.sonder(kDelaiSondageMs)) return;
  minitel.demarrer(reglages::vitesseVisee());
  DEBUG_UART.printf("[EspTel] Minitel allumé : %d bauds, %s, avancé %d\n", minitel.vitesse(),
                    minitel.identite().modele(), minitel.avance());
  minitel.clavierPagesLocales();
  moteur.reafficher();
}

}  // namespace

void setup() {
  DEBUG_UART.begin(DEBUG_VITESSE);
  DEBUG_UART.printf("\n[EspTel] %s %s\n", FIRMWARE_NOM, FIRMWARE_VERSION);

  reglages::charger();
  minitel.demarrer(reglages::vitesseVisee());
  DEBUG_UART.printf("[EspTel] Minitel %d bauds, %s, avancé %d\n", minitel.vitesse(),
                    minitel.identite().modele(), minitel.avance());
  minitel.clavierPagesLocales();

  // Les gros blocs (annuaire, pile de session) avant le WiFi, tant que le tas est d'un seul tenant.
  annuaire::charger();
  fichiers::monter();
  appel::initialiser(minitel);
  reseau::demarrer();
  moteur.demarrer(pageMire, pageSommaire, etatRepos);
}

void loop() {
  reseau::tic();
  appel::tic();
  chercherMinitel();
  portail::tic();
  // Le portail ne vit que sur P52 (le GUIDE de P52 le laisse ouvert).
  if (portail::ouvert() && moteur.courante() != &pagePortail) portail::fermer();
  // Session ouverte : le Minitel appartient à la tâche d'appel (P80).
  if (!appel::minitelCede()) moteur.boucle();
  delay(1);
}
