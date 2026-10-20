#include "../config.h"

#include "../Pages/Charte.h"
#include "Appel.h"
#include "Pages.h"

namespace {

constexpr int kColMdp = 4;
constexpr int kRangeeMdp = 11;  // et 12 : 44 caractères sur deux rangées de 22
constexpr uint8_t kLargeurMdp = 22;

// « nas.local:22 », coupé à `colonnes` caractères.
void hotePort(char* dest, size_t taille, int colonnes) {
  const annuaire::Fiche& f = appel::fiche();
  char t[72];
  snprintf(t, sizeof(t), "%s:%u", f.hote, f.port ? f.port : 22);
  strlcpy(dest, t, min(taille, size_t(colonnes) + 1));
}

// « ssh-ed25519 » → « ED25519 », « ecdsa-sha2-nistp256 » → « ECDSA ».
void nomCle(const char* type, char* dest, size_t taille) {
  if (strncmp(type, "ssh-", 4) == 0) type += 4;
  size_t k = 0;
  for (; type[k] && type[k] != '-' && k + 1 < taille; ++k) dest[k] = char(toupper(static_cast<unsigned char>(type[k])));
  dest[k] = '\0';
}

void abandon(Minitel& m) {
  m.newXY(14, charte::kRangeeRappel);
  charte::touche(m, "CONNEXION/FIN");
  m.attributs(charte::kLibelle);
  m.print(" : abandon");
}

}  // namespace

// ------------------------------------------------------------------ P71

void PageIdentification::entrer(pages::Moteur&) {
  _champ.configurer(kColMdp, kRangeeMdp, 44, false, kLargeurMdp);
  _champ.masquer(true);
}

void PageIdentification::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::bandeau(m, "IDENTIFICATION", nullptr);
  char t[40];
  nomAffiche(appel::fiche().nom, t, sizeof(t), 26);
  charte::ligneInfo(m, 6, "Service", t);
  hotePort(t, sizeof(t), 26);
  charte::ligneInfo(m, 7, "Hôte", t);
  nomAffiche(appel::fiche().utilisateur, t, sizeof(t), 26);
  charte::ligneInfo(m, 8, "Utilisateur", t);

  charte::texte(m, 2, kRangeeMdp - 1, "Mot de passe :");
  _champ.dessiner(m);
  if (appel::essai() > 1) {
    snprintf(t, sizeof(t), "Refusé : essai %d sur 3.", appel::essai());
    charte::texte(m, 2, 14, t, CARACTERE_ROUGE);
  }
  charte::texte(m, 2, 16, "Majuscule : Maj + lettre.", charte::kLibelle);
  charte::texte(m, 2, 17, "Chaque lettre reste visible", charte::kLibelle);
  charte::texte(m, 2, 18, "un instant. Rien n'est mémorisé.", charte::kLibelle);

  charte::filet(m, 21);
  m.newXY(2, charte::kRangeeInvite);
  m.attributs(charte::kValeur);
  m.print("Se connecter     → ");
  charte::touche(m, "ENVOI");
  m.newXY(2, 23);
  m.attributs(charte::kValeur);
  m.print("Renoncer         → ");
  charte::touche(m, "ANNULATION");
  abandon(m);
}

void PageIdentification::replacerCurseur(pages::Moteur& mo) { _champ.placerCurseur(mo.ecran()); }

bool PageIdentification::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (evt.est(mio::Touche::Envoi)) {
    if (_champ.vide()) {
      mo.message("Tapez le mot de passe");
      return true;
    }
    appel::repondreMotDePasse(_champ.texte());
    _champ.vider();
    mo.retour();
    return true;
  }
  if (evt.est(mio::Touche::ConnexionFin)) {
    appel::abandonner();
    mo.retour();
    return true;
  }
  if (evt.est(mio::Touche::Guide)) return false;
  const pages::Champ::Resultat r = _champ.touche(mo.ecran(), evt);
  if (_champ.dernierEnClair()) _clairDepuis = millis();
  if (r == pages::Champ::Resultat::Abandon) {
    appel::repondreMotDePasse(nullptr);
    mo.retour();
  } else if (r == pages::Champ::Resultat::Ignore) {
    mo.ecran().bip();
  }
  return true;
}

