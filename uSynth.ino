#define dipSwitchPort (PORTA)
#define dipSwitchStatus (PINA)
#define dipSwitchPin (5) //Attiny84 pin 5
#define dipSwitchNum (5)
#define DacRegister (DDRB)
#define DacPort (PORTB)
#define DacLatchPin (1) //Attiny84 pin 9
#define DacClockPin (2) //Attiny84 pin 8 
#define DacDataPin (0) //Attiny84 pin 10
#define photoResistorPin (0)
#define pot1Pin (7)
#define pot2Pin (6)

//#define DEBUG
#ifdef DEBUG
	#include <SoftwareSerial.h>
	#define rxPin 2
	#define txPin 3
	SoftwareSerial serial(rxPin, txPin);
#endif

#include <AnalogInputs.h>
#include <Synth.h>

const unsigned int sampleRate = 8000;
const byte numSynths = 2,
	chainSawTickLength = sampleRate / 1000;

#define audioInterrupt (TIM1_COMPA_vect)

const byte synth1NumWaves = 4,
	synth2NumWaves = 2,
	synth1WaveNoteOffset[synth1NumWaves] = {0, 7, 12, 16},
	synth2WaveNoteOffset[synth2NumWaves] = {0, 12};

byte synthOn = 0,
	chainSawTickCount = 0,
	photoResistorCalibrate = 0;

bool photoResistorEnabled = 0;

unsigned int photoResistorMin = 1023,
	photoResistorMax = 0;

Synth synths[numSynths] = {
	Synth(sampleRate, synth1NumWaves, synth1WaveNoteOffset),
	Synth(sampleRate, synth2NumWaves, synth2WaveNoteOffset)
};

void photoResistorOnChange(byte pin, int read);
void onChange(byte pin, int read);
AnalogInputs analogInputs(onChange);

void setNote(byte synthId, int read);
void photoResistor();

void setup() {
	#ifdef DEBUG
		pinMode(rxPin, INPUT);
		pinMode(txPin, OUTPUT);
		serial.begin(19200);
	#endif

	byte x;

	for(x=0; x<dipSwitchNum; x++) dipSwitchPort |= (1 << dipSwitchPin - x); //Set Pullups

	analogInputs.setup(pot1Pin);
	analogInputs.setup(pot2Pin);
	analogInputs.setup(photoResistorPin, photoResistorOnChange);

	//Set random scale & root on every init.
	randomSeed(analogRead(pot1Pin) + analogRead(pot2Pin)); //this should be an unused pin.. but there are none left ;P
	const byte scale = random(0, Synth::numScales),
		root = random(0, Synth::numNotes);
	
	for(x=0; x<numSynths; x++) synths[x].setScale(scale, root);

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
	for(byte x=0; x<numSynths; x++) {
		if(dipSwitchStatus & (1 << (dipSwitchPin - x))) synthOn &= ~(1 << x);
		else synthOn |= (1 << x);
	}
	analogInputs.read();
}

void setNote(byte synth, int read) {
	synths[synth].setNote(map(read, 0, 1023, 0, Synth::numNotes * (Synth::numOctaves - 2)));
}

void setChainsaw(byte synth, int read) {
	synths[synth].setChainSaw(map(read, 0, 1023, 255, 10));
}

void setScale(byte synth, int read) {
	synths[synth].setScale(map(read, 0, 1023, 0, Synth::numScales - 1), synths[synth].selectedRoot);
}

void setRoot(byte synth, int read) {
	synths[synth].setScale(synths[synth].selectedScale, map(read, 0, 1023, 0, Synth::numNotes - 1));
}

void onChange(byte pin, int read) {
	if(!synthOn) return;
	
	bool sel;
	byte synth;

	switch(pin) {
		case pot1Pin:
			if(synthOn & (1 << 0)) { //FIESTA
				sel = !(dipSwitchStatus & (1 << dipSwitchPin - 2));
				synth = 0;	
			} else {
				sel = dipSwitchStatus & (1 << dipSwitchPin - 3);
				synth = 1;	
			}		
		break;
		case pot2Pin:
			if(synthOn & (1 << 1)) { //FIESTA
				sel = !(dipSwitchStatus & (1 << dipSwitchPin - 3));
				synth = 1;	
			} else {
				sel = dipSwitchStatus & (1 << dipSwitchPin - 2);
				synth = 0;	
			}
	}

	if(!photoResistorEnabled && !(dipSwitchStatus & (1 << dipSwitchPin - 4))) { //Alt Mode
		if(sel) setScale(synth, read);
		else setRoot(synth, read);
	} else {
		if(sel) setChainsaw(synth, read);
		else setNote(synth, read);
	}
}

void photoResistorOnChange(byte pin, int read) {
	if(read < (photoResistorCalibrate != 255 ? 1 : 200)) {
		photoResistorEnabled = 0;
		return;
	}
	if(photoResistorCalibrate != 255) {
		if(read < 200) return;
		photoResistorCalibrate++;
		photoResistorMax < read && (photoResistorMax = read);
		photoResistorMin > read && (photoResistorMin = read);
		return;
	}
	photoResistorEnabled = 1;
	read = map(constrain(read, photoResistorMin, photoResistorMax), photoResistorMin, photoResistorMax, 1023, 0);
	byte synth = (dipSwitchStatus & (1 << dipSwitchPin - 4)) ? 0 : 1;
	if(dipSwitchStatus & (1 << dipSwitchPin - (synth == 0 ? 2 : 3))) setChainsaw(synth, read);
	else setNote(synth, read);
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
