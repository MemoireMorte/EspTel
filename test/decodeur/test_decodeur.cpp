// Test du décodeur clavier de MinitelIO, compilé sur PC avec des bouchons.
// Depuis la racine du projet (WSL ou Linux) :
//   g++ -std=c++17 -w -Itest/decodeur/stubs -Isrc/MinitelIO test/decodeur/test_decodeur.cpp src/MinitelIO/MinitelIO.cpp -o /tmp/td && /tmp/td

#include "MinitelIO.h"
uint32_t g_ms=0;
using namespace mio;
HardwareSerial u; MinitelIO io(u,16,17);
int fails=0;
void feed(std::initializer_list<int> b){for(int x:b)u.in.push_back(x);}
void expect(const char* want){ Evenement e; bool ok=false; for(int i=0;i<200 && !ok;i++){ok=io.lire(e); if(!ok) g_ms+=1;}
  char d[40]; if(ok) MinitelIO::decrire(e,d,sizeof d); else strcpy(d,"(rien)");
  bool p=strcmp(d,want)==0; if(!p) fails++; printf("%s %-20s attendu %s\n",p?"ok ":"KO ",d,want);}
int main(){
 feed({0x13,0x41}); expect("ENVOI");
 feed({0x13,0x49}); expect("CONNEXION/FIN");
 feed({0x13,0x59,0x13,0x43}); expect("REPETITION");  // 5/9 juste après : accusé du modem, ignoré
 g_ms+=2000;
 feed({0x13,0x59}); expect("CONNEXION/FIN");          // la touche du Minitel 1B
 feed({0x13,0x43}); expect("REPETITION");
 feed({'a'}); expect("'a'");
 feed({0x19,0x42,'e'}); expect("'é'");
 feed({0x19,0x4B,'c'}); expect("'ç'");
 feed({0x19,0x41,0x13,0x46}); expect("SOMMAIRE");   // accent puis touche de fonction
 feed({0x19,0x23}); expect("'£'");
 feed({0x1B,'[','A'}); expect("Flèche haut");
 feed({0x1B,'[','D'}); expect("Flèche gauche");
 feed({0x1B,'[','2','J'}); expect("Séquence ESC");
 feed({0x1B,0x3A,0x75,0x76}); expect("Réponse protocole");
 feed({0x12}); expect("Ctrl+R");
 feed({0x1B}); expect("Echap");
 feed({0x13}); expect("Inconnu");  // séquence tronquée
 feed({' '}); expect("Espace");
 feed({0x13,0x41,'x',0x13,0x48}); expect("ENVOI"); expect("'x'"); expect("SUITE");
 printf("%d échec(s)\n",fails); return fails;
}
