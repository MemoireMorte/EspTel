#include "../config.h"

#include <time.h>

#include "../Pages/Charte.h"
#include "Pages.h"

namespace {

constexpr int kColChamp = 10;
constexpr int kLargeurValeur = 26;  // colonnes 14 à 39

void duree(uint32_t s, char* t, size_t n) {
  snprintf(t, n, "%02lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60),
           (unsigned long)(s % 60));
}

}  // namespace

void PageFiche::entrer(pages::Moteur&) { _champ.configurer(kColChamp, charte::kRangeeInvite, 1, true); }

void PageFiche::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  const annuaire::Fiche& f = annuaire::fiche(_indice);
  char t[48];
  nomAffiche(f.nom, t, sizeof(t), 30);
  charte::bandeau(m, t, nullptr);

  charte::ligneInfo(m, 6, "Code", f.code);
  charte::ligneInfo(m, 7, "Protocole", annuaire::nomProtocole(f.proto));
  // L'adresse peut tenir sur deux rangées.
  char morceau[kLargeurValeur + 1];
  strlcpy(morceau, f.hote, sizeof(morceau));
  charte::ligneInfo(m, 8, "Adresse", morceau);
  if (strlen(f.hote) > kLargeurValeur) charte::texte(m, 14, 9, f.hote + kLargeurValeur);
  ses::Cible c;  // pour le port par défaut du protocole
  c.protocole = f.proto;
  c.port = f.port;
  snprintf(t, sizeof(t), f.port ? "%u" : "%u (défaut)", c.portEffectif());
  charte::ligneInfo(m, 10, "Port", t);
  int rangee = 11;
  if (f.proto == ses::Protocole::Ws || f.proto == ses::Protocole::Wss) {
    strlcpy(morceau, f.chemin, sizeof(morceau));
    charte::ligneInfo(m, rangee++, "Chemin", morceau);
  } else if (f.proto == ses::Protocole::Ssh) {
    charte::ligneInfo(m, rangee++, "Utilisateur", f.utilisateur[0] ? f.utilisateur : "-");
    if (f.cle[0]) charte::ligneInfo(m, rangee++, "Clé SSH", f.cle);
  }
  charte::ligneInfo(m, rangee++, "Affichage", annuaire::nomAffichage(f.affichage));
  t[0] = '\0';
  if (f.echo) strlcat(t, "écho ", sizeof(t));
  if (f.defilement) strlcat(t, "rouleau ", sizeof(t));
  if (f.finTransmise) strlcat(t, "FIN au serveur ", sizeof(t));
  if (f.reconnexion) strlcat(t, "reconnexion", sizeof(t));
  charte::ligneInfo(m, rangee++, "Options", t[0] ? t : "-");
  if (f.derniere) {
    const time_t d = f.derniere;
    struct tm local;
    localtime_r(&d, &local);
    strftime(t, sizeof(t), "%d/%m/%Y %H:%M", &local);
    charte::ligneInfo(m, rangee++, "Dernière", t);
  }
  if (f.cumulS) {
    duree(f.cumulS, t, sizeof(t));
    charte::ligneInfo(m, rangee++, "Cumul", t);
  }

  // L'invite clignote, comme celle des anciens annuaires.
  m.newXY(9, 18);
  m.attributs(CLIGNOTEMENT);
  charte::touche(m, "CONNEXION/FIN");
  m.attributs(FIXE);
  m.attributs(charte::kValeur);
  m.print(" : appel");

  charte::filet(m, 21);
  charte::texte(m, 2, charte::kRangeeInvite, "Choix :");
  _champ.dessiner(m);
  m.newXY(kColChamp + 2, charte::kRangeeInvite);
  m.attributs(charte::kValeur);
  m.print("puis ");
  charte::touche(m, "ENVOI");
  m.newXY(2, 23);
  m.attributs(INVERSION_FOND);
  m.print(" M ");
  m.attributs(FOND_NORMAL);
  m.attributs(charte::kLibelle);
  m.print(" Modifier   ");
  m.attributs(INVERSION_FOND);
  m.print(" S ");
  m.attributs(FOND_NORMAL);
  m.attributs(charte::kLibelle);
  m.print(" Supprimer");
  charte::rappel(m);
}

void PageFiche::replacerCurseur(pages::Moteur& mo) { _champ.placerCurseur(mo.ecran()); }

bool PageFiche::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (evt.est(mio::Touche::ConnexionFin)) {
    appeler(mo, annuaire::fiche(_indice), _indice);
    return true;
  }
  if (evt.est(mio::Touche::Envoi)) {
    const char c = _champ.texte()[0];
    _champ.vider(&mo.ecran());
    if (c == 'M') {
      pageEdition.modifier(_indice);
      mo.aller(pageEdition);
    } else if (c == 'S') {
      mo.aller(pageSuppression);
    } else {
      mo.message(c ? "Choix inconnu" : "Tapez M ou S");
    }
    return true;
  }
  return _champ.touche(mo.ecran(), evt) != pages::Champ::Resultat::Ignore;
}

const char* PageFiche::aide() const {
  return "La fiche d'un service.\n"
         "\n"
         "CONNEXION/FIN appelle le service.\n"
         "Pendant la session, CONNEXION/FIN\n"
         "raccroche (ou Ctrl+R si la fiche\n"
         "transmet FIN au serveur).\n"
         "\n"
         "M puis ENVOI : modifier la fiche.\n"
         "S puis ENVOI : la supprimer.";
}

// ------------------------------------------------------------------ P23

void PageSuppression::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  const annuaire::Fiche& f = annuaire::fiche(pageFiche.indice());
  charte::bandeau(m, "SUPPRESSION");
  charte::texte(m, 2, 7, "Supprimer ce service de l'annuaire ?");
  char nom[32];
  nomAffiche(f.nom, nom, sizeof(nom), 30);
  charte::grand(m, 4, 10, nom);
  char t[24];
  snprintf(t, sizeof(t), "code %s", f.code);
  charte::texte(m, 4, 12, t, charte::kLibelle);
  charte::texte(m, 2, 15, "La fiche sera perdue.", charte::kLibelle);

  charte::filet(m, 21);
  m.newXY(2, charte::kRangeeInvite);
  m.attributs(charte::kValeur);
  m.print("Supprimer        → ");
  charte::touche(m, "ENVOI");
  m.newXY(2, 23);
  m.attributs(charte::kValeur);
  m.print("Garder la fiche  → ");
  charte::touche(m, "RETOUR");
}

bool PageSuppression::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (!evt.est(mio::Touche::Envoi)) return false;
  char t[48];
  char nom[28];
  nomAffiche(annuaire::fiche(pageFiche.indice()).nom, nom, sizeof(nom), 24);
  snprintf(t, sizeof(t), "%s supprimé", nom);
  annuaire::supprimer(pageFiche.indice());
  mo.retourA(pageAnnuaire);
  mo.message(t, false);
  return true;
}

const char* PageSuppression::aide() const {
  return "ENVOI supprime la fiche de\n"
         "l'annuaire, RETOUR la garde.";
}