void PageIdentification::tic(pages::Moteur& mo) {
  // L'appel a pu finir sans nous (délai dépassé, serveur parti) : P70 conclut.
  if (!appel::attendReponse()) {
    mo.retour();
    return;
  }
  if (_champ.dernierEnClair() && millis() - _clairDepuis > kClairMs) _champ.cacherDernier(mo.ecran());
}

const char* PageIdentification::aide() const {
  return "Le serveur SSH demande le mot\n"
         "de passe de cet utilisateur.\n"
         "Il s'affiche en étoiles ; la\n"
         "dernière lettre tapée reste\n"
         "visible un instant pour en\n"
         "vérifier la casse.\n"
         "\n"
         "ENVOI se connecte. Après trois\n"
         "refus, l'appel échoue.\n"
         "ANNULATION sur le champ vide\n"
         "renonce à l'appel.\n"
         "\n"
         "Pour ne plus le taper, mettez-le\n"
         "dans la fiche du service (P22).";
}

// ------------------------------------------------------------------ P72

void PageCle::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::bandeau(m, "SECURITE", nullptr);
  char t[52];
  charte::texte(m, 2, 6, "Première connexion à :");
  hotePort(t, sizeof(t), 36);
  charte::texte(m, 4, 7, t);

  char type[16];
  nomCle(appel::typeCle(), type, sizeof(type));
  snprintf(t, sizeof(t), "Empreinte de la clé (%s) :", type);
  charte::texte(m, 2, 9, t);
  // SHA256: et 43 caractères : 34 sur la première rangée, la suite dessous.
  const char* e = appel::empreinte();
  strlcpy(t, e, min(sizeof(t), size_t(35)));
  charte::texte(m, 4, 10, t, CARACTERE_VERT);
  if (strlen(e) > 34) charte::texte(m, 4, 11, e + 34, CARACTERE_VERT);

  charte::texte(m, 2, 13, "Vérifiez-la sur le serveur :");
  charte::texte(m, 2, 14, "ssh-keygen -lf <clé publique>", charte::kLibelle);
  charte::texte(m, 2, 16, "Une empreinte différente lors d'une");
  charte::texte(m, 2, 17, "prochaine connexion bloquera l'appel.");

  charte::filet(m, 21);
  m.newXY(2, charte::kRangeeInvite);
  m.attributs(charte::kValeur);
  m.print("Accepter et mémoriser → ");
  charte::touche(m, "ENVOI");
  m.newXY(2, 23);
  m.attributs(charte::kValeur);
  m.print("Refuser               → ");
  charte::touche(m, "ANNULATION");
  abandon(m);
}

bool PageCle::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (evt.est(mio::Touche::Envoi) || evt.est(mio::Touche::Annulation)) {
    appel::repondreCle(evt.est(mio::Touche::Envoi));
    mo.retour();
    return true;
  }
  if (evt.est(mio::Touche::ConnexionFin)) {
    appel::abandonner();
    mo.retour();
    return true;
  }
  if (evt.est(mio::Touche::Guide)) return false;
  mo.ecran().bip();
  return true;
}

void PageCle::tic(pages::Moteur& mo) {
  if (!appel::attendReponse()) mo.retour();
}

const char* PageCle::aide() const {
  return "Ce serveur SSH n'a jamais été\n"
         "appelé : sa clé est inconnue.\n"
         "\n"
         "Comparez l'empreinte avec celle\n"
         "du serveur (ssh-keygen -lf sur\n"
         "sa clé publique, dans /etc/ssh).\n"
         "\n"
         "ENVOI l'accepte et la mémorise ;\n"
         "aux appels suivants, une clé\n"
         "différente bloquera l'appel.\n"
         "ANNULATION refuse et raccroche.";
}
