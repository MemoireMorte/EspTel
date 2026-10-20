#pragma once

// Session WebSocket (ws et wss) sur arduinoWebSockets.
// La bibliothèque fonctionne par rappels : les données reçues sont mises dans
// un tampon circulaire que lire() vide. TLS sans vérification de certificat
// (comme iodeo) : la vérification viendra avec le magasin de certificats (V2).

#include <WebSocketsClient.h>

#include "Session.h"

namespace ses {

class SessionWs : public Session {
 public:
  bool connecter(const Cible& cible) override;
  int lire(uint8_t* buf, size_t taille) override;
  bool envoyer(const uint8_t* donnees, size_t n) override;
  void fermer() override;
  bool ouverte() const override { return _ouverte; }

  uint32_t octetsPerdus() const { return _perdus; }

 private:
  void evenement(WStype_t type, uint8_t* payload, size_t len);
  void empiler(const uint8_t* donnees, size_t n);
  void diagnostiquer(const Cible& cible);

  static constexpr size_t kTampon = 4096;

  WebSocketsClient* _ws = nullptr;
  volatile bool _ouverte = false;
  uint8_t _tampon[kTampon];
  size_t _tete = 0;   // prochaine écriture
  size_t _queue = 0;  // prochaine lecture
  uint32_t _perdus = 0;
  char _raison[48] = {0};  // raison donnée par la bibliothèque à la fermeture
};

}  // namespace ses
