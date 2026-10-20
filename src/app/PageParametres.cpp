#include "../config.h"

#include "../Pages/Charte.h"
#include "Annuaire.h"
#include "Cles.h"
#include "Hotes.h"
#include "Pages.h"
#include "Reglages.h"
#include "Reseau.h"
#include "Vdt.h"

namespace {

constexpr int kColChamp = 10;
constexpr int kColValeur = 30;
constexpr int kRangeePremier = 6;  // un réglage toutes les deux rangées

// Valeurs proposées, dans l'ordre où ENVOI les fait défiler.
const uint8_t kMire[] = {3, 8, 15, 30};
const uint8_t kFin[] = {5, 15, 30, 0};
const uint16_t kVitesses[] = {0, 1200, 4800, 9600};

template <typename T, size_t N>
T suivant(const T (&valeurs)[N], T courant) {
  for (size_t i = 0; i < N; ++i) {
    if (valeurs[i] == courant) return valeurs[(i + 1) % N];
  }
  return valeurs[0];
}

void valeurReglage(int i, char* t, size_t n) {
  const reglages::Valeurs& v = reglages::lire();
  switch (i) {
    case 0: strlcpy(t, v.bruit ? "oui" : "non", n); break;
    case 1: strlcpy(t, v.cout ? "oui" : "non", n); break;
    case 2: snprintf(t, n, "%u s", v.mireS); break;
    case 3:
      if (v.finS) snprintf(t, n, "%u s", v.finS);
      else strlcpy(t, "jamais", n);
      break;
    case 4:
      if (v.vitesse) snprintf(t, n, "%u", v.vitesse);
      else strlcpy(t, "auto", n);
      break;
  }
}

constexpr int kNbReglages = 5;
const char* const kLibelles[kNbReglages] = {
    "Bruit de connexion", "Coût en fin d'appel", "Délai de la mire", "Retour après la fin",
    "Vitesse du Minitel",
};

// Une invite commune aux deux pages : "Choix : .  puis ENVOI".
void invite(Minitel& m, pages::Champ& champ) {
  charte::filet(m, 21);
  charte::texte(m, 2, charte::kRangeeInvite, "Choix :");
  champ.dessiner(m);
  m.newXY(kColChamp + 2, charte::kRangeeInvite);
  m.attributs(charte::kValeur);
  m.print("puis ");
  charte::touche(m, "ENVOI");
  charte::rappel(m);
}

}  // namespace

// ------------------------------------------------------------------ P50

void PageParametres::entrer(pages::Moteur&) { _champ.configurer(kColChamp, charte::kRangeeInvite, 1, true); }

void PageParametres::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::bandeau(m, "PARAMETRES");
  for (int i = 0; i < kNbReglages; ++i) {
    charte::choix(m, kRangeePremier + 2 * i, char('1' + i), kLibelles[i]);
    dessinerValeur(mo, i);
  }
  charte::texte(m, 8, kRangeePremier + 2 * 4 + 1, "au prochain démarrage", charte::kLibelle);
  charte::choix(m, 16, '6', "Portail web et mise à jour");
  charte::choix(m, 18, '9', "Réinitialisation");
  invite(m, _champ);
}

void PageParametres::dessinerValeur(pages::Moteur& mo, int i) {
  Minitel& m = mo.ecran();
  char t[12];
  valeurReglage(i, t, sizeof(t));
  m.newXY(kColValeur, kRangeePremier + 2 * i);
  m.attributs(CARACTERE_VERT);
  m.print(t);
  m.cancel();  // efface la fin d'une valeur plus longue ("jamais" -> "5 s")
}

void PageParametres::replacerCurseur(pages::Moteur& mo) { _champ.placerCurseur(mo.ecran()); }

bool PageParametres::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (!evt.est(mio::Touche::Envoi)) return _champ.touche(mo.ecran(), evt) != pages::Champ::Resultat::Ignore;
  const char c = _champ.texte()[0];
  _champ.vider(&mo.ecran());
  if (c == '9') {
    mo.aller(pageReinitialisation);
    return true;
  }
  if (c == '6') {
    mo.aller(pagePortail);
    return true;
  }
  const int i = c - '1';
  if (i < 0 || i >= kNbReglages) {
    mo.ecran().bip();
    return true;
  }
  reglages::Valeurs v = reglages::lire();
  switch (i) {
    case 0: v.bruit = !v.bruit; break;
    case 1: v.cout = !v.cout; break;
    case 2: v.mireS = suivant(kMire, v.mireS); break;
    case 3: v.finS = suivant(kFin, v.finS); break;
    case 4:
      v.vitesse = suivant(kVitesses, v.vitesse);
      // Le Minitel 1 reste à 1200 bauds quoi qu'on demande : on le dit.
      if (v.vitesse > 1200 && mo.io().identite().valide && mo.io().identite().estMinitel1()) {
        mo.message("Ce Minitel reste à 1200 bauds", false);
      }
      break;
  }
  reglages::enregistrer(v);
  dessinerValeur(mo, i);
  replacerCurseur(mo);
  return true;
}

