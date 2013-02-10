#define dipSwitchPin (5)
#define dipSwitchNum (5)
#define pot1Pin (7)
#define pot2Pin (6)
#define DacRegister (DDRB)
#define DacPort (PORTB)
#define DacLatchPin (1) //Attiny84 pin 9
#define DacClockPin (2) //Attiny84 pin 8 
#define DacDataPin (0) //Attiny84 pin 10

//#define DEBUG
#ifdef DEBUG
	#include <SoftwareSerial.h>
	#define rxPin 2
	#define txPin 3
	SoftwareSerial serial(rxPin, txPin);
#endif

#include <AnalogInputs.h>
#include <Buttons.h>
#include <Synth.h>

const unsigned int sampleRate = 8000;
const byte numSynths = 2,
	chainSawTickLength = sampleRate / 1000;

#define audioInterrupt (TIM1_COMPA_vect)

byte synthOn = 0, 
	chainSawTickCount = 0;

const byte synth1NumWaves = 4,
	synth2NumWaves = 2,
	synth1WaveNoteOffset[synth1NumWaves] = {0, 7, 12, 16},
	synth2WaveNoteOffset[synth2NumWaves] = {0, 12};

Synth synths[numSynths] = {
	Synth(sampleRate, synth1NumWaves, synth1WaveNoteOffset),
	Synth(sampleRate, synth2NumWaves, synth2WaveNoteOffset)
};

void onChange(byte id, int read);
AnalogInputs analogInputs(onChange, 0);

void switchON(byte id);
void switchOFF(byte id);
Buttons buttons(switchOFF, switchON, 0);

void setNote(byte synthId, int read);

void setup() {
	#ifdef DEBUG
		pinMode(rxPin, INPUT);
		pinMode(txPin, OUTPUT);
		serial.begin(19200);
	#endif

	randomSeed(analogRead(pot1Pin)); //this should be an unused pin.. but there are none left ;P

	for(byte x=0; x<dipSwitchNum; x++) buttons.setup(dipSwitchPin - x);

	analogInputs.setup(pot1Pin);
	analogInputs.setup(pot2Pin);

	DacRegister |= (1 << DacLatchPin);
	DacRegister |= (1 << DacClockPin);
	DacRegister |= (1 << DacDataPin);

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

void setNote(byte synth, int read) {
	synths[synth].setNote(map(read, 0, 1023, 0, (Synth::numNotes * (Synth::numOctaves - 1)) - 1));
}

void setChainsaw(byte synth, int read) {
	synths[synth].setChainSaw(map(read, 0, 1023, 255, 10));
}

void setScale(byte synth, int read) {
	synths[synth].setScale(map(read, 0, 1023, 0, Synth::numScales - 1));
}

void setDistortion(byte synth, int read) {
	synths[synth].setDistortion(map(read, 0, 1023, 0, 127));
}

void onChange(byte pin, int read) {
	const bool alt = !buttons.get(dipSwitchPin - 4)->status;
	
	if(!synthOn) return;
	
	bool sel;
	byte synth;
	
	switch(pin) {
		case pot1Pin:
			if(synthOn & (1 << 0)) { //FIESTA
				sel = !buttons.get(dipSwitchPin - 2)->status;
				synth = 0;	
			} else {
				sel = buttons.get(dipSwitchPin - 3)->status;
				synth = 1;	
			}		
		break;
		case pot2Pin:
			if(synthOn & (1 << 1)) { //FIESTA
				sel = !buttons.get(dipSwitchPin - 3)->status;
				synth = 1;	
			} else {
				sel = buttons.get(dipSwitchPin - 2)->status;
				synth = 0;	
			}
	}

	if(alt) { //Alt Mode
		if(sel) setDistortion(synth, read);
		else setScale(synth, read);
	} else {
		if(sel) setChainsaw(synth, read);
		else setNote(synth, read);
	}
}

void switchON(byte pin) {
	pin = dipSwitchPin - pin;
	switch(pin) {
		case 0:
		case 1: //synths
			synthOn |= (1 << pin);
	}
}

void switchOFF(byte pin) {
	pin = dipSwitchPin - pin;
	switch(pin) {
		case 0:
		case 1: //synths
			synthOn &= ~(1 << pin);
	}
}

ISR(audioInterrupt) {
	byte x;

	chainSawTickCount++;
	if(chainSawTickCount >= chainSawTickLength) {
		chainSawTickCount = 0;
		for(x=0; x<numSynths; x++) synths[x].chainSawTick();
	}
	
	int output = 127;
	for(x=0; x<numSynths; x++) {
		if(!(synthOn & (1 << x))) continue;
		output += synths[x].output();
	}

	uint8_t out = constrain(output, 0, 255);
	DacPort &= ~(1 << DacLatchPin);
	for(x=0; x<8; x++) {
		if(out & (1 << (7 - x))) DacPort |= (1 << DacDataPin);
		else DacPort &= ~(1 << DacDataPin);
		DacPort |= (1 << DacClockPin);
		DacPort &= ~(1 << DacClockPin);
	}
	DacPort |= (1 << DacLatchPin);
}
