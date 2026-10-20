#pragma once

// Appel d'un service : connexion, session (P80), fin.
//
// La session tourne dans sa propre tâche FreeRTOS, sur une pile fixe réservée
// au démarrage (allouée à chaque appel, elle morcelait le tas). Pendant la
// connexion, la boucle principale garde le Minitel (page P70 et CONNEXION/FIN
// pour abandonner). Une fois la session ouverte, la tâche attend que la boucle
// lui cède le Minitel, puis elle seule le lit et l'écrit jusqu'à la fin.
//
// En SSH, la connexion s'arrête deux fois au besoin pour l'usager : clé d'hôte
// inconnue (P72) et mot de passe à taper ou refusé (P71). La tâche attend sa
// réponse, la boucle garde le Minitel.

#include <Arduino.h>

#include "../MinitelIO/MinitelIO.h"
#include "Annuaire.h"

namespace appel {

enum class Phase : uint8_t {
  Repos,
  Connexion,  // la tâche ouvre la session ; la boucle affiche P70
  Cle,        // SSH : clé d'hôte inconnue, réponse attendue (P72)
  Motdepasse, // SSH : mot de passe attendu (P71)
  Ouverte,    // ouverte, la tâche attend que la boucle lui cède le Minitel
  Session,    // le Minitel appartient au serveur
  Fin,        // terminé (ou échoué) ; écran et clavier remis en état
};

void initialiser(mio::MinitelIO& io);

// indice : rang dans l'annuaire, ou -1 (connexion directe).
void lancer(const annuaire::Fiche& f, int indice);
void abandonner();  // pendant Phase::Connexion
void tic();         // dans loop(), avant le moteur de pages
void liberer();     // après Phase::Fin : prêt pour l'appel suivant

Phase phase();
// La boucle ne doit plus toucher au Minitel (ni pages, ni rangée 0).
inline bool minitelCede() { return phase() == Phase::Ouverte || phase() == Phase::Session; }

const annuaire::Fiche& fiche();
int indice();
bool reussi();         // la session s'est ouverte
bool abandonne();
const char* raison();  // pourquoi la connexion a échoué, ou comment la session a fini
uint32_t dureeS();     // durée de la session
uint32_t depuisMs();   // depuis le lancement (pour l'animation de P70)

// SSH
bool identification();         // P70 : la clé est vérifiée, l'identification est en cours
bool attendReponse();          // Phase::Cle ou Phase::Motdepasse, pas encore de réponse
const char* typeCle();         // "ssh-ed25519"
const char* empreinte();       // "SHA256:..."
void repondreCle(bool accepte);
void repondreMotDePasse(const char* motDePasse);  // nullptr : identification annulée
int essai();                   // 1 au premier mot de passe demandé, 2 après un refus...
bool cleChangee();             // l'appel a échoué sur une clé différente de celle connue

}  // namespace appel
