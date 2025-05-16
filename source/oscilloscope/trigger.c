//
// Created on 5/16/25.
//

#include "trigger.h"

#include <ogc/lwp_watchdog.h>
#include "../draw.h"
#include "../polling.h"
#include "../print.h"

const static u8 SCREEN_TIMEPLOT_START = 70;

static u32 *pressed = NULL;
static u32 *held = NULL;

static enum TRIG_MENU_STATE menuState = TRIG_SETUP;
static enum TRIG_STATE trigState = TRIG_DISPLAY;

static u8 triggerValues[500] = {0};
static int triggerValuesPointer = 0;

static sampling_callback cb;
static u64 prevSampleCallbackTick = 0;
static u64 sampleCallbackTick = 0;
static u64 pressedTimer = 0;
static u8 ellipseCounter = 0;
static bool pressLocked = false;

void triggerSamplingCallback() {
	// time from last call of this function calculation
	prevSampleCallbackTick = sampleCallbackTick;
	sampleCallbackTick = gettime();
	
	//static s8 x, y, cx, cy;
	PAD_ScanPads();
	
	// keep buttons in a "pressed" state long enough for code to see it
	// TODO: I don't like this implementation
	if (!pressLocked) {
		*pressed = PAD_ButtonsDown(0);
		if ((*pressed) != 0) {
			pressLocked = true;
			pressedTimer = gettime();
		}
	} else {
		if (ticks_to_millisecs(gettime() - pressedTimer) > 32) {
			pressLocked = false;
		}
	}
	
	*held = PAD_ButtonsHeld(0);
	
	// temp for testing
	triggerValues[triggerValuesPointer] = PAD_TriggerL(0);
	triggerValuesPointer++;
	if (triggerValuesPointer == 500) {
		triggerValuesPointer = 0;
	}
	
	if (trigState == TRIG_INPUT) {
		// actual capture logic will go here
	}
}

static void setup(u32 *p, u32 *h) {
	pressed = p;
	held = h;
	//data.endPoint = WAVEFORM_SAMPLES - 1;
	setSamplingRateHigh();
	cb = PAD_SetSamplingCallback(triggerSamplingCallback);
	menuState = TRIG_POST_SETUP;
}

void displayInstructions(void *currXfb) {
	printStr("press buttons good", currXfb);
}

void menu_triggerOscilloscope(void *currXfb, u32 *p, u32 *h) {
	switch (menuState) {
		case TRIG_SETUP:
			setup(p, h);
			break;
		case TRIG_POST_SETUP:
			switch (trigState) {
				case TRIG_INPUT:
					// nothing happens here other than showing the message about waiting for an input
					// the sampling callback function will change the trigState enum when an input is done
					setCursorPos(2,0);
					printStr("Waiting for input.", currXfb);
					if (ellipseCounter > 20) {
						printStr(".", currXfb);
					}
					if (ellipseCounter > 40) {
						printStr(".", currXfb);
					}
					ellipseCounter++;
					if (ellipseCounter == 60) {
						ellipseCounter = 0;
					}
					break;
				case TRIG_DISPLAY:
					int waveformPrevXPos = 0;
					int waveformXPos = 1;
					for (int i = 0; i < 500; i++) {
						DrawLine(SCREEN_TIMEPLOT_START + waveformPrevXPos, (SCREEN_POS_CENTER_Y + 128) - triggerValues[i],
						         SCREEN_TIMEPLOT_START + waveformXPos, (SCREEN_POS_CENTER_Y + 128),
						         COLOR_BLUE, currXfb);
						waveformPrevXPos = waveformXPos;
						waveformXPos++;
					}
					break;
				default:
					printStr("trigState default case! how did we get here?", currXfb);
					break;
			}
			break;
		case TRIG_INSTRUCTIONS:
			displayInstructions(currXfb);
			break;
		default:
			printStr("menuState default case! how did we get here?", currXfb);
			break;
	}
}

void menu_triggerOscilloscopeEnd() {
	setSamplingRateNormal();
	PAD_SetSamplingCallback(cb);
	pressed = NULL;
	held = NULL;
	menuState = TRIG_SETUP;
}
