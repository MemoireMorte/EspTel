#include "../config.h"

#include "../Pages/Charte.h"
#include "Appel.h"
#include "Hotes.h"
#include "Pages.h"
#include "Reglages.h"

namespace {

constexpr int kRangeeEtape = 11;
constexpr int kRangeeBarre = 13;
constexpr int kColBarre = 10;
constexpr int kLargeurBarre = 20;
constexpr uint32_t kDureeBarreMs = 10000;  // délai de connexion des sessions
constexpr uint32_t kEcartBipsMs = 250;  // bruit de connexion : deux bips

// Octets des `colonnes` premiers caractères d'un texte UTF-8.
size_t octets(const char* t, int colonnes) {
  size_t i = 0;
  for (int c = 0; t[i] && c <= colonnes; ++i) {
    if ((static_cast<uint8_t>(t[i]) & 0xC0) != 0x80 && ++c > colonnes) break;
  }
  return i;
}

// Message (UTF-8, accents compris) sur deux rangées au plus de `largeur` colonnes.
void deuxRangees(Minitel& m, int col, int rangee, const char* t, int largeur, byte couleur) {
  char ligne[100];
  const size_t n = octets(t, largeur);
  strlcpy(ligne, t, min(n + 1, sizeof(ligne)));
  charte::texte(m, col, rangee, ligne, couleur);
  if (!t[n]) return;
  strlcpy(ligne, t + n, min(octets(t + n, largeur) + 1, sizeof(ligne)));
  charte::texte(m, col, rangee + 1, ligne, couleur);
}

}  // namespace

void appeler(pages::Moteur& mo, const annuaire::Fiche& f, int indice, bool remplacer) {
  const annuaire::Fiche copie = f;  // f peut être la fiche de l'appel précédent
  appel::lancer(copie, indice);
  if (remplacer) mo.remplacer(pageAppel);
  else mo.aller(pageAppel);
}

// ------------------------------------------------------------------ P30

constexpr int kRangeeAffichage = 18;

void PageDirecte::entrer(pages::Moteur& mo) {
  _champ.configurer(2, 8, 64, false, 38);
  _affichageChoisi = false;
  _affichage = affichageAuto(mo);
}

annuaire::Affichage PageDirecte::affichageAuto(pages::Moteur& mo) const {
  // lireAdresse fixe le protocole avant toute erreur : une adresse incomplète suffit.
  annuaire::Fiche f;
  const char* erreur = nullptr;
  annuaire::lireAdresse(_champ.texte(), f, mo.io().avance(), &erreur);
  return annuaire::affichageParDefaut(f.proto, mo.io().avance());
}

void PageDirecte::dessinerAffichage(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  m.newXY(2, kRangeeAffichage);
  m.attributs(charte::kLibelle);
  m.print("Affichage : ");
  m.print("                 ");  // efface le nom précédent (17 colonnes au plus)
  charte::texte(m, 14, kRangeeAffichage, annuaire::nomAffichage(_affichage));
  m.newXY(32, kRangeeAffichage);
  charte::touche(m, "SUITE");
  if (!mo.io().avance()) {
    charte::texte(m, 2, kRangeeAffichage + 1, "80 col. : Minitel non reconnu.", charte::kLibelle);
  }
  _champ.placerCurseur(m);
}

void PageDirecte::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::bandeau(m, "CONNEXION DIRECTE", nullptr);
  charte::texte(m, 2, 6, "Tapez l'adresse du service :");
  _champ.dessiner(m);
  charte::texte(m, 2, 11, "Exemples :", charte::kLibelle);
  charte::texte(m, 3, 12, "telnet://bbs.exemple.fr:23");
  charte::texte(m, 3, 13, "ws://serveur.fr:8080/ws");
  charte::texte(m, 3, 14, "wss://serveur.fr/minitel");
  charte::texte(m, 3, 15, "ssh://moi@nas.local");
  charte::texte(m, 2, 17, "Sans préfixe : telnet, port 23.", charte::kLibelle);
  dessinerAffichage(mo);

  charte::filet(m, 21);
  m.newXY(2, charte::kRangeeInvite);
  m.attributs(charte::kValeur);
  m.print("Appel            → ");
  charte::touche(m, "CONNEXION/FIN");
  m.newXY(2, 23);
  m.attributs(charte::kLibelle);
  m.print("Après l'appel, A l'ajoute à l'annuaire.");
  charte::rappel(m);
}

