#include "../config.h"

#include "Json.h"

namespace json {

void chaine(String& out, const char* s) {
  out += '"';
  for (; *s; ++s) {
    const unsigned char c = static_cast<unsigned char>(*s);
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char t[8];
          snprintf(t, sizeof(t), "\\u%04x", c);
          out += t;
        } else {
          out += char(c);
        }
    }
  }
  out += '"';
}

namespace {

class Lecteur {
 public:
  explicit Lecteur(char* t) : _debut(t), _p(t) {}

  const char* erreur = nullptr;
  size_t position() const { return size_t(_p - _debut); }

  void blancs() {
    while (*_p == ' ' || *_p == '\t' || *_p == '\n' || *_p == '\r') ++_p;
  }
  bool voir(char c) {
    blancs();
    return *_p == c;
  }
  bool prendre(char c) {
    if (!voir(c)) return false;
    ++_p;
    return true;
  }
  bool attendre(char c, const char* message) {
    if (prendre(c)) return true;
    return echec(message);
  }
  bool echec(const char* message) {
    if (!erreur) erreur = message;
    return false;
  }

  // Chaîne décodée sur place : renvoie son début, terminé par un zéro.
  char* chaine() {
    if (!attendre('"', "guillemet attendu")) return nullptr;
    char* debut = _p;
    char* w = _p;
    while (*_p && *_p != '"') {
      if (*_p != '\\') {
        *w++ = *_p++;
        continue;
      }
      ++_p;
      const char c = *_p++;
      switch (c) {
        case '"': *w++ = '"'; break;
        case '\\': *w++ = '\\'; break;
        case '/': *w++ = '/'; break;
        case 'b': *w++ = '\b'; break;
        case 'f': *w++ = '\f'; break;
        case 'n': *w++ = '\n'; break;
        case 'r': *w++ = '\r'; break;
        case 't': *w++ = '\t'; break;
        case 'u': {
          unsigned v = 0;
          for (int i = 0; i < 4; ++i) {
            const char h = *_p++;
            v <<= 4;
            if (h >= '0' && h <= '9') v |= unsigned(h - '0');
            else if (h >= 'a' && h <= 'f') v |= unsigned(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') v |= unsigned(h - 'A' + 10);
            else {
              echec("\\u incorrect");
              return nullptr;
            }
          }
          // Hors du plan de base (paires de substitution) : un « ? » suffit ici.
          if (v >= 0xD800 && v <= 0xDFFF) v = '?';
          if (v < 0x80) {
            *w++ = char(v);
          } else if (v < 0x800) {
            *w++ = char(0xC0 | (v >> 6));
            *w++ = char(0x80 | (v & 0x3F));
          } else {
            *w++ = char(0xE0 | (v >> 12));
            *w++ = char(0x80 | ((v >> 6) & 0x3F));
            *w++ = char(0x80 | (v & 0x3F));
          }
          break;
        }
        default:
          echec("échappement inconnu");
          return nullptr;
      }
    }
    if (*_p != '"') {
      echec("chaîne non terminée");
      return nullptr;
    }
    ++_p;
    *w = '\0';  // w <= _p - 1 : le décodage ne fait que raccourcir
    return debut;
  }

  // Une valeur quelconque ; les objets et tableaux sont sautés.
  bool valeur(Valeur& v) {
    blancs();
    v = Valeur();
    if (*_p == '"') {
      char* s = chaine();
      if (!s) return false;
      v.type = Valeur::Type::Texte;
      v.texte = s;
      return true;
    }
    if (*_p == '{' || *_p == '[') {
      v.type = Valeur::Type::Autre;
      return sauter();
    }
    if (strncmp(_p, "true", 4) == 0) {
      _p += 4;
      v.type = Valeur::Type::Booleen;
      v.booleen = true;
      return true;
    }
    if (strncmp(_p, "false", 5) == 0) {
      _p += 5;
      v.type = Valeur::Type::Booleen;
      return true;
    }
    if (strncmp(_p, "null", 4) == 0) {
      _p += 4;
      return true;
    }
    if (*_p == '-' || (*_p >= '0' && *_p <= '9')) {
      char* fin = nullptr;
      const double d = strtod(_p, &fin);
      if (fin == _p) return echec("nombre incorrect");
      _p = fin;
      v.type = Valeur::Type::Nombre;
      v.nombre = long(d);
      return true;
    }
    return echec("valeur attendue");
  }

  // Saute un objet ou un tableau entier.
  bool sauter() {
    const char ouvrant = *_p++;
    const char fermant = ouvrant == '{' ? '}' : ']';
    if (prendre(fermant)) return true;
    for (;;) {
      if (ouvrant == '{') {
        if (!chaine() || !attendre(':', "« : » attendu")) return false;
      }
      Valeur v;
      if (!valeur(v)) return false;
      if (prendre(fermant)) return true;
      if (!attendre(',', "« , » attendue")) return false;
    }
  }

  bool fini() {
    blancs();
    return *_p == '\0';
  }

 private:
  char* _debut;
  char* _p;
};

}  // namespace

bool lireObjets(char* texte, const char* tableau, const std::function<void()>& debut,
                const std::function<void(const char*, const Valeur&)>& champ, const std::function<void()>& fin,
                const char** erreur, size_t* position) {
  Lecteur l(texte);
  auto rendre = [&](bool ok) {
    if (!ok) {
      *erreur = l.erreur ? l.erreur : "JSON incorrect";
      *position = l.position();
    }
    return ok;
  };

  // Racine : un objet dont on cherche la clé `tableau`, ou directement le tableau.
  if (l.prendre('{')) {
    bool trouve = false;
    while (!trouve) {
      if (l.voir('}')) return rendre(l.echec("tableau des services absent"));
      char* cle = l.chaine();
      if (!cle || !l.attendre(':', "« : » attendu")) return rendre(false);
      if (strcmp(cle, tableau) == 0 && l.voir('[')) {
        trouve = true;
        break;
      }
      Valeur v;
      if (!l.valeur(v)) return rendre(false);
      if (!l.prendre(',') && !l.voir('}')) return rendre(l.echec("« , » attendue"));
    }
  } else if (!l.voir('[')) {
    return rendre(l.echec("objet ou tableau attendu"));
  }

  l.prendre('[');
  if (l.prendre(']')) return rendre(true);
  for (;;) {
    if (!l.attendre('{', "objet attendu")) return rendre(false);
    debut();
    if (!l.prendre('}')) {
      for (;;) {
        char* cle = l.chaine();
        if (!cle || !l.attendre(':', "« : » attendu")) return rendre(false);
        Valeur v;
        if (!l.valeur(v)) return rendre(false);
        champ(cle, v);
        if (l.prendre('}')) break;
        if (!l.attendre(',', "« , » attendue")) return rendre(false);
      }
    }
    fin();
    if (l.prendre(']')) break;
    if (!l.attendre(',', "« , » attendue")) return rendre(false);
  }
  // Le reste de la racine n'intéresse pas l'import.
  return rendre(true);
}

}  // namespace json
