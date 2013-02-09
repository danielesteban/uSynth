#define dipSwitchPin (5)
#define dipSwitchNum (5)
#define pot1Pin (7)
#define pot2Pin (6)
#define srRegister (DDRB)
#define srPort (PORTB)
#define lPin (1) //Attiny84 pin 9
#define cPin (2) //Attiny84 pin 8 
#define dPin (0) //Attiny84 pin 10

//#define DEBUG
#ifdef DEBUG
	#include <SoftwareSerial.h>
	#define rxPin 2
	#define txPin 3
	SoftwareSerial serial(rxPin, txPin);
#endif

#include <AnalogInputs.h>
#include <Buttons.h>
#include <Wave.h>

const unsigned int sampleRate = 6000;

const byte sampleBits = 8,
	numWaves = 5,
	numNotes = 7,
    numOctaves = 5,
    numScales = 13,
    chainSawTickLength = sampleRate / 1000;

#define audioInterrupt (TIM1_COMPA_vect)

prog_uint16_t midiNotes[] PROGMEM = {8,9,9,10,10,11,12,12,13,14,15,15,16,17,18,19,21,22,23,24,26,28,29,31,33,35,37,39,41,44,46,49,52,55,58,62,65,69,73,78,82,87,92,98,104,110,117,123,131,139,147,156,165,175,185,196,208,220,233,247,262,277,294,311,330,349,370,392,415,440,466,494,523,554,587,622,659,698,740,784,831,880,932,988,1047,1109,1175,1245,1319,1397,1480,1568,1661,1760,1865,1976,2093,2217,2349,2489,2637,2794,2960,3136,3322,3520,3729,3951,4186,4435,4699,4978,5274,5588,5920,6272,6645,7040,7459,7902,8372,8870,9397,9956,10548,11175,11840,12544,13290,14080,14917,15804,16744};

prog_uchar scales[(numNotes - 1) * numScales] PROGMEM = {
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

byte waveOn = 0,
	note = 255,
	waveNoteOffset[numWaves] = {0, 7, 12, 16, 23},
	chainSawInterval = 255,
	chainSawTime = 0,
	chainSawTickCount = 0,
	potMode = 0;

bool chainSaw = 0;
int output = 0;

unsigned int gain = (1 << sampleBits) / 4,
	scale[numOctaves * numNotes];

Wave waves[numWaves] = {
	Wave(WaveShapeSquare, sampleRate),
	Wave(WaveShapeSquare, sampleRate),
	Wave(WaveShapeSquare, sampleRate),
	Wave(WaveShapeSquare, sampleRate),
	Wave(WaveShapeSquare, sampleRate)
};

void onChange(byte id, int read);
AnalogInputs analogInputs(onChange, 0);

void switchON(byte id);
void switchOFF(byte id);
Buttons buttons(switchOFF, switchON, 0);

void setNote(byte nt, bool force = false) {
	if(!force && note == nt) return;
	/*if(midiEnabled && note != 255) {
		for(byte x=0; x<numWaves; x++) {
			if(!(waveOn & (1 << x))) continue;
			midi.sendNoteOff(scale[_note] + waveNoteOffset[x], map(gain, 0, 1 << sampleBits, 0, 127), midiChannel);
		}
	}*/
	note = nt;
	if(nt == 255) return;
	for(byte x=0; x<numWaves; x++) {
		waves[x].setFrequency(pgm_read_word_near(midiNotes + (scale[note] + waveNoteOffset[x])));
		//if(midiEnabled && waveOn & (1 << x)) midi.sendNoteOn(scale[note] + waveNoteOffset[x], map(gain, 0, 1 << sampleBits, 0, 127), midiChannel);
	}
}

void setScale(byte id, byte root = 0) {
	const byte offset = (numNotes - 1) * id;
	byte octave, nt,
		n[numNotes],
		cn = note,
		c = 0;

	root += 24;
	//if(midiEnabled && cn != 255) setNote(255);
	for(octave = 0; octave < numOctaves; octave++) {
		for(nt = 0; nt < numNotes; nt++) {
			if(octave == 0) n[nt] = nt == 0 ? root : n[nt - 1] + pgm_read_byte_near(scales + (offset + (nt - 1)));
			else n[nt] += 12;
			scale[c] = n[nt];
			c++;
		}
	}
	if(cn != 255) setNote(note, true);
}

void setup() {
	#ifdef DEBUG
		pinMode(rxPin, INPUT);
		pinMode(txPin, OUTPUT);
		serial.begin(19200);
	#endif

	setScale(0);

	analogInputs.setup(pot1Pin);
	analogInputs.setup(pot2Pin);

	for(byte x=0; x<dipSwitchNum; x++) buttons.setup(dipSwitchPin - x);

	srRegister |= (1 << lPin);
	srRegister |= (1 << cPin);
	srRegister |= (1 << dPin);

	cli(); //stop interrupts

	TCCR1A = 0;
	TCCR1B = 0;
	TCNT1  = 0;
  	TCCR1B |= (1 << WGM12); //turn on CTC mode
	TCCR1B |= (1 << CS11); //Set CS11 bit for 8 prescaler
	OCR1A = (F_CPU / ((long) sampleRate * 8)) - 1; //set compare match register to sampleRate
	TIMSK1 |= (1 << OCIE1A); //enable timer compare interrupt

	sei(); //allow interrupts
}

void loop() {
	analogInputs.read();
	buttons.read();
}

void onChange(byte pin, int read) {
	switch(pin) {
		case pot1Pin:
			switch(potMode) {
				case 0:
					setNote(map(read, 0, 1023, 0, (numNotes * numOctaves) - 1));
				break;
				case 1:
					read = map(read, 0, 1023, 0, WaveShapeSaw);
					if(waves[0].getShape() != read) {
						for(byte x=0; x<numWaves; x++) waves[x].setShape(read);	
						gain = (1 << sampleBits) / (read == WaveShapeTriangle ? 1.5 : 4);
					}
			}
		break;
		case pot2Pin:
			switch(potMode) {
				case 0:
					(chainSawInterval = map(read, 0, 1023, 255, 10)) && (chainSawInterval == 255) && (chainSaw = 0);
				break;
				case 1:
					setScale(map(read, 0, 1023, 0, numScales - 1));
			}
	}
}

void switchON(byte pin) {
	pin = dipSwitchPin - pin;
	switch(pin) {
		default: //waves
			waveOn |= (1 << pin);
		break;
		case 5:
			potMode = 1;
		break;
	}
}

void switchOFF(byte pin) {
	pin = dipSwitchPin - pin;
	switch(pin) {
		default:  //waves
			waveOn &= ~(1 << pin);
		break;
		case 5:
			potMode = 0;
		break;
	}
}

void chainSawTick() {
	if(!waveOn || note == 255 || chainSawInterval == 255) return;
	chainSawTickCount++;
	if(chainSawTickCount < chainSawTickLength) return;
	chainSawTickCount = 0;
	chainSawTime++;
	if(chainSawTime >= chainSawInterval) {
		chainSawTime = chainSaw = 0;
		/*if(_midiEnabled && _note != 255) for(byte x=0; x<numWaves; x++) {
			if(!(waveOn & (1 << x))) continue;
			_midi.sendNoteOn(_scale[_note] + _waveNoteOffset[x], map(gain, 0, 1 << _sampleBits, 0, 127), _midiChannel);
		}*/
	} else if(!chainSaw && chainSawTime >= (chainSawInterval / (chainSawInterval > 50 ? 3 : 2))) {
		chainSaw = 1;
		/*if(_midiEnabled && _note != 255) for(byte x=0; x<numWaves; x++) {
			if(!(waveOn & (1 << x))) continue;
			_midi.sendNoteOff(_scale[_note] + _waveNoteOffset[x], map(gain, 0, 1 << _sampleBits, 0, 127), _midiChannel);
		}*/
	}
}

ISR(audioInterrupt) {
	chainSawTick();

	if(!chainSaw && note != 255) {
		output = 127;
		for(byte i=0; i<numWaves; i++) {
			if(!(waveOn & (1 << i))) continue;
			output += 127 - waves[i].next();
		}
	} else if(output != 0) {
		output += output > 0 ? -1 : 1;
	}

	uint8_t out = constrain(127 + (((long) output * (long) gain) >> sampleBits), 0, 255);
	srPort &= ~(1 << lPin);
	for(byte x=0; x<8; x++)  {
		if(out & (1 << (7 - x))) srPort |= (1 << dPin);
		else srPort &= ~(1 << dPin);
		srPort |= (1 << cPin);
		srPort &= ~(1 << cPin);
	}
	srPort |= (1 << lPin);
}
