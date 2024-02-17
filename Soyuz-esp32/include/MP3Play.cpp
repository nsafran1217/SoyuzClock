/**
 * @file streams-sd_mp3-i2s.ino
 * @author Phil Schatzmann
 * @brief decode MP3 file and output it on I2S
 * @version 0.1
 * @date 2021-96-25
 *
 * @copyright Copyright (c) 2021
 */

#include <SPI.h>
#include <SD.h>
#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"

// const int chipSelect=10;
I2SStream i2s;                                           // final output of decoded stream
EncodedAudioStream decoder(&i2s, new MP3DecoderHelix()); // Decoding stream
StreamCopy copier;
File audioFile;
unsigned long nextLoop = millis() + 1000UL;
bool in = false;

void setup()
{
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    // setup file
    SD.begin();
    audioFile = SD.open("/sound2.mp3");

    // setup i2s
    auto config = i2s.defaultConfig(TX_MODE);
    config.pin_bck = 15;
    config.pin_ws = 13;
    config.pin_data = 4;
    i2s.begin(config);
    pinMode(2, OUTPUT);

    // setup I2S based on sampling rate provided by decoder
    decoder.setNotifyAudioChange(i2s);
    decoder.begin();

    // begin copy
    copier.begin(decoder, audioFile);
}

void loop()
{
    if (millis() > nextLoop)
    {
        nextLoop = millis() + 1000UL;
        if (in)
        {
            
            //digitalWrite(2, LOW);
            in = false;
        }
        else
        {
            digitalWrite(2, HIGH);
            in = true;
        }
    }

    if (!copier.copy())
    {
        stop();
    }
}