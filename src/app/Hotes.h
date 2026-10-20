#pragma once

// Empreintes des clés d'hôte SSH déjà acceptées (TOFU, plan §9).
//
// Une entrée par « hote:port » dans le NVS (espace "ssh_hotes"). À la première
// connexion l'usager accepte la clé (P72) ; une clé différente ensuite bloque
// l'appel, jusqu'à ce qu'il oublie l'ancienne depuis P73.

#include <Arduino.h>

namespace hotes {

enum class Verdict : uint8_t { Inconnu, Connu, Change };

Verdict verifier(const char* hote, uint16_t port, const char* empreinte);
void memoriser(const char* hote, uint16_t port, const char* empreinte);
void oublier(const char* hote, uint16_t port);
void toutOublier();  // P54

}  // namespace hotes
