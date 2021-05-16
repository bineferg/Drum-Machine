/*
 * assignment2_drums
 * ECS7012 Music and Audio Programming
 *
 * Second assignment, to create a sequencer-based
 * drum machine which plays sampled drum sounds in loops.
 *
 * This code runs on the Bela embedded audio platform (bela.io).
 *
 * Andrew McPherson, Becky Stewart and Victor Zappi
 * 2015-2020
 */

/*
 * @author: Eva Fineberg 
 */

#include <Bela.h>
#include <libraries/Scope/Scope.h>
#include <cmath>
#include "drums.h"
#include <vector>
#include <iostream>





/* Variables which are given to you: */

/* Drum samples are pre-loaded in these buffers. Length of each
 * buffer is given in gDrumSampleBufferLengths.
 */
extern float *gDrumSampleBuffers[NUMBER_OF_DRUMS];
extern int gDrumSampleBufferLengths[NUMBER_OF_DRUMS];

/* Read pointer into the current drum sample buffer.
 *
 * TODO (step 3): you will replace this with two arrays, one
 * holding each read pointer, the other saying which buffer
 * each read pointer corresponds to.
 */
 
// int gReadPointer = 0; Used in Step 2
std::vector<int> gReadPointers(16);
std::vector<int> gDrumBufferForReadPointer(16);

/* Patterns indicate which drum(s) should play on which beat.
 * Each element of gPatterns is an array, whose length is given
 * by gPatternLengths.
 */
extern int *gPatterns[NUMBER_OF_PATTERNS];
extern int gPatternLengths[NUMBER_OF_PATTERNS];

/* These variables indicate which pattern we're playing, and
 * where within the pattern we currently are. Used in Step 4c.
 */
int gCurrentPattern = 0;
int gCurrentIndexInPattern = 0;

/* This variable holds the interval between events in **milliseconds**
 * To use it (Step 4a), you will need to work out how many samples
 * it corresponds to.
 */
int gEventIntervalMilliseconds = 250;




/* This indicates whether we should play the samples backwards.
 */
int gPlaysBackwards = 0;

/* For bonus step only: these variables help implement a fill
 * (temporary pattern) which is triggered by tapping the board.
 */
int gShouldPlayFill = 0;
int gPreviousPattern = 0;

/* TODO: Declare any further global variables you need here */

// Start/top Button
const int kButtonPin = 1;

// Initilase last button value to HIGH
int gLastButtonValue = HIGH;

/* This variable indicates whether samples should be triggered or
 * not.
 */
int gIsPlaying = 0;

//Variable to keep track of which audio sample
int gCurrentSample = 0;

// Analog read from potentiometer as tempo
const float kInputTempo = 0;

// Analog OUT Led pin
const int kLEDPin = 0;

// Metronome variables used for the LED light
unsigned int gMetronomeInterval = 22050;
unsigned int gMetronomeCounter = 0;
unsigned int gLEDInterval = 2205;

// Accelerometer pins used to switch patterns
float kXAccInputPin = 3;
float kYAccInputPin = 4;
float kZAccInputPin = 5;

/*Error Threshold for Accelerometer*/
float ERR_THRESHOLD=0.15;

// Bela oscilloscope used to view analog signals
Scope gScope;


	
// setup() is called once before the audio rendering starts.
// Use it to perform any initialisation and allocation which is dependent
// on the period size or sample rate.
//
// userData holds an opaque pointer to a data structure that was passed
// in from the call to initAudio().
//
// Return true on success; returning false halts the program.


bool setup(BelaContext *context, void *userData)
{
	/* Step 2: initialise GPIO pins */
	for(unsigned int i = 0; i<16; i++) {
		gReadPointers[i] = 0;
		gDrumBufferForReadPointer[i]=-1;
	}
	
	// Initialise the scope for accelerometer readings
	gScope.setup(3, context->audioSampleRate);
	
	/* Calculate time(s) that the LED should stay lit/triggered. */
	gMetronomeInterval = 0.5*context->audioSampleRate;
	gLEDInterval = 0.05*context->audioSampleRate;
	
	/* Set up the digital button as input, they are input 
	by default but we specify for clarity */
	pinMode(context, 0, kButtonPin, INPUT);
	pinMode(context, 0, kInputTempo, INPUT);
	pinMode(context, 0, kLEDPin, OUTPUT);
	
	return true;
}

/*Leave room for error/noise in accelerometer readings*/
float about(float val, float target) {
	return (val-ERR_THRESHOLD <= target && target <=val+ERR_THRESHOLD);
}


// render() is called regularly at the highest priority by the audio engine.
// Input and output are given from the audio hardware and the other
// ADCs and DACs (if available). If only audio is available, numMatrixFrames
// will be 0.

