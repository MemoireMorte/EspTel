#pragma once

// Les pages du firmware. Codes et maquettes : plan, §6 et §7.

#include "../Pages/Champ.h"
#include "../Pages/Moteur.h"
#include "Annuaire.h"

// P00 : mire d'accueil. Passe au Sommaire sur ENVOI ou après quelques secondes.
class PageMire : public pages::Page {
 public:
  PageMire() : Page("P00") {}
  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void tic(pages::Moteur& m) override;
  const char* aide() const override;
  bool rangeeZeroMasquee() const override;

 private:
  void dessinerReseau(pages::Moteur& m);
  void dessinerInvite(Minitel& m);
  uint32_t _debut = 0;
  int _etatAffiche = -1;
  bool _fichier = false;  // mire en fichier (/vdt/MIRE.vdt)
};

// P11 : page locale en fichier .vdt (Vdt.h), ouverte depuis le Sommaire par son
// nom. RETOUR ou SOMMAIRE la quittent.
class PageVdt : public pages::Page {
 public:
  PageVdt() : Page("P11") {}
  void choisir(const char* nom) { strlcpy(_nom, nom, sizeof(_nom)); }
  void dessiner(pages::Moteur& m) override;
  bool rangeeZeroMasquee() const override { return true; }
  const char* aide() const override;

 private:
  char _nom[12] = "";
};

// P10 : Sommaire. Un chiffre puis ENVOI ouvre une rubrique ; un code puis
// CONNEXION/FIN (ou ENVOI) appelle un service de l'annuaire, ou ouvre une page .vdt.
class PageSommaire : public pages::Page {
 public:
  PageSommaire() : Page("P10") {}
  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void replacerCurseur(pages::Moteur& m) override;
  const char* aide() const override;

 private:
  void valider(pages::Moteur& m, bool connexion);
  pages::Champ _champ;
};

// P60 : Informations (version, Minitel, mémoire, réseau, crédits).
class PageInfos : public pages::Page {
 public:
  PageInfos() : Page("P60") {}
  void dessiner(pages::Moteur& m) override;
  void tic(pages::Moteur& m) override;
  const char* aide() const override;

 private:
  void dessinerVariables(pages::Moteur& m, bool forcer);
  uint32_t _prochaineMaj = 0;
  char _affiche[5][40] = {{0}};
};

// P40 : état du WiFi et accès à la recherche et aux réseaux mémorisés.
class PageWifi : public pages::Page {
 public:
  PageWifi() : Page("P40") {}
  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void tic(pages::Moteur& m) override;
  void replacerCurseur(pages::Moteur& m) override;
  const char* aide() const override;

 private:
  void dessinerEtat(pages::Moteur& m, bool forcer);
  void valider(pages::Moteur& m);
  pages::Champ _champ;
  uint32_t _prochaineMaj = 0;
  uint32_t _prochainSignal = 0;
  int _rssi = 0;
  char _affiche[5][40] = {{0}};
};

// P41 : réseaux trouvés, 8 par page. N° + ENVOI pour se connecter.
class PageReseaux : public pages::Page {
 public:
  PageReseaux() : Page("P41") {}
  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void tic(pages::Moteur& m) override;
  void replacerCurseur(pages::Moteur& m) override;
  const char* aide() const override;

 private:
  void dessinerListe(pages::Moteur& m);
  void valider(pages::Moteur& m);
  int nbPages() const;
  pages::Champ _champ;
  int _page = 0;
  bool _rechercheAffichee = false;
};

// P42 : mot de passe d'un réseau choisi en P41, ou nom et mot de passe d'un
// réseau caché.
class PageMotDePasse : public pages::Page {
 public:
  PageMotDePasse() : Page("P42") {}
  void preparer(const char* ssid, uint8_t secu, const char* mdpConnu);
  void preparerCache();

  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void replacerCurseur(pages::Moteur& m) override;
  const char* aide() const override;

 private:
  void valider(pages::Moteur& m);
  pages::Champ& actif() { return _cache && _surNom ? _nom : _mdp; }
  char _ssid[33] = "";
  char _mdpConnu[65] = "";
  uint8_t _secu = 0;
  bool _cache = false;
  bool _surNom = false;
  pages::Champ _nom;
  pages::Champ _mdp;
};

