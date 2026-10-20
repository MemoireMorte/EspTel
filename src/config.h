#pragma once

// Réglages matériels (carte EspTel ou devkit ESP32-WROOM-32) et choix par défaut du firmware.

// Prise péri-informatique du Minitel : UART2 de l'ESP32.
#define MINITEL_UART      Serial2
#define MINITEL_RX_PIN    16
#define MINITEL_TX_PIN    17

// Vitesse visée sur la prise. 4800 est éprouvée par iodeo sur 1B et 2.
// 9600 n'existe que sur le Minitel 2 : passer à 9600 pour l'essayer.
#ifndef MINITEL_VITESSE_MAX
#define MINITEL_VITESSE_MAX 4800
#endif

// Port de débogage USB.
#define DEBUG_UART        Serial
#define DEBUG_VITESSE     115200

#define FIRMWARE_NOM      "EspTel"
#define FIRMWARE_VERSION  "0.8.2-j8"