void PageDirecte::replacerCurseur(pages::Moteur& mo) { _champ.placerCurseur(mo.ecran()); }

bool PageDirecte::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (evt.est(mio::Touche::ConnexionFin) || evt.est(mio::Touche::Envoi)) {
    if (_champ.vide()) {
      mo.message("Tapez une adresse");
      return true;
    }
    annuaire::Fiche f;
    const char* erreur = nullptr;
    if (!annuaire::lireAdresse(_champ.texte(), f, mo.io().avance(), &erreur)) {
      mo.message(erreur);
      return true;
    }
    f.affichage = _affichage;
    appeler(mo, f, -1);
    return true;
  }
  if (evt.est(mio::Touche::Suite)) {
    // Vidéotex -> Texte 40 -> Texte 80 (Minitel 1B et suivants) -> Vidéotex
    int a = (static_cast<int>(_affichage) + 1) % 3;
    if (a == static_cast<int>(annuaire::Affichage::Texte80) && !mo.io().avance()) a = 0;
    _affichage = static_cast<annuaire::Affichage>(a);
    _affichageChoisi = true;
    dessinerAffichage(mo);
    return true;
  }
  const pages::Champ::Resultat r = _champ.touche(mo.ecran(), evt);
  if (r == pages::Champ::Resultat::Modifie && !_affichageChoisi) {
    const annuaire::Affichage a = affichageAuto(mo);
    if (a != _affichage) {
      _affichage = a;
      dessinerAffichage(mo);
    }
  }
  return r != pages::Champ::Resultat::Ignore;
}

const char* PageDirecte::aide() const {
  return "Appel d'un service par son\n"
         "adresse, sans passer par\n"
         "l'annuaire.\n"
         "\n"
         "telnet://hote:port\n"
         "ws://hote:port/chemin\n"
         "wss://hote:port/chemin\n"
         "ssh://utilisateur@hote:port\n"
         "Sans préfixe : telnet, port 23.\n"
         "\n"
         "L'affichage suit le préfixe ;\n"
         "SUITE le change : Vidéotex,\n"
         "Texte 40 ou Texte 80 colonnes.\n"
         "\n"
         "Après la session, A ajoute le\n"
         "service à l'annuaire.";
}

// ------------------------------------------------------------------ P70

void PageAppel::entrer(pages::Moteur&) {
  _etape = -1;
  _barre = 0;
  _bips = reglages::lire().bruit ? 0 : 2;
}

void PageAppel::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  char nom[32];
  nomAffiche(appel::fiche().nom, nom, sizeof(nom), 28);
  charte::grand(m, 11, 8, nom);
  m.newXY(kColBarre, kRangeeBarre);
  m.attributs(charte::kFilet);
  m.graphicMode();
  m.graphic(0b110000);  // le rail de la barre, rempli au fil de l'attente
  m.repeat(kLargeurBarre - 1);
  m.textMode();
  m.newXY(14, charte::kRangeeRappel);
  charte::touche(m, "CONNEXION/FIN");
  m.attributs(charte::kLibelle);
  m.print(" : abandon");
  _etape = -1;
  _barre = 0;
  dessinerEtape(mo);
  // Bruit de connexion : deux bips, une seule fois par appel. On les joue d'un
  // trait (la boucle attend 1/4 s) : un serveur rapide prendrait le Minitel avant
  // le second.
  if (!_bips) {
    m.bip();
    delay(kEcartBipsMs);
    m.bip();
    _bips = 2;
  }
}

