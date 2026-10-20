#include "../config.h"

#include "Appel.h"

#include "Cles.h"
#include "Hotes.h"
#include "Reseau.h"

#ifndef SIMULATEUR
#include <esp_heap_caps.h>

#include "../Sessions/SessionSsh.h"
#include "../Sessions/SessionTelnet.h"
#include "../Sessions/SessionWs.h"
#endif

#ifndef APPEL_PILE
#define APPEL_PILE 51200  // SSH, ~14 Ko utilisés mesurés ; le reste en prend bien moins
#endif

// Accents par SS2 en 80 colonnes (mode mixte). Le Minitel 1B les affiche mal dans
// ce mode (vu par Ben le 2026-10-18) : 0, les lettres y perdent leur accent.
#ifndef TEXTE80_ACCENTS
#define TEXTE80_ACCENTS 0
#endif

#ifdef SIMULATEUR
ses::Session* simCreerSession(ses::Protocole p);  // dans test/rendu/simulateur.cpp
#endif

namespace appel {

namespace {

mio::MinitelIO* io = nullptr;

annuaire::Fiche ficheCourante;
int indiceCourant = -1;
ses::Cible cible;
ses::Session* session = nullptr;
bool texte80 = false;

volatile Phase phaseCourante = Phase::Repos;
volatile bool demandeAbandon = false;
volatile bool cede = false;
bool ok = false;
char raisonFin[96] = "";
uint32_t lancement = 0;
uint32_t debutSession = 0;
uint32_t duree = 0;

// SSH (P71, P72)
constexpr uint32_t kDelaiReponseMs = 110000;  // sshd coupe à 120 s sans identification
constexpr int kEssaisMax = 3;
bool transportFait = false;   // la clé est vérifiée : reste l'identification
volatile int8_t reponse = -1;  // -1 en attente, 0 non, 1 oui
uint32_t attenteDepuis = 0;
char motDePasseSaisi[48] = "";
int essais = 0;
int reconnexions = 0;  // WebSocket rétabli en cours de session
bool finPendantReconnexion = false;
bool changee = false;
bool cleEssayee = false;
char typeCleHote[24] = "";
char empreinteHote[64] = "";

#ifndef SIMULATEUR
ses::SessionTelnet sessionTelnet;
ses::SessionWs sessionWs;
ses::SessionSsh sessionSsh;

ses::Session* creerSession(ses::Protocole p) {
  switch (p) {
    case ses::Protocole::Telnet: return &sessionTelnet;
    case ses::Protocole::Ssh: return &sessionSsh;
    default: return &sessionWs;
  }
}

// Pile prise sur le tas une seule fois au démarrage, jamais rendue : pas de
// fragmentation, et la DRAM statique (~124 Ko au plus sur ESP32) reste libre.
StackType_t* pile = nullptr;
StaticTask_t tcb;
TaskHandle_t tache = nullptr;
#else
ses::Session* creerSession(ses::Protocole p) { return simCreerSession(p); }
#endif

// ------------------------------------------------------------------ traductions

// Caractère Unicode vers Vidéotex : ASCII tel quel, accents et symboles du jeu
// G2 par SS2 (0x19), capitales accentuées sans leur accent (le Minitel n'en a
// pas), cadres en +-|, sinon '?'. Renvoie le nombre d'octets écrits (3 au plus).
size_t versVideotex(uint32_t u, uint8_t* out, bool g2) {
  struct Accent {
    uint16_t u;
    uint8_t diacritique;  // 0 : symbole G2 seul
    uint8_t c;
  };
  static const Accent kG2[] = {
      {0xE0, 0x41, 'a'}, {0xE2, 0x43, 'a'}, {0xE4, 0x48, 'a'}, {0xE7, 0x4B, 'c'}, {0xE8, 0x41, 'e'},
      {0xE9, 0x42, 'e'}, {0xEA, 0x43, 'e'}, {0xEB, 0x48, 'e'}, {0xEE, 0x43, 'i'}, {0xEF, 0x48, 'i'},
      {0xF4, 0x43, 'o'}, {0xF6, 0x48, 'o'}, {0xF9, 0x41, 'u'}, {0xFB, 0x43, 'u'}, {0xFC, 0x48, 'u'},
      {0xA3, 0, 0x23},   {0xA7, 0, 0x27},   {0xB0, 0, 0x30},   {0xB1, 0, 0x31},   {0xF7, 0, 0x38},
      {0xBC, 0, 0x3C},   {0xBD, 0, 0x3D},   {0xBE, 0, 0x3E},   {0x152, 0, 0x6A},  {0x153, 0, 0x7A},
      {0xDF, 0, 0x7B},   {0x2190, 0, 0x2C}, {0x2192, 0, 0x2E}, {0x2193, 0, 0x2F},
  };
  if (u < 0x80) {
    out[0] = uint8_t(u);
    return 1;
  }
  if (g2) {
    for (const Accent& a : kG2) {
      if (a.u != u) continue;
      out[0] = 0x19;
      if (!a.diacritique) {
        out[1] = a.c;
        return 2;
      }
      out[1] = a.diacritique;
      out[2] = a.c;
      return 3;
    }
  }
  uint8_t c = '?';
  if (u >= 0x2500 && u <= 0x257F) {
    c = (u == 0x2500 || u == 0x2501 || u == 0x2550) ? '-' : (u == 0x2502 || u == 0x2503 || u == 0x2551) ? '|' : '+';
  } else {
    switch (u) {
      case 0xE0: case 0xE2: case 0xE4: c = 'a'; break;
      case 0xE8: case 0xE9: case 0xEA: case 0xEB: c = 'e'; break;
      case 0xEE: case 0xEF: c = 'i'; break;
      case 0xF4: case 0xF6: c = 'o'; break;
      case 0xF9: case 0xFB: case 0xFC: c = 'u'; break;
      case 0xE7: c = 'c'; break;
      case 0xC0: case 0xC2: case 0xC4: c = 'A'; break;
      case 0xC8: case 0xC9: case 0xCA: case 0xCB: c = 'E'; break;
      case 0xCE: case 0xCF: c = 'I'; break;
      case 0xD4: case 0xD6: c = 'O'; break;
      case 0xD9: case 0xDB: case 0xDC: c = 'U'; break;
      case 0xC7: c = 'C'; break;
      case 0xAB: case 0xBB: case 0x201C: case 0x201D: c = '"'; break;
      case 0x2018: case 0x2019: c = '\''; break;
      case 0x2013: case 0x2014: c = '-'; break;
      case 0x2026: c = '.'; break;
      case 0xA0: c = ' '; break;
    }
  }
  out[0] = c;
  return 1;
}

// Flux d'un serveur texte (Telnet, SSH) vers le Minitel : UTF-8 traduit, et les
// séquences d'échappement que le Minitel ne connaît pas retirées. En 40 colonnes
// (mode Vidéotex) restent les déplacements du curseur et les effacements ; en 80
// colonnes (mode mixte) aussi les insertions, suppressions et attributs simples
// (souligné, clignotant, inverse), sans les couleurs. Les titres de fenêtre (OSC),
// les modes privés (ESC [ ? ...) et les changements de jeu sont jetés.
struct Texte {
  enum : uint8_t { Rien, Esc, Csi, Osc, Jeu };
  uint32_t cp = 0;
  uint8_t reste = 0;
  uint8_t etat = Rien;
  uint8_t csi[16];
  uint8_t nCsi = 0;
  bool prive = false;

