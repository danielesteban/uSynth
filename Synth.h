/*
    Synth.h - µSynth logic & memory storage.
    Created by Daniel Esteban, February 10, 2013.
*/

#ifndef Synth_h
#define Synth_h

#if defined(ARDUINO) && ARDUINO >= 100
#include <Arduino.h>
#else
#include <stdlib.h>
#include <WProgram.h>
#endif
#include <TinyWave.h>

class Synth {
    public:
        Synth(unsigned int sampleRate, const byte numWaves, const byte waveNoteOffset[]);
        void setScale(byte id, byte root = 0);
        void setNote(byte nt, bool force = 0);
        void setChainSaw(byte interval);
        void setDistortion(byte distortion);
        void chainSawTick();
        int output();

        static const byte numNotes = 7,
            numOctaves = 5,
            numScales = 13;
    private:
        TinyWave * _waves[4];

        const byte _numWaves,
            * _waveNoteOffset;

        byte _note,
            _chainSawInterval,
            _chainSawTime,
            _distortion;

        bool _chainSaw;

        unsigned int _scale[numOctaves * numNotes];

        static prog_uint16_t _frequencies[] PROGMEM;
        static prog_uchar _scales[(numNotes - 1) * numScales] PROGMEM;
};
 
#endif
