// Aperçu du portail web sans ESP32 : les vraies routes de src/app/Portail.cpp
// derrière un serveur factice, chaque page écrite dans un fichier HTML.
//
// Depuis la racine du sketch, avec MINITEL1B le dossier de Minitel1B_Hard :
//   g++ -std=c++17 -DARDUINO=10819 -w -Itest/portail/stubs -Itest/rendu/stubs -I"$MINITEL1B"
//       test/portail/apercu.cpp "$MINITEL1B/Minitel1B_Hard.cpp" src/MinitelIO/MinitelIO.cpp
//       src/app/Portail.cpp src/app/Annuaire.cpp src/app/Json.cpp src/app/Reglages.cpp src/app/Reseau.cpp
//       src/app/Cles.cpp src/app/Fichiers.cpp src/app/Vdt.cpp src/Sessions/Session.cpp
//       -DFICHIERS_EN_MEMOIRE -o /tmp/apercu
//   /tmp/apercu /tmp/portail      (le dossier doit exister)

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include <map>
#include <string>
#include <vector>

#include "../../src/MinitelIO/MinitelIO.h"
#include "../../src/app/Annuaire.h"
#include "../../src/app/Cles.h"
#include "../../src/app/Portail.h"
#include "../../src/app/Reseau.h"

uint32_t g_ms = 0;
HardwareSerial Serial;
HardwareSerial Serial2;
EspClass ESP;
WiFiClass WiFi;
int32_t simWifiDelaiMs = 1000;
std::map<std::string, std::string> simNvs;
std::vector<Borne> simBornes = {{"MaBox", -48, WIFI_AUTH_WPA2_PSK, "motdepasse"}};

namespace {
std::string dossier;

void ecrire(const char* nom) {
  WebServer& s = *WebServer::instance;
  const std::string chemin = dossier + "/" + nom;
  FILE* f = fopen(chemin.c_str(), "w");
  fwrite(s.corps.data(), 1, s.corps.size(), f);
  fclose(f);
  printf("%-22s %d  %5zu octets  %s", nom, s.code, s.corps.size(), s.entetes.c_str());
  if (s.entetes.empty()) printf("\n");
}
}  // namespace

int main(int argc, char** argv) {
  dossier = argc > 1 ? argv[1] : ".";
  simNvs["wifi/n"] = "1";
  simNvs["wifi/s0"] = "MaBox";
  simNvs["wifi/p0"] = "motdepasse";
  simNvs["wifi/init"] = "1";
  annuaire::charger();
  reseau::demarrer();
  while (reseau::etat() != reseau::Etat::Connecte && g_ms < 20000) {
    reseau::tic();
    delay(100);
  }
  mio::MinitelIO minitel(Serial2, 16, 17);
  if (!portail::ouvrir(minitel)) {
    printf("portail fermé\n");
    return 1;
  }
  WebServer& s = *WebServer::instance;
  s.requete(HTTP_GET, "/", {});
  ecrire("accueil.html");
  s.requete(HTTP_GET, "/annuaire", {{"m", "enr"}});
  ecrire("annuaire.html");
  s.requete(HTTP_GET, "/fiche", {{"code", "PAVI"}});
  ecrire("fiche.html");
  s.requete(HTTP_POST, "/fiche", {{"ancien", ""}, {"code", "pavi"}, {"hote", "x"}, {"protocole", "ws"}});
  ecrire("fiche-erreur.html");
  s.requete(HTTP_POST, "/fiche",
            {{"ancien", ""}, {"code", "nas"}, {"nom", "NAS"}, {"url", "ssh://ben@nas.local:2222"}, {"protocole", "ws"},
             {"affichage", "videotex"}, {"echo", "1"}});
  ecrire("fiche-url.html");
  s.requete(HTTP_GET, "/annuaire.json", {});
  ecrire("annuaire.json");
  const std::string fichier = s.corps;
  s.requete(HTTP_POST, "/importer", {{"tout", "1"}}, &fichier);
  ecrire("import.html");
  const std::string casse = "{\"services\":[{\"code\":\"A\" \"hote\":\"x\"}]}";
  s.requete(HTTP_POST, "/importer", {}, &casse);
  ecrire("import-casse.html");
  s.requete(HTTP_GET, "/reglages", {});
  ecrire("reglages.html");
  s.requete(HTTP_POST, "/reglages", {{"bruit", "1"}, {"mire", "15"}, {"fin", "0"}, {"vitesse", "1200"}});
  ecrire("reglages-ok.html");
  s.requete(HTTP_POST, "/wifi/ajouter", {{"ssid", "Atelier"}, {"mdp", "court"}});
  ecrire("wifi-erreur.html");
  s.requete(HTTP_POST, "/wifi/ajouter", {{"ssid", "Atelier"}, {"mdp", "motdepasse2"}});
  ecrire("wifi.html");
  s.requete(HTTP_POST, "/cles/generer", {{"nom", "nas"}});
  ecrire("cles-generee.html");
  s.requete(HTTP_POST, "/cles/importer", {{"nom", "Mauvais Nom"}, {"texte", "x"}});
  ecrire("cles-erreur.html");
  s.requete(HTTP_POST, "/cles/importer",
            {{"nom", "pc"}, {"texte", "-----BEGIN OPENSSH PRIVATE KEY-----\r\nabc\r\n-----END OPENSSH PRIVATE KEY-----\r\n"}});
  ecrire("cles.html");
  s.requete(HTTP_GET, "/fiche", {{"code", "PAVI"}});
  ecrire("fiche.html");
  s.nomFichier = "Mire.VDT";
  const std::string page = "\x0c\x1f\x46\x4a" "MIRE";
  s.requete(HTTP_POST, "/pages/envoyer", {}, &page);
  ecrire("pages-envoyee.html");
  s.nomFichier = "1.vdt";
  s.requete(HTTP_POST, "/pages/envoyer", {}, &page);
  ecrire("pages-erreur.html");
  s.nomFichier = "meteo.vdt";
  s.requete(HTTP_POST, "/pages/envoyer", {}, &page);
  s.requete(HTTP_GET, "/pages", {});
  ecrire("pages.html");
  s.requete(HTTP_GET, "/pages/fichier", {{"nom", "MIRE"}});
  ecrire("mire.vdt");
  s.requete(HTTP_GET, "/", {});
  ecrire("accueil.html");
  s.requete(HTTP_GET, "/maj", {});
  ecrire("maj.html");
  s.requete(HTTP_GET, "/inconnu", {});
  ecrire("inconnu.html");
  printf("\nJournal :\n");
  for (int i = 0; i < portail::kLignesJournal; ++i) printf("  %s\n", portail::journal(i));
  printf("NVS reglages : mire=%s fin=%s vitesse=%s cout=%s\n", simNvs["reglages/mire"].c_str(),
         simNvs["reglages/fin"].c_str(), simNvs["reglages/vitesse"].c_str(), simNvs["reglages/cout"].c_str());
  return 0;
}
