#include "MinitelIO.h"

#include "../config.h"

namespace mio {

namespace {

// Commandes protocole (STUM 1B, p.141)
constexpr uint8_t kEsc = 0x1B;
constexpr uint8_t kPro1 = 0x39;
constexpr uint8_t kPro2 = 0x3A;
constexpr uint8_t kPro3 = 0x3B;
constexpr uint8_t kStatusVitesse = 0x74;
constexpr uint8_t kReponseVitesse = 0x75;
constexpr uint8_t kDeconnexion = 0x67;  // PRO1 : coupe le modem (p.139)

// Comptes rendus de l'état du modem, SEP 5/3 (connecté) et SEP 5/9 (déconnecté).
constexpr uint8_t kModemConnecte = 0x53;
constexpr uint8_t kModemDeconnecte = 0x59;
constexpr uint32_t kDelaiCompteRenduMs = 2000;
// Fenêtre où tomberait la fin d'essai du modem, comptée depuis l'appui (~40 s mesurées).
constexpr uint32_t kFinEssaiMinMs = 30000;
constexpr uint32_t kFinEssaiMaxMs = 50000;

constexpr uint8_t kSep = 0x13;   // préfixe des touches de fonction
constexpr uint8_t kSs2 = 0x19;   // accès au jeu G2 (accents, symboles)
constexpr uint8_t kUs = 0x1F;
constexpr uint8_t kLf = 0x0A;
constexpr uint8_t kCan = 0x18;

// Délai au-delà duquel une séquence entamée est considérée comme terminée.
// À 1200 bauds un octet met 8,3 ms : 80 ms laisse une large marge.
constexpr uint32_t kDelaiSequenceMs = 80;

int vitesseDepuisCode(uint8_t code) {
  switch (code) {
    case 0x52: return 300;
    case 0x64: return 1200;
    case 0x76: return 4800;
    case 0x7F: return 9600;
  }
  return -1;
}

uint32_t accentue(uint8_t diacritique, uint8_t lettre) {
  switch (diacritique) {
    case 0x41:  // grave
      switch (lettre) { case 'a': return 0xE0; case 'e': return 0xE8; case 'u': return 0xF9; }
      break;
    case 0x42:  // aigu
      if (lettre == 'e') return 0xE9;
      break;
    case 0x43:  // circonflexe
      switch (lettre) {
        case 'a': return 0xE2; case 'e': return 0xEA; case 'i': return 0xEE;
        case 'o': return 0xF4; case 'u': return 0xFB;
      }
      break;
    case 0x48:  // tréma
      switch (lettre) {
        case 'a': return 0xE4; case 'e': return 0xEB; case 'i': return 0xEF;
        case 'o': return 0xF6; case 'u': return 0xFC;
      }
      break;
    case 0x4B:  // cédille
      if (lettre == 'c') return 0xE7;
      break;
  }
  return lettre;
}

uint32_t symboleG2(uint8_t c) {
  switch (c) {
    case 0x23: return 0xA3;    // £
    case 0x24: return '$';
    case 0x26: return '#';
    case 0x27: return 0xA7;    // §
    case 0x2C: return 0x2190;  // ←
    case 0x2D: return 0x2191;  // ↑
    case 0x2E: return 0x2192;  // →
    case 0x2F: return 0x2193;  // ↓
    case 0x30: return 0xB0;    // °
    case 0x31: return 0xB1;    // ±
    case 0x38: return 0xF7;    // ÷
    case 0x3C: return 0xBC;    // ¼
    case 0x3D: return 0xBD;    // ½
    case 0x3E: return 0xBE;    // ¾
    case 0x6A: return 0x152;   // Œ
    case 0x7A: return 0x153;   // œ
    case 0x7B: return 0xDF;    // ß
  }
  return 0;
}

bool estDiacritique(uint8_t c) {
  return c == 0x41 || c == 0x42 || c == 0x43 || c == 0x48 || c == 0x4B;
}

Touche toucheFonction(uint8_t c) {
  switch (c) {
    case 0x41: return Touche::Envoi;
    case 0x42: return Touche::Retour;
    case 0x43: return Touche::Repetition;
    case 0x44: return Touche::Guide;
    case 0x45: return Touche::Annulation;
    case 0x46: return Touche::Sommaire;
    case 0x47: return Touche::Correction;
    case 0x48: return Touche::Suite;
    case 0x49: return Touche::ConnexionFin;  // Fnct + CONNEXION/FIN (iodeo)
    case 0x59: return Touche::ConnexionFin;  // CONNEXION/FIN seule : le modem passe en C (relevé sur 1B)
  }
  return Touche::Aucune;
}

size_t utf8(uint32_t u, char* dest) {
  if (u < 0x80) { dest[0] = char(u); return 1; }
  if (u < 0x800) {
    dest[0] = char(0xC0 | (u >> 6));
    dest[1] = char(0x80 | (u & 0x3F));
    return 2;
  }
  dest[0] = char(0xE0 | (u >> 12));
  dest[1] = char(0x80 | ((u >> 6) & 0x3F));
  dest[2] = char(0x80 | (u & 0x3F));
  return 3;
}

}  // namespace

// ---------------------------------------------------------------- Identite

const char* Identite::modele() const {
  if (!valide) return "inconnu";
  switch (type) {
    case 0x62: case 0x63: return "Minitel 1";
    case 0x72: return "Minitel 1 Dialogue";
    case 0x73: return "Minitel 1 Couleur";
    case 0x64: case 0x66: return "Minitel 10";
    case 0x75: return "Minitel 1B";
    case 0x77: return "Minitel 10B";
    case 0x76: return "Minitel 2";
    case 0x7A: return "Minitel 12";
    case 0x79: return "Minitel 5";
  }
  return "modèle inconnu";
}

const char* Identite::nomConstructeur() const {
  if (!valide) return "?";
  switch (constructeur) {
    case 'A': return "Matra";
    case 'B': return "RTIC (Philips)";
    case 'C': return "Telic-Alcatel";
    case 'D': return "Thomson";
  }
  return "?";
}

bool Identite::estMinitel1() const {
  if (!valide) return false;
  switch (type) {
    case 0x62: case 0x63: case 0x72: case 0x73: case 0x64: case 0x66: return true;
  }
  return false;
}

int Identite::vitesseMax() const {
  if (!valide) return 4800;  // inconnu : on tente comme iodeo
  if (estMinitel1()) return 1200;
  if (type == 0x76 || type == 0x7A) return 9600;  // Minitel 2 et 12
  return 4800;
}

// ---------------------------------------------------------------- MinitelIO

MinitelIO::MinitelIO(HardwareSerial& uart, int8_t rx, int8_t tx) : _uart(uart), _rx(rx), _tx(tx) {}

void MinitelIO::viderEntree() {
  while (_uart.available() > 0) _uart.read();
}

// Envoie PRO1 STATUS_VITESSE et attend ESC 3A 75 xx. Renvoie -1 sans réponse.
int MinitelIO::sonderVitesse(uint32_t delaiMs) {
  viderEntree();
  const uint8_t demande[] = {kEsc, kPro1, kStatusVitesse};
  _uart.write(demande, sizeof(demande));
  uint32_t trame = 0;
  const uint32_t t0 = millis();
  while (millis() - t0 < delaiMs) {
    if (_uart.available() > 0) {
      trame = (trame << 8) | (uint8_t(_uart.read()) & 0x7F);
      if ((trame >> 8) == ((uint32_t(kEsc) << 16) | (uint32_t(kPro2) << 8) | kReponseVitesse)) {
        return vitesseDepuisCode(trame & 0xFF);
      }
    } else {
      delay(1);
    }
  }
  return -1;
}

int MinitelIO::detecterVitesse() {
  // Ordre d'essai : 1200 (mise sous tension), 4800 (redémarrage de l'ESP32 après
  // un passage à 4800), 9600, 300. Deux tours au plus.
  static const int kVitesses[] = {1200, 4800, 9600, 300};
  for (int tour = 0; tour < 2; ++tour) {
    for (int v : kVitesses) {
      _uart.updateBaudRate(v);
      delay(20);
      const int r = sonderVitesse(250);
      if (r > 0) {
        if (r != v) _uart.updateBaudRate(r);
        return r;
      }
    }
  }
  return -1;
}

int MinitelIO::demarrer(int vitesseVisee) {
  if (_m == nullptr) {
    _m = new Minitel(_uart, _rx, _tx);  // ouvre l'UART à 1200 bauds, 7E1
  }

  _vitesseDetectee = detecterVitesse();
  _repond = _vitesseDetectee > 0;
  if (!_repond) {
    // Pas de réponse : Minitel éteint, débranché, ou modèle muet. On reste à 1200
    // pour que l'affichage marche dès qu'il s'allume.
    _uart.updateBaudRate(1200);
    _vitesse = 1200;
    _avance = false;
    return _vitesse;
  }
  _vitesse = _vitesseDetectee;

  const uint32_t id = _m->identifyDevice();
  _identite.valide = id != 0;
  _identite.constructeur = (id >> 16) & 0xFF;
  _identite.type = (id >> 8) & 0xFF;
  _identite.version = id & 0xFF;

  int cible = min(vitesseVisee, _identite.vitesseMax());
  if (cible > _vitesse) {
    const int obtenue = _m->changeSpeed(cible);
    if (obtenue > 0) {
      _vitesse = obtenue;
    } else {
      // Changement refusé ou acquittement perdu : on retrouve la vitesse réelle.
      const int r = detecterVitesse();
      _vitesse = r > 0 ? r : 1200;
      if (r <= 0) _uart.updateBaudRate(1200);
    }
  }

  _avance = _vitesse > 1200 || (_identite.valide && !_identite.estMinitel1());
  return _vitesse;
}

bool MinitelIO::sonder(uint32_t delaiMs) { return _m != nullptr && sonderVitesse(delaiMs) > 0; }

void MinitelIO::clavierPagesLocales() {
  // Minitel1B_Hard attend l'acquittement de ces commandes sans délai : un Minitel
  // éteint ou débranché bloquerait le démarrage.
  if (_m == nullptr || !_repond) return;
  _m->echo(false);
  if (_avance) {
    _m->extendedKeyboard();
    _m->smallMode();
  }
  viderEntree();
  _n = 0;
}

void MinitelIO::ecrire(uint8_t b) { _uart.write(b); }

void MinitelIO::ecrire(const uint8_t* donnees, size_t n) { _uart.write(donnees, n); }

void MinitelIO::ligneService(const char* texteUtf8) {
  // US 0x40 0x41 : rangée 0, colonne 1 (STUM p.96)
  const uint8_t entree[] = {kUs, 0x40, 0x41};
  ecrire(entree, sizeof(entree));
  _m->print(String(texteUtf8));
  ecrire(kCan);  // efface la fin de la rangée
  ecrire(kLf);   // sortie de la rangée 0
}

bool MinitelIO::terminer(Evenement& evt, Type type, Touche touche, uint32_t unicode) {
  evt.type = type;
  evt.touche = touche;
  evt.unicode = unicode;
  evt.nbBrut = _n;
  memcpy(evt.brut, _seq, _n);
  _n = 0;
  return true;
}

// Examine la séquence en cours ; renvoie true si elle forme un événement complet.
bool MinitelIO::decoder(Evenement& evt) {
  const uint8_t premier = _seq[0];

  if (premier == kSep) {
    if (_n < 2) return false;
    const uint8_t code = _seq[1];
    if (code == 0x49 || (code & 0xF0) == 0x50) {
      DEBUG_UART.printf("[Minitel] SEP %X/%X, %lu ms après CONNEXION/FIN\n", code >> 4, code & 0x0F,
                        static_cast<unsigned long>(millis() - _appuiMs));
    }
    // Comptes rendus du modem, pas des touches : SEP 5/3 (connecté), SEP 5/9 juste après
    // la coupure demandée ci-dessous (~250 ms mesurées sur Minitel 1B). Si ce compte rendu
    // n'est pas venu, le modem essaie peut-être encore : on ignore aussi la fin de ses
    // ~40 s d'essai.
    const uint32_t depuisAppui = millis() - _appuiMs;
    const bool acquittement = code == kModemDeconnecte && _modemCoupe && depuisAppui < kDelaiCompteRenduMs;
    const bool finEssai = code == kModemDeconnecte && _modemCoupe && depuisAppui > kFinEssaiMinMs &&
                          depuisAppui < kFinEssaiMaxMs;
    if (code == kModemConnecte || acquittement || finEssai) {
      _modemCoupe = false;
      _n = 0;
      return false;
    }
    const Touche t = toucheFonction(code);
    if (t == Touche::ConnexionFin) {
      // CONNEXION/FIN met aussi le modem du Minitel en connexion (le F devient C) ; sans
      // ligne, il abandonne au bout de ~40 s et envoie SEP 5/9, lu comme un second appui.
      // On essaie de le recouper tout de suite.
      const uint8_t coupe[] = {kEsc, kPro1, kDeconnexion};
      _uart.write(coupe, sizeof(coupe));
      _modemCoupe = true;
      _appuiMs = millis();
    }
    return terminer(evt, t == Touche::Aucune ? Type::Inconnu : Type::Fonction, t);
  }

  if (premier == kSs2) {
    if (_n < 2) return false;
    if (!estDiacritique(_seq[1])) {
      const uint32_t u = symboleG2(_seq[1]);
      return terminer(evt, u ? Type::Caractere : Type::Inconnu, Touche::Aucune, u);
    }
    if (_n < 3) return false;
    return terminer(evt, Type::Caractere, Touche::Aucune, accentue(_seq[1], _seq[2]));
  }

  if (premier == kEsc) {
    if (_n < 2) return false;
    const uint8_t second = _seq[1];
    if (second == kPro1) return _n >= 3 && terminer(evt, Type::Protocole);
    if (second == kPro2) return _n >= 4 && terminer(evt, Type::Protocole);
    if (second == kPro3) return _n >= 5 && terminer(evt, Type::Protocole);
    if (second == '[') {
      if (_n < 3) return false;
      const uint8_t final = _seq[_n - 1];
      if (final < 0x40 || final > 0x7E) return false;  // paramètres CSI en cours
      Touche t = Touche::Aucune;
      if (_n == 3) {
        switch (final) {
          case 'A': t = Touche::Haut; break;
          case 'B': t = Touche::Bas; break;
          case 'C': t = Touche::Droite; break;
          case 'D': t = Touche::Gauche; break;
        }
      }
      return terminer(evt, Type::Etendu, t);
    }
    return terminer(evt, Type::Etendu);
  }

  return false;
}

bool MinitelIO::lire(Evenement& evt) {
  while (_uart.available() > 0) {
    const uint8_t b = uint8_t(_uart.read()) & 0x7F;

    if (_n == 0) {
      if (b == kSep || b == kSs2 || b == kEsc) {
        _seq[_n++] = b;
        _t0 = millis();
        continue;
      }
      _seq[_n++] = b;
      return terminer(evt, b < 0x20 || b == 0x7F ? Type::Controle : Type::Caractere, Touche::Aucune, b);
    }

    // Après un accent, une touche de fonction annule l'accent (bug n°2 de Minitel1B_Hard).
    if (_seq[0] == kSs2 && _n == 2 && estDiacritique(_seq[1]) && (b == kSep || b == kSs2 || b == kEsc)) {
      _n = 0;
      _seq[_n++] = b;
      _t0 = millis();
      continue;
    }

    _seq[_n++] = b;
    if (decoder(evt)) return true;
    if (_n >= sizeof(_seq)) return terminer(evt, Type::Inconnu);
  }

  if (_n > 0 && millis() - _t0 > kDelaiSequenceMs) {
    // Séquence tronquée. Un ESC seul est une vraie touche (Minitel 2, clavier PC).
    if (_n == 1 && _seq[0] == kEsc) return terminer(evt, Type::Controle, Touche::Aucune, kEsc);
    return terminer(evt, Type::Inconnu);
  }
  return false;
}

const char* MinitelIO::nomTouche(Touche t) {
  switch (t) {
    case Touche::Envoi: return "ENVOI";
    case Touche::Retour: return "RETOUR";
    case Touche::Repetition: return "REPETITION";  // pas de capitales accentuées sur Minitel
    case Touche::Guide: return "GUIDE";
    case Touche::Annulation: return "ANNULATION";
    case Touche::Sommaire: return "SOMMAIRE";
    case Touche::Correction: return "CORRECTION";
    case Touche::Suite: return "SUITE";
    case Touche::ConnexionFin: return "CONNEXION/FIN";
    case Touche::Haut: return "Flèche haut";
    case Touche::Bas: return "Flèche bas";
    case Touche::Droite: return "Flèche droite";
    case Touche::Gauche: return "Flèche gauche";
    case Touche::Aucune: break;
  }
  return "";
}

void MinitelIO::decrire(const Evenement& evt, char* dest, size_t taille) {
  if (taille == 0) return;
  switch (evt.type) {
    case Type::Fonction:
    case Type::Etendu:
      if (evt.touche != Touche::Aucune) {
        snprintf(dest, taille, "%s", nomTouche(evt.touche));
      } else {
        snprintf(dest, taille, "Séquence ESC");
      }
      return;
    case Type::Caractere: {
      char u[4] = {0};
      const size_t n = utf8(evt.unicode, u);
      if (evt.unicode == ' ') snprintf(dest, taille, "Espace");
      else snprintf(dest, taille, "'%.*s'", int(n), u);
      return;
    }
    case Type::Controle:
      if (evt.unicode == 0x1B) snprintf(dest, taille, "Echap");
      else if (evt.unicode == 0x7F) snprintf(dest, taille, "DEL");
      else if (evt.unicode == 0x0D) snprintf(dest, taille, "CR");
      else snprintf(dest, taille, "Ctrl+%c", char('@' + evt.unicode));
      return;
    case Type::Protocole:
      snprintf(dest, taille, "Réponse protocole");
      return;
    case Type::Inconnu:
      snprintf(dest, taille, "Inconnu");
      return;
  }
}

}  // namespace mio