void PageAppel::dessinerEtape(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  const uint32_t t = appel::depuisMs();

  const int etape = appel::abandonne() ? 2 : appel::identification() ? 3 : t < 1500 ? 0 : 1;
  if (etape != _etape) {
    _etape = etape;
    charte::effacer(m, 1, kRangeeEtape);
    char texte[48];
    if (etape == 0) strlcpy(texte, "Recherche du service ...", sizeof(texte));
    else if (etape == 1) snprintf(texte, sizeof(texte), "Appel de %.24s", appel::fiche().hote);
    else if (etape == 3) strlcpy(texte, "Identification ...", sizeof(texte));
    else strlcpy(texte, "Abandon ...", sizeof(texte));
    charte::texte(m, kColBarre - 3, kRangeeEtape, texte);
  }
  const int barre = static_cast<int>(min<uint32_t>(t, kDureeBarreMs) * kLargeurBarre / kDureeBarreMs);
  if (barre > _barre) {
    m.newXY(kColBarre + _barre, kRangeeBarre);
    m.attributs(CARACTERE_VERT);
    m.graphicMode();
    m.graphic(0b111111);
    if (barre - _barre > 1) m.repeat(barre - _barre - 1);
    m.textMode();
    _barre = barre;
  }
}

bool PageAppel::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  // La tâche d'appel tourne : aucune touche ne quitte la page, sauf CONNEXION/FIN.
  if (evt.est(mio::Touche::ConnexionFin) && !appel::abandonne()) {
    appel::abandonner();
    dessinerEtape(mo);
  } else {
    mo.ecran().bip();
  }
  return true;
}

void PageAppel::tic(pages::Moteur& mo) {
  // SSH : la tâche attend l'usager (clé d'hôte inconnue, mot de passe).
  if (appel::attendReponse()) {
    mo.aller(appel::phase() == appel::Phase::Cle ? static_cast<pages::Page&>(pageCle)
                                                 : static_cast<pages::Page&>(pageIdentification));
    return;
  }
  if (appel::phase() != appel::Phase::Fin) {
    dessinerEtape(mo);
    return;
  }
  appel::liberer();
  if (appel::abandonne() && !appel::reussi()) {
    mo.sommaire();
    return;
  }
  if (appel::reussi()) {
    annuaire::noterAppel(appel::indice(), appel::dureeS());
    mo.remplacer(pageFin);
  } else {
    mo.remplacer(pageEchec);
  }
}

bool PageAppel::ligneService(char* dest, size_t n) {
  // « TEASER ws  ...... » : les points avancent pendant l'attente.
  const int points = static_cast<int>(appel::depuisMs() / 1000 % 7);
  snprintf(dest, n, "%s %s", appel::fiche().code, ses::nomProtocole(appel::fiche().proto));
  const size_t k = strlen(dest);
  if (k + 2 + points < n) {
    memset(dest + k, ' ', 2);
    memset(dest + k + 2, '.', points);
    dest[k + 2 + points] = '\0';
  }
  return true;
}

const char* PageAppel::aide() const { return nullptr; }

// ------------------------------------------------------------------ P73

void PageEchec::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::grand(m, 5, 8, "SERVICE MOMENTANEMENT");
  charte::grand(m, 5, 10, "INDISPONIBLE");
  char nom[32];
  nomAffiche(appel::fiche().nom, nom, sizeof(nom), 30);
  charte::texte(m, 5, 13, nom);
  deuxRangees(m, 5, 14, appel::raison(), 34, charte::kLibelle);
  if (appel::cleChangee()) {
    m.newXY(5, 17);
    if (_oubliee) {
      m.attributs(charte::kValeur);
      m.print("Ancienne clé oubliée.");
    } else {
      charte::touche(m, "O");
      m.attributs(charte::kValeur);
      m.print(" : oublier l'ancienne clé");
    }
  }

  m.newXY(5, 19);
  charte::touche(m, "REPETITION");
  m.attributs(charte::kValeur);
  m.print(" : nouvel essai");
  m.newXY(5, 20);
  charte::touche(m, "RETOUR");
  m.attributs(charte::kValeur);
  m.print(" : page précédente");
  m.newXY(5, 21);
  charte::touche(m, "SOMMAIRE");
  m.attributs(charte::kValeur);
  m.print(" : menu");
}