  void reinit() { *this = Texte(); }

  // Ne garde de ESC [ ... m que les attributs connus du mode mixte.
  static size_t attributs(const uint8_t* p, uint8_t n, uint8_t* out) {
    size_t k = 0;
    int v = 0;
    bool chiffre = false;
    for (uint8_t i = 0; i <= n; ++i) {
      if (i < n && p[i] >= '0' && p[i] <= '9') {
        v = v * 10 + (p[i] - '0');
        chiffre = true;
        continue;
      }
      if (i < n && p[i] != ';') return 0;  // forme inconnue : on jette tout
      if (!chiffre) v = 0;
      if (v == 0 || v == 4 || v == 5 || v == 7 || v == 22 || v == 24 || v == 25 || v == 27) {
        out[k++] = 0x1B;
        out[k++] = '[';
        if (v >= 10) out[k++] = uint8_t('0' + v / 10);
        out[k++] = uint8_t('0' + v % 10);
        out[k++] = 'm';
      }
      v = 0;
      chiffre = false;
    }
    return k;
  }

  void pousser(uint8_t b, bool mixte, bool g2, uint8_t* out, size_t& k) {
    switch (etat) {
      case Esc:
        etat = b == '[' ? Csi : b == ']' || b == 'P' || b == '_' || b == '^' ? Osc
             : b == '(' || b == ')' || b == '#' ? Jeu : Rien;
        nCsi = 0;
        prive = false;
        return;
      case Jeu:  // ESC ( B et consorts : un octet de plus à jeter
        etat = Rien;
        return;
      case Osc:  // jusqu'à BEL, ou ESC \ (l'ESC relance une séquence, '\' est jeté)
        if (b == 0x07) etat = Rien;
        else if (b == 0x1B) etat = Esc;
        return;
      case Csi: {
        if (b < 0x40 || b > 0x7E) {
          if (nCsi == 0 && (b == '?' || b == '>' || b == '=')) prive = true;
          if (nCsi < sizeof(csi)) csi[nCsi++] = b;
          return;
        }
        etat = Rien;  // lettre finale
        if (prive || nCsi >= sizeof(csi)) return;
        if (b == 'f') b = 'H';
        if (b == 'm') {
          if (mixte) k += attributs(csi, nCsi, out + k);
          return;
        }
        if (!strchr(mixte ? "ABCDHJKLMP@" : "ABCDHJK", b)) return;
        out[k++] = 0x1B;
        out[k++] = '[';
        for (uint8_t i = 0; i < nCsi; ++i) out[k++] = csi[i];
        out[k++] = b;
        return;
      }
      default:
        break;
    }
    if (b == 0x1B) {
      reste = 0;
      etat = Esc;
      return;
    }
    // UTF-8
    if (reste == 0) {
      if (b < 0x80) { out[k++] = b; return; }
      if ((b & 0xE0) == 0xC0) { cp = b & 0x1F; reste = 1; return; }
      if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; reste = 2; return; }
      if ((b & 0xF8) == 0xF0) { cp = b & 0x07; reste = 3; return; }
      out[k++] = '?';
      return;
    }
    if ((b & 0xC0) != 0x80) {
      reste = 0;
      pousser(b, mixte, g2, out, k);
      return;
    }
    cp = (cp << 6) | (b & 0x3F);
    if (--reste == 0) k += versVideotex(cp, out + k, g2);
  }
};

Texte texte;

void versMinitel(const uint8_t* buf, int n) {
  if (ficheCourante.affichage == annuaire::Affichage::Videotex) {
    io->ecrire(buf, size_t(n));
    return;
  }
  const bool g2 = !texte80 || TEXTE80_ACCENTS;
  uint8_t sortie[300];
  size_t k = 0;
  for (int i = 0; i < n; ++i) {
    texte.pousser(buf[i], texte80, g2, sortie, k);
    if (k > sizeof(sortie) - 48) {  // une séquence d'attributs fait 40 octets au plus
      io->ecrire(sortie, k);
      k = 0;
    }
  }
  if (k) io->ecrire(sortie, k);
}

// Clavier Minitel vers terminal texte (plan §5, colonne Telnet/SSH).
size_t clavierVersTexte(const mio::Evenement& evt, uint8_t* out) {
  using mio::Touche;
  switch (evt.type) {
    case mio::Type::Fonction:
      switch (evt.touche) {
        case Touche::Envoi: out[0] = '\r'; return 1;
        case Touche::Correction: out[0] = 0x7F; return 1;
        case Touche::Annulation: out[0] = 0x15; return 1;  // Ctrl+U
        case Touche::Repetition: out[0] = 0x0C; return 1;  // Ctrl+L : réaffiche
        case Touche::Suite: memcpy(out, "\x1b[6~", 4); return 4;
        case Touche::Retour: memcpy(out, "\x1b[5~", 4); return 4;
        default: return 0;  // SOMMAIRE, GUIDE : ignorées pour l'instant
      }
    case mio::Type::Caractere: {
      const uint32_t u = evt.unicode;
      if (u < 0x80) { out[0] = uint8_t(u); return 1; }
      if (u < 0x800) { out[0] = 0xC0 | (u >> 6); out[1] = 0x80 | (u & 0x3F); return 2; }
      out[0] = 0xE0 | (u >> 12); out[1] = 0x80 | ((u >> 6) & 0x3F); out[2] = 0x80 | (u & 0x3F);
      return 3;
    }
    case mio::Type::Controle:
    case mio::Type::Etendu:  // flèches : ESC [ A..D, déjà au format VT100
      memcpy(out, evt.brut, evt.nbBrut);
      return evt.nbBrut;
    default:
      return 0;
  }
}

// ------------------------------------------------------------------ écran

// Les commandes de Minitel1B_Hard attendent leur acquittement : on vide d'abord
// l'entrée (touches en attente, compte rendu du modem après CONNEXION/FIN).
void viderEntree() {
  mio::Evenement reste;
  while (io->lire(reste)) {
  }
}

void preparerEcran() {
  Minitel& m = io->ecran();
  viderEntree();
  io->ligneService("");
  m.newScreen();
  if (io->repond()) {
    // Écho imposé dans les deux sens : un Minitel resté en écho (allumé après
    // l'ESP32, serveur précédent) doublerait chaque touche renvoyée par le serveur.
    m.echo(ficheCourante.echo);
    if (texte80) {
      m.modeMixte();
      m.newScreen();
    } else if (ficheCourante.defilement) {
      m.scrollMode();
    }
    // Comme iodeo pour les fiches Vidéotex de l'annuaire : un serveur Minitel attend
    // les majuscules, Maj + lettre pour les minuscules. En connexion directe (indice
    // -1), le clavier reste celui des pages locales (Maj + lettre = majuscule) : on y
    // tape souvent un identifiant et un mot de passe, une casse inversée les fausse.
    if (ficheCourante.affichage == annuaire::Affichage::Videotex && io->avance() &&
        indiceCourant >= 0)
      m.capitalMode();
  }
  m.cursor();
}

void restaurerEcran() {
  Minitel& m = io->ecran();
  viderEntree();
  if (io->repond()) {
    if (texte80) m.modeVideotex();
    else if (ficheCourante.defilement) m.pageMode();
  }
  // Un serveur Vidéotex a pu changer le jeu de caractères, les attributs, le clavier.
  const uint8_t g0[] = {0x1B, 0x28, 0x40};
  io->ecrire(g0, sizeof(g0));
  m.textMode();
  m.attributs(FIXE);
  m.noCursor();
  m.newScreen();
  io->clavierPagesLocales();
}

// ------------------------------------------------------------------ déroulement

enum class Pas : uint8_t { Continuer, Attendre, Fini };

void terminer(bool reussite, const char* pourquoi) {
  ok = reussite;
  strlcpy(raisonFin, pourquoi, sizeof(raisonFin));
  if (session) session->fermer();
  phaseCourante = Phase::Fin;
  DEBUG_UART.printf("[Appel] %s : %s (%lu s)\n", ficheCourante.code, raisonFin, (unsigned long)duree);
}

void attendre(Phase p) {
  reponse = -1;
  attenteDepuis = millis();
  phaseCourante = p;
}

// Connexion SSH : transport et clé d'hôte, puis identification. Chaque appel
// reprend là où la réponse de l'usager l'a laissée.
Pas etapeSsh() {
  ses::SessionAuth* ssh = static_cast<ses::SessionAuth*>(session);
  if (!transportFait) {
    session = creerSession(ses::Protocole::Ssh);
    ssh = static_cast<ses::SessionAuth*>(session);
    const bool ouverte = ssh->transport(cible);
    if (demandeAbandon) {
      terminer(false, "abandon");
      return Pas::Fini;
    }
    if (!ouverte) {
      terminer(false, ssh->erreur());
      return Pas::Fini;
    }
    transportFait = true;
    strlcpy(typeCleHote, ssh->typeCle(), sizeof(typeCleHote));
    strlcpy(empreinteHote, ssh->empreinte(), sizeof(empreinteHote));
    if (!empreinteHote[0]) {
      terminer(false, "clé de l'hôte illisible");
      return Pas::Fini;
    }
    switch (hotes::verifier(ficheCourante.hote, cible.portEffectif(), empreinteHote)) {
      case hotes::Verdict::Change:
        changee = true;
        DEBUG_UART.printf("[Appel] %s : clé changée, %s\n", ficheCourante.code, empreinteHote);
        terminer(false, "la clé de l'hôte a changé : appel bloqué");
        return Pas::Fini;
      case hotes::Verdict::Inconnu:
        attendre(Phase::Cle);
        return Pas::Attendre;
      case hotes::Verdict::Connu:
        break;
    }
  }

  // La clé de la fiche d'abord, une seule fois ; refusée, on passe au mot de passe.
  if (!cleEssayee && ficheCourante.cle[0]) {
    cleEssayee = true;
    if (!cles::lire(ficheCourante.cle, cible.clePrivee)) {
      DEBUG_UART.printf("[Appel] %s : clé %s absente, mot de passe\n", ficheCourante.code, ficheCourante.cle);
    } else {
      const ses::SessionAuth::Auth a = ssh->authentifierCle(cible);
      cible.clePrivee = "";
      if (demandeAbandon) {
        terminer(false, "abandon");
        return Pas::Fini;
      }
      if (a == ses::SessionAuth::Auth::Ok) {
        DEBUG_UART.printf("[Appel] %s : identifié par la clé %s\n", ficheCourante.code, ficheCourante.cle);
        if (!ssh->ouvrirShell(cible)) {
          terminer(false, ssh->erreur());
          return Pas::Fini;
        }
        phaseCourante = Phase::Ouverte;
        return Pas::Attendre;
      }
      if (a == ses::SessionAuth::Auth::Erreur) {
        terminer(false, ssh->erreur());
        return Pas::Fini;
      }
      DEBUG_UART.printf("[Appel] %s : %s, mot de passe\n", ficheCourante.code, ssh->erreur());
    }
  }

  if (cible.motDePasse.length() == 0) {
    ++essais;
    attendre(Phase::Motdepasse);
    return Pas::Attendre;
  }
  const ses::SessionAuth::Auth a = ssh->authentifier(cible);
  if (demandeAbandon) {
    terminer(false, "abandon");
    return Pas::Fini;
  }
  if (a == ses::SessionAuth::Auth::Refusee) {
    cible.motDePasse = "";
    if (essais >= kEssaisMax) {
      terminer(false, "mot de passe refusé");
      return Pas::Fini;
    }
    if (essais == 0) essais = 1;  // le mot de passe de la fiche comptait pour un essai
    ++essais;
    attendre(Phase::Motdepasse);
    return Pas::Attendre;
  }
  if (a == ses::SessionAuth::Auth::Erreur || !ssh->ouvrirShell(cible)) {
    terminer(false, ssh->erreur());
    return Pas::Fini;
  }
  phaseCourante = Phase::Ouverte;
  return Pas::Attendre;
}

// WebSocket coupé, fiche « reconnexion » : jusqu'à trois nouvelles connexions,
// annoncées en rangée 0. Le serveur renvoie sa page d'accueil. CONNEXION/FIN
// entre deux essais renonce. false : la session finit pour de bon.
constexpr int kReconnexionsMax = 3;
const uint32_t kAttentesMs[kReconnexionsMax] = {1000, 4000, 10000};

bool reconnecter() {
  const bool ws = ficheCourante.proto == ses::Protocole::Ws || ficheCourante.proto == ses::Protocole::Wss;
  if (!ficheCourante.reconnexion || !ws) return false;
  DEBUG_UART.printf("[Appel] %s : %s, reconnexion\n", ficheCourante.code, session->erreur());
  for (int i = 0; i < kReconnexionsMax; ++i) {
    char t[40];
    snprintf(t, sizeof(t), "Liaison perdue : reconnexion %d/%d", i + 1, kReconnexionsMax);
    io->ligneService(t);
    const uint32_t debut = millis();
    while (millis() - debut < kAttentesMs[i]) {
      mio::Evenement evt;
      while (io->lire(evt)) {
        if (evt.est(mio::Touche::ConnexionFin)) {
          finPendantReconnexion = true;
          return false;
        }
      }
      delay(20);
    }
    if (reseau::etat() != reseau::Etat::Connecte) continue;  // le WiFi revient d'abord
    if (session->connecter(cible)) {
      ++reconnexions;
      io->ligneService("");
      DEBUG_UART.printf("[Appel] %s : reconnecté (%d)\n", ficheCourante.code, reconnexions);
      return true;
    }
  }
  return false;
}

// Un pas de l'appel. La tâche (ou le simulateur) le répète jusqu'à Fini.
Pas etape() {
  switch (phaseCourante) {
    case Phase::Connexion: {
      if (ficheCourante.proto == ses::Protocole::Ssh) return etapeSsh();
      session = creerSession(ficheCourante.proto);
      const bool ouverte = session->connecter(cible);
      if (demandeAbandon) {
        terminer(false, "abandon");
        return Pas::Fini;
      }
      if (!ouverte) {
        terminer(false, session->erreur());
        return Pas::Fini;
      }
      phaseCourante = Phase::Ouverte;
      return Pas::Attendre;
    }

    case Phase::Cle:
    case Phase::Motdepasse: {
      if (demandeAbandon) {
        terminer(false, "abandon");
        return Pas::Fini;
      }
      if (reponse < 0) {
        if (millis() - attenteDepuis < kDelaiReponseMs) return Pas::Attendre;
        terminer(false, "pas de réponse, appel abandonné");
        return Pas::Fini;
      }
      if (reponse == 0) {
        terminer(false, phaseCourante == Phase::Cle ? "clé de l'hôte refusée" : "identification annulée");
        return Pas::Fini;
      }
      if (phaseCourante == Phase::Cle) {
        hotes::memoriser(ficheCourante.hote, cible.portEffectif(), empreinteHote);
        DEBUG_UART.printf("[Appel] %s : clé %s acceptée\n", ficheCourante.code, empreinteHote);
      } else {
        cible.motDePasse = motDePasseSaisi;
        memset(motDePasseSaisi, 0, sizeof(motDePasseSaisi));
      }
      phaseCourante = Phase::Connexion;
      return Pas::Continuer;
    }

    case Phase::Ouverte:
      if (!cede) return Pas::Attendre;
      phaseCourante = Phase::Session;
      texte.reinit();
      preparerEcran();
      debutSession = millis();
      return Pas::Continuer;

    case Phase::Session: {
      uint8_t buf[512];
      const int n = session->lire(buf, sizeof(buf));
      if (n < 0 && reconnecter()) return Pas::Continuer;
      if (n < 0) {
        duree = (millis() - debutSession) / 1000;
        char pourquoi[96];
        if (finPendantReconnexion) strlcpy(pourquoi, "CONNEXION/FIN", sizeof(pourquoi));
        else if (reconnexions) snprintf(pourquoi, sizeof(pourquoi), "%.60s, après %d reconnexion%s", session->erreur(), reconnexions,
                                    reconnexions > 1 ? "s" : "");
        else strlcpy(pourquoi, session->erreur(), sizeof(pourquoi));
        restaurerEcran();
        terminer(true, pourquoi);
        return Pas::Fini;
      }
      if (n > 0) versMinitel(buf, n);

      bool touche = false;
      mio::Evenement evt;
      while (io->lire(evt)) {
        touche = true;
        const bool fin = ficheCourante.finTransmise
                             ? (evt.type == mio::Type::Controle && evt.unicode == 0x12)  // Ctrl+R
                             : evt.est(mio::Touche::ConnexionFin);
        if (fin) {
          duree = (millis() - debutSession) / 1000;
          restaurerEcran();
          terminer(true, "CONNEXION/FIN");
          return Pas::Fini;
        }
        uint8_t out[16];
        size_t len;
        if (ficheCourante.affichage == annuaire::Affichage::Videotex) {
          memcpy(out, evt.brut, evt.nbBrut);  // tout passe tel quel, réponses PRO comprises
          len = evt.nbBrut;
        } else {
          len = clavierVersTexte(evt, out);
          // Touches hors ASCII simple (accents, inconnues) : octets reçus et envoyés,
          // pour mettre au point le clavier en mode texte. Les lettres ASCII d'un mot
          // de passe tapé dans le shell ne passent jamais ici.
          if (!(evt.type == mio::Type::Caractere && evt.unicode < 0x80) && evt.type != mio::Type::Fonction &&
              evt.type != mio::Type::Controle) {
            char h[80];
            size_t k = 0;
            for (int i = 0; i < evt.nbBrut && k + 4 < sizeof(h); ++i) k += snprintf(h + k, sizeof(h) - k, "%02X ", evt.brut[i]);
            k += snprintf(h + k, sizeof(h) - k, "->");
            for (size_t i = 0; i < len && k + 4 < sizeof(h); ++i) k += snprintf(h + k, sizeof(h) - k, " %02X", out[i]);
            DEBUG_UART.printf("[Appel] touche %s (U+%04lX)\n", h, static_cast<unsigned long>(evt.unicode));
          }
        }
        if (len) session->envoyer(out, len);
      }
      return (n == 0 && !touche) ? Pas::Attendre : Pas::Continuer;
    }

    default:
      return Pas::Fini;
  }
}

#ifndef SIMULATEUR
void tacheAppel(void*) {
  while (true) {
    const Pas p = etape();
    if (p == Pas::Fini) break;
    if (p == Pas::Attendre) vTaskDelay(pdMS_TO_TICKS(5));
  }
  // Pile et TCB statiques : la tâche s'arrête, la boucle la supprimera.
  vTaskSuspend(nullptr);
}
#endif

}  // namespace

