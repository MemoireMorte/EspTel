# EspTel

Firmware pour ESP32 relié à la prise péri-informatique du Minitel : le Minitel
devient un terminal Telnet, WebSocket et SSH, piloté par des pages au look
Télétel (mire, Sommaire, annuaire, appel, fin de connexion chiffrée en francs).
Un portail web permet de tout régler depuis un navigateur et de mettre le
firmware à jour sans câble.

Matériel : la [carte Esp32-Minitel-dev-kit](https://github.com/MemoireMorte/Esp32-Minitel-dev-kit) ou un devkit ESP32-WROOM-32, câblés comme la [carte
de Iodeo](https://hackaday.io/project/180473-minitel-esp32) : UART0 sur l'USB (débogage), UART2 sur la prise du Minitel (broches
16/17). Le firmware ne pilote aucune autre broche : les GPIO libres restent à
l'utilisateur.

Version : **0.8.2-j8**. Minitels visés : 1B et 2 ; le Minitel 1 fonctionne mais en mode
dégradé (1200 bauds, 40 colonnes max) et avec une alimentation externe.

## Sommaire

1. [Installer et compiler](#installer-et-compiler)
2. [Premier démarrage](#premier-démarrage)
3. [Utilisation sur le Minitel](#utilisation-sur-le-minitel)
4. [Portail web](#portail-web)
5. [Ce que l'ESP32 mémorise](#ce-que-lesp32-mémorise)
6. [Arborescence](#arborescence)
7. [Essayer sur PC](#essayer-sur-pc)
8. [Choix techniques](#choix-techniques)
9. [Historique](#historique)

## Installer et compiler

Le dossier `EspTel` est un sketch Arduino : l'IDE ouvre `EspTel.ino` et
compile tout `src/`.

1. **Core ESP32 en 2.0.17** : Outils > Carte > Gestionnaire de carte, « esp32 »
   d'Espressif, version **2.0.17** (pas une 3.x).
2. **Bibliothèques** :
   - **LibSSH-ESP32** d'Ewan Parker, par le gestionnaire de bibliothèques
     (testé avec 5.10.0) ;
   - **WebSockets** de Markus Sattler, par le gestionnaire (testé avec 2.7.3) ;
   - **Minitel1B_Hard**, le fork d'iodeo : zip de
     https://github.com/iodeo/Minitel1B_Hard (Code > Download ZIP), puis
     Croquis > Inclure une bibliothèque > Ajouter la bibliothèque .ZIP. Il faut
     ce fork, pas l'original, pour le constructeur avec les broches de l'ESP32.
3. **Réglages de la carte**, menu Outils :
   - Carte : **ESP32 Dev Module** ou **ESP32 WROOM DA** ;
   - Partition Scheme : **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)** ;
   - Flash Mode : **DIO** ;
   - le reste par défaut. Moniteur série à **115200** bauds.

> Si la compilation échoue avec « Sketch too big ... Maximum is 1310720
> bytes », l'IDE est revenu au schéma de partitions par défaut (1,25 Mo) :
> reprendre Minimal SPIFFS. Le firmware fait 1,4 Mo sur 1,9 Mo.
> Après un changement de schéma, téléverser par USB : la mise à jour par le
> réseau ne réécrit pas la table des partitions.

`src/secrets.h` est facultatif pour le firmware (voir
[Premier démarrage](#premier-démarrage)) ; il est ignoré par git.

## Premier démarrage

- La partition de fichiers (LittleFS) est formatée : le moniteur série
  l'annonce par `[Fichiers]`.
- L'annuaire reçoit les services préréglés d'iodeo : TEASER, HACKER, PAVI
  (MiniPavi), RETRO (Retrocampus BBS), 3611, CODE (3615co.de), SM. TEASER et
  SM ne répondent plus côté serveur.
- WiFi : si `src/secrets.h` (copie de `src/secrets.example.h`) définit
  `WIFI_SSID` et `WIFI_MDP`, ce réseau amorce une seule fois la liste des
  réseaux mémorisés. Sinon, la mire reste affichée et ENVOI ouvre la recherche
  des réseaux (P41).
- Le Minitel peut être allumé avant ou après l'ESP32 : tant qu'il ne répond
  pas, il est cherché toutes les 5 secondes, puis la page est redessinée.

## Utilisation sur le Minitel

Partout : SOMMAIRE ramène au Sommaire, RETOUR à la page précédente, GUIDE
ouvre l'aide de la page (sauf P70, pendant l'appel), RÉPÉTITION réaffiche.
Dans un champ, CORRECTION efface un caractère, ANNULATION tout le champ ; le
clavier est en minuscules, Maj + lettre donne une capitale.

La rangée 0 montre l'état du WiFi et l'heure (NTP, heure de Paris), réécrits
au plus une fois par minute et jamais au-delà de la colonne 36 ; un message
temporaire avec bip y passe quand une saisie est refusée. Les pages .vdt la
laissent vide.

### Les pages

| Page | Rôle |
|---|---|
| **P00 Mire** | modèle et vitesse du Minitel, réseau, adresse IP. Passe au Sommaire sur ENVOI ou après le délai réglé (P50). Remplacée par la page `MIRE` si elle a été déposée. |
| **P10 Sommaire** | `1` annuaire, `2` connexion directe, `3` WiFi, `4` paramètres, `5` informations (le chiffre puis ENVOI). Un code de service puis CONNEXION/FIN ou ENVOI l'appelle ; un nom de page .vdt l'affiche. |
| **P11 Page locale** | une page .vdt déposée par le portail, en plein écran. RETOUR ou SOMMAIRE la quittent. |
| **P20 Annuaire** | 8 services par page (SUITE, RETOUR). N° puis ENVOI ouvre la fiche, N° puis CONNEXION/FIN appelle, `+` puis ENVOI crée un service. 30 services au plus. |
| **P21 Fiche** | CONNEXION/FIN appelle, `M` modifie, `S` supprime (P23 confirme). Date du dernier appel et durée cumulée. |
| **P22 Modification** | deux écrans, SUITE et RETOUR d'un champ à l'autre, ENVOI enregistre. Écran 2 : affichage (Vidéotex, texte 40 ou 80 colonnes), écho local, rouleau, CONNEXION/FIN transmise au serveur (on sort alors par Ctrl+R), sous-protocole, ping et reconnexion (WebSocket), utilisateur, mot de passe et clé (SSH). |
| **P30 Connexion directe** | `telnet://hote[:port]` (ou sans préfixe), `ws://`, `wss://`, `ssh://utilisateur@hote[:port]`. SUITE change l'affichage. Après l'appel, `A` sur la page de fin ajoute le service à l'annuaire. |
| **P40 Réseau WiFi** | état, réseau, signal, adresse, cause d'un échec. `1` recherche, `2` réseaux mémorisés, `3` réessaie. |
| **P41 Réseaux WiFi** | la recherche, du plus fort au plus faible. N° puis ENVOI se connecte (P42 si un mot de passe est demandé). `*` marque un réseau mémorisé, `R` relance, `M` saisit un réseau caché. 802.1X refusé. |
| **P42 Mot de passe WiFi** | 64 caractères, affiché en clair, prérempli si le réseau est connu. |
| **P43 Réseaux mémorisés** | N° puis ENVOI se connecte, `S` et le N° (`S2`) oublie. |
| **P50 Paramètres** | le N° puis ENVOI fait défiler la valeur, mémorisée aussitôt (tableau ci-dessous). `6` ouvre le portail web (P52), `9` la réinitialisation (P54). |
| **P52 Portail web** | adresse, identifiant et code du portail ; le portail ne vit que tant que cette page est affichée. Voir [Portail web](#portail-web). |
| **P54 Réinitialisation** | effacer l'annuaire (les préréglages reviennent), les réseaux WiFi, les clés d'hôtes SSH acceptées, ou tout (clés privées et pages comprises) puis redémarrer. ENVOI confirme, ANNULATION renonce. |
| **P60 Informations** | version, Minitel, mémoire, durée de marche, WiFi, MAC, auteur. |
| **P70 Connexion en cours** | barre d'attente ; CONNEXION/FIN abandonne. |
| **P71 Identification** | mot de passe SSH, en étoiles (le dernier caractère tapé visible 1,5 s), jamais mémorisé, trois essais. |
| **P72 Sécurité** | premier appel d'un hôte SSH : empreinte SHA256 de sa clé, à comparer avec `ssh-keygen -lf` sur le serveur. ENVOI l'accepte et la mémorise. |
| **P73 Échec** | la cause (DNS, TCP, HTTP, clé d'hôte changée...). RÉPÉTITION rappelle. Si la clé d'hôte a changé, `O` oublie l'ancienne. |
| **P90 Fin de connexion** | durée et coût au tarif 3615 de 1990 ; ENVOI ou le délai réglé ramènent à la page de départ. |

### Paramètres (P50)

| N° | Réglage | Valeurs |
|---|---|---|
| 1 | Bruit de connexion : deux bips au début de l'appel | oui, non |
| 2 | Coût en fin d'appel (P90, 1,29 F/min) | oui, non |
| 3 | Délai de la mire avant le Sommaire | 3, 8, 15, 30 s |
| 4 | Retour automatique après la fin de connexion | 5, 15, 30 s, jamais |
| 5 | Vitesse du Minitel, au prochain démarrage | auto (`MINITEL_VITESSE_MAX`), 1200, 4800, 9600 |
| 6 | Portail web | ouvre P52 |
| 9 | Réinitialisation | ouvre P54 |

### Pendant l'appel (P80)

Le Minitel appartient au serveur ; CONNEXION/FIN raccroche.

- **Vidéotex** : tout passe tel quel. Pour les fiches de l'annuaire, clavier
  en majuscules comme chez iodeo (Maj + lettre pour une minuscule).
- **Texte 40 ou 80 colonnes** (Telnet, SSH) : ENVOI devient CR, CORRECTION
  DEL, les flèches des séquences VT100. Les accents reçus en UTF-8 passent en
  Vidéotex (é, è, à, ç, ô, ½, £...) ; les capitales accentuées perdent leur
  accent. Les séquences inconnues du Minitel sont retirées (titres, modes
  privés, couleurs) ; en 80 colonnes restent les déplacements, effacements,
  insertions, suppressions, souligné, clignotant et inverse.
- En 80 colonnes, le Minitel 1B affiche mal les accents envoyés par SS2 :
  `TEXTE80_ACCENTS` (en tête de `src/app/Appel.cpp`) vaut 0 et la lettre y
  perd son accent.
- **SSH** : 80 colonnes par défaut sur 1B et suivants. La clé privée de la
  fiche est essayée d'abord, puis le mot de passe (puis keyboard-interactive
  avec le même mot de passe). Clé refusée ou absente : P71.
- **WebSocket avec reconnexion** : une liaison coupée est rétablie jusqu'à
  trois fois (après 1, 4 puis 10 s), annoncée en rangée 0 ; CONNEXION/FIN
  pendant l'attente renonce.

## Portail web

P50, choix `6`. P52 affiche l'adresse (`http://192.168.x.y` ou
`http://esptel.local`), l'identifiant `esptel` et un code à 6 chiffres tiré à
chaque ouverture, que le navigateur demande. Sans WiFi, P52 attend la
connexion. RETOUR ou SOMMAIRE ferme le portail.

- **Annuaire** : liste, modification et création d'une fiche (le champ
  « adresse complète » accepte `wss://hote:port/chemin` ou `ssh://moi@hote`),
  suppression, export JSON (`annuaire-esptel.json`) et import (ajout, ou
  « remplacer tout »). Les mots de passe SSH sont en clair dans l'export.
- **Clés SSH** : quatre clés privées au plus, nommées `a-z 0-9 - _` (15
  caractères). Générer une Ed25519 sur l'ESP32, ou déposer une clé existante
  (fichier ou texte, OpenSSH ou PEM, avec sa phrase de passe). La page montre
  la ligne publique à mettre dans `~/.ssh/authorized_keys` du serveur, et son
  empreinte.
- **Pages** : dépôt, téléchargement et suppression des pages .vdt (16 Ko au
  plus, nom tiré du fichier : 8 lettres ou chiffres, pas un chiffre seul).
  `MIRE.vdt` remplace la mire.
- **Réglages** : ceux de P50.
- **WiFi** : réseaux mémorisés (oublier, ajouter sans se connecter, pour ne
  pas couper le portail).
- **Mise à jour** : envoi d'un `EspTel.ino.bin` (dans l'IDE : Croquis >
  Exporter les binaires compilés, dossier `build` du croquis, pas le
  `.merged.bin`). P52 montre la progression, puis l'ESP32 redémarre.

Tant que P52 est affichée, l'IDE voit aussi l'ESP32 dans Outils > Port,
rubrique réseau (`esptel`) : téléversement sans câble, avec le code de P52
comme mot de passe.

Le portail utilise le serveur web et la mise à jour du core ESP32 : aucune
bibliothèque de plus.

## Ce que l'ESP32 mémorise

| Où | Quoi |
|---|---|
| NVS « wifi » | 5 réseaux au plus, mots de passe en clair, le dernier utilisé en tête |
| NVS « annuaire » | 30 fiches (format v2 ; les fiches v1 se relisent) |
| NVS « ssh_hotes » | les clés d'hôtes SSH acceptées (une par hote:port) |
| NVS « reglages » | les paramètres de P50 |
| LittleFS `/cles/` | les clés privées SSH, **déchiffrées** : qui a la carte en main peut les lire |
| LittleFS `/vdt/` | les pages .vdt |

LittleFS occupe la partition « spiffs » du schéma Minimal SPIFFS (190 Ko).

## Arborescence

```
EspTel.ino                vide : setup() et loop() sont dans src/
src/config.h              broches, vitesse visée, version
src/secrets.example.h     modèle de src/secrets.h (facultatif)
src/MinitelIO/            prise péri : vitesse, identification, clavier, rangée 0
src/Pages/                moteur de pages, charte Télétel, champs de saisie
src/Sessions/             interface Session commune, Telnet, WebSocket, SSH
src/app/                  le firmware : pages, WiFi, annuaire, appel (tâche de
                          session), clés d'hôtes, réglages, portail web,
                          fichiers LittleFS (clés privées, pages .vdt)
test/decodeur/            test du décodeur clavier (PC)
test/rendu/               le firmware sur PC face à un Minitel simulé
test/portail/             les pages du portail écrites en HTML (PC)
```

## Essayer sur PC

`test/rendu/simulateur.cpp` fait tourner le vrai firmware face à un Minitel 2
simulé, qui interprète le Vidéotex et répond aux commandes PRO. Il imprime
chaque écran et le nombre d'octets envoyés (donc le temps d'affichage à 4800
bauds). Commande de compilation en tête du fichier (Linux ou WSL, g++).

Scénarios : sans argument, l'annuaire et les appels face à un serveur
factice ; `j8` (mire et page .vdt, clé SSH, reconnexion WebSocket) ; `j7`
(portail, aller-retour JSON) ; `j6` (paramètres, réinitialisation) ;
`allumage` (Minitel allumé après l'ESP32) ; `j5` (SSH : clé d'hôte, mot de
passe, 80 et 40 colonnes) ; `j3` (WiFi depuis le Minitel) ; `j2` (demande un
`src/secrets.h`).

`test/portail/apercu.cpp` appelle les vraies routes du portail derrière un
serveur web factice et écrit chaque page en HTML, à ouvrir dans un navigateur.
Commande en tête du fichier.

## Choix techniques

- **Core arduino-esp32 2.0.17**, comme iodeo ; LibSSH-ESP32 5.10.0 est testée
  dessus.
- **Minitel1B_Hard pour l'affichage**, mais MinitelIO fait sa propre détection
  de vitesse (bornée : l'original boucle sans fin si le Minitel est éteint) et
  son propre décodage clavier (non bloquant, octets bruts conservés pour le
  Vidéotex).
- **Une session = une tâche FreeRTOS**, seule à écrire sur le Minitel tant
  qu'elle tourne, avec une pile réservée une fois pour toutes au démarrage :
  allouer et libérer 50 Ko à chaque appel morcelait le tas. `ssh_init` une
  seule fois, jamais de `ssh_finalize`, pour enchaîner les appels sans
  redémarrer. La boucle principale a une pile de 16 Ko pour déchiffrer les
  clés déposées au portail.
- **CONNEXION/FIN sur Minitel 1B** : la touche démarre aussi le modem, qui
  renverrait la touche une quarantaine de secondes plus tard. Chaque appui
  envoie donc PRO1 DECONNEXION ; l'accusé du Minitel est ignoré.
- **WebSocket sécurisé (wss) sans vérification de certificat**, comme iodeo ;
  pas encore essayé sur le Minitel, faute de serveur wss en service.
- **Aucune broche libre pilotée** : les GPIO non câblés restent disponibles
  sur la carte (la LED bleue qui clignote sur certains devkits est une broche
  flottante, sans effet).

## Historique

Tous les jalons sont validés sur un Minitel, sauf mention contraire.

| Jalon | Version | Contenu |
|---|---|---|
| J0 | | socle MinitelIO : vitesse, identification, clavier, rangée 0 |
| J2 | 0.2 | moteur de pages, charte Télétel : mire, Sommaire, Informations |
| J3 | 0.3 | WiFi depuis le Minitel (P40 à P43) |
| J4 | 0.4 | annuaire, connexion directe, appel Telnet et WebSocket |
| J5 | 0.5 | SSH : clé d'hôte, mot de passe, 80 colonnes, accents reçus |
| J6 | 0.6 | paramètres, réinitialisation, GUIDE partout ; nom EspTel |
| J7 | 0.7 | portail web et mise à jour OTA |
| J8 | 0.8 | clés SSH, pages .vdt, reconnexion WebSocket (dépôt des pages vérifié, le reste à essayer) |

Reste ouvert : la saisie des accents au clavier pendant une session SSH (rien
n'est envoyé aujourd'hui).
