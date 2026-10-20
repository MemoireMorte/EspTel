#pragma once

// Le peu de JSON dont le portail a besoin, sans bibliothèque : écrire des
// chaînes échappées, et relire un tableau d'objets plats (l'annuaire exporté).

#include <Arduino.h>

#include <functional>

namespace json {

// Ajoute s entre guillemets, échappé.
void chaine(String& out, const char* s);

struct Valeur {
  enum class Type : uint8_t { Texte, Nombre, Booleen, Nul, Autre } type = Type::Nul;
  const char* texte = "";  // Texte : décodé en UTF-8
  long nombre = 0;         // Nombre : partie entière
  bool booleen = false;
};

// Parcourt les objets du tableau nommé `tableau` à la racine ({"services":[...]})
// ou, si la racine est elle-même un tableau, de celui-ci. Pour chaque objet :
// debut(), puis champ() pour chaque clé, puis fin(). Les valeurs imbriquées
// (objets, tableaux) arrivent en Type::Autre. Le texte est décodé sur place.
// false, avec erreur et la position, si le JSON est mal formé.
bool lireObjets(char* texte, const char* tableau, const std::function<void()>& debut,
                const std::function<void(const char* cle, const Valeur& v)>& champ,
                const std::function<void()>& fin, const char** erreur, size_t* position);

}  // namespace json