const char* PageParametres::aide() const {
  return "Tapez le numéro d'un réglage\n"
         "puis ENVOI : sa valeur passe à\n"
         "la suivante, et reste mémorisée.\n"
         "\n"
         "1 deux bips au début d'un appel.\n"
         "2 le coût au tarif 3615 de 1990.\n"
         "3 attente de la mire d'accueil.\n"
         "4 attente avant de quitter la\n"
         "  page de fin de connexion.\n"
         "5 auto : la vitesse la plus\n"
         "  haute du modèle (4800).\n"
         "6 réglages par un navigateur web.\n"
         "9 efface l'annuaire, le WiFi\n"
         "  ou les clés SSH.";
}

// ------------------------------------------------------------------ P54

namespace {

const char* const kEffacements[] = {
    "l'annuaire",
    "les réseaux WiFi",
    "les clés SSH",
    "tout",
};

}  // namespace

void PageReinitialisation::entrer(pages::Moteur&) {
  _champ.configurer(kColChamp, charte::kRangeeInvite, 1, true);
  _choix = -1;
}

void PageReinitialisation::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::bandeau(m, "REINITIALISATION");
  charte::texte(m, 2, 6, "Effacer :");
  charte::choix(m, 8, '1', "L'annuaire (services d'origine)");
  charte::choix(m, 10, '2', "Les réseaux WiFi mémorisés");
  charte::choix(m, 12, '3', "Les clés SSH acceptées");
  charte::choix(m, 14, '4', "Tout (clés et pages comprises),");
  charte::texte(m, 8, 15, "puis redémarrage");
  dessinerConfirmation(mo);
  invite(m, _champ);
}

void PageReinitialisation::dessinerConfirmation(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::effacer(m, 1, 17);
  charte::effacer(m, 1, 18);
  if (_choix < 0) return;
  char t[40];
  snprintf(t, sizeof(t), "Effacer %s ?", kEffacements[_choix]);
  charte::texte(m, 3, 17, t, CARACTERE_ROUGE);
  m.newXY(3, 18);
  m.attributs(charte::kValeur);
  m.print("Oui ");
  charte::touche(m, "ENVOI");
  m.attributs(charte::kValeur);
  m.print("  Non ");
  charte::touche(m, "ANNULATION");
}

void PageReinitialisation::replacerCurseur(pages::Moteur& mo) { _champ.placerCurseur(mo.ecran()); }

bool PageReinitialisation::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (_choix >= 0) {
    // Confirmation en attente : ENVOI efface, ANNULATION renonce, le reste bipe.
    if (evt.est(mio::Touche::Envoi)) {
      effacer(mo);
    } else if (evt.est(mio::Touche::Annulation)) {
      _choix = -1;
      dessinerConfirmation(mo);
      replacerCurseur(mo);
    } else if (evt.est(mio::Touche::Retour) || evt.est(mio::Touche::Sommaire) || evt.est(mio::Touche::Guide)) {
      return false;
    } else {
      mo.ecran().bip();
    }
    return true;
  }
  if (!evt.est(mio::Touche::Envoi)) return _champ.touche(mo.ecran(), evt) != pages::Champ::Resultat::Ignore;
  const int c = _champ.texte()[0] - '1';
  _champ.vider(&mo.ecran());
  if (c < 0 || c > 3) {
    mo.ecran().bip();
    return true;
  }
  _choix = c;
  dessinerConfirmation(mo);
  replacerCurseur(mo);
  return true;
}

void PageReinitialisation::effacer(pages::Moteur& mo) {
  const int c = _choix;
  _choix = -1;
  switch (c) {
    case 0:
      annuaire::reinitialiser();
      mo.message("Annuaire remis à l'origine", false);
      break;
    case 1:
      while (reseau::nbMemorises() > 0) reseau::oublier(0);
      mo.message("Réseaux WiFi oubliés", false);
      break;
    case 2:
      hotes::toutOublier();
      mo.message("Clés SSH oubliées", false);
      break;
    case 3:
      cles::toutOublier();
      vdt::toutOublier();
      reglages::effacerTout();
      DEBUG_UART.println("[Reglages] tout effacé, redémarrage");
      mo.ecran().newScreen();
      charte::grand(mo.ecran(), 8, 12, "REDEMARRAGE ...");
      delay(500);
      ESP.restart();
      return;
  }
  dessinerConfirmation(mo);
  replacerCurseur(mo);
}

const char* PageReinitialisation::aide() const {
  return "Efface des données mémorisées.\n"
         "Tapez le numéro puis ENVOI ;\n"
         "une confirmation est demandée.\n"
         "\n"
         "1 remet les services d'origine\n"
         "  (vos fiches sont perdues).\n"
         "2 oublie les réseaux WiFi ; la\n"
         "  connexion en cours reste.\n"
         "3 oublie les empreintes SSH :\n"
         "  P72 les redemandera.\n"
         "4 efface tout, réglages, clés\n"
         "  privées et pages .vdt compris,\n"
         "  et redémarre.";
}
