#include "SessionWs.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace ses {

bool SessionWs::connecter(const Cible& cible) {
  fermer();
  _tete = _queue = 0;
  _perdus = 0;
  _erreur[0] = '\0';
  _raison[0] = '\0';

  _ws = new WebSocketsClient();
  _ws->onEvent([this](WStype_t type, uint8_t* payload, size_t len) { evenement(type, payload, len); });
  // On garde l'intervalle de reconnexion par défaut (500 ms) pendant l'ouverture :
  // begin() remet le dernier échec à 0, donc un intervalle géant empêcherait
  // toute tentative. Il passe à « jamais » une fois connecté (voir evenement()).

  const uint16_t port = cible.portEffectif();
  const char* chemin = cible.chemin.length() ? cible.chemin.c_str() : "/";
  const bool tls = cible.protocole == Protocole::Wss;
  // Sans sous-protocole demandé, on envoie celui par défaut de la bibliothèque
  // ("arduino"), exactement comme iodeo dont les presets fonctionnent.
  const char* protocole = cible.sousProtocole.length() ? cible.sousProtocole.c_str() : "arduino";
  if (tls) _ws->beginSSL(cible.hote.c_str(), port, chemin, "", protocole);
  else _ws->begin(cible.hote.c_str(), port, chemin, protocole);
  if (cible.pingMs > 0) _ws->enableHeartbeat(cible.pingMs, 3000, 2);

  // Comme iodeo, on laisse la bibliothèque réessayer (toutes les 500 ms) jusqu'au
  // délai : un premier échec TCP/TLS peut être passager. Un refus HTTP, lui, est définitif.
  const uint32_t t0 = millis();
  while (!_ouverte && millis() - t0 < cible.delaiConnexionMs) {
    _ws->loop();
    if (strncmp(_raison, "HTTP ", 5) == 0 || strstr(_raison, "handshake") != nullptr) break;
    if (cible.abandon && *cible.abandon) {
      fermer();
      definirErreur("abandon");
      return false;
    }
    delay(5);
  }
  if (!_ouverte) {
    fermer();
    diagnostiquer(cible);
    return false;
  }
  return true;
}

// Après un échec, refait les étapes une à une pour dire laquelle coince :
// DNS, TCP, TLS, puis la poignée de main WebSocket (raison donnée par la bibliothèque).
void SessionWs::diagnostiquer(const Cible& cible) {
  const uint16_t port = cible.portEffectif();
  const char* hote = cible.hote.c_str();
  IPAddress ip;
  if (!WiFi.hostByName(hote, ip)) {
    definirErreur("DNS : %s introuvable", hote);
    return;
  }
  if (cible.protocole == Protocole::Wss) {
    WiFiClientSecure c;
    c.setInsecure();
    if (!c.connect(hote, port, 8000)) {
      char msg[64] = {0};
      const int code = c.lastError(msg, sizeof(msg));
      definirErreur("TLS %s:%u : %s (%d)", ip.toString().c_str(), port, msg, code);
      return;
    }
    c.stop();
  } else {
    WiFiClient c;
    if (!c.connect(ip, port, 5000)) {
      definirErreur("TCP %s:%u : refusé ou injoignable", ip.toString().c_str(), port);
      return;
    }
    c.stop();
  }
  if (strcmp(_raison, "TCP connection cleanup") == 0) {
    // La bibliothèque n'a pas réussi sa connexion TLS alors que le test ci-dessus l'a eue.
    definirErreur("TLS ok au 2e essai, échec dans WebSockets");
  } else if (_raison[0]) {
    definirErreur("WebSocket : %s", _raison);  // ex. "HTTP 404", "Header response timeout"
  } else {
    definirErreur("WebSocket : pas de réponse en %lu ms", (unsigned long)cible.delaiConnexionMs);
  }
}

void SessionWs::evenement(WStype_t type, uint8_t* payload, size_t len) {
  switch (type) {
    case WStype_CONNECTED:
      _ouverte = true;
      // Pas de reconnexion automatique : une coupure termine la session (P90).
      _ws->setReconnectInterval(0x7FFFFFFF);
      break;
    case WStype_DISCONNECTED:
      if (_ouverte) definirErreur("fermée par le serveur");
      else if (len > 0) snprintf(_raison, sizeof(_raison), "%.*s", int(len), (const char*)payload);
      _ouverte = false;
      break;
    case WStype_TEXT:
    case WStype_BIN:
      empiler(payload, len);
      break;
    case WStype_ERROR:
      definirErreur("erreur WebSocket");
      break;
    default:
      break;
  }
}

void SessionWs::empiler(const uint8_t* donnees, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    const size_t suivante = (_tete + 1) % kTampon;
    if (suivante == _queue) {  // plein : le Minitel n'a pas suivi
      _perdus += n - i;
      return;
    }
    _tampon[_tete] = donnees[i];
    _tete = suivante;
  }
}

int SessionWs::lire(uint8_t* buf, size_t taille) {
  if (_ws == nullptr) return -1;
  _ws->loop();
  size_t n = 0;
  while (n < taille && _queue != _tete) {
    buf[n++] = _tampon[_queue];
    _queue = (_queue + 1) % kTampon;
  }
  if (n == 0 && !_ouverte) return -1;
  return int(n);
}

bool SessionWs::envoyer(const uint8_t* donnees, size_t n) {
  if (_ws == nullptr || !_ouverte) return false;
  // Les serveurs Vidéotex attendent les touches en trames texte (comme iodeo).
  return _ws->sendTXT(donnees, n);
}

void SessionWs::fermer() {
  _ouverte = false;  // avant disconnect() : son rappel ne doit pas écraser l'erreur
  if (_ws != nullptr) {
    _ws->disconnect();
    delete _ws;
    _ws = nullptr;
  }
}

}  // namespace ses
