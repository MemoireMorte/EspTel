#pragma once

// Session Telnet sur WiFiClient, avec la négociation minimale du plan (§3) :
// ECHO et SGA acceptés du serveur, NAWS (taille de l'écran) et TTYPE envoyés
// à la demande, tout le reste refusé. Les commandes IAC sont retirées du flux.

#include <WiFi.h>

#include "Session.h"

namespace ses {

class SessionTelnet : public Session {
 public:
  bool connecter(const Cible& cible) override;
  int lire(uint8_t* buf, size_t taille) override;
  bool envoyer(const uint8_t* donnees, size_t n) override;
  void fermer() override;
  bool ouverte() const override { return _ouverte; }

 private:
  void commande(uint8_t verbe, uint8_t option);
  void sousNegociation();
  void repondre(const uint8_t* d, size_t n) { _c.write(d, n); }

  enum class Etat : uint8_t { Donnees, Iac, Verbe, Sb, SbIac };

  WiFiClient _c;
  bool _ouverte = false;
  Etat _etat = Etat::Donnees;
  uint8_t _verbe = 0;
  uint8_t _sb[16];
  uint8_t _nSb = 0;
  uint16_t _colonnes = 40;
  uint16_t _lignes = 24;
  char _ttype[12] = "MINITEL";
};

}  // namespace ses