// P43 : réseaux mémorisés. N° + ENVOI se connecte, S et N° oublie.
class PageMemorises : public pages::Page {
 public:
  PageMemorises() : Page("P43") {}
  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void replacerCurseur(pages::Moteur& m) override;
  const char* aide() const override;

 private:
  void valider(pages::Moteur& m);
  pages::Champ _champ;
};

// P20 : annuaire des services, 8 par page. N° + ENVOI : fiche ; N° +
// CONNEXION/FIN : appel ; + ENVOI : nouvelle fiche.
class PageAnnuaire : public pages::Page {
 public:
  PageAnnuaire() : Page("P20") {}
  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void replacerCurseur(pages::Moteur& m) override;
  const char* aide() const override;

 private:
  void dessinerListe(pages::Moteur& m);
  void valider(pages::Moteur& m, bool appel);
  int nbPages() const;
  pages::Champ _champ;
  int _page = 0;
};

// P21 : fiche d'un service. CONNEXION/FIN appelle ; M modifie, S supprime.
class PageFiche : public pages::Page {
 public:
  PageFiche() : Page("P21") {}
  void choisir(int indice) { _indice = indice; }
  int indice() const { return _indice; }
  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void replacerCurseur(pages::Moteur& m) override;
  const char* aide() const override;

 private:
  pages::Champ _champ;
  int _indice = 0;
};

// P22 : création ou modification d'une fiche, sur deux écrans.
class PageEdition : public pages::Page {
 public:
  static constexpr int kNbChamps = 16;

  PageEdition() : Page("P22") {}
  void nouvelle(const annuaire::Fiche* modele);  // modele : fiche pré-remplie (après P30), ou nullptr
  void modifier(int indice);
  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void replacerCurseur(pages::Moteur& m) override;
  const char* aide() const override;

 private:
  bool visible(int i) const;
  int ecranDe(int i) const;
  void aller(pages::Moteur& m, int i, int sens);
  void remplir(const annuaire::Fiche& f);
  bool lire(pages::Moteur& m, annuaire::Fiche& f);
  void enregistrer(pages::Moteur& m);
  void quitter(pages::Moteur& m);

  pages::Champ _champs[kNbChamps];
  annuaire::Fiche _modele;
  int _indice = -1;  // -1 : nouvelle fiche
  int _actif = 0;
  int _ecran = 0;
  char _protoAffiche = 0;
};

// P23 : suppression d'une fiche, avec confirmation.
class PageSuppression : public pages::Page {
 public:
  PageSuppression() : Page("P23") {}
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  const char* aide() const override;
};

// P30 : connexion directe à une adresse tapée.
class PageDirecte : public pages::Page {
 public:
  PageDirecte() : Page("P30") {}
  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void replacerCurseur(pages::Moteur& m) override;
  const char* aide() const override;

 private:
  annuaire::Affichage affichageAuto(pages::Moteur& mo) const;  // d'après le préfixe tapé
  void dessinerAffichage(pages::Moteur& mo);
  pages::Champ _champ;
  annuaire::Affichage _affichage = annuaire::Affichage::Videotex;
  bool _affichageChoisi = false;  // SUITE pressée : le préfixe ne décide plus
};

// P70 : connexion en cours. CONNEXION/FIN abandonne.
class PageAppel : public pages::Page {
 public:
  PageAppel() : Page("P70") {}
  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void tic(pages::Moteur& m) override;
  bool sommaireAutorise() const override { return false; }
  bool ligneService(char* dest, size_t n) override;
  const char* aide() const override;

 private:
  void dessinerEtape(pages::Moteur& m);
  int _etape = -1;
  int _barre = 0;
  int _bips = 2;  // 0 : bruit de connexion à jouer
};

// P73 : échec de connexion. RÉPÉTITION réessaie.
class PageEchec : public pages::Page {
 public:
  PageEchec() : Page("P73") {}
  void entrer(pages::Moteur&) override { _oubliee = false; }
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  const char* aide() const override;

 private:
  bool _oubliee = false;  // clé SSH changée : l'ancienne vient d'être oubliée (O)
};

