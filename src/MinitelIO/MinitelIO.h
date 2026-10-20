#pragma once

// MinitelIO : tout ce qui passe par la prise péri-informatique.
//
// - détection de la vitesse sans jamais bloquer (Minitel éteint ou absent compris),
// - identification du modèle (ENQROM) et des capacités qui en découlent,
// - lecture du clavier non bloquante, avec décodage des touches de fonction
//   (préfixe 0x13), des caractères accentués (SS2) et du clavier étendu (ESC [),
// - écriture brute et écriture de la rangée 0 (ligne de service).
//
// L'affichage s'appuie sur Minitel1B_Hard (fork iodeo), accessible par ecran().

#include <Arduino.h>
#include <Minitel1B_Hard.h>

namespace mio {

enum class Touche : uint8_t {
  Aucune,
  Envoi, Retour, Repetition, Guide, Annulation, Sommaire, Correction, Suite,
  ConnexionFin,
  Haut, Bas, Droite, Gauche,
};

enum class Type : uint8_t {
  Caractere,   // caractère imprimable, unicode renseigné (accents compris)
  Controle,    // code de contrôle seul (Ctrl+R = 0x12, ESC isolé...)
  Fonction,    // touche de fonction Minitel (0x13 xx)
  Etendu,      // clavier étendu (ESC [ ...), flèches comprises
  Protocole,   // réponse du Minitel à une commande PRO (ESC 39/3A/3B ...)
  Inconnu,     // séquence incomplète ou non reconnue
};

struct Evenement {
  Type type = Type::Inconnu;
  Touche touche = Touche::Aucune;
  uint32_t unicode = 0;        // pour Caractere et Controle
  uint8_t brut[12] = {0};      // octets reçus tels quels (à retransmettre en Vidéotex)
  uint8_t nbBrut = 0;

  bool est(Touche t) const { return touche == t; }
};

struct Identite {
  bool valide = false;
  uint8_t constructeur = 0;
  uint8_t type = 0;
  uint8_t version = 0;

  const char* modele() const;        // "Minitel 2", "Minitel 1B"...
  const char* nomConstructeur() const;
  bool estMinitel1() const;          // Minitel 1 d'origine : 1200 bauds, pas de 80 colonnes
  int vitesseMax() const;            // vitesse maximale connue de la prise pour ce modèle
};

class MinitelIO {
 public:
  MinitelIO(HardwareSerial& uart, int8_t rx, int8_t tx);

  // Détecte la vitesse courante, monte jusqu'à vitesseVisee si le modèle le permet,
  // identifie le Minitel et prépare le clavier. Ne bloque jamais plus de quelques
  // secondes, même sans Minitel. Renvoie la vitesse finale.
  int demarrer(int vitesseVisee);

  // Minitel resté muet au démarrage (allumé après l'ESP32) : une demande de vitesse
  // à la vitesse courante, bloquante delaiMs au plus. True s'il répond enfin ;
  // il reste alors à rappeler demarrer().
  bool sonder(uint32_t delaiMs);

  int vitesse() const { return _vitesse; }
  int vitesseDetectee() const { return _vitesseDetectee; }
  bool repond() const { return _repond; }        // le Minitel a répondu à la demande de vitesse
  const Identite& identite() const { return _identite; }
  bool avance() const { return _avance; }        // 1B ou mieux : 80 colonnes, minuscules, clavier étendu

  // Prépare le clavier pour les pages locales : pas d'écho local, clavier étendu
  // et minuscules si le modèle sait faire.
  void clavierPagesLocales();

  // Lecture non bloquante : renvoie true quand un événement complet est disponible.
  bool lire(Evenement& evt);

  void ecrire(uint8_t b);
  void ecrire(const uint8_t* donnees, size_t n);

  // Rangée 0 (ligne de service) : écrit le texte puis sort par LF, ce qui doit
  // ramener le curseur à sa position antérieure (vérifié sur 1B et 2).
  void ligneService(const char* texteUtf8);

  Minitel& ecran() { return *_m; }
  HardwareSerial& uart() { return _uart; }

  static const char* nomTouche(Touche t);
  // Décrit l'événement en clair, ex. "ENVOI", "é", "Ctrl+R", "Flèche haut".
  static void decrire(const Evenement& evt, char* dest, size_t taille);

 private:
  int sonderVitesse(uint32_t delaiMs);
  int detecterVitesse();
  void viderEntree();
  bool terminer(Evenement& evt, Type type, Touche touche = Touche::Aucune, uint32_t unicode = 0);
  bool decoder(Evenement& evt);

  HardwareSerial& _uart;
  int8_t _rx, _tx;
  Minitel* _m = nullptr;
  int _vitesse = 1200;
  int _vitesseDetectee = -1;
  bool _repond = false;
  bool _avance = false;
  Identite _identite;

  uint8_t _seq[12] = {0};
  uint8_t _n = 0;
  uint32_t _t0 = 0;

  // Modem du Minitel recoupé après CONNEXION/FIN, compte rendu pas encore reçu.
  bool _modemCoupe = false;
  uint32_t _appuiMs = 0;
};

}  // namespace mio
