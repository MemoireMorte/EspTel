#include "../config.h"

#include <WiFi.h>

#include "../Pages/Charte.h"
#include "Pages.h"
#include "Reseau.h"

namespace {

constexpr uint32_t kPeriodeMajMs = 5000;
constexpr int kRangeeVariables = 11;  // mémoire, minimum, en marche, WiFi, adresse

}  // namespace

void PageInfos::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::bandeau(m, "INFORMATIONS");

  char t[48];
  charte::ligneInfo(m, 6, "Firmware", FIRMWARE_NOM " " FIRMWARE_VERSION);
  // __DATE__ vaut "Oct 17 2026" : on garde l'ordre du compilateur, plus court.
  snprintf(t, sizeof(t), "%s %.5s", __DATE__, __TIME__);
  charte::ligneInfo(m, 7, "Compilé", t);
  const mio::MinitelIO& io = mo.io();
  snprintf(t, sizeof(t), "%s, %d bauds", io.identite().valide ? io.identite().modele() : "inconnu", io.vitesse());
  charte::ligneInfo(m, 8, "Minitel", t);
  snprintf(t, sizeof(t), "rév. %d, %lu MHz", ESP.getChipRevision(), (unsigned long)ESP.getCpuFreqMHz());
  charte::ligneInfo(m, 9, "ESP32", t);

  for (auto& ligne : _affiche) ligne[0] = '\0';
  dessinerVariables(mo, true);
  charte::ligneInfo(m, 16, "MAC", WiFi.macAddress().c_str());

  charte::ligneInfo(m, 17, "Merci à", "iodeo (carte, Minitel1B)");
  charte::texte(m, 14, 18, "Eric Sérandour (Minitel1B)");
  charte::texte(m, 14, 19, "Ewan Parker (LibSSH-ESP32)");
  charte::texte(m, 14, 20, "Markus Sattler (WebSockets)");

  charte::ligneInfo(m, 21, "Auteur", "MemoireMorte (Ben A.)");
  charte::filet(m, 22);
  m.newXY(2, charte::kRangeeRappel);
  charte::touche(m, "RETOUR");
  m.print(" ");
  charte::touche(m, "SOMMAIRE");
  m.attributs(charte::kLibelle);
  m.print("  Aide ");
  charte::touche(m, "GUIDE");
}

void PageInfos::tic(pages::Moteur& mo) {
  if (static_cast<int32_t>(millis() - _prochaineMaj) < 0) return;
  dessinerVariables(mo, false);
}

void PageInfos::dessinerVariables(pages::Moteur& mo, bool forcer) {
  _prochaineMaj = millis() + kPeriodeMajMs;
  static const char* const kLibelles[5] = {"Mémoire", "Minimum", "En marche", "WiFi", "Adresse"};
  char valeurs[5][40];

  snprintf(valeurs[0], 40, "%u Ko libres, bloc %u Ko", ESP.getFreeHeap() / 1024, ESP.getMaxAllocHeap() / 1024);
  snprintf(valeurs[1], 40, "%u Ko depuis le démarrage", ESP.getMinFreeHeap() / 1024);
  const unsigned long minutes = millis() / 60000UL;
  snprintf(valeurs[2], 40, "%lu h %02lu min", minutes / 60, minutes % 60);
  char nom[20];
  nomAffiche(reseau::ssid(), nom, sizeof(nom), 14);
  switch (reseau::etat()) {
    case reseau::Etat::NonConfigure: snprintf(valeurs[3], 40, "non configuré"); break;
    case reseau::Etat::Connexion: snprintf(valeurs[3], 40, "%s, connexion", nom); break;
    case reseau::Etat::Echec: snprintf(valeurs[3], 40, "%s, échec", nom); break;
    case reseau::Etat::Connecte: snprintf(valeurs[3], 40, "%s, %d dBm", nom, reseau::rssi()); break;
  }
  const String ip = reseau::ip();
  snprintf(valeurs[4], 40, "%s", ip.length() ? ip.c_str() : "-");

  // On ne réécrit que les lignes qui ont changé : à 4800 bauds, chaque octet compte.
  for (int i = 0; i < 5; ++i) {
    if (!forcer && strcmp(valeurs[i], _affiche[i]) == 0) continue;
    strlcpy(_affiche[i], valeurs[i], sizeof(_affiche[i]));
    if (forcer) charte::ligneInfo(mo.ecran(), kRangeeVariables + i, kLibelles[i], valeurs[i]);
    else {
      charte::texte(mo.ecran(), 14, kRangeeVariables + i, valeurs[i]);
      mo.ecran().cancel();
    }
  }
}

const char* PageInfos::aide() const {
  return "Version du firmware, modèle du\n"
         "Minitel, mémoire de l'ESP32 et\n"
         "état du réseau.\n"
         "\n"
         "La mémoire, la durée de marche et\n"
         "le réseau se mettent à jour\n"
         "toutes les 5 secondes.\n"
         "\n"
         "RETOUR ramène au Sommaire.";
}
