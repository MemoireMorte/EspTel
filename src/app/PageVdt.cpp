#include "../config.h"

#include "../Pages/Charte.h"
#include "Pages.h"
#include "Vdt.h"

void PageVdt::dessiner(pages::Moteur& mo) {
  if (vdt::afficher(mo.io(), _nom)) return;
  // Supprimée depuis le portail entre-temps.
  Minitel& m = mo.ecran();
  charte::bandeau(m, "PAGE LOCALE");
  char t[40];
  snprintf(t, sizeof(t), "La page %s n'existe plus.", _nom);
  charte::texte(m, 2, 8, t);
  charte::rappel(m);
}

const char* PageVdt::aide() const {
  return "Page locale : un fichier .vdt\n"
         "déposé par le portail web\n"
         "(Paramètres, choix 6), affiché\n"
         "quand son nom est tapé au\n"
         "Sommaire.\n"
         "\n"
         "RETOUR ou SOMMAIRE la quittent.";
}
