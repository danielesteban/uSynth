/*
    Synth.cpp - µSynth logic & memory storage.
    Created by Daniel Esteban, February 10, 2013.
*/

#include "Synth.h"

prog_uint16_t Synth::_frequencies[] PROGMEM = {33,35,37,39,41,44,46,49,52,55,58,62,65,69,73,78,82,87,92,98,104,110,117,123,131,139,147,156,165,175,185,196,208,220,233,247,262,277,294,311,330,349,370,392,415,440,466,494,523,554,587,622,659,698,740,784,831,880,932,988,1047,1109,1175,1245,1319,1397,1480,1568,1661,1760,1865,1976,2093,2217,2349,2489,2637,2794,2960,3136,3322,3520,3729,3951,4186,4435,4699,4978,5274,5588,5920,6272,6645,7040,7459,7902,8372,8870,9397,9956,10548,11175,11840,12544,13290,14080,14917,15804,16744};

prog_uchar Synth::_scales[(Synth::numNotes - 1) * Synth::numScales] PROGMEM = {
    2, 1, 2, 2, 1, 2, // Aeolian
    1, 2, 2, 1, 2, 2, // Locrian
    2, 2, 1, 2, 2, 2, // Ionian
    2, 1, 2, 2, 2, 1, // Dorian
    1, 2, 2, 2, 1, 2, // Phrygian
    2, 2, 2, 1, 2, 2, // Lydian
    2, 2, 1, 2, 2, 1, // Mixolydian
    2, 1, 2, 2, 2, 2, // Melodic ascending minor
    1, 2, 2, 2, 2, 2, // Phrygian raised sixth
    2, 2, 2, 2, 1, 2, // Lydian raised fifth
    2, 2, 1, 2, 1, 2, // Major minor
    1, 2, 1, 2, 2, 2, // Altered
    1, 2, 2, 2, 1, 3 // Arabic
};

Synth::Synth(unsigned int sampleRate, const byte numWaves, const byte waveNoteOffset[]) : _numWaves(numWaves), _waveNoteOffset(waveNoteOffset) {
	_chainSawTime = _chainSaw = selectedRoot = 0;
	_chainSawInterval = /* _setScaleTime = _setScaleNote = */ _note = selectedScale = selectedRoot = 255;
	setScale(0, 0);
	for(byte x=0; x<_numWaves; x++) _waves[x] = new TinyWave(sampleRate);
}

void Synth::setScale(byte id, byte root) {
	if(selectedScale == id && selectedRoot == root) return;
	selectedScale = id;
	selectedRoot = root;

	const byte offset = (numNotes - 1) * id;
	byte octave, nt,
		n[numNotes],
		c = 0;

	for(octave = 0; octave < numOctaves; octave++) {
		for(nt = 0; nt < numNotes; nt++) {
			if(octave == 0) n[nt] = nt == 0 ? root : n[nt - 1] + pgm_read_byte_near(_scales + (offset + (nt - 1)));
			else n[nt] += 12;
			_scale[c] = n[nt];
			c++;
		}
	}
	//_setScaleTime != 255 && (_note = _setScaleNote);
	//_setScaleNote = _note;
	//_setScalePreview = _note != 255 ? (_note >= numNotes ? _note - numNotes : _note) : numNotes;
	//setNote(_setScalePreview);
	//_setScaleTime = 0;
	if(_note != 255) setNote(_note);
}

void Synth::setNote(byte nt) {
	if(_note == nt) return;
	_note = nt;
	//_setScaleTime = 255;
	if(nt == 255) return;
	for(byte x=0; x<_numWaves; x++) _waves[x]->setFrequency(pgm_read_word_near(_frequencies + (_scale[_note] + _waveNoteOffset[x])));
}

void Synth::setChainSaw(byte interval) {
	_chainSawInterval = interval;
	(interval == 255) && (_chainSaw = 0);
}

void Synth::chainSawTick() {
	/*if(_setScaleTime != 255) {
		_setScaleTime++;
		if(_setScaleTime == 255) {
			_setScalePreview++;
			if(_setScalePreview == (_setScaleNote != 255 ? (_setScaleNote >= numNotes ? _setScaleNote : _setScaleNote + numNotes) : numNotes << 1)) setNote(_setScaleNote);
			else {
				setNote(_setScalePreview);
				_setScaleTime = 0;
			}
		}
	}*/
	if(_chainSawInterval == 255 || _note == 255) return;
	_chainSawTime++;
	if(_chainSawTime >= _chainSawInterval) _chainSawTime = _chainSaw = 0;
	else if(!_chainSaw && _chainSawTime >= (_chainSawInterval / (_chainSawInterval > 50 ? 3 : 2))) _chainSaw = 1;
}

int Synth::output() {
	if(_chainSaw || _note == 255) return 0;
	int output = 0;
	for(byte x=0; x<_numWaves; x++) output += _waves[x]->next();
	return output;
}