bool PageEchec::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (evt.est(mio::Touche::Repetition) || evt.est(mio::Touche::ConnexionFin)) {
    appeler(mo, appel::fiche(), appel::indice(), true);
    return true;
  }
  // Clé SSH changée : l'oublier est un geste volontaire ; au rappel, P72 montrera
  // la nouvelle empreinte à vérifier.
  if (appel::cleChangee() && !_oubliee && evt.type == mio::Type::Caractere &&
      (evt.unicode == 'O' || evt.unicode == 'o')) {
    const annuaire::Fiche& f = appel::fiche();
    hotes::oublier(f.hote, f.port ? f.port : 22);
    _oubliee = true;
    mo.reafficher();
    return true;
  }
  return false;
}

const char* PageEchec::aide() const {
  return "Le service n'a pas répondu.\n"
         "La cause technique est écrite\n"
         "sous le nom du service.\n"
         "\n"
         "RÉPÉTITION (ou CONNEXION/FIN)\n"
         "refait l'appel.\n"
         "\n"
         "Clé SSH changée : si le serveur\n"
         "a été réinstallé, O oublie\n"
         "l'ancienne clé. Sinon, méfiance.\n"
         "RETOUR revient à la page d'où\n"
         "l'appel est parti.";
}

// ------------------------------------------------------------------ P90

void PageFin::entrer(pages::Moteur&) { _debut = millis(); }

void PageFin::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  _debut = millis();
  charte::grand(m, 9, 8, "FIN DE CONNEXION");

  char nom[32];
  nomAffiche(appel::fiche().nom, nom, sizeof(nom), 22);
  char t[32];
  const uint32_t s = appel::dureeS();
  charte::texte(m, 5, 12, "Service", charte::kLibelle);
  charte::texte(m, 16, 12, nom);
  snprintf(t, sizeof(t), "%02lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60),
           (unsigned long)(s % 60));
  charte::texte(m, 5, 13, "Durée", charte::kLibelle);
  charte::texte(m, 16, 13, t);
  const bool cout = reglages::lire().cout;
  if (cout) {
    // 1,29 F la minute, à la seconde près, arrondi au centime.
    const uint32_t centimes = (s * 129 + 30) / 60;
    snprintf(t, sizeof(t), "%lu,%02lu F *", (unsigned long)(centimes / 100), (unsigned long)(centimes % 100));
    charte::texte(m, 5, 14, "Coût", charte::kLibelle);
    charte::texte(m, 16, 14, t);
  }
  if (strcmp(appel::raison(), "CONNEXION/FIN") != 0) {
    charte::texte(m, 5, 15, "Fin", charte::kLibelle);
    deuxRangees(m, 16, 15, appel::raison(), 24, charte::kValeur);
  }

  charte::texte(m, 5, 18, "Merci de votre visite");
  if (cout) charte::texte(m, 3, 20, "* au tarif 3615 de 1990, 1,29 F/min", charte::kLibelle);
  if (appel::indice() < 0) {
    m.newXY(3, 22);
    m.attributs(charte::kValeur);
    m.print("Ajouter à l'annuaire  → ");
    charte::touche(m, "A");
  }
  m.newXY(3, 23);
  m.attributs(charte::kValeur);
  m.print("Continuer             → ");
  charte::touche(m, "ENVOI");
}

bool PageFin::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (evt.est(mio::Touche::Envoi)) {
    mo.retour();
    return true;
  }
  if (appel::indice() < 0 && evt.type == mio::Type::Caractere && (evt.unicode == 'A' || evt.unicode == 'a')) {
    pageEdition.nouvelle(&appel::fiche());
    mo.remplacer(pageEdition);
    return true;
  }
  return false;
}

void PageFin::tic(pages::Moteur& mo) {
  const uint32_t delai = uint32_t(reglages::lire().finS) * 1000;
  if (delai && millis() - _debut > delai) mo.retour();
}

const char* PageFin::aide() const {
  return "La session est terminée.\n"
         "\n"
         "ENVOI, ou quelques secondes\n"
         "d'attente, ramène à la page d'où\n"
         "l'appel est parti.\n"
         "\n"
         "Après une connexion directe, A\n"
         "ajoute le service à l'annuaire.\n"
         "\n"
         "Le coût est un clin d'oeil : le\n"
         "3615 coûtait 1,29 F la minute.\n"
         "Paramètres (rubrique 4) : coût\n"
         "et délai de retour.";
}
