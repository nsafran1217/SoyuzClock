/*
Library for my INS-1 Nixie Tube Matrix.
Nathan Safran - 9/10/2023
*/

#ifndef SoyuzDisplay_h
#define SoyuzDisplay_h
#include "Arduino.h"
#include "LedControl.h"

class SoyuzDisplay
{
  public:
    SoyuzDisplay(int dataPin, int clockPin, int loadPin);
    void writeValueToDisplay(int number[], bool dot[]); 
    void writeValueToDisplay(int number, int position, bool dot);
    void writeTimeToDisplay(int hour, int minute, int second, byte dotsMask); 
    void writeTimeToSmallDisplay(int minute, int second, byte dotsMask); 
    void blankTimeDisplay();
    void blankSmallDisplay();
    
  private:
    LedControl lc;  
};

#endif