void render(BelaContext *context, void *userData)
{
	for(unsigned int n=0; n<context->audioFrames; n++) {

		// Read in button pin status
		int buttonValue = digitalRead(context, n, kButtonPin);
		
		// Check if the button was just pressed
		if(buttonValue == LOW && gLastButtonValue == HIGH) {
			// If the button was pushed invert IsPlaying bit
			if(gIsPlaying == 1) {
				gIsPlaying = 0;
			} else {
				gIsPlaying = 1;
			}
		}
		gLastButtonValue = buttonValue;
		

		/*Potentiometer read input and mapping for event intervals*/
		float input = analogRead(context, n/2, kInputTempo); // Read in potentiometer value
		float gEventIntervalMilliseconds = map(input, 0, 3.3/4.096, 50, 1000); // Map it to range [50-1000] ms
		
		float bpm = map(input, 0, 3.3/4.096, 40, 208);
		gMetronomeInterval = 60.0 * context->audioSampleRate / bpm;

		
		/*Accelerometer analogRead inputs at half the frames*/
		float xVal = analogRead(context, n/2, kXAccInputPin);
		float yVal = analogRead(context, n/2, kYAccInputPin);
		float zVal = analogRead(context, n/2, kZAccInputPin);
		
		/*Map the values to the normalised Accelerometer range*/
		float xx = map(xVal, 0, 3.3/4.096, -1.5, 1.5);
		float yy = map(yVal, 0, 3.3/4.096, -1.5, 1.5);
		float zz = map(zVal, 0, 3.3/4.096, -1.5, 1.5);
		
		/*Log Accelerometer readings to the scope for clarity*/
		gScope.log(xVal, yVal, zVal);

		/*Only check for directional changes once every 100 samples*/
		if(gCurrentSample % 100 == 0) {
			if(about(xx, 0) && about(yy, 0) && zz > 0){
			/*Direction is resting flat*/
				gCurrentPattern = 5;
				gPlaysBackwards = 0;
			} 
			else if(xx<0 && about(yy,0) && about(zz,0)){
			/*Direction is vertically on left side*/
				gCurrentPattern = 1;
				gPlaysBackwards = 0;
			} 
			else if(xx>0 && about(yy, 0) && about(zz,0)){
				/*Direction is vertically on right side*/
				gCurrentPattern = 2;
				gPlaysBackwards = 0;
			}
			else if(about(xx, 0) && yy < 0 && about(zz,0)) {
				/*Direction is on front side*/
				gCurrentPattern = 3;
				gPlaysBackwards = 0;
			}
			else if(about(xx, 0) && yy<0 && about(zz, 0)) {
				/*Direction is on back side*/
				gCurrentPattern = 4;
				gPlaysBackwards = 0;
			} 
			else if(about(xx, 0) && about(yy, 0) && zz< 0) {
				/*Direction is upsidedown*/
				gPlaysBackwards = 1;
			}
		}
	
		/*Interval at which we trigger the next event*/
		if(++gCurrentSample*1000.0 / context->audioSampleRate >= gEventIntervalMilliseconds) {
			if(gIsPlaying==1) {
				gCurrentSample = 0;
				startNextEvent();
			} 
		}

		/*Drum machine part*/
		if(gIsPlaying==1){
			for(unsigned int i=0; i<gDrumBufferForReadPointer.size(); i++) {
				/*Find a free buffer read pointer*/
				if (gDrumBufferForReadPointer[i] != -1 ) {
					/*Account for reading samples backwards*/
					if(gPlaysBackwards == 1) {
						/*safty check*/
						if(gReadPointers[i] == 0) {
							gReadPointers[i]=gDrumSampleBufferLengths[gDrumBufferForReadPointer[i]];
						}
						if(gReadPointers[i]< 0) {
							gReadPointers[i]=gDrumSampleBufferLengths[gDrumBufferForReadPointer[i]];
							gDrumBufferForReadPointer[i] = -1;
						} else {
							gReadPointers[i]--;
						}
					}
					else { 
						if(gReadPointers[i]>=gDrumSampleBufferLengths[gDrumBufferForReadPointer[i]]) {
							gDrumBufferForReadPointer[i] = -1;
							gReadPointers[i]=0;
						} else {
							gReadPointers[i]++;
						}
					}
				}
			}
		}
		
		/*Write the drum samples to out*/
		float out = 0;
		for(unsigned int j=0; j<gDrumBufferForReadPointer.size(); j++) {
			if(gDrumBufferForReadPointer[j]!=-1){
				out += gDrumSampleBuffers[gDrumBufferForReadPointer[j]][gReadPointers[j]];
			}
		}
		
		/*Write audio to both channels*/
		for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
			audioWrite(context, n, channel, out);
		}
			
		/*Reset the Metronome counter if we hit the interval*/
		if (++gMetronomeCounter >= gMetronomeInterval) {
			gMetronomeCounter = 0;
		}
		
		/*Trigger th LED on the beat*/
		if(gIsPlaying == 1 && (gMetronomeCounter<gLEDInterval)) {
			digitalWriteOnce(context, n, kLEDPin, HIGH);
		} else {
			digitalWriteOnce(context, n, kLEDPin, LOW);
		}
	}
}



/* Start playing a particular drum sound given by drumIndex.
 */
void startPlayingDrum(int drumIndex) 
{
	/* TODO in Steps 3a and 3b */
	for(unsigned int i=0; i<gDrumBufferForReadPointer.size(); i++) {
		if (gDrumBufferForReadPointer[i] == -1) {
			gDrumBufferForReadPointer[i] = drumIndex;
			gReadPointers[i]= 0;
			break;
		}
	}
	
}

/* Start playing the next event in the pattern */
void startNextEvent() 
{
	int currEvent = gPatterns[gCurrentPattern][gCurrentIndexInPattern];
	for(unsigned int i=0; i<NUMBER_OF_DRUMS; i++) {
		if(eventContainsDrum(currEvent, i) == 1) {
			startPlayingDrum(i);
		}
	}
	if (++gCurrentIndexInPattern >= gPatternLengths[gCurrentPattern]){
		gCurrentIndexInPattern = 0;
	}
	
}

/* Returns whether the given event contains the given drum sound */
int eventContainsDrum(int event, int drum) 
{
	if(event & (1 << drum))
		return 1;
	return 0;
}

// cleanup_render() is called once at the end, after the audio has stopped.
// Release any resources that were allocated in initialise_render().

void cleanup(BelaContext *context, void *userData)
{

}
