#include "../config.h"

#include "Reglages.h"

#include <Preferences.h>

namespace reglages {

namespace {

constexpr const char* kEspace = "reglages";
Valeurs courantes;

}  // namespace

void charger() {
  Preferences p;
  if (!p.begin(kEspace, true)) return;  // jamais enregistré : valeurs par défaut
  const Valeurs d;
  courantes.bruit = p.getUChar("bruit", d.bruit) != 0;
  courantes.cout = p.getUChar("cout", d.cout) != 0;
  courantes.mireS = p.getUChar("mire", d.mireS);
  courantes.finS = p.getUChar("fin", d.finS);
  // La vitesse en centaines de bauds tient dans un octet : 12, 48, 96.
  courantes.vitesse = uint16_t(p.getUChar("vitesse", d.vitesse / 100)) * 100;
  p.end();
}

const Valeurs& lire() { return courantes; }

void enregistrer(const Valeurs& v) {
  courantes = v;
  Preferences p;
  p.begin(kEspace, false);
  p.putUChar("bruit", v.bruit);
  p.putUChar("cout", v.cout);
  p.putUChar("mire", v.mireS);
  p.putUChar("fin", v.finS);
  p.putUChar("vitesse", uint8_t(v.vitesse / 100));
  p.end();
}

int vitesseVisee() { return courantes.vitesse ? courantes.vitesse : MINITEL_VITESSE_MAX; }

void effacerTout() {
  static const char* const kEspaces[] = {"reglages", "annuaire", "wifi", "ssh_hotes"};
  for (const char* e : kEspaces) {
    Preferences p;
    p.begin(e, false);
    p.clear();
    p.end();
  }
  // Sans cette marque, le réseau de src/secrets.h amorcerait de nouveau la liste au
  // redémarrage (Reseau.cpp) : le WiFi semblerait ne pas avoir été effacé.
  Preferences p;
  p.begin("wifi", false);
  p.putUChar("init", 1);
  p.end();
}

}  // namespace reglages
