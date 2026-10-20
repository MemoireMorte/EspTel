#pragma once

// Interface commune des sessions réseau (plan §10, module 3).
//
// Une session s'utilise depuis une seule tâche FreeRTOS, celle qui la crée :
//   connecter() bloque jusqu'à l'ouverture ou l'échec,
//   lire() et envoyer() ne bloquent pas (ou peu), fermer() libère tout.
// Une session fermée peut être reconnectée : c'est ce qui permet de revenir
// au Sommaire sans redémarrer l'ESP32.

#include <Arduino.h>

namespace ses {

enum class Protocole : uint8_t { Telnet, Ws, Wss, Ssh };

const char* nomProtocole(Protocole p);

struct Cible {
  Protocole protocole = Protocole::Wss;
  String hote;
  uint16_t port = 0;           // 0 : port par défaut du protocole
  String chemin = "/";         // WebSocket
  String sousProtocole;        // WebSocket, vide si aucun
  uint32_t pingMs = 0;         // WebSocket, 0 : pas de ping
  String utilisateur;          // SSH
  String motDePasse;           // SSH
  String clePrivee;            // SSH : clé OpenSSH en clair, vide si aucune
  uint16_t colonnes = 80;      // SSH : taille du pty
  uint16_t lignes = 24;
  uint32_t delaiConnexionMs = 10000;
  // Mis à true par une autre tâche pour abandonner pendant connecter().
  const volatile bool* abandon = nullptr;

  uint16_t portEffectif() const;
};

class Session {
 public:
  virtual ~Session() = default;

  virtual bool connecter(const Cible& cible) = 0;
  // Copie dans buf les octets reçus. Renvoie le nombre d'octets (0 si rien),
  // ou -1 si la session est fermée (par le serveur ou sur erreur).
  virtual int lire(uint8_t* buf, size_t taille) = 0;
  virtual bool envoyer(const uint8_t* donnees, size_t n) = 0;
  virtual void fermer() = 0;
  virtual bool ouverte() const = 0;

  // Raison du dernier échec ou de la fermeture, en clair.
  const char* erreur() const { return _erreur; }

 protected:
  void definirErreur(const char* format, ...);
  char _erreur[96] = {0};
};

// Session à clé d'hôte et identification (SSH). L'appel la mène en trois temps
// pour pouvoir demander à l'usager, entre deux, d'accepter la clé (P72) ou de
// taper son mot de passe (P71). connecter() enchaîne les trois sans rien demander.
class SessionAuth : public Session {
 public:
  enum class Auth : uint8_t { Ok, Refusee, Erreur };

  // Connexion TCP et échange de clés ; la clé d'hôte est relevée.
  virtual bool transport(const Cible& cible) = 0;
  virtual const char* typeCle() const = 0;    // "ssh-ed25519"
  virtual const char* empreinte() const = 0;  // "SHA256:..."
  // Refusee : on peut réessayer sur la même connexion. Erreur : elle est perdue.
  virtual Auth authentifier(const Cible& cible) = 0;
  // Par la clé privée de la cible, avant le mot de passe. Refusee : le mot de
  // passe peut encore être essayé sur la même connexion.
  virtual Auth authentifierCle(const Cible&) { return Auth::Refusee; }
  virtual bool ouvrirShell(const Cible& cible) = 0;

  bool connecter(const Cible& cible) override {
    if (!transport(cible)) return false;
    if (authentifier(cible) != Auth::Ok || !ouvrirShell(cible)) {
      fermer();
      return false;
    }
    return true;
  }
};

}  // namespace ses
