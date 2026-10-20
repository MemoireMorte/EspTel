#include "Session.h"

#include <stdarg.h>

namespace ses {

const char* nomProtocole(Protocole p) {
  switch (p) {
    case Protocole::Telnet: return "telnet";
    case Protocole::Ws: return "ws";
    case Protocole::Wss: return "wss";
    case Protocole::Ssh: return "ssh";
  }
  return "?";
}

uint16_t Cible::portEffectif() const {
  if (port != 0) return port;
  switch (protocole) {
    case Protocole::Telnet: return 23;
    case Protocole::Ws: return 80;
    case Protocole::Wss: return 443;
    case Protocole::Ssh: return 22;
  }
  return 0;
}

void Session::definirErreur(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vsnprintf(_erreur, sizeof(_erreur), format, args);
  va_end(args);
}

}  // namespace ses
