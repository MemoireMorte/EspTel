#pragma once

// WiFi et heure, sans jamais bloquer la boucle.
//
// Les réseaux connus sont mémorisés en NVS (5 au plus, le dernier utilisé en
// tête). Au démarrage, ils sont essayés dans l'ordre. Une connexion demandée
// depuis les pages (P41, P42, P43) mémorise le réseau quand elle réussit.
// Si la NVS est vide, le réseau de src/secrets.h (WIFI_SSID) l'amorce une
// fois.

#include <Arduino.h>

namespace reseau {

enum class Etat : uint8_t {
  NonConfigure,  // aucun réseau connu
  Connexion,     // tentative en cours
  Connecte,
  Echec,         // pas de connexion ; raison() dit pourquoi
};

constexpr int kMaxMemorises = 5;
constexpr int kMaxTrouves = 24;     // 3 pages de 8 en P41
constexpr size_t kTailleSsid = 33;  // 32 caractères + zéro
constexpr size_t kTailleMdp = 65;   // 64 caractères + zéro

void demarrer();
void tic();  // à appeler dans loop()

Etat etat();
const char* ssid();               // réseau connecté ou en cours d'essai
const char* raison();             // cause du dernier échec, "" sinon
int rssi();                       // dBm, 0 si non connecté
String ip();                      // "" si non connecté
bool heure(char* hhmm, size_t n);  // "12:04" une fois l'heure reçue par NTP

// Connexion à un réseau précis ; mémorisé s'il répond.
void connecter(const char* ssid, const char* mdp);
// Essaie les réseaux mémorisés dans l'ordre. false s'il n'y en a aucun.
bool reconnecter();

// Réseaux mémorisés, le plus récent en 0.
int nbMemorises();
const char* memorise(int i);
const char* motDePasse(int i);
int indiceMemorise(const char* ssid);  // -1 si inconnu
void oublier(int i);
// Portail : ajoute un réseau en fin de liste sans s'y connecter (ou change le
// mot de passe d'un réseau connu). false si la liste est pleine.
bool ajouter(const char* ssid, const char* mdp);

// Recherche des réseaux, en tâche de fond (2 à 4 secondes).
struct Trouve {
  char ssid[kTailleSsid];
  int8_t rssi;
  uint8_t secu;  // wifi_auth_mode_t
};
void lancerRecherche();
bool rechercheEnCours();
int nbTrouves();  // triés du plus fort au plus faible, un par nom
const Trouve& trouve(int i);

const char* nomSecu(uint8_t secu);  // "WPA2", "---" pour un réseau ouvert
bool secuOuverte(uint8_t secu);
bool secuEntreprise(uint8_t secu);  // 802.1X : pas géré
int barres(int rssi);               // 0 à 5

}  // namespace reseau
