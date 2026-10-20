#include "SessionTelnet.h"

namespace ses {

namespace {

constexpr uint8_t kIac = 255, kDont = 254, kDo = 253, kWont = 252, kWill = 251, kSb = 250, kSe = 240;
constexpr uint8_t kOptEcho = 1, kOptSga = 3, kOptTtype = 24, kOptNaws = 31;

}  // namespace

bool SessionTelnet::connecter(const Cible& cible) {
  fermer();
  _erreur[0] = '\0';
  _etat = Etat::Donnees;
  _colonnes = cible.colonnes;
  _lignes = cible.lignes;
  // Un serveur Vidéotex en Telnet reconnaît le Minitel ; un BBS texte attend un terminal.
  strlcpy(_ttype, cible.colonnes == 80 ? "VT100" : "MINITEL", sizeof(_ttype));

  const uint16_t port = cible.portEffectif();
  IPAddress ip;
  if (!WiFi.hostByName(cible.hote.c_str(), ip)) {
    definirErreur("DNS : %s introuvable", cible.hote.c_str());
    return false;
  }
  if (cible.abandon && *cible.abandon) {
    definirErreur("abandon");
    return false;
  }
  if (!_c.connect(ip, port, static_cast<int32_t>(cible.delaiConnexionMs))) {
    definirErreur("TCP %s:%u : refusé ou injoignable", ip.toString().c_str(), port);
    return false;
  }
  _c.setNoDelay(true);
  _ouverte = true;
  return true;
}

void SessionTelnet::commande(uint8_t verbe, uint8_t option) {
  uint8_t r[3] = {kIac, 0, option};
  if (verbe == kWill) {  // le serveur propose de faire
    r[1] = (option == kOptEcho || option == kOptSga) ? kDo : kDont;
  } else if (verbe == kDo) {  // le serveur nous demande de faire
    if (option == kOptSga || option == kOptTtype || option == kOptNaws) {
      r[1] = kWill;
      repondre(r, 3);
      if (option == kOptNaws) {
        const uint8_t naws[] = {kIac, kSb, kOptNaws, uint8_t(_colonnes >> 8), uint8_t(_colonnes & 0xFF),
                                uint8_t(_lignes >> 8), uint8_t(_lignes & 0xFF), kIac, kSe};
        repondre(naws, sizeof(naws));
      }
      return;
    }
    r[1] = kWont;
  } else {
    return;  // WONT et DONT : rien à répondre
  }
  repondre(r, 3);
}

void SessionTelnet::sousNegociation() {
  // TTYPE SEND : on répond IS <nom>.
  if (_nSb >= 2 && _sb[0] == kOptTtype && _sb[1] == 1) {
    uint8_t r[24] = {kIac, kSb, kOptTtype, 0};
    size_t n = 4;
    for (const char* p = _ttype; *p && n < sizeof(r) - 2; ++p) r[n++] = uint8_t(*p);
    r[n++] = kIac;
    r[n++] = kSe;
    repondre(r, n);
  }
}

int SessionTelnet::lire(uint8_t* buf, size_t taille) {
  if (!_ouverte) return -1;
  size_t n = 0;
  uint8_t brut[256];
  const int dispo = _c.available();
  if (dispo <= 0) {
    if (!_c.connected()) {
      _ouverte = false;
      definirErreur("fermée par le serveur");
      return -1;
    }
    return 0;
  }
  const int lus = _c.read(brut, min<size_t>(sizeof(brut), taille));
  for (int i = 0; i < lus; ++i) {
    const uint8_t b = brut[i];
    switch (_etat) {
      case Etat::Donnees:
        if (b == kIac) _etat = Etat::Iac;
        else buf[n++] = b;
        break;
      case Etat::Iac:
        if (b == kIac) {
          buf[n++] = b;
          _etat = Etat::Donnees;
        } else if (b == kWill || b == kWont || b == kDo || b == kDont) {
          _verbe = b;
          _etat = Etat::Verbe;
        } else if (b == kSb) {
          _nSb = 0;
          _etat = Etat::Sb;
        } else {
          _etat = Etat::Donnees;  // NOP, GA... ignorées
        }
        break;
      case Etat::Verbe:
        commande(_verbe, b);
        _etat = Etat::Donnees;
        break;
      case Etat::Sb:
        if (b == kIac) _etat = Etat::SbIac;
        else if (_nSb < sizeof(_sb)) _sb[_nSb++] = b;
        break;
      case Etat::SbIac:
        if (b == kSe) {
          sousNegociation();
          _etat = Etat::Donnees;
        } else {
          if (_nSb < sizeof(_sb)) _sb[_nSb++] = b;
          _etat = Etat::Sb;
        }
        break;
    }
  }
  return int(n);
}

bool SessionTelnet::envoyer(const uint8_t* donnees, size_t n) {
  if (!_ouverte) return false;
  for (size_t i = 0; i < n; ++i) {
    if (donnees[i] == kIac) _c.write(kIac);  // 0xFF doublé
    _c.write(donnees[i]);
  }
  return true;
}

void SessionTelnet::fermer() {
  _ouverte = false;
  _c.stop();
}

}  // namespace ses
