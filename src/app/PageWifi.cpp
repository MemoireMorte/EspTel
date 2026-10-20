#include "../config.h"

#include "../Pages/Charte.h"
#include "Pages.h"
#include "Reseau.h"

namespace {

constexpr uint32_t kPeriodeMajMs = 1000;
constexpr uint32_t kPeriodeSignalMs = 10000;  // le RSSI bouge sans cesse : relu rarement
constexpr int kRangeeEtat = 6;                // état, réseau, signal, adresse, cause
constexpr int kColChamp = 10;

}  // namespace

void nomAffiche(const char* ssid, char* dest, size_t taille, int colonnes) {
  size_t n = 0;
  for (const uint8_t* p = reinterpret_cast<const uint8_t*>(ssid); *p && n + 1 < taille && int(n) < colonnes; ++p) {
    if ((*p & 0xC0) == 0x80) continue;  // suite d'un caractère UTF-8 déjà compté
    // ^ et ` n'ont pas leur dessin habituel en G0 Vidéotex.
    const bool ascii = *p >= 0x20 && *p <= 0x7E && *p != '^' && *p != '`';
    dest[n++] = ascii ? static_cast<char>(*p) : '?';
  }
  dest[n] = '\0';
}

void PageWifi::entrer(pages::Moteur&) { _champ.configurer(kColChamp, charte::kRangeeInvite, 1, true); }

void PageWifi::dessiner(pages::Moteur& mo) {
  Minitel& m = mo.ecran();
  charte::bandeau(m, "RESEAU WIFI");

  charte::texte(m, 2, kRangeeEtat, "État", charte::kLibelle);
  charte::texte(m, 2, kRangeeEtat + 1, "Réseau", charte::kLibelle);
  charte::texte(m, 2, kRangeeEtat + 2, "Signal", charte::kLibelle);
  charte::texte(m, 2, kRangeeEtat + 3, "Adresse", charte::kLibelle);
  for (auto& ligne : _affiche) ligne[0] = '\0';
  _rssi = reseau::rssi();
  _prochainSignal = millis() + kPeriodeSignalMs;
  dessinerEtat(mo, true);

  charte::choix(m, 12, '1', "Rechercher les réseaux");
  charte::choix(m, 14, '2', "Réseaux mémorisés");
  charte::choix(m, 16, '3', "Se reconnecter");

  charte::filet(m, 21);
  charte::texte(m, 2, charte::kRangeeInvite, "Choix :");
  _champ.dessiner(m);
  m.newXY(kColChamp + 2, charte::kRangeeInvite);
  m.attributs(charte::kValeur);
  m.print("puis ");
  charte::touche(m, "ENVOI");
  charte::rappel(m);
}

void PageWifi::dessinerEtat(pages::Moteur& mo, bool forcer) {
  _prochaineMaj = millis() + kPeriodeMajMs;
  if (static_cast<int32_t>(millis() - _prochainSignal) >= 0) {
    _prochainSignal = millis() + kPeriodeSignalMs;
    _rssi = reseau::rssi();
  }
  const reseau::Etat e = reseau::etat();
  if (e != reseau::Etat::Connecte) _rssi = 0;
  else if (_rssi == 0) _rssi = reseau::rssi();

  char valeurs[5][40];
  switch (e) {
    case reseau::Etat::NonConfigure: strlcpy(valeurs[0], "aucun réseau mémorisé", 40); break;
    case reseau::Etat::Connexion: strlcpy(valeurs[0], "connexion...", 40); break;
    case reseau::Etat::Connecte: strlcpy(valeurs[0], "connecté", 40); break;
    case reseau::Etat::Echec: strlcpy(valeurs[0], "échec", 40); break;
  }
  strlcpy(valeurs[4], e == reseau::Etat::Echec ? reseau::raison() : "", 40);
  nomAffiche(e == reseau::Etat::NonConfigure ? "" : reseau::ssid(), valeurs[1], 40, 26);
  if (!valeurs[1][0]) strlcpy(valeurs[1], "-", 40);
  snprintf(valeurs[2], 40, "%d", _rssi);
  const String ip = reseau::ip();
  snprintf(valeurs[3], 40, "%s", ip.length() ? ip.c_str() : "-");

  Minitel& m = mo.ecran();
  bool ecrit = false;
  for (int i = 0; i < 5; ++i) {
    if (!forcer && strcmp(valeurs[i], _affiche[i]) == 0) continue;
    strlcpy(_affiche[i], valeurs[i], sizeof(_affiche[i]));
    ecrit = true;
    const int rangee = kRangeeEtat + i;
    if (i == 4) {  // cause de l'échec, seulement quand il y en a une
      charte::effacer(m, 2, rangee);
      if (valeurs[4][0]) charte::ligneInfo(m, rangee, "Cause", valeurs[4]);
      continue;
    }
    m.newXY(14, rangee);
    if (i == 0) {
      m.attributs(e == reseau::Etat::Connecte ? CARACTERE_VERT
                  : e == reseau::Etat::Echec  ? CARACTERE_ROUGE
                                              : charte::kValeur);
      m.print(valeurs[0]);
    } else if (i == 2) {
      if (_rssi) {
        charte::barres(m, reseau::barres(_rssi));
        m.attributs(charte::kValeur);
        char t[16];
        snprintf(t, sizeof(t), "  %d dBm", _rssi);
        m.print(t);
      } else {
        m.attributs(charte::kValeur);
        m.print("-");
      }
    } else {
      m.attributs(charte::kValeur);
      m.print(valeurs[i]);
    }
    m.cancel();
  }
  if (ecrit && !forcer) replacerCurseur(mo);
}

void PageWifi::tic(pages::Moteur& mo) {
  if (static_cast<int32_t>(millis() - _prochaineMaj) < 0) return;
  dessinerEtat(mo, false);
}

void PageWifi::replacerCurseur(pages::Moteur& mo) { _champ.placerCurseur(mo.ecran()); }

bool PageWifi::touche(pages::Moteur& mo, const mio::Evenement& evt) {
  if (evt.est(mio::Touche::Envoi)) {
    valider(mo);
    return true;
  }
  return _champ.touche(mo.ecran(), evt) != pages::Champ::Resultat::Ignore;
}

void PageWifi::valider(pages::Moteur& mo) {
  const char c = _champ.texte()[0];
  _champ.vider(&mo.ecran());
  switch (c) {
    case '1':
      mo.aller(pageReseaux);
      return;
    case '2':
      mo.aller(pageMemorises);
      return;
    case '3':
      if (reseau::reconnecter()) dessinerEtat(mo, false);
      else mo.message("Aucun réseau mémorisé");
      return;
    default:
      mo.ecran().bip();
      return;
  }
}

const char* PageWifi::aide() const {
  return "État de la connexion WiFi.\n"
         "\n"
         "1 cherche les réseaux autour et\n"
         "  permet de s'y connecter.\n"
         "2 liste les réseaux mémorisés :\n"
         "  s'y connecter, ou les oublier.\n"
         "3 essaie à nouveau les réseaux\n"
         "  mémorisés, le dernier d'abord.\n"
         "\n"
         "Au démarrage, le Minitel se\n"
         "connecte tout seul au dernier\n"
         "réseau qui a marché.";
}
