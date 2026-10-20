#pragma once

// Moteur de pages : pile de navigation, touches communes, GUIDE et rangée 0.
//
// Une page dessine son écran et répond à ses touches. Ce qu'elle ne traite pas
// (touche() renvoie false) prend le comportement commun du plan (§5) :
//
//   SOMMAIRE     retour au Sommaire, la pile est vidée jusqu'à lui
//   RETOUR       page précédente
//   GUIDE        aide de la page (P99) ; RETOUR ou GUIDE la referme
//   RÉPÉTITION   réaffiche la page entière
//   autre        bip
//
// La rangée 0 affiche au repos l'état fourni par l'application (WiFi, heure),
// et par moments un message (« Code inconnu ») qui s'efface tout seul.

#include <Arduino.h>

#include <initializer_list>
#include <Minitel1B_Hard.h>

#include "../MinitelIO/MinitelIO.h"

namespace pages {

class Moteur;

class Page {
 public:
  explicit Page(const char* code) : _code(code) {}
  virtual ~Page() = default;

  const char* code() const { return _code; }

  // Appelée quand la page arrive au sommet de la pile (pas au réaffichage).
  virtual void entrer(Moteur&) {}
  // Dessine la page entière ; l'écran vient d'être effacé.
  virtual void dessiner(Moteur& m) = 0;
  // true si la page a traité la touche.
  virtual bool touche(Moteur&, const mio::Evenement&) { return false; }
  // Appelée à chaque tour de boucle tant que la page est affichée.
  virtual void tic(Moteur&) {}
  // Remet le curseur là où la page l'attend (après une écriture en rangée 0).
  virtual void replacerCurseur(Moteur& m);
  // Texte du GUIDE, lignes séparées par '\n' ; nullptr si la page n'en a pas.
  virtual const char* aide() const { return nullptr; }
  // P70 refuse SOMMAIRE.
  virtual bool sommaireAutorise() const { return true; }
  // Rangée 0 propre à la page (P70 : service appelé) ; false pour l'état de repos.
  virtual bool ligneService(char*, size_t) { return false; }
  // true : rangée 0 laissée vide (page .vdt plein écran) ; seuls les messages
  // temporaires y passent.
  virtual bool rangeeZeroMasquee() const { return false; }

 private:
  const char* _code;
};

// Fabrique le texte de la rangée 0 au repos (39 caractères au plus).
using EtatRepos = void (*)(char* dest, size_t taille);

class Moteur {
 public:
  static constexpr int kProfondeurMax = 8;
  static constexpr uint32_t kDureeMessageMs = 5000;

  explicit Moteur(mio::MinitelIO& io) : _io(io) {}

  void demarrer(Page& premiere, Page& sommaire, EtatRepos etat);
  void boucle();  // à appeler dans loop()

  void aller(Page& p);       // empile
  void remplacer(Page& p);   // remplace le sommet (la mire cède la place au Sommaire)
  void retour();             // dépile, bip au fond de la pile
  void retourA(Page& p);     // dépile jusqu'à p (empile p s'il n'y est pas)
  // Ouvre une page profonde d'un coup : Sommaire, puis les pages données, la
  // dernière seule dessinée (P00 en échec WiFi -> P10 > P40 > P41).
  void chemin(std::initializer_list<Page*> pages);
  void sommaire();
  void reafficher();

  // Message temporaire en rangée 0, avec bip par défaut (saisie invalide).
  void message(const char* texteUtf8, bool bip = true);

  Minitel& ecran() { return _io.ecran(); }
  mio::MinitelIO& io() { return _io; }
  Page* courante() { return _n ? _pile[_n - 1] : nullptr; }

 private:
  void afficher();
  void afficherGuide();
  bool toucheGuide(const mio::Evenement& evt);
  void toucheCommune(const mio::Evenement& evt);
  void rangeeZero(bool forcer);

  mio::MinitelIO& _io;
  Page* _pile[kProfondeurMax] = {nullptr};
  int _n = 0;
  Page* _sommaire = nullptr;
  EtatRepos _etat = nullptr;
  bool _guide = false;

  char _message[48] = {0};
  uint32_t _finMessage = 0;
  char _ligneAffichee[48] = {0};
  uint32_t _prochainEtat = 0;
};

}  // namespace pages