// P71 : SSH, mot de passe demandé pendant l'appel (pas de mot de passe dans la
// fiche, ou mot de passe refusé). La connexion attend, ouverte.
class PageIdentification : public pages::Page {
 public:
  PageIdentification() : Page("P71") {}
  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void tic(pages::Moteur& m) override;
  void replacerCurseur(pages::Moteur& m) override;
  bool sommaireAutorise() const override { return false; }
  const char* aide() const override;

 private:
  static constexpr uint32_t kClairMs = 1500;  // dernier caractère tapé visible
  pages::Champ _champ;
  uint32_t _clairDepuis = 0;
};

// P72 : SSH, première connexion à un hôte. Son empreinte est montrée pour être
// vérifiée, puis mémorisée (TOFU).
class PageCle : public pages::Page {
 public:
  PageCle() : Page("P72") {}
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void tic(pages::Moteur& m) override;
  bool sommaireAutorise() const override { return false; }
  const char* aide() const override;
};

// P90 : fin de connexion, durée et coût d'époque. Retour automatique.
class PageFin : public pages::Page {
 public:
  PageFin() : Page("P90") {}
  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void tic(pages::Moteur& m) override;
  const char* aide() const override;

 private:
  uint32_t _debut = 0;
};

// Lance l'appel d'un service (P70). indice : rang dans l'annuaire, -1 sinon.
// remplacer : P70 prend la place de la page courante (nouvel essai depuis P73).
void appeler(pages::Moteur& m, const annuaire::Fiche& f, int indice, bool remplacer = false);

// Nom de réseau affichable : ASCII seulement (un « ? » par caractère qui ne
// l'est pas), coupé à `colonnes`.
void nomAffiche(const char* ssid, char* dest, size_t taille, int colonnes);

// P50 : réglages. N° + ENVOI fait défiler la valeur ; 9 mène à P54.
class PageParametres : public pages::Page {
 public:
  PageParametres() : Page("P50") {}
  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void replacerCurseur(pages::Moteur& m) override;
  const char* aide() const override;

 private:
  void dessinerValeur(pages::Moteur& m, int i);
  pages::Champ _champ;
};

// P52 : portail web. Ouvert tant que la page est affichée (main.cpp le ferme
// dès qu'elle ne l'est plus) ; elle montre l'adresse, le code et l'activité.
class PagePortail : public pages::Page {
 public:
  PagePortail() : Page("P52") {}
  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  void tic(pages::Moteur& m) override;
  const char* aide() const override;

  void dessinerActivite(pages::Moteur& m);  // aussi pendant une mise à jour

 private:
  uint32_t _version = 0;
  uint32_t _prochainEssai = 0;
  int _barre = -1;  // cases de la barre déjà dessinées
};

// P54 : réinitialisation, avec confirmation par ENVOI.
class PageReinitialisation : public pages::Page {
 public:
  PageReinitialisation() : Page("P54") {}
  void entrer(pages::Moteur& m) override;
  void dessiner(pages::Moteur& m) override;
  bool touche(pages::Moteur& m, const mio::Evenement& evt) override;
  void replacerCurseur(pages::Moteur& m) override;
  const char* aide() const override;

 private:
  void dessinerConfirmation(pages::Moteur& m);
  void effacer(pages::Moteur& m);
  pages::Champ _champ;
  int _choix = -1;  // effacement en attente de confirmation
};

extern PageMire pageMire;
extern PageSommaire pageSommaire;
extern PageInfos pageInfos;
extern PageWifi pageWifi;
extern PageReseaux pageReseaux;
extern PageMotDePasse pageMotDePasse;
extern PageMemorises pageMemorises;
extern PageAnnuaire pageAnnuaire;
extern PageFiche pageFiche;
extern PageEdition pageEdition;
extern PageSuppression pageSuppression;
extern PageDirecte pageDirecte;
extern PageAppel pageAppel;
extern PageEchec pageEchec;
extern PageIdentification pageIdentification;
extern PageCle pageCle;
extern PageFin pageFin;
extern PageParametres pageParametres;
extern PageReinitialisation pageReinitialisation;
extern PagePortail pagePortail;
extern PageVdt pageVdt;
