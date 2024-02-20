/*
*/

#include "Arduino.h"
#include "SoyuzDisplay.h"
#include "LedControl.h"

SoyuzDisplay::SoyuzDisplay(int dataPin, int clockPin, int loadPin)
    : lc(dataPin, clockPin, loadPin, 2)
{
    lc.shutdown(0, false);
    lc.shutdown(1, false);
    lc.setIntensity(0, 15);
    lc.setIntensity(1, 15);
    // lc.clearDisplay(0);
    lc.setScanLimit(0, 4); // 5 displays per max
    lc.setScanLimit(1, 4); // 5 displays per max
}

void SoyuzDisplay::writeValueToDisplay(int number[], bool dot[])
{
    for (int i = 0; i < 10; i++)
    {
        lc.setDigit((i > 4 ? 1 : 0), (i > 4 ? i - 5 : i), number[i], dot[i]);
    }
}

void SoyuzDisplay::writeValueToDisplay(int number, int position, bool dot)
{
    if (position > 4)
    {
        lc.setDigit(1, position - 5, number, dot);
    }
    else
    {
        lc.setDigit(0, position, number, dot);
    }
}

void SoyuzDisplay::writeTimeToDisplay(int hour, int minute, int second, byte dotsMask)
{
    lc.setDigit(0, 0, second % 10, dotsMask & 1);
    lc.setDigit(0, 1, second / 10, dotsMask >> 1 & 1);
    lc.setDigit(0, 2, minute % 10, dotsMask >> 2 & 1);
    lc.setDigit(0, 3, minute / 10, dotsMask >> 3 & 1);
    lc.setDigit(0, 4, hour % 10, dotsMask >> 4 & 1);
    lc.setDigit(1, 0, hour / 10, dotsMask >> 5 & 1);
}

void SoyuzDisplay::writeTimeToSmallDisplay(int minute, int second, byte dotsMask)
{
    lc.setDigit(1, 1, second % 10, dotsMask & 1);
    lc.setDigit(1, 2, second / 10, dotsMask >> 1 & 1);
    lc.setDigit(1, 3, minute % 10, dotsMask >> 2 & 1);
    lc.setDigit(1, 4, minute / 10, dotsMask >> 3 & 1);
}

void SoyuzDisplay::writeChar(char val, int position, bool dot)
{
    if (val > 127)
        val = 32;
    byte value = myCharTable[val] | (dot ? B10000000 : 0);
    if (position > 4)
    {
        lc.setRow(1, position - 5, value);
    }
    else
    {
        lc.setRow(0, position, value);
    }
}

void SoyuzDisplay::writeStringToDisplay(String s) //  only displays first 10 chars. overflows to stop watch
{
    for (int i = 0; i < s.length(); i++)
    {
        if (i > 9)
            return;
        writeChar(s[i], i, 0);
    }
}
void SoyuzDisplay::writeSoyuz()
{
    writeChar('C', 5, 0);
    writeChar('o', 4, 0);
    lc.setRow(0, 3, B00000111); // half of yu character
    writeChar('0', 2, 0);
    writeChar('3', 1, 0);
    writeChar(' ', 0, 0);
}

void SoyuzDisplay::blankTimeDisplay()
{
    lc.clearDisplay(0);
    lc.setChar(1, 0, ' ', false);
}

void SoyuzDisplay::blankSmallDisplay()
{
    lc.setChar(1, 1, ' ', false);
    lc.setChar(1, 2, ' ', false);
    lc.setChar(1, 3, ' ', false);
    lc.setChar(1, 4, ' ', false);
}
