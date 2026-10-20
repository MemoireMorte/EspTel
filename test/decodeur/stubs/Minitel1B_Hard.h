#pragma once
#include "Arduino.h"
struct Minitel { Minitel(HardwareSerial&,int,int){} unsigned long identifyDevice(){return 0;} int changeSpeed(int){return -1;}
 byte echo(bool){return 0;} byte extendedKeyboard(){return 0;} byte smallMode(){return 0;} void print(String){} };
