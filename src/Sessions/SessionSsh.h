#pragma once

// Session SSH sur LibSSH-ESP32 : clé privée, mot de passe (ou
// keyboard-interactive avec le même mot de passe), pty, shell. L'empreinte SHA256 de la clé d'hôte est
// relevée à chaque connexion, pour la vérification TOFU de l'appel (P72).
//
// À utiliser depuis une tâche à grosse pile (iodeo : 51 200 octets).
// sshInitialiser() doit avoir été appelée une fois au démarrage.

#include "Session.h"

struct ssh_session_struct;
struct ssh_channel_struct;

namespace ses {

void sshInitialiser();

class SessionSsh : public SessionAuth {
 public:
  bool transport(const Cible& cible) override;
  Auth authentifier(const Cible& cible) override;
  Auth authentifierCle(const Cible& cible) override;
  bool ouvrirShell(const Cible& cible) override;
  int lire(uint8_t* buf, size_t taille) override;
  bool envoyer(const uint8_t* donnees, size_t n) override;
  void fermer() override;
  bool ouverte() const override { return _canal != nullptr; }

  // Après transport() : "ssh-ed25519", "SHA256:..." (vides si inconnues).
  const char* typeCle() const override { return _typeCle; }
  const char* empreinte() const override { return _empreinte; }

 private:
  void releverEmpreinte();

  ssh_session_struct* _session = nullptr;
  ssh_channel_struct* _canal = nullptr;
  char _typeCle[24] = {0};
  char _empreinte[64] = {0};
};

}  // namespace ses
