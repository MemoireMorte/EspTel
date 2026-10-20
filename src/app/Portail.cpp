#include "../config.h"

#include "Portail.h"

#include "Annuaire.h"
#include "Cles.h"
#include "Fichiers.h"
#include "Reglages.h"
#include "Reseau.h"
#include "Vdt.h"

#ifndef SIMULATEUR
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <esp_ota_ops.h>
#endif

namespace portail {

namespace {

const mio::MinitelIO* minitel = nullptr;
char codeAcces[8] = "";

char lignes[kLignesJournal][40] = {{0}};
uint32_t versionJournal = 0;

Maj etatMaj = Maj::Aucune;
int pourcent = 0;
const char* source = "";
char erreurMaj[40] = "";
void (*progression)() = nullptr;
uint32_t redemarrerA = 0;

void noter(const char* format, ...) {
  char t[sizeof(lignes[0])];
  va_list args;
  va_start(args, format);
  vsnprintf(t, sizeof(t), format, args);
  va_end(args);
  DEBUG_UART.printf("[Portail] %s\n", t);
  for (int i = 0; i + 1 < kLignesJournal; ++i) memcpy(lignes[i], lignes[i + 1], sizeof(lignes[i]));
  strlcpy(lignes[kLignesJournal - 1], t, sizeof(lignes[0]));
  ++versionJournal;
}

// Étapes d'une mise à jour : P52 n'est redessinée qu'à chaque point de pourcentage.
void avancer(Maj e, int p) {
  if (e == etatMaj && p == pourcent) return;
  etatMaj = e;
  pourcent = p;
  ++versionJournal;
  if (progression) progression();
}

void echecMaj(const char* raison) {
  strlcpy(erreurMaj, raison, sizeof(erreurMaj));
  noter("Mise à jour refusée");
  avancer(Maj::Echouee, pourcent);
}

void reussiteMaj() {
  noter("Mise à jour reçue");
  avancer(Maj::Reussie, 100);
  redemarrerA = millis() + 2000;  // le temps de répondre au navigateur et d'afficher P52
}

}  // namespace

const char* code() { return codeAcces; }
uint32_t version() { return versionJournal; }
const char* journal(int i) { return i >= 0 && i < kLignesJournal ? lignes[i] : ""; }
Maj maj() { return etatMaj; }
int majPourcent() { return pourcent; }
const char* majSource() { return source; }
const char* majErreur() { return erreurMaj; }
void surProgression(void (*fn)()) { progression = fn; }
String adresse() { return String("http://") + reseau::ip(); }

#ifndef SIMULATEUR

namespace {

WebServer* serveur = nullptr;
String dernierClient;

// Import de l'annuaire : le fichier entier en mémoire (30 fiches font ~10 Ko).
constexpr size_t kTailleImportMax = 24 * 1024;
char* tamponImport = nullptr;
size_t tailleImport = 0;
bool importTropGros = false;

// ------------------------------------------------------------------ HTML

const char kStyle[] PROGMEM = R"(<style>
:root{--fond:#000;--bleu:#0033cc;--jaune:#ffee00;--cyan:#00e0e0;--gris:#8a8a8a}
body{background:var(--fond);color:#fff;font:16px/1.45 "Courier New",Courier,monospace;margin:0}
header{background:var(--bleu);padding:.5em 1em;display:flex;gap:1em;align-items:baseline;flex-wrap:wrap}
header b{color:var(--jaune);font-size:1.5em;letter-spacing:.25em}
header span{color:#fff}
nav{padding:.4em 1em;border-bottom:4px solid var(--bleu)}
nav a{color:var(--cyan);margin-right:1.2em;text-decoration:none;white-space:nowrap}
nav a.ici{color:var(--jaune);text-decoration:underline}
main{max-width:46em;margin:0 auto;padding:.5em 1em 2em}
h1{color:var(--jaune);font-size:1.25em;margin:.8em 0 .5em}
h2{color:var(--cyan);font-size:1.05em;margin:1.4em 0 .4em}
a{color:var(--cyan)}
table{border-collapse:collapse;width:100%}
td,th{padding:.25em .4em;border-bottom:1px solid #1a2a55;text-align:left;vertical-align:top}
th{color:var(--cyan);font-weight:normal}
td.l{color:var(--cyan);width:9em}
td.a{overflow-wrap:anywhere}
label{color:var(--cyan);display:block;margin-top:.7em}
label.c{display:flex;gap:.5em;align-items:center}
input,select,textarea{background:#0b0b1a;color:#fff;border:1px solid var(--cyan);font:inherit;padding:.3em;width:100%;box-sizing:border-box}
input[type=checkbox]{width:auto}
input[type=file]{border-style:dashed}
button{background:var(--cyan);color:#000;border:0;padding:.4em 1.1em;font:inherit;cursor:pointer;margin-top:.9em}
button.danger{background:#d03030;color:#fff}
form.ligne{display:inline;margin:0}
form.ligne button{margin:0;padding:.1em .6em}
.grille{display:grid;grid-template-columns:1fr 1fr;gap:0 1em}
.msg{border-left:4px solid #20c020;padding:.3em .7em;margin:.8em 0;color:#7f7}
.err{border-left:4px solid #e03030;padding:.3em .7em;margin:.8em 0;color:#f88}
.note{color:var(--gris);font-size:.9em}
.barre{border:1px solid var(--cyan);height:1.1em;margin-top:.8em}
.barre div{background:var(--jaune);height:100%;width:0}
@media (max-width:560px){.grille{grid-template-columns:1fr}td.l{width:auto}}
</style>)";

void echapper(String& h, const char* s) {
  for (; *s; ++s) {
    switch (*s) {
      case '&': h += "&amp;"; break;
      case '<': h += "&lt;"; break;
      case '>': h += "&gt;"; break;
      case '"': h += "&quot;"; break;
      case '\'': h += "&#39;"; break;
      default: h += *s;
    }
  }
}

void coderUrl(String& h, const char* s) {
  for (; *s; ++s) {
    const unsigned char c = static_cast<unsigned char>(*s);
    if (isalnum(c) || c == '-' || c == '_' || c == '.') {
      h += char(c);
    } else {
      char t[4];
      snprintf(t, sizeof(t), "%%%02X", c);
      h += t;
    }
  }
}

void entete(String& h, const char* titre, const char* ici) {
  h.reserve(6000);
  h += F("<!doctype html><html lang=fr><head><meta charset=utf-8>"
         "<meta name=viewport content='width=device-width,initial-scale=1'><title>");
  h += titre;
  h += F(" - EspTel</title>");
  h += FPSTR(kStyle);
  h += F("</head><body><header><b>ESPTEL</b><span>");
  h += FIRMWARE_VERSION;
  h += F("</span></header><nav>");
  static const char* const kLiens[][2] = {
      {"/", "Accueil"}, {"/annuaire", "Annuaire"}, {"/reglages", "Réglages"}, {"/wifi", "WiFi"},
      {"/cles", "Clés SSH"}, {"/pages", "Pages"}, {"/maj", "Mise à jour"}};
  for (const auto& l : kLiens) {
    h += F("<a href='");
    h += l[0];
    h += strcmp(l[0], ici) == 0 ? "' class=ici>" : "'>";
    h += l[1];
    h += F("</a>");
  }
  h += F("</nav><main><h1>");
  h += titre;
  h += F("</h1>");
}

void pied(String& h) { h += F("</main></body></html>"); }

void envoyer(String& h, int statut = 200) { serveur->send(statut, "text/html; charset=utf-8", h); }

void message(String& h, const char* texte, bool erreur = false) {
  if (!texte || !texte[0]) return;
  h += erreur ? F("<p class=err>") : F("<p class=msg>");
  echapper(h, texte);
  h += F("</p>");
}

void rediriger(const char* vers) {
  serveur->sendHeader("Location", vers);
  serveur->send(303, "text/plain", "");
}

void ligneTable(String& h, const char* libelle, const char* valeur) {
  h += F("<tr><td class=l>");
  h += libelle;
  h += F("</td><td class=a>");
  echapper(h, valeur);
  h += F("</td></tr>");
}

void champTexte(String& h, const char* nom, const char* libelle, const char* valeur, size_t taille,
                const char* type = "text", const char* extra = "") {
  h += F("<label>");
  h += libelle;
  h += F("<input type=");
  h += type;
  h += F(" name=");
  h += nom;
  h += F(" maxlength=");
  h += unsigned(taille - 1);
  h += F(" value='");
  echapper(h, valeur);
  h += "' ";
  h += extra;
  h += F("></label>");
}

void caseACocher(String& h, const char* nom, const char* libelle, bool coche) {
  h += F("<label class=c><input type=checkbox name=");
  h += nom;
  h += F(" value=1");
  if (coche) h += F(" checked");
  h += '>';
  h += libelle;
  h += F("</label>");
}

void option(String& h, const char* valeur, const char* libelle, bool choisie) {
  h += F("<option value='");
  h += valeur;
  h += choisie ? "' selected>" : "'>";
  h += libelle;
  h += F("</option>");
}

// ------------------------------------------------------------------ accès

bool autorise() {
  if (serveur->authenticate(kIdentifiant, codeAcces)) {
    const String ip = serveur->client().remoteIP().toString();
    if (ip != dernierClient) {
      dernierClient = ip;
      noter("Navigateur %s", ip.c_str());
    }
    return true;
  }
  serveur->requestAuthentication(BASIC_AUTH, "EspTel", "Code affiche sur le Minitel (page P52)");
  return false;
}

// ------------------------------------------------------------------ accueil

void pageAccueil() {
  if (!autorise()) return;
  String h;
  entete(h, "Accueil", "/");
  char t[64];
  h += F("<table>");
  ligneTable(h, "Firmware", FIRMWARE_NOM " " FIRMWARE_VERSION);
  snprintf(t, sizeof(t), "%s %.5s", __DATE__, __TIME__);
  ligneTable(h, "Compilé", t);
  if (minitel) {
    snprintf(t, sizeof(t), "%s, %d bauds",
             minitel->identite().valide ? minitel->identite().modele() : "inconnu", minitel->vitesse());
    ligneTable(h, "Minitel", t);
  }
  snprintf(t, sizeof(t), "%u Ko libres, bloc %u Ko", ESP.getFreeHeap() / 1024, ESP.getMaxAllocHeap() / 1024);
  ligneTable(h, "Mémoire", t);
  const unsigned long minutes = millis() / 60000UL;
  snprintf(t, sizeof(t), "%lu h %02lu min", minutes / 60, minutes % 60);
  ligneTable(h, "En marche", t);
  snprintf(t, sizeof(t), "%s, %d dBm", reseau::ssid(), reseau::rssi());
  ligneTable(h, "WiFi", t);
  ligneTable(h, "Adresse", reseau::ip().c_str());
  snprintf(t, sizeof(t), "%d sur %d", annuaire::nombre(), annuaire::kMax);
  ligneTable(h, "Services", t);
  snprintf(t, sizeof(t), "%u Ko utilisés sur %u", unsigned(fichiers::utilise() / 1024),
           unsigned(fichiers::total() / 1024));
  ligneTable(h, "Stockage", fichiers::monte() ? t : "absent");
  h += F("</table><p class=note>Le portail reste ouvert tant que la page P52 est affichée sur le Minitel. "
         "Le code change à chaque ouverture.</p>");
  pied(h);
  envoyer(h);
}

// ------------------------------------------------------------------ annuaire

void listeAnnuaire(const char* texte, bool erreur) {
  String h;
  entete(h, "Annuaire", "/annuaire");
  message(h, texte, erreur);
  h += F("<table><tr><th>Code</th><th>Nom</th><th>Adresse</th><th></th></tr>");
  for (int i = 0; i < annuaire::nombre(); ++i) {
    const annuaire::Fiche& f = annuaire::fiche(i);
    h += F("<tr><td><a href='/fiche?code=");
    coderUrl(h, f.code);
    h += "'>";
    echapper(h, f.code);
    h += F("</a></td><td>");
    echapper(h, f.nom);
    h += F("</td><td class=a>");
    h += annuaire::cleProtocole(f.proto);
    h += F("://");
    if (f.proto == ses::Protocole::Ssh && f.utilisateur[0]) {
      echapper(h, f.utilisateur);
      h += '@';
    }
    echapper(h, f.hote);
    if (f.port) {
      h += ':';
      h += unsigned(f.port);
    }
    if (f.proto == ses::Protocole::Ws || f.proto == ses::Protocole::Wss) echapper(h, f.chemin);
    h += F("</td><td><form class=ligne method=post action=/supprimer onsubmit=\"return confirm('Supprimer ");
    echapper(h, f.code);
    h += F(" ?')\"><input type=hidden name=code value='");
    echapper(h, f.code);
    h += F("'><button class=danger title=Supprimer>X</button></form></td></tr>");
  }
  h += F("</table>");
  char t[48];
  snprintf(t, sizeof(t), "<p class=note>%d services sur %d.</p>", annuaire::nombre(), annuaire::kMax);
  h += t;
  h += F("<p><a href=/fiche>Nouveau service</a> &nbsp; <a href=/annuaire.json>Exporter (JSON)</a></p>"
         "<h2>Importer</h2><form method=post action=/importer enctype=multipart/form-data>"
         "<input type=file name=fichier accept='.json,application/json' required>"
         "<label class=c><input type=checkbox name=tout value=1>Remplacer tout l'annuaire</label>"
         "<p class=note>Sans cette case, les services du fichier s'ajoutent ; un code déjà présent "
         "remplace sa fiche. Les mots de passe SSH sont en clair dans le fichier exporté.</p>"
         "<button>Importer</button></form>");
  pied(h);
  envoyer(h);
}

void pageAnnuaire() {
  if (!autorise()) return;
  const String m = serveur->arg("m");
  const char* texte = m == "enr" ? "Fiche enregistrée." : m == "sup" ? "Fiche supprimée." : "";
  listeAnnuaire(texte, false);
}

void formulaireFiche(const annuaire::Fiche& f, const char* ancien, const char* erreur) {
  String h;
  entete(h, ancien[0] ? "Modifier un service" : "Nouveau service", "/annuaire");
  message(h, erreur, true);
  h += F("<form method=post action=/fiche><input type=hidden name=ancien value='");
  echapper(h, ancien);
  h += F("'>");
  champTexte(h, "url", "Adresse complète (facultatif, remplit protocole, hôte, port et chemin)", "", 160, "text",
             "placeholder='wss://exemple.fr:8080/ws  ou  ssh://moi@serveur'");
  h += F("<div class=grille>");
  champTexte(h, "code", "Code (tapé au Sommaire)", f.code, sizeof(f.code), "text", "required");
  champTexte(h, "nom", "Nom", f.nom, sizeof(f.nom));
  h += F("<label>Protocole<select name=protocole>");
  option(h, "ws", "WebSocket", f.proto == ses::Protocole::Ws);
  option(h, "wss", "WebSocket TLS", f.proto == ses::Protocole::Wss);
  option(h, "telnet", "Telnet", f.proto == ses::Protocole::Telnet);
  option(h, "ssh", "SSH", f.proto == ses::Protocole::Ssh);
  h += F("</select></label>");
  h += F("<label>Affichage<select name=affichage>");
  option(h, "videotex", "Vidéotex brut", f.affichage == annuaire::Affichage::Videotex);
  option(h, "texte40", "Texte 40 colonnes", f.affichage == annuaire::Affichage::Texte40);
  option(h, "texte80", "Texte 80 colonnes", f.affichage == annuaire::Affichage::Texte80);
  h += F("</select></label>");
  champTexte(h, "hote", "Hôte", f.hote, sizeof(f.hote));
  char t[8];
  snprintf(t, sizeof(t), "%u", unsigned(f.port));
  champTexte(h, "port", "Port (0 : celui du protocole)", t, 6, "number", "min=0 max=65535");
  champTexte(h, "chemin", "Chemin (WebSocket)", f.chemin, sizeof(f.chemin));
  champTexte(h, "sousProtocole", "Sous-protocole (WebSocket)", f.sousProto, sizeof(f.sousProto));
  snprintf(t, sizeof(t), "%u", unsigned(f.pingS));
  champTexte(h, "ping", "Ping en secondes (WebSocket, 0 : aucun)", t, 4, "number", "min=0 max=600");
  champTexte(h, "utilisateur", "Utilisateur (SSH)", f.utilisateur, sizeof(f.utilisateur));
  champTexte(h, "motDePasse", "Mot de passe (SSH)", "", sizeof(f.motDePasse), "password",
             f.motDePasse[0] ? "placeholder='(inchangé si vide)' autocomplete=new-password"
                             : "placeholder='(demandé à chaque appel si vide)' autocomplete=new-password");
  h += F("<label>Clé SSH (essayée avant le mot de passe)<select name=cle>");
  option(h, "", "aucune", !f.cle[0]);
  bool dansLaListe = false;
  cles::lister([&](const char* nom) {
    option(h, nom, nom, strcmp(nom, f.cle) == 0);
    dansLaListe |= strcmp(nom, f.cle) == 0;
  });
  if (f.cle[0] && !dansLaListe) {
    String l = String(f.cle) + " (absente)";
    option(h, f.cle, l.c_str(), true);
  }
  h += F("</select></label></div>");
  caseACocher(h, "echo", "Écho local des touches", f.echo);
  caseACocher(h, "rouleau", "Mode rouleau (défilement)", f.defilement);
  caseACocher(h, "finTransmise", "CONNEXION/FIN transmise au serveur", f.finTransmise);
  caseACocher(h, "reconnexion", "Reconnexion automatique après une coupure (WebSocket)", f.reconnexion);
  if (f.motDePasse[0]) caseACocher(h, "oublierMdp", "Effacer le mot de passe mémorisé", false);
  h += F("<button>Enregistrer</button> &nbsp; <a href=/annuaire>Annuler</a></form>");
  pied(h);
  envoyer(h);
}

void pageFiche() {
  if (!autorise()) return;
  const String code = serveur->arg("code");
  const int i = code.length() ? annuaire::chercher(code.c_str()) : -1;
  if (code.length() && i < 0) {
    listeAnnuaire("Ce service n'existe plus.", true);
    return;
  }
  annuaire::Fiche f;
  if (i >= 0) f = annuaire::fiche(i);
  formulaireFiche(f, i >= 0 ? annuaire::fiche(i).code : "", nullptr);
}

void enregistrerFiche() {
  if (!autorise()) return;
  const String ancien = serveur->arg("ancien");
  const int i = ancien.length() ? annuaire::chercher(ancien.c_str()) : -1;
  annuaire::Fiche f;
  if (i >= 0) f = annuaire::fiche(i);  // garde le mot de passe et l'historique
  auto lire = [](const char* nom, char* dest, size_t n) { strlcpy(dest, serveur->arg(nom).c_str(), n); };
  lire("code", f.code, sizeof(f.code));
  lire("nom", f.nom, sizeof(f.nom));
  lire("hote", f.hote, sizeof(f.hote));
  lire("chemin", f.chemin, sizeof(f.chemin));
  lire("sousProtocole", f.sousProto, sizeof(f.sousProto));
  lire("utilisateur", f.utilisateur, sizeof(f.utilisateur));
  if (serveur->arg("motDePasse").length()) lire("motDePasse", f.motDePasse, sizeof(f.motDePasse));
  else if (serveur->hasArg("oublierMdp")) f.motDePasse[0] = '\0';
  annuaire::lireProtocole(serveur->arg("protocole").c_str(), f.proto);
  annuaire::lireAffichage(serveur->arg("affichage").c_str(), f.affichage);
  f.port = uint16_t(constrain(serveur->arg("port").toInt(), 0L, 65535L));
  f.pingS = uint16_t(constrain(serveur->arg("ping").toInt(), 0L, 65535L));
  f.echo = serveur->hasArg("echo");
  f.defilement = serveur->hasArg("rouleau");
  f.finTransmise = serveur->hasArg("finTransmise");
  f.reconnexion = serveur->hasArg("reconnexion");
  lire("cle", f.cle, sizeof(f.cle));

  const char* erreur = nullptr;
  const String url = serveur->arg("url");
  if (url.length()) {
    // L'adresse complète l'emporte sur les champs séparés.
    annuaire::Fiche a;
    if (url.length() > 150) {
      erreur = "Adresse complète trop longue";
    } else if (annuaire::lireAdresse(url.c_str(), a, minitel && minitel->avance(), &erreur)) {
      f.proto = a.proto;
      strlcpy(f.hote, a.hote, sizeof(f.hote));
      f.port = a.port;
      strlcpy(f.chemin, a.chemin, sizeof(f.chemin));
      if (a.utilisateur[0]) strlcpy(f.utilisateur, a.utilisateur, sizeof(f.utilisateur));
      if (serveur->arg("hote").length() == 0) f.affichage = a.affichage;
    }
  }
  if (!erreur) erreur = annuaire::verifier(f, i);
  if (!erreur && annuaire::enregistrer(f, i) < 0) erreur = "Annuaire plein (30 services)";
  if (erreur) {
    formulaireFiche(f, ancien.c_str(), erreur);
    return;
  }
  noter("Fiche %s enregistrée", f.code);
  rediriger("/annuaire?m=enr");
}

void supprimerFiche() {
  if (!autorise()) return;
  const int i = annuaire::chercher(serveur->arg("code").c_str());
  if (i >= 0) {
    noter("Fiche %s supprimée", annuaire::fiche(i).code);
    annuaire::supprimer(i);
  }
  rediriger("/annuaire?m=sup");
}

void exporterAnnuaire() {
  if (!autorise()) return;
  String j;
  annuaire::exporter(j);
  serveur->sendHeader("Content-Disposition", "attachment; filename=\"annuaire-esptel.json\"");
  serveur->send(200, "application/json; charset=utf-8", j);
  noter("Annuaire exporté");
}

void libererImport() {
  free(tamponImport);
  tamponImport = nullptr;
  tailleImport = 0;
}

void recevoirImport() {
  HTTPUpload& u = serveur->upload();
  if (u.status == UPLOAD_FILE_START) {
    libererImport();
    importTropGros = false;
    // Seul un envoi authentifié a droit au tampon.
    if (!serveur->authenticate(kIdentifiant, codeAcces)) return;
    tamponImport = static_cast<char*>(malloc(kTailleImportMax + 1));
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (!tamponImport) return;
    if (tailleImport + u.currentSize > kTailleImportMax) {
      importTropGros = true;
      return;
    }
    memcpy(tamponImport + tailleImport, u.buf, u.currentSize);
    tailleImport += u.currentSize;
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    libererImport();
  }
}

void importerAnnuaire() {
  if (!autorise()) {
    libererImport();
    return;
  }
  char t[160];
  if (!tamponImport) {
    listeAnnuaire("Fichier non reçu (mémoire insuffisante ?)", true);
    return;
  }
  if (importTropGros) {
    libererImport();
    snprintf(t, sizeof(t), "Fichier trop gros (%u Ko au plus)", unsigned(kTailleImportMax / 1024));
    listeAnnuaire(t, true);
    return;
  }
  tamponImport[tailleImport] = '\0';
  annuaire::BilanImport b;
  const bool ok = annuaire::importer(tamponImport, serveur->hasArg("tout"), b);
  libererImport();
  if (!ok) {
    snprintf(t, sizeof(t), "JSON illisible : %s (octet %u). Rien n'a changé.", b.erreur, unsigned(b.position));
    listeAnnuaire(t, true);
    return;
  }
  noter("Import : %d ajoutés, %d remplacés", b.ajoutees, b.remplacees);
  if (b.refusees) {
    snprintf(t, sizeof(t), "%d ajoutés, %d remplacés, %d refusés (%s).", b.ajoutees, b.remplacees, b.refusees,
             b.premierRefus);
  } else {
    snprintf(t, sizeof(t), "%d ajoutés, %d remplacés.", b.ajoutees, b.remplacees);
  }
  listeAnnuaire(t, b.refusees > 0);
}

// ------------------------------------------------------------------ réglages

void formulaireReglages(const char* texte) {
  const reglages::Valeurs& v = reglages::lire();
  String h;
  entete(h, "Réglages", "/reglages");
  message(h, texte);
  h += F("<form method=post action=/reglages>");
  caseACocher(h, "bruit", "Bruit de connexion (deux bips au début d'un appel)", v.bruit);
  caseACocher(h, "cout", "Coût en fin d'appel (tarif 3615 de 1990)", v.cout);
  char t[8];
  h += F("<label>Délai de la mire<select name=mire>");
  for (uint8_t s : {3, 8, 15, 30}) {
    snprintf(t, sizeof(t), "%u", s);
    char l[12];
    snprintf(l, sizeof(l), "%u s", s);
    option(h, t, l, v.mireS == s);
  }
  h += F("</select></label><label>Retour après la fin de connexion<select name=fin>");
  for (uint8_t s : {5, 15, 30, 0}) {
    snprintf(t, sizeof(t), "%u", s);
    char l[12];
    if (s) snprintf(l, sizeof(l), "%u s", s);
    else strlcpy(l, "jamais", sizeof(l));
    option(h, t, l, v.finS == s);
  }
  h += F("</select></label><label>Vitesse du Minitel (au prochain démarrage)<select name=vitesse>");
  for (uint16_t s : {0, 1200, 4800, 9600}) {
    snprintf(t, sizeof(t), "%u", s);
    option(h, t, s ? t : "auto", v.vitesse == s);
  }
  h += F("</select></label><p class=note>9600 bauds n'existe que sur le Minitel 2 ; le Minitel 1 reste à "
         "1200.</p><button>Enregistrer</button></form>");
  pied(h);
  envoyer(h);
}

void pageReglages() {
  if (!autorise()) return;
  formulaireReglages(nullptr);
}

void enregistrerReglages() {
  if (!autorise()) return;
  reglages::Valeurs v = reglages::lire();
  v.bruit = serveur->hasArg("bruit");
  v.cout = serveur->hasArg("cout");
  const long mire = serveur->arg("mire").toInt();
  if (mire == 3 || mire == 8 || mire == 15 || mire == 30) v.mireS = uint8_t(mire);
  const long fin = serveur->arg("fin").toInt();
  if (fin == 0 || fin == 5 || fin == 15 || fin == 30) v.finS = uint8_t(fin);
  const long vitesse = serveur->arg("vitesse").toInt();
  if (vitesse == 0 || vitesse == 1200 || vitesse == 4800 || vitesse == 9600) v.vitesse = uint16_t(vitesse);
  reglages::enregistrer(v);
  noter("Réglages enregistrés");
  formulaireReglages("Réglages enregistrés.");
}

// ------------------------------------------------------------------ WiFi

void listeWifi(const char* texte, bool erreur) {
  String h;
  entete(h, "Réseaux WiFi", "/wifi");
  message(h, texte, erreur);
  h += F("<p class=note>Essayés dans cet ordre au démarrage et après une coupure.</p><table>");
  for (int i = 0; i < reseau::nbMemorises(); ++i) {
    h += F("<tr><td>");
    echapper(h, reseau::memorise(i));
    if (strcmp(reseau::memorise(i), reseau::ssid()) == 0 && reseau::etat() == reseau::Etat::Connecte) {
      h += F(" <span class=note>(connecté)</span>");
    }
    h += F("</td><td><form class=ligne method=post action=/wifi/oublier onsubmit=\"return confirm('Oublier ce "
           "réseau ?')\"><input type=hidden name=ssid value='");
    echapper(h, reseau::memorise(i));
    h += F("'><button class=danger>Oublier</button></form></td></tr>");
  }
  if (reseau::nbMemorises() == 0) h += F("<tr><td>Aucun réseau mémorisé.</td></tr>");
  h += F("</table><h2>Ajouter un réseau</h2><form method=post action=/wifi/ajouter>");
  champTexte(h, "ssid", "Nom du réseau", "", reseau::kTailleSsid, "text", "required");
  champTexte(h, "mdp", "Mot de passe (vide pour un réseau ouvert)", "", reseau::kTailleMdp, "password",
             "autocomplete=new-password");
  h += F("<p class=note>Le réseau est mémorisé sans connexion immédiate : la liaison en cours, et donc ce "
         "portail, restent. Un réseau déjà connu change de mot de passe.</p><button>Ajouter</button></form>");
  pied(h);
  envoyer(h);
}

void pageWifi() {
  if (!autorise()) return;
  listeWifi(nullptr, false);
}

void ajouterWifi() {
  if (!autorise()) return;
  const String s = serveur->arg("ssid");
  const String p = serveur->arg("mdp");
  if (!s.length() || s.length() >= reseau::kTailleSsid || p.length() >= reseau::kTailleMdp) {
    listeWifi("Nom ou mot de passe trop long.", true);
    return;
  }
  if (p.length() && p.length() < 8) {
    listeWifi("Un mot de passe WPA fait au moins 8 caractères.", true);
    return;
  }
  if (!reseau::ajouter(s.c_str(), p.c_str())) {
    listeWifi("Liste pleine (5 réseaux) : oubliez-en un d'abord.", true);
    return;
  }
  noter("WiFi %s mémorisé", s.c_str());
  listeWifi("Réseau mémorisé.", false);
}

void oublierWifi() {
  if (!autorise()) return;
  const int i = reseau::indiceMemorise(serveur->arg("ssid").c_str());
  if (i >= 0) {
    noter("WiFi %s oublié", reseau::memorise(i));
    reseau::oublier(i);
  }
  listeWifi(i >= 0 ? "Réseau oublié (la liaison en cours reste)." : nullptr, false);
}


// ------------------------------------------------------------------ clés SSH

void listeCles(const char* texte, bool erreur) {
  String h;
  entete(h, "Clés SSH", "/cles");
  message(h, texte, erreur);
  h += F("<p class=note>Une fiche SSH nomme sa clé ; l'appel l'essaie avant le mot de passe. Les clés sont "
         "gardées sans phrase de passe dans la mémoire de l'ESP32 : qui a la carte en main peut les lire. "
         "Mettez la ligne publique dans ~/.ssh/authorized_keys du serveur.</p>");
  int n = 0;
  cles::lister([&](const char* nom) {
    ++n;
    cles::Description d;
    const bool lisible = cles::decrire(nom, d);
    h += F("<h2>");
    echapper(h, nom);
    h += F("</h2><table>");
    ligneTable(h, "Type", lisible ? d.type : "illisible");
    if (lisible) ligneTable(h, "Empreinte", d.empreinte);
    h += F("</table>");
    if (lisible && d.publique.length()) {
      h += F("<label>Clé publique<textarea readonly rows=3 onclick='this.select()'>");
      echapper(h, d.publique.c_str());
      h += F("</textarea></label>");
    }
    h += F("<form class=ligne method=post action=/cles/supprimer onsubmit=\"return confirm('Supprimer la clé ");
    echapper(h, nom);
    h += F(" ?')\"><input type=hidden name=nom value='");
    echapper(h, nom);
    h += F("'><button class=danger>Supprimer</button></form>");
  });
  if (n == 0) h += F("<p>Aucune clé.</p>");
  char t[64];
  snprintf(t, sizeof(t), "<p class=note>Clés : %d sur %d.</p>", n, cles::kMax);
  h += t;
  h += F("<h2>Générer une clé Ed25519</h2><form method=post action=/cles/generer>");
  champTexte(h, "nom", "Nom (a-z, 0-9, - ou _)", "", cles::kTailleNom, "text", "required pattern='[a-z0-9_-]+'");
  h += F("<button>Générer</button></form>"
         "<h2>Déposer une clé privée</h2><form method=post action=/cles/importer>");
  champTexte(h, "nom", "Nom (a-z, 0-9, - ou _)", "", cles::kTailleNom, "text", "required pattern='[a-z0-9_-]+'");
  h += F("<label>Fichier (id_ed25519, id_rsa...)<input type=file id=fc></label>"
         "<label>Ou texte collé<textarea name=texte id=tc rows=6 required "
         "placeholder='-----BEGIN OPENSSH PRIVATE KEY-----'></textarea></label>");
  champTexte(h, "phrase", "Phrase de passe (si la clé en a une)", "", 128, "password", "autocomplete=off");
  h += F("<button>Déposer</button></form>"
         "<script>document.getElementById('fc').onchange=e=>{const f=e.target.files[0];if(!f)return;"
         "const r=new FileReader();r.onload=()=>{document.getElementById('tc').value=r.result;};r.readAsText(f);};"
         "</script>");
  pied(h);
  envoyer(h);
}

void pageCles() {
  if (!autorise()) return;
  listeCles(nullptr, false);
}

void importerCle() {
  if (!autorise()) return;
  String texte = serveur->arg("texte");
  texte.replace("\r", "");
  const String nom = serveur->arg("nom");
  const char* e = cles::importer(nom.c_str(), texte.c_str(), serveur->arg("phrase").c_str());
  if (e) {
    listeCles(e, true);
    return;
  }
  noter("Clé %s déposée", nom.c_str());
  listeCles("Clé enregistrée.", false);
}

void genererCle() {
  if (!autorise()) return;
  const String nom = serveur->arg("nom");
  const char* e = cles::generer(nom.c_str());
  if (e) {
    listeCles(e, true);
    return;
  }
  noter("Clé %s générée", nom.c_str());
  listeCles("Clé générée : copiez sa ligne publique sur le serveur.", false);
}

void supprimerCle() {
  if (!autorise()) return;
  const String nom = serveur->arg("nom");
  if (cles::supprimer(nom.c_str())) noter("Clé %s supprimée", nom.c_str());
  listeCles("Clé supprimée.", false);
}

// ------------------------------------------------------------------ pages .vdt

constexpr const char* kEnvoiVdt = "/envoi.vdt";
String nomEnvoiVdt;  // tiré du nom du fichier envoyé
bool envoiVdtOk = false;
const char* erreurEnvoiVdt = nullptr;

// "Mire.VDT" -> "MIRE" ; caractères hors A-Z 0-9 ignorés, 8 au plus.
String nomDepuisFichier(const String& f) {
  String n;
  for (unsigned i = 0; i < f.length() && n.length() < vdt::kTailleNom - 1; ++i) {
    const char c = f.charAt(i);
    if (c == '.') break;
    if (isalnum(static_cast<unsigned char>(c))) n += char(toupper(static_cast<unsigned char>(c)));
  }
  return n;
}

void listePages(const char* texte, bool erreur) {
  String h;
  entete(h, "Pages Vidéotex", "/pages");
  message(h, texte, erreur);
  h += F("<p class=note>Des fichiers .vdt dessinés avec un éditeur Vidéotex. Tapé au Sommaire, le nom d'une "
         "page l'affiche (quand aucun service n'a ce code). La page <b>MIRE</b> remplace la mire d'accueil.</p>"
         "<table><tr><th>Nom</th><th>Taille</th><th></th></tr>");
  int n = 0;
  vdt::lister([&](const char* nom, size_t taille) {
    ++n;
    char t[24];
    h += F("<tr><td><a href='/pages/fichier?nom=");
    h += nom;
    h += "'>";
    h += nom;
    h += F("</a></td><td>");
    snprintf(t, sizeof(t), "%u octets", unsigned(taille));
    h += t;
    h += F("</td><td><form class=ligne method=post action=/pages/supprimer onsubmit=\"return confirm('Supprimer ");
    h += nom;
    h += F(" ?')\"><input type=hidden name=nom value='");
    h += nom;
    h += F("'><button class=danger title=Supprimer>X</button></form></td></tr>");
  });
  if (n == 0) h += F("<tr><td>Aucune page.</td></tr>");
  h += F("</table>");
  char t[96];
  snprintf(t, sizeof(t), "<p class=note>Stockage : %u Ko utilisés sur %u.</p>", unsigned(fichiers::utilise() / 1024),
           unsigned(fichiers::total() / 1024));
  h += t;
  h += F("<h2>Déposer une page</h2><form method=post action=/pages/envoyer enctype=multipart/form-data>"
         "<input type=file name=fichier accept='.vdt' required>"
         "<p class=note>Le nom de la page vient du fichier (MIRE.vdt donne MIRE) : 8 lettres ou chiffres "
         "au plus, 16 Ko au plus. Une page du même nom est remplacée.</p><button>Déposer</button></form>");
  pied(h);
  envoyer(h);
}

void pagePages() {
  if (!autorise()) return;
  listePages(nullptr, false);
}

void recevoirVdt() {
  HTTPUpload& u = serveur->upload();
  switch (u.status) {
    case UPLOAD_FILE_START:
      envoiVdtOk = false;
      erreurEnvoiVdt = nullptr;
      if (!serveur->authenticate(kIdentifiant, codeAcces)) return;
      nomEnvoiVdt = nomDepuisFichier(u.filename);
      if (!fichiers::commencer(kEnvoiVdt)) erreurEnvoiVdt = "Stockage LittleFS absent";
      else envoiVdtOk = true;
      break;
    case UPLOAD_FILE_WRITE:
      if (!envoiVdtOk) return;
      if (u.totalSize + u.currentSize > vdt::kTailleMax) {
        erreurEnvoiVdt = "Page trop grosse (16 Ko au plus)";
      } else if (!fichiers::ajouter(u.buf, u.currentSize)) {
        erreurEnvoiVdt = "Écriture impossible (place insuffisante ?)";
      } else {
        return;
      }
      envoiVdtOk = false;
      fichiers::terminer(false);
      break;
    case UPLOAD_FILE_END:
      if (envoiVdtOk) envoiVdtOk = fichiers::terminer(true);
      break;
    case UPLOAD_FILE_ABORTED:
      if (envoiVdtOk) fichiers::terminer(false);
      envoiVdtOk = false;
      erreurEnvoiVdt = "Envoi interrompu";
      break;
  }
}

void enregistrerVdt() {
  if (!autorise()) return;
  if (!envoiVdtOk) {
    listePages(erreurEnvoiVdt ? erreurEnvoiVdt : "Fichier non reçu", true);
    return;
  }
  envoiVdtOk = false;
  if (!vdt::nomValide(nomEnvoiVdt.c_str())) {
    fichiers::supprimer(kEnvoiVdt);
    listePages("Nom de fichier inutilisable : 1 à 8 lettres ou chiffres (pas un chiffre seul).", true);
    return;
  }
  if (!fichiers::renommer(kEnvoiVdt, vdt::chemin(nomEnvoiVdt.c_str()).c_str())) {
    fichiers::supprimer(kEnvoiVdt);
    listePages("Enregistrement impossible", true);
    return;
  }
  noter("Page %s déposée", nomEnvoiVdt.c_str());
  String t = String("Page ") + nomEnvoiVdt + " enregistrée.";
  listePages(t.c_str(), false);
}

void telechargerVdt() {
  if (!autorise()) return;
  const String nom = serveur->arg("nom");
  if (!vdt::existe(nom.c_str())) {
    listePages("Cette page n'existe plus.", true);
    return;
  }
  const String c = vdt::chemin(nom.c_str());
  serveur->sendHeader("Content-Disposition", String("attachment; filename=\"") + nom + ".vdt\"");
  serveur->setContentLength(fichiers::taille(c.c_str()));
  serveur->send(200, "application/octet-stream", "");
  fichiers::parcourir(c.c_str(), [](const uint8_t* d, size_t n) {
    serveur->sendContent(reinterpret_cast<const char*>(d), n);
    return true;
  });
}

void supprimerVdt() {
  if (!autorise()) return;
  const String nom = serveur->arg("nom");
  if (vdt::supprimer(nom.c_str())) noter("Page %s supprimée", nom.c_str());
  listePages("Page supprimée.", false);
}

// ------------------------------------------------------------------ mise à jour

void pageMaj() {
  if (!autorise()) return;
  String h;
  entete(h, "Mise à jour du firmware", "/maj");
  const esp_partition_t* courante = esp_ota_get_running_partition();
  const esp_partition_t* suivante = esp_ota_get_next_update_partition(nullptr);
  char t[64];
  h += F("<table>");
  ligneTable(h, "Version", FIRMWARE_NOM " " FIRMWARE_VERSION);
  if (courante) {
    snprintf(t, sizeof(t), "%s", courante->label);
    ligneTable(h, "Partition", t);
  }
  if (suivante) {
    snprintf(t, sizeof(t), "%u Ko au plus", unsigned(suivante->size / 1024));
    ligneTable(h, "Place", t);
  }
  h += F("</table><h2>Depuis ce navigateur</h2>"
         "<p class=note>Dans l'IDE Arduino : Croquis &gt; Exporter les binaires compilés, puis prenez "
         "<b>EspTel.ino.bin</b> dans le dossier build du croquis (pas le fichier .merged.bin).</p>"
         "<form id=f><input type=file id=bin accept='.bin' required><button>Envoyer</button></form>"
         "<div class=barre><div id=b></div></div><p id=etat class=note></p>"
         "<h2>Depuis l'IDE Arduino</h2><p class=note>Tant que cette page P52 est affichée, l'ESP32 "
         "apparaît dans Outils &gt; Port, rubrique réseau, sous le nom <b>esptel</b>. Téléversez comme "
         "d'habitude ; le mot de passe demandé est le code affiché sur le Minitel.</p>"
         "<script>"
         "const f=document.getElementById('f'),b=document.getElementById('b'),e=document.getElementById('etat');"
         "f.onsubmit=ev=>{ev.preventDefault();const x=document.getElementById('bin').files[0];if(!x)return;"
         "const d=new FormData();d.append('firmware',x,x.name);const r=new XMLHttpRequest();"
         "r.upload.onprogress=p=>{if(p.lengthComputable){const c=Math.round(100*p.loaded/p.total);"
         "b.style.width=c+'%';e.textContent='Envoi : '+c+' %';}};"
         "r.onload=()=>{e.textContent=r.responseText;if(r.status==200)"
         "setTimeout(()=>{location.href='/';},15000);};"
         "r.onerror=()=>{e.textContent='Liaison perdue pendant l\\'envoi.';};"
         "r.open('POST','/maj');r.send(d);e.textContent='Envoi...';};"
         "</script>");
  pied(h);
  envoyer(h);
}

bool envoiMajAutorise = false;

void recevoirMaj() {
  HTTPUpload& u = serveur->upload();
  switch (u.status) {
    case UPLOAD_FILE_START:
      envoiMajAutorise = serveur->authenticate(kIdentifiant, codeAcces);
      if (!envoiMajAutorise) return;
      source = "le navigateur";
      erreurMaj[0] = '\0';
      noter("Mise à jour : %.24s", u.filename.c_str());
      avancer(Maj::EnCours, 0);
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) echecMaj(Update.errorString());
      break;
    case UPLOAD_FILE_WRITE:
      if (!envoiMajAutorise || etatMaj != Maj::EnCours) return;
      if (Update.write(u.buf, u.currentSize) != u.currentSize) {
        echecMaj(Update.errorString());
        Update.abort();
        return;
      }
      if (serveur->clientContentLength() > 0) {
        avancer(Maj::EnCours, int(min<size_t>(99, u.totalSize * 100 / size_t(serveur->clientContentLength()))));
      }
      break;
    case UPLOAD_FILE_END:
      if (!envoiMajAutorise || etatMaj != Maj::EnCours) return;
      if (Update.end(true)) reussiteMaj();
      else echecMaj(Update.errorString());
      break;
    case UPLOAD_FILE_ABORTED:
      if (!envoiMajAutorise) return;
      Update.abort();
      echecMaj("envoi interrompu");
      break;
  }
}

void finMaj() {
  if (!autorise()) return;
  if (etatMaj == Maj::Reussie) {
    serveur->send(200, "text/plain; charset=utf-8",
                  "Mise à jour reçue. L'ESP32 redémarre ; cette page reviendra à l'accueil.");
  } else {
    String t = "Mise à jour refusée : ";
    t += erreurMaj[0] ? erreurMaj : "fichier absent";
    serveur->send(500, "text/plain; charset=utf-8", t);
  }
}

void introuvable() {
  if (!autorise()) return;
  serveur->send(404, "text/plain; charset=utf-8", "Page inconnue");
}

// ------------------------------------------------------------------ IDE Arduino

void demarrerOtaIde() {
  ArduinoOTA.setHostname(kNomHote);
  ArduinoOTA.setPassword(codeAcces);
  ArduinoOTA.setRebootOnSuccess(false);  // c'est tic() qui redémarre, après l'avoir dit sur le Minitel
  ArduinoOTA.onStart([] {
    source = "l'IDE Arduino";
    erreurMaj[0] = '\0';
    noter("Mise à jour depuis l'IDE");
    avancer(Maj::EnCours, 0);
  });
  ArduinoOTA.onProgress([](unsigned int fait, unsigned int total) {
    if (total) avancer(Maj::EnCours, int(min<unsigned>(99, fait * 100U / total)));
  });
  ArduinoOTA.onEnd([] { reussiteMaj(); });
  ArduinoOTA.onError([](ota_error_t e) {
    static const char* const kRaisons[] = {"mot de passe refusé", "début refusé", "liaison perdue",
                                           "réception incomplète", "fin refusée"};
    echecMaj(unsigned(e) < 5 ? kRaisons[e] : "erreur");
  });
  ArduinoOTA.begin();  // démarre aussi mDNS sous le nom esptel
  MDNS.addService("http", "tcp", 80);
}

}  // namespace

bool ouvert() { return serveur != nullptr; }

bool ouvrir(const mio::MinitelIO& io) {
  if (serveur) return true;
  if (reseau::etat() != reseau::Etat::Connecte) return false;
  minitel = &io;
  snprintf(codeAcces, sizeof(codeAcces), "%06lu", (unsigned long)(esp_random() % 1000000UL));
  etatMaj = Maj::Aucune;
  pourcent = 0;
  dernierClient = "";
  serveur = new WebServer(80);
  serveur->on("/", HTTP_GET, pageAccueil);
  serveur->on("/annuaire", HTTP_GET, pageAnnuaire);
  serveur->on("/annuaire.json", HTTP_GET, exporterAnnuaire);
  serveur->on("/fiche", HTTP_GET, pageFiche);
  serveur->on("/fiche", HTTP_POST, enregistrerFiche);
  serveur->on("/supprimer", HTTP_POST, supprimerFiche);
  serveur->on("/importer", HTTP_POST, importerAnnuaire, recevoirImport);
  serveur->on("/reglages", HTTP_GET, pageReglages);
  serveur->on("/reglages", HTTP_POST, enregistrerReglages);
  serveur->on("/wifi", HTTP_GET, pageWifi);
  serveur->on("/wifi/ajouter", HTTP_POST, ajouterWifi);
  serveur->on("/wifi/oublier", HTTP_POST, oublierWifi);
  serveur->on("/cles", HTTP_GET, pageCles);
  serveur->on("/cles/importer", HTTP_POST, importerCle);
  serveur->on("/cles/generer", HTTP_POST, genererCle);
  serveur->on("/cles/supprimer", HTTP_POST, supprimerCle);
  serveur->on("/pages", HTTP_GET, pagePages);
  serveur->on("/pages/envoyer", HTTP_POST, enregistrerVdt, recevoirVdt);
  serveur->on("/pages/fichier", HTTP_GET, telechargerVdt);
  serveur->on("/pages/supprimer", HTTP_POST, supprimerVdt);
  serveur->on("/maj", HTTP_GET, pageMaj);
  serveur->on("/maj", HTTP_POST, finMaj, recevoirMaj);
  serveur->onNotFound(introuvable);
  serveur->begin();
  demarrerOtaIde();
  noter("Ouvert sur %s", reseau::ip().c_str());
  return true;
}

void fermer() {
  if (!serveur) return;
  ArduinoOTA.end();  // arrête aussi mDNS
  serveur->stop();
  delete serveur;
  serveur = nullptr;
  libererImport();
  noter("Fermé");
}

void tic() {
  if (redemarrerA && static_cast<int32_t>(millis() - redemarrerA) >= 0) {
    DEBUG_UART.println("[Portail] redémarrage après la mise à jour");
    delay(100);
    ESP.restart();
  }
  if (!serveur) return;
  serveur->handleClient();
  ArduinoOTA.handle();
}

#else  // SIMULATEUR : pas de réseau, le portail fait semblant.

namespace {
bool simOuvert = false;
}

bool ouvert() { return simOuvert; }

bool ouvrir(const mio::MinitelIO& io) {
  if (simOuvert) return true;
  if (reseau::etat() != reseau::Etat::Connecte) return false;
  minitel = &io;
  strlcpy(codeAcces, "482107", sizeof(codeAcces));
  simOuvert = true;
  noter("Ouvert sur %s", reseau::ip().c_str());
  return true;
}

void fermer() {
  if (!simOuvert) return;
  simOuvert = false;
  noter("Fermé");
}

void tic() {}

// Pour le simulateur : rejoue ce que ferait un navigateur.
void simNoter(const char* t) { noter("%s", t); }
void simMaj(int p) {
  source = "le navigateur";
  if (p < 0) echecMaj("fichier refusé");
  else if (p >= 100) reussiteMaj();
  else avancer(Maj::EnCours, p);
}

#endif  // SIMULATEUR

}  // namespace portail
