#include "../config.h"

#include "Annuaire.h"

#include <Preferences.h>

#include "Cles.h"
#include "Json.h"

namespace annuaire {

namespace {

Fiche* fiches = nullptr;  // sur le tas (7,5 Ko), alloué une fois par charger()
int nb = 0;

// Version 2 : clé SSH et reconnexion en plus ; la version 1 se lit toujours.
constexpr char kVersion = '2';

// Services préréglés d'iodeo (Minitel1B_Telnet_Pro), adresses de 2022 : certains
// ont pu fermer depuis. TELSTAR (Prestel) n'est pas repris.
void preregler() {
  struct Preset {
    const char* code;
    const char* nom;
    ses::Protocole proto;
    const char* hote;
    uint16_t port;
    const char* chemin;
    const char* sousProto;
    uint16_t pingS;
    bool defilement;
  };
  static const Preset kPresets[] = {
      {"TEASER", "3614 Teaser", ses::Protocole::Ws, "minitel.3614teaser.fr", 8080, "/ws", "tty", 10, false},
      {"HACKER", "3614 Hacker", ses::Protocole::Ws, "mntl.joher.com", 2018, "/?echo", "", 0, false},
      {"PAVI", "MiniPavi", ses::Protocole::Ws, "go.minipavi.fr", 8182, "/", "", 0, false},
      {"RETRO", "Retrocampus BBS", ses::Protocole::Telnet, "bbs.retrocampus.com", 1651, "/", "", 0, true},
      {"3611", "3611.re", ses::Protocole::Ws, "3611.re", 0, "/ws", "", 0, false},
      {"CODE", "3615co.de", ses::Protocole::Ws, "3615co.de", 0, "/ws", "", 0, false},
      {"SM", "3615 SM", ses::Protocole::Wss, "wss.3615.live", 9991, "/?echo", "", 0, false},
  };
  nb = 0;
  for (const Preset& p : kPresets) {
    Fiche& f = fiches[nb++];
    f = Fiche();
    strlcpy(f.code, p.code, sizeof(f.code));
    strlcpy(f.nom, p.nom, sizeof(f.nom));
    f.proto = p.proto;
    strlcpy(f.hote, p.hote, sizeof(f.hote));
    f.port = p.port;
    strlcpy(f.chemin, p.chemin, sizeof(f.chemin));
    strlcpy(f.sousProto, p.sousProto, sizeof(f.sousProto));
    f.pingS = p.pingS;
    f.defilement = p.defilement;
    f.affichage = Affichage::Videotex;  // iodeo passe tout tel quel, Telnet compris
  }
}

// Une fiche par chaîne, champs séparés par des tabulations (les champs de
// saisie n'acceptent que l'ASCII imprimable, donc jamais de tabulation).
void serialiser(const Fiche& f, char* s, size_t n) {
  snprintf(s, n, "%c\t%s\t%s\t%u\t%s\t%u\t%s\t%u\t%u\t%u\t%u\t%s\t%u\t%s\t%s\t%lu\t%lu\t%s\t%u", kVersion, f.code,
           f.nom, unsigned(f.proto), f.hote, unsigned(f.port), f.chemin, unsigned(f.affichage), unsigned(f.echo),
           unsigned(f.defilement), unsigned(f.finTransmise), f.sousProto, unsigned(f.pingS), f.utilisateur,
           f.motDePasse, (unsigned long)f.derniere, (unsigned long)f.cumulS, f.cle, unsigned(f.reconnexion));
}

bool deserialiser(char* s, Fiche& f) {
  char* champs[19];
  int k = 0;
  champs[k++] = s;
  for (char* p = s; *p && k < 19; ++p) {
    if (*p == '\t') {
      *p = '\0';
      champs[k++] = p + 1;
    }
  }
  const bool v1 = champs[0][0] == '1' && k >= 17;
  const bool v2 = champs[0][0] == '2' && k >= 19;
  if (!v1 && !v2) return false;
  f = Fiche();
  strlcpy(f.code, champs[1], sizeof(f.code));
  strlcpy(f.nom, champs[2], sizeof(f.nom));
  f.proto = static_cast<ses::Protocole>(atoi(champs[3]) & 3);
  strlcpy(f.hote, champs[4], sizeof(f.hote));
  f.port = static_cast<uint16_t>(atoi(champs[5]));
  strlcpy(f.chemin, champs[6], sizeof(f.chemin));
  f.affichage = static_cast<Affichage>(atoi(champs[7]) % 3);
  f.echo = atoi(champs[8]);
  f.defilement = atoi(champs[9]);
  f.finTransmise = atoi(champs[10]);
  strlcpy(f.sousProto, champs[11], sizeof(f.sousProto));
  f.pingS = static_cast<uint16_t>(atoi(champs[12]));
  strlcpy(f.utilisateur, champs[13], sizeof(f.utilisateur));
  strlcpy(f.motDePasse, champs[14], sizeof(f.motDePasse));
  f.derniere = strtoul(champs[15], nullptr, 10);
  f.cumulS = strtoul(champs[16], nullptr, 10);
  if (v2) {
    strlcpy(f.cle, champs[17], sizeof(f.cle));
    f.reconnexion = atoi(champs[18]);
  }
  return f.code[0] != '\0';
}

void sauver() {
  Preferences p;
  p.begin("annuaire", false);
  char cle[8];
  char s[360];
  for (int i = 0; i < kMax; ++i) {
    snprintf(cle, sizeof(cle), "f%d", i);
    if (i < nb) {
      serialiser(fiches[i], s, sizeof(s));
      p.putString(cle, s);
    } else {
      p.remove(cle);
    }
  }
  p.putUChar("n", nb);
  p.end();
}

}  // namespace

void Fiche::cible(ses::Cible& c, bool avance) const {
  c = ses::Cible();
  c.protocole = proto;
  c.hote = hote;
  c.port = port;
  c.chemin = chemin[0] ? chemin : "/";
  c.sousProtocole = sousProto;
  c.pingMs = uint32_t(pingS) * 1000;
  c.utilisateur = utilisateur;
  c.motDePasse = motDePasse;
  c.colonnes = (affichage == Affichage::Texte80 && avance) ? 80 : 40;
  c.lignes = 24;
}

void charger() {
  if (!fiches) fiches = new Fiche[kMax];
  Preferences p;
  p.begin("annuaire", true);
  const int n = p.getUChar("n", 0xFF);
  nb = 0;
  if (n != 0xFF) {
    char cle[8];
    char s[360];
    for (int i = 0; i < n && i < kMax; ++i) {
      snprintf(cle, sizeof(cle), "f%d", i);
      if (p.getString(cle, s, sizeof(s)) && deserialiser(s, fiches[nb])) ++nb;
    }
  }
  p.end();
  if (n == 0xFF) {  // jamais enregistré : premier démarrage
    preregler();
    sauver();
  }
}

int nombre() { return nb; }
Fiche& fiche(int i) { return fiches[i < 0 ? 0 : i >= kMax ? kMax - 1 : i]; }

int chercher(const char* code) {
  for (int i = 0; i < nb; ++i) {
    if (strcasecmp(fiches[i].code, code) == 0) return i;
  }
  return -1;
}

int enregistrer(const Fiche& f, int i) {
  if (i < 0 || i >= nb) {
    if (nb == kMax) return -1;
    i = nb++;
  }
  fiches[i] = f;
  sauver();
  return i;
}

void reinitialiser() {
  Preferences p;
  p.begin("annuaire", false);
  p.clear();
  p.end();
  nb = 0;
  preregler();
  sauver();
}

void supprimer(int i) {
  if (i < 0 || i >= nb) return;
  for (; i + 1 < nb; ++i) fiches[i] = fiches[i + 1];
  --nb;
  sauver();
}

void noterAppel(int i, uint32_t dureeS) {
  if (i < 0 || i >= nb) return;
  const time_t t = time(nullptr);
  if (t > 1700000000) fiches[i].derniere = uint32_t(t);
  fiches[i].cumulS += dureeS;
  sauver();
}

const char* nomProtocole(ses::Protocole p) {
  switch (p) {
    case ses::Protocole::Telnet: return "Telnet";
    case ses::Protocole::Ws: return "WebSocket";
    case ses::Protocole::Wss: return "WebSocket TLS";
    case ses::Protocole::Ssh: return "SSH";
  }
  return "?";
}

const char* abregeProtocole(ses::Protocole p) {
  switch (p) {
    case ses::Protocole::Telnet: return "TEL";
    case ses::Protocole::Ws: return "WS";
    case ses::Protocole::Wss: return "WSS";
    case ses::Protocole::Ssh: return "SSH";
  }
  return "?";
}

const char* nomAffichage(Affichage a) {
  switch (a) {
    case Affichage::Videotex: return "Vidéotex brut";
    case Affichage::Texte40: return "Texte 40 colonnes";
    case Affichage::Texte80: return "Texte 80 colonnes";
  }
  return "?";
}

Affichage affichageParDefaut(ses::Protocole p, bool avance) {
  switch (p) {
    case ses::Protocole::Ws:
    case ses::Protocole::Wss:
    case ses::Protocole::Telnet: return Affichage::Videotex;
    case ses::Protocole::Ssh: return avance ? Affichage::Texte80 : Affichage::Texte40;
  }
  return Affichage::Videotex;
}

bool lireAdresse(const char* adresse, Fiche& f, bool avance, const char** erreur) {
  f = Fiche();
  while (*adresse == ' ') ++adresse;
  struct Prefixe {
    const char* texte;
    ses::Protocole proto;
  };
  static const Prefixe kPrefixes[] = {
      {"telnet://", ses::Protocole::Telnet}, {"wss://", ses::Protocole::Wss}, {"ws://", ses::Protocole::Ws},
      {"ssh://", ses::Protocole::Ssh},       {"telnet:", ses::Protocole::Telnet}, {"wss:", ses::Protocole::Wss},
      {"ws:", ses::Protocole::Ws},           {"ssh:", ses::Protocole::Ssh},
  };
  f.proto = ses::Protocole::Telnet;
  for (const Prefixe& p : kPrefixes) {
    const size_t n = strlen(p.texte);
    if (strncasecmp(adresse, p.texte, n) == 0) {
      f.proto = p.proto;
      adresse += n;
      break;
    }
  }

  // [utilisateur@]hote[:port][/chemin]
  const char* arobase = strchr(adresse, '@');
  const char* barre = strchr(adresse, '/');
  if (arobase && (!barre || arobase < barre)) {
    const size_t n = size_t(arobase - adresse);
    strlcpy(f.utilisateur, adresse, min(n + 1, sizeof(f.utilisateur)));
    adresse = arobase + 1;
    barre = strchr(adresse, '/');
  }
  const char* finHote = barre ? barre : adresse + strlen(adresse);
  const char* deuxPoints = static_cast<const char*>(memchr(adresse, ':', size_t(finHote - adresse)));
  const char* finNom = deuxPoints ? deuxPoints : finHote;
  if (finNom == adresse) {
    *erreur = "Adresse vide";
    return false;
  }
  strlcpy(f.hote, adresse, min(size_t(finNom - adresse) + 1, sizeof(f.hote)));
  if (deuxPoints) {
    const long port = strtol(deuxPoints + 1, nullptr, 10);
    if (port <= 0 || port > 65535) {
      *erreur = "Port incorrect";
      return false;
    }
    f.port = uint16_t(port);
  }
  strlcpy(f.chemin, barre ? barre : "/", sizeof(f.chemin));
  if (f.proto == ses::Protocole::Ssh && !f.utilisateur[0]) {
    *erreur = "SSH : utilisateur@hote";
    return false;
  }
  f.affichage = affichageParDefaut(f.proto, avance);
  f.defilement = f.proto == ses::Protocole::Telnet;
  // Nom et code proposés si on l'ajoute ensuite à l'annuaire.
  strlcpy(f.nom, f.hote, sizeof(f.nom));
  size_t k = 0;
  for (const char* p = f.hote; *p && *p != '.' && k < sizeof(f.code) - 1; ++p) {
    f.code[k++] = char(toupper(static_cast<unsigned char>(*p)));
  }
  f.code[k] = '\0';
  *erreur = nullptr;
  return true;
}

// ------------------------------------------------------------------ portail

const char* cleProtocole(ses::Protocole p) {
  switch (p) {
    case ses::Protocole::Telnet: return "telnet";
    case ses::Protocole::Ws: return "ws";
    case ses::Protocole::Wss: return "wss";
    case ses::Protocole::Ssh: return "ssh";
  }
  return "ws";
}

bool lireProtocole(const char* cle, ses::Protocole& p) {
  static const ses::Protocole kTous[] = {ses::Protocole::Telnet, ses::Protocole::Ws, ses::Protocole::Wss,
                                         ses::Protocole::Ssh};
  for (ses::Protocole q : kTous) {
    if (strcasecmp(cle, cleProtocole(q)) == 0) {
      p = q;
      return true;
    }
  }
  return false;
}

const char* cleAffichage(Affichage a) {
  switch (a) {
    case Affichage::Videotex: return "videotex";
    case Affichage::Texte40: return "texte40";
    case Affichage::Texte80: return "texte80";
  }
  return "videotex";
}

bool lireAffichage(const char* cle, Affichage& a) {
  static const Affichage kTous[] = {Affichage::Videotex, Affichage::Texte40, Affichage::Texte80};
  for (Affichage b : kTous) {
    if (strcasecmp(cle, cleAffichage(b)) == 0) {
      a = b;
      return true;
    }
  }
  return false;
}

const char* verifier(Fiche& f, int i) {
  for (char* c = f.code; *c; ++c) *c = char(toupper(static_cast<unsigned char>(*c)));
  if (!f.code[0]) return "Code manquant";
  for (const char* c = f.code; *c; ++c) {
    if (*c <= ' ' || *c > '~') return "Code : lettres et chiffres, sans espace ni accent";
  }
  if (!f.code[1] && f.code[0] >= '1' && f.code[0] <= '9') return "Un chiffre seul est une rubrique du Sommaire";
  const int deja = chercher(f.code);
  if (deja >= 0 && deja != i) return "Code déjà pris par un autre service";
  if (!f.nom[0]) strlcpy(f.nom, f.code, sizeof(f.nom));
  if (!f.hote[0]) return "Adresse manquante";
  if (!f.chemin[0]) strlcpy(f.chemin, "/", sizeof(f.chemin));
  if (f.pingS > 600) return "Ping : 0 à 600 s";
  if (f.proto == ses::Protocole::Ssh && !f.utilisateur[0]) return "SSH : utilisateur manquant";
  if (f.cle[0] && !cles::nomValide(f.cle)) return "Clé SSH : nom incorrect";
  return nullptr;
}

void exporter(String& out) {
  out.reserve(out.length() + 64 + nb * 340);
  out += "{\"format\":\"esptel-annuaire\",\"version\":1,\"services\":[";
  for (int i = 0; i < nb; ++i) {
    const Fiche& f = fiches[i];
    char t[16];
    out += i ? ",\n{" : "\n{";
    out += "\"code\":";
    json::chaine(out, f.code);
    out += ",\"nom\":";
    json::chaine(out, f.nom);
    out += ",\"protocole\":";
    json::chaine(out, cleProtocole(f.proto));
    out += ",\"hote\":";
    json::chaine(out, f.hote);
    snprintf(t, sizeof(t), "%u", unsigned(f.port));
    out += ",\"port\":";
    out += t;
    out += ",\"chemin\":";
    json::chaine(out, f.chemin);
    out += ",\"affichage\":";
    json::chaine(out, cleAffichage(f.affichage));
    out += f.echo ? ",\"echo\":true" : ",\"echo\":false";
    out += f.defilement ? ",\"rouleau\":true" : ",\"rouleau\":false";
    out += f.finTransmise ? ",\"finTransmise\":true" : ",\"finTransmise\":false";
    out += ",\"sousProtocole\":";
    json::chaine(out, f.sousProto);
    snprintf(t, sizeof(t), "%u", unsigned(f.pingS));
    out += ",\"ping\":";
    out += t;
    out += ",\"utilisateur\":";
    json::chaine(out, f.utilisateur);
    out += ",\"motDePasse\":";
    json::chaine(out, f.motDePasse);
    out += ",\"cle\":";
    json::chaine(out, f.cle);
    out += f.reconnexion ? ",\"reconnexion\":true" : ",\"reconnexion\":false";
    out += '}';
  }
  out += "\n]}\n";
}

namespace {

// Copie UTF-8 tronquée sans couper un caractère.
void copier(char* dest, const char* src, size_t taille) {
  size_t n = strlen(src);
  if (n >= taille) {
    n = taille - 1;
    while (n > 0 && (static_cast<unsigned char>(src[n]) & 0xC0) == 0x80) --n;
  }
  memcpy(dest, src, n);
  dest[n] = '\0';
}

}  // namespace

bool importer(char* texte, bool remplacerTout, BilanImport& bilan) {
  bilan = BilanImport();
  // Première lecture à blanc : un JSON illisible ne doit rien changer. La
  // lecture décode les chaînes sur place, donc la seconde passe travaille sur
  // une copie faite avant.
  const size_t longueur = strlen(texte);
  char* copie = static_cast<char*>(malloc(longueur + 1));
  if (!copie) {
    bilan.erreur = "mémoire insuffisante";
    return false;
  }
  memcpy(copie, texte, longueur + 1);
  auto rien = [] {};
  auto rienChamp = [](const char*, const json::Valeur&) {};
  if (!json::lireObjets(copie, "services", rien, rienChamp, rien, &bilan.erreur, &bilan.position)) {
    free(copie);
    return false;
  }
  free(copie);

  if (remplacerTout) nb = 0;
  Fiche f;
  bool incorrecte = false;
  auto texteDe = [&](const json::Valeur& v, char* dest, size_t n) {
    if (v.type == json::Valeur::Type::Texte) copier(dest, v.texte, n);
    else if (v.type != json::Valeur::Type::Nul) incorrecte = true;
  };
  auto debut = [&] {
    f = Fiche();
    f.chemin[0] = '\0';
    incorrecte = false;
  };
  auto champ = [&](const char* cle, const json::Valeur& v) {
    const bool nombre = v.type == json::Valeur::Type::Nombre;
    const bool booleen = v.type == json::Valeur::Type::Booleen;
    if (strcmp(cle, "code") == 0) texteDe(v, f.code, sizeof(f.code));
    else if (strcmp(cle, "nom") == 0) texteDe(v, f.nom, sizeof(f.nom));
    else if (strcmp(cle, "hote") == 0) texteDe(v, f.hote, sizeof(f.hote));
    else if (strcmp(cle, "chemin") == 0) texteDe(v, f.chemin, sizeof(f.chemin));
    else if (strcmp(cle, "sousProtocole") == 0) texteDe(v, f.sousProto, sizeof(f.sousProto));
    else if (strcmp(cle, "utilisateur") == 0) texteDe(v, f.utilisateur, sizeof(f.utilisateur));
    else if (strcmp(cle, "motDePasse") == 0) texteDe(v, f.motDePasse, sizeof(f.motDePasse));
    else if (strcmp(cle, "cle") == 0) texteDe(v, f.cle, sizeof(f.cle));
    else if (strcmp(cle, "protocole") == 0) {
      if (v.type != json::Valeur::Type::Texte || !lireProtocole(v.texte, f.proto)) incorrecte = true;
    } else if (strcmp(cle, "affichage") == 0) {
      if (v.type != json::Valeur::Type::Texte || !lireAffichage(v.texte, f.affichage)) incorrecte = true;
    } else if (strcmp(cle, "port") == 0) {
      if (!nombre || v.nombre < 0 || v.nombre > 65535) incorrecte = true;
      else f.port = uint16_t(v.nombre);
    } else if (strcmp(cle, "ping") == 0) {
      if (!nombre || v.nombre < 0 || v.nombre > 600) incorrecte = true;
      else f.pingS = uint16_t(v.nombre);
    } else if (strcmp(cle, "echo") == 0) {
      f.echo = booleen && v.booleen;
    } else if (strcmp(cle, "rouleau") == 0) {
      f.defilement = booleen && v.booleen;
    } else if (strcmp(cle, "finTransmise") == 0) {
      f.finTransmise = booleen && v.booleen;
    } else if (strcmp(cle, "reconnexion") == 0) {
      f.reconnexion = booleen && v.booleen;
    }
    // Clé inconnue : ignorée (fichier d'une version plus récente).
  };
  auto fin = [&] {
    const int deja = chercher(f.code);
    const char* pourquoi = incorrecte ? "valeur incorrecte" : verifier(f, deja);
    if (!pourquoi && deja < 0 && nb == kMax) pourquoi = "annuaire plein";
    if (pourquoi) {
      if (!bilan.refusees) snprintf(bilan.premierRefus, sizeof(bilan.premierRefus), "%s : %s", f.code[0] ? f.code : "?", pourquoi);
      ++bilan.refusees;
      return;
    }
    if (deja >= 0) {
      // On garde l'historique des appels de la fiche remplacée.
      f.derniere = fiches[deja].derniere;
      f.cumulS = fiches[deja].cumulS;
      fiches[deja] = f;
      ++bilan.remplacees;
    } else {
      fiches[nb++] = f;
      ++bilan.ajoutees;
    }
  };
  const bool ok = json::lireObjets(texte, "services", debut, champ, fin, &bilan.erreur, &bilan.position);
  sauver();
  return ok;
}

}  // namespace annuaire