void initialiser(mio::MinitelIO& m) {
  io = &m;
#ifndef SIMULATEUR
  if (!pile) {
    pile = static_cast<StackType_t*>(heap_caps_malloc(APPEL_PILE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }
  if (!pile) DEBUG_UART.println("[Appel] pile de session impossible à allouer");
  ses::sshInitialiser();
#endif
}

void lancer(const annuaire::Fiche& f, int i) {
  liberer();
  ficheCourante = f;
  indiceCourant = i;
  texte80 = f.affichage == annuaire::Affichage::Texte80 && io->avance();
  f.cible(cible, io->avance());
  cible.abandon = &demandeAbandon;
  demandeAbandon = false;
  cede = false;
  ok = false;
  duree = 0;
  raisonFin[0] = '\0';
  transportFait = false;
  reponse = -1;
  essais = 0;
  changee = false;
  cleEssayee = false;
  reconnexions = 0;
  finPendantReconnexion = false;
  typeCleHote[0] = '\0';
  empreinteHote[0] = '\0';
  lancement = millis();
  phaseCourante = Phase::Connexion;
  DEBUG_UART.printf("[Appel] %s : %s://%s:%u%s\n", f.code, ses::nomProtocole(f.proto), f.hote,
                    cible.portEffectif(), f.chemin);

  if (reseau::etat() != reseau::Etat::Connecte) {
    terminer(false, "WiFi non connecté");
    return;
  }
#ifndef SIMULATEUR
  if (!pile) {
    terminer(false, "mémoire insuffisante");
    return;
  }
  tache = xTaskCreateStaticPinnedToCore(tacheAppel, "appel", APPEL_PILE, nullptr, 2, pile, &tcb,
                                        ARDUINO_RUNNING_CORE);
  if (!tache) terminer(false, "tâche impossible à créer");
#endif
}

void abandonner() { demandeAbandon = true; }

void tic() {
  // La boucle est à un point sûr : elle cède le Minitel à la session ouverte.
  if (phaseCourante == Phase::Ouverte) cede = true;
#ifdef SIMULATEUR
  if (phaseCourante != Phase::Repos && phaseCourante != Phase::Fin) etape();
#endif
}

void liberer() {
#ifndef SIMULATEUR
  if (tache) {
    while (eTaskGetState(tache) != eSuspended) delay(1);
    vTaskDelete(tache);
    tache = nullptr;
  }
#endif
  session = nullptr;
  phaseCourante = Phase::Repos;
}

Phase phase() { return phaseCourante; }
const annuaire::Fiche& fiche() { return ficheCourante; }
int indice() { return indiceCourant; }
bool reussi() { return ok; }
bool abandonne() { return demandeAbandon; }
const char* raison() { return raisonFin; }
uint32_t dureeS() { return duree; }
uint32_t depuisMs() { return millis() - lancement; }

bool identification() { return transportFait && phaseCourante == Phase::Connexion; }
bool attendReponse() {
  return (phaseCourante == Phase::Cle || phaseCourante == Phase::Motdepasse) && reponse < 0;
}
const char* typeCle() { return typeCleHote; }
const char* empreinte() { return empreinteHote; }
void repondreCle(bool accepte) {
  if (phaseCourante == Phase::Cle) reponse = accepte ? 1 : 0;
}
void repondreMotDePasse(const char* motDePasse) {
  if (phaseCourante != Phase::Motdepasse) return;
  if (motDePasse) strlcpy(motDePasseSaisi, motDePasse, sizeof(motDePasseSaisi));
  reponse = motDePasse ? 1 : 0;
}
int essai() { return essais; }
bool cleChangee() { return changee; }

}  // namespace appel
