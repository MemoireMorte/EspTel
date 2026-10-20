#pragma once

// Annuaire des services (plan §9) : 30 fiches au plus, en NVS (espace
// « annuaire », une chaîne par fiche). Au premier démarrage, il est rempli
// avec les services préréglés d'iodeo.

#include <Arduino.h>

#include "../Sessions/Session.h"

namespace annuaire {

enum class Affichage : uint8_t {
  Videotex,  // les octets passent tels quels (serveurs Minitel)
  Texte40,   // UTF-8 vers ASCII, séquences ANSI filtrées
  Texte80,   // mode mixte 80 colonnes (Minitel 1B et suivants)
};

struct Fiche {
  char code[9] = "";  // majuscules, unique ; tapé au Sommaire
  char nom[25] = "";
  ses::Protocole proto = ses::Protocole::Ws;
  char hote[59] = "";
  uint16_t port = 0;  // 0 : port par défaut du protocole
  char chemin[59] = "/";
  Affichage affichage = Affichage::Videotex;
  bool echo = false;          // écho local des touches
  bool defilement = false;    // mode rouleau
  bool finTransmise = false;  // CONNEXION/FIN envoyée au serveur (sortie par Ctrl+R)
  char sousProto[16] = "";    // WebSocket
  uint16_t pingS = 0;         // WebSocket, 0 : pas de ping
  char utilisateur[23] = "";  // SSH
  char motDePasse[45] = "";   // SSH
  char cle[16] = "";          // SSH : nom d'une clé de Cles.h, essayée avant le mot de passe
  bool reconnexion = false;   // WebSocket : la session se rétablit après une coupure
  uint32_t derniere = 0;      // date du dernier appel (epoch), 0 : jamais
  uint32_t cumulS = 0;        // durée cumulée des appels

  void cible(ses::Cible& c, bool avance) const;
};

constexpr int kMax = 30;

void charger();
int nombre();
Fiche& fiche(int i);
int chercher(const char* code);  // -1 si inconnu (majuscules ou non)
// Ajoute (i < 0) ou remplace la fiche i, et enregistre. Renvoie son indice.
int enregistrer(const Fiche& f, int i);
void supprimer(int i);
void reinitialiser();  // efface tout, remet les services d'origine (P54)
void noterAppel(int i, uint32_t dureeS);  // date et cumul, après un appel

const char* nomProtocole(ses::Protocole p);  // "Telnet", "WebSocket"...
const char* abregeProtocole(ses::Protocole p);  // "TEL", "WS", "WSS", "SSH"
const char* nomAffichage(Affichage a);
Affichage affichageParDefaut(ses::Protocole p, bool avance);

// Adresse de la connexion directe : telnet://hote:port, ws://hote:port/chemin,
// wss://..., ssh://utilisateur@hote:port ; sans préfixe, telnet port 23.
// Renvoie false (et un message) si l'adresse est inutilisable.
bool lireAdresse(const char* adresse, Fiche& f, bool avance, const char** erreur);

// Contrôle d'une fiche saisie ailleurs que sur le Minitel (portail) : met le
// code en majuscules, le nom par défaut, et renvoie nullptr si elle peut être
// enregistrée à l'indice i (-1 : nouvelle), sinon la raison.
const char* verifier(Fiche& f, int i);

// Échange avec le portail web : {"format":"esptel-annuaire","services":[...]}.
// Les mots de passe SSH y sont en clair.
void exporter(String& out);
struct BilanImport {
  int ajoutees = 0;
  int remplacees = 0;  // même code qu'une fiche déjà là
  int refusees = 0;    // incorrectes, ou annuaire plein
  const char* erreur = nullptr;  // JSON illisible : rien n'est changé
  size_t position = 0;
  char premierRefus[72] = "";
};
// texte est modifié pendant la lecture. remplacerTout : l'annuaire est vidé
// avant l'import (seulement si le JSON est lisible).
bool importer(char* texte, bool remplacerTout, BilanImport& bilan);

// Noms courts des valeurs, ceux du JSON et des formulaires.
const char* cleProtocole(ses::Protocole p);  // "telnet", "ws", "wss", "ssh"
bool lireProtocole(const char* cle, ses::Protocole& p);
const char* cleAffichage(Affichage a);  // "videotex", "texte40", "texte80"
bool lireAffichage(const char* cle, Affichage& a);

}  // namespace annuaire
