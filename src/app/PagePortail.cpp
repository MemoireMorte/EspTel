#include "../config.h"

#include "../Pages/Charte.h"
#include "Pages.h"
#include "Portail.h"
#include "Reseau.h"

namespace {

constexpr int kRangeeActivite = 15;  // titre, puis kLignesJournal lignes
constexpr int kRangeeBarre = 17;
constexpr int kRangeeEtatMaj = 19;
constexpr int kColBarre = 2;
constexpr int kCasesBarre = 30;
constexpr uint32_t kEssaiMs = 2000;  // WiFi absent : nouvel essai d'ouverture

pages::Moteur* moteurP52 = nullptr;
portail::Maj majAffichee = portail::Maj::Aucune;

void progression() {
  if (moteurP52) pagePortail.dessinerActivite(*moteurP52);
}

}  // namespace

void PagePortail::entrer(pages::Moteur& mo) {
  moteurP52 = &mo;
  portail::surProgression(progression);
  portail::ouvrir(mo.io());
  _prochainEssai = millis() + kEssaiMs;
}

void PagePortail::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::bandeau(m, "PORTAIL WEB");
  _version = portail::version();
  _barre = -1;
  majAffichee = portail::Maj::Aucune;

  if (!portail::ouvert()) {
    charte::texte(m, 2, 8, "Le WiFi n'est pas connecté.");
    charte::texte(m, 2, 10, "Le portail s'ouvrira dès que la");
    charte::texte(m, 2, 11, "liaison sera établie (Sommaire, 3).");
  } else {
    charte::texte(m, 2, 6, "Depuis un navigateur sur le même");
    charte::texte(m, 2, 7, "réseau WiFi, ouvrez :");
    charte::grand(m, 2, 9, portail::adresse().c_str());
    m.newXY(2, 10);
    m.attributs(charte::kLibelle);
    m.print("ou ");
    m.attributs(charte::kValeur);
    m.print("http://");
    m.print(portail::kNomHote);
    m.print(".local");
    charte::ligneInfo(m, 12, "Identifiant", portail::kIdentifiant);
    charte::ligneInfo(m, 13, "Code", portail::code());
    dessinerActivite(mo);
  }

  charte::filet(m, 21);
  charte::texte(m, 2, charte::kRangeeInvite, "Fermé dès que vous quittez la page", charte::kLibelle);
  charte::rappel(m);
}

void PagePortail::dessinerActivite(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  const portail::Maj etat = portail::maj();

  if (etat == portail::Maj::Aucune) {
    charte::texte(m, 2, kRangeeActivite, "Activité", charte::kLibelle);
    for (int i = 0; i < portail::kLignesJournal; ++i) {
      charte::texte(m, 2, kRangeeActivite + 1 + i, portail::journal(i));
      m.cancel();
    }
    return;
  }

  // Mise à jour : la zone d'activité devient une barre de progression.
  if (_barre < 0) {
    for (int r = kRangeeActivite; r <= kRangeeEtatMaj; ++r) charte::effacer(m, 1, r);
    char t[40];
    snprintf(t, sizeof(t), "Mise à jour depuis %s", portail::majSource());
    charte::texte(m, 2, kRangeeActivite, t, charte::kTitre);
    // Le fond de la barre en bleu (gris foncé sur un écran monochrome).
    m.newXY(kColBarre, kRangeeBarre);
    m.graphicMode();
    m.attributs(CARACTERE_BLEU);
    m.graphic(0b111111);
    m.repeat(kCasesBarre - 1);
    m.textMode();
    _barre = 0;
  }
  const int cases = portail::majPourcent() * kCasesBarre / 100;
  if (cases > _barre) {
    m.newXY(kColBarre + _barre, kRangeeBarre);
    m.graphicMode();
    m.attributs(CARACTERE_JAUNE);
    m.graphic(0b111111);
    if (cases - _barre > 1) m.repeat(cases - _barre - 1);
    m.textMode();
    _barre = cases;
  }
  char p[8];
  snprintf(p, sizeof(p), "%3d %%", portail::majPourcent());
  charte::texte(m, kColBarre + kCasesBarre + 2, kRangeeBarre, p);

  if (etat != majAffichee) {
    majAffichee = etat;
    charte::effacer(m, 1, kRangeeEtatMaj);
    switch (etat) {
      case portail::Maj::EnCours:
        charte::texte(m, 2, kRangeeEtatMaj, "Ne coupez pas l'alimentation", CARACTERE_ROUGE);
        break;
      case portail::Maj::Reussie:
        charte::texte(m, 2, kRangeeEtatMaj, "Réussie : redémarrage...", CARACTERE_VERT);
        break;
      case portail::Maj::Echouee: {
        char t[48];
        snprintf(t, sizeof(t), "Refusée : %.26s", portail::majErreur());
        charte::texte(m, 2, kRangeeEtatMaj, t, CARACTERE_ROUGE);
        break;
      }
      case portail::Maj::Aucune: break;
    }
  }
}

void PagePortail::tic(pages::Moteur& mo) {
  if (!portail::ouvert()) {
    if (static_cast<int32_t>(millis() - _prochainEssai) < 0) return;
    _prochainEssai = millis() + kEssaiMs;
    if (portail::ouvrir(mo.io())) mo.reafficher();
    return;
  }
  if (portail::version() != _version) {
    _version = portail::version();
    dessinerActivite(mo);
  }
}

const char* PagePortail::aide() const {
  return "Le portail règle EspTel depuis un\n"
         "ordinateur ou un téléphone :\n"
         "annuaire (fiches, import, export\n"
         "JSON), réglages, réseaux WiFi et\n"
         "mise à jour du firmware.\n"
         "\n"
         "Le navigateur demande\n"
         "l'identifiant et le code affichés.\n"
         "Le code change à chaque ouverture.\n"
         "\n"
         "L'IDE Arduino voit aussi l'ESP32\n"
         "comme port réseau esptel ; le mot\n"
         "de passe est le même code.";
}
