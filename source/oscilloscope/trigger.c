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
static bool buttonLock = false;
static u8 buttonPressCooldown = 0;

static enum TRIG_MENU_STATE menuState = TRIG_SETUP;
static enum TRIG_STATE trigState = TRIG_INPUT;
static enum TRIG_CAPTURE_SELECTION captureSelection = TRIGGER_L;

//static u8 triggerValues[500] = {0};
//static int triggerValuesPointer = 0;
static TriggerData data = { {{0}}, 0, 0, 0, 0, false };
static TriggerDatapoint curr;
static bool startedCapture = false;

static sampling_callback cb;
static u64 prevSampleCallbackTick = 0;
static u64 sampleCallbackTick = 0;
static u64 pressedTimer = 0;
static u8 ellipseCounter = 0;
static bool pressLocked = false;
static int dataScrollOffset = 0;

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
	
	// detection logic
	if (trigState != TRIG_DISPLAY_LOCK) {
		// record current data
		curr.triggerLAnalog = PAD_TriggerL(0);
		curr.triggerRAnalog = PAD_TriggerR(0);
		curr.triggerLDigital = (*held & PAD_TRIGGER_L);
		curr.triggerRDigital = (*held & PAD_TRIGGER_R);
		
		if (startedCapture) {
			// capture data
			data.data[data.endPoint].triggerLAnalog = curr.triggerLAnalog;
			data.data[data.endPoint].triggerRAnalog = curr.triggerRAnalog;
			data.data[data.endPoint].triggerLDigital = curr.triggerLDigital;
			data.data[data.endPoint].triggerRDigital = curr.triggerRDigital;
			data.endPoint++;
			
			// has the trigger moved back within bounds and triggers not being pressed anymore
			switch (captureSelection) {
				case TRIGGER_L:
					if ((curr.triggerLAnalog < 43 && !curr.triggerLDigital) || data.endPoint == WAVEFORM_SAMPLES) {
						trigState = TRIG_DISPLAY;
						startedCapture = false;
						data.isDataReady = true;
					}
					break;
				case TRIGGER_R:
					if ((curr.triggerRAnalog < 43 && !curr.triggerRDigital) || data.endPoint == WAVEFORM_SAMPLES) {
						trigState = TRIG_DISPLAY;
						startedCapture = false;
						data.isDataReady = true;
					}
					break;
			}
		} else {
			// check for analog value above 42, or for any digital trigger
			if (curr.triggerLAnalog >= 43 || curr.triggerLDigital) {
				captureSelection = TRIGGER_L;
				data.data[0].triggerLAnalog = curr.triggerLAnalog;
				data.data[0].triggerRAnalog = curr.triggerRAnalog;
				data.data[0].triggerLDigital = curr.triggerLDigital;
				data.data[0].triggerRDigital = curr.triggerRDigital;
				data.endPoint = 1;
				data.isDataReady = false;
				trigState = TRIG_INPUT;
				startedCapture = true;
			} else if (curr.triggerRAnalog >= 43 || curr.triggerRDigital) {
				captureSelection = TRIGGER_R;
				data.data[0].triggerLAnalog = curr.triggerLAnalog;
				data.data[0].triggerRAnalog = curr.triggerRAnalog;
				data.data[0].triggerLDigital = curr.triggerLDigital;
				data.data[0].triggerRDigital = curr.triggerRDigital;
				data.endPoint = 1;
				data.isDataReady = false;
				trigState = TRIG_INPUT;
				startedCapture = true;
			}
		}
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
	
	if (!buttonLock) {
		if (*pressed & PAD_TRIGGER_Z) {
			menuState = TRIG_POST_SETUP;
			buttonLock = true;
			buttonPressCooldown = 5;
		}
	}
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
				case TRIG_DISPLAY_LOCK:
					setCursorPos(2, 28);
					printStrColor("LOCKED", currXfb, COLOR_WHITE, COLOR_BLACK);
				case TRIG_DISPLAY:
					if (data.isDataReady) {
						DrawBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 128, COLOR_WHITE, currXfb);
						
						u8 curr = 0, prev = 0;
						bool currDigital = false;
						int waveformXPos = 1, waveformPrevXPos = 0;
						// draw 500 datapoints from the scroll offset
						for (int i = dataScrollOffset + 1; i < dataScrollOffset + 500; i++) {
							// make sure we haven't gone outside our bounds
							if (i == data.endPoint || waveformXPos >= 500) {
								break;
							}
							
							switch (captureSelection) {
								case TRIGGER_L:
									curr = data.data[i].triggerLAnalog;
									currDigital = data.data[i].triggerLDigital;
									break;
								case TRIGGER_R:
									curr = data.data[i].triggerRAnalog;
									currDigital = data.data[i].triggerRDigital;
									break;
							}
							// analog
							DrawLine(SCREEN_TIMEPLOT_START + waveformPrevXPos, (SCREEN_POS_CENTER_Y + 128) - prev,
							         SCREEN_TIMEPLOT_START + waveformXPos, (SCREEN_POS_CENTER_Y + 128) - curr,
							         COLOR_WHITE, currXfb);
							prev = curr;
							
							// digital
							if (currDigital) {
								DrawDot(SCREEN_TIMEPLOT_START + waveformXPos, (SCREEN_POS_CENTER_Y + 28),
										COLOR_WHITE, currXfb);
							}
							
							// update scaling factor
							waveformPrevXPos = waveformXPos;
							waveformXPos++;
						}
						
						setCursorPos(3, 27);
						switch (captureSelection) {
							case TRIGGER_L:
								printStr("L Trigger", currXfb);
								break;
							case TRIGGER_R:
								printStr("R Trigger", currXfb);
								break;
						}
					}
					if (!buttonLock) {
						if (*pressed & PAD_TRIGGER_Z) {
							menuState = TRIG_INSTRUCTIONS;
							buttonLock = true;
							buttonPressCooldown = 5;
						} else if (*pressed & PAD_BUTTON_A) {
							if (trigState == TRIG_DISPLAY) {
								trigState = TRIG_DISPLAY_LOCK;
							} else {
								trigState = TRIG_DISPLAY;
							}
							buttonLock = true;
							buttonPressCooldown = 5;
						}
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
	
	if (buttonLock) {
		// don't allow button press until a number of frames has passed
		if (buttonPressCooldown > 0) {
			buttonPressCooldown--;
		} else {
			// allow L and R to be held and not prevent buttonLock from being reset
			// this is needed _only_ because we have a check for a pressed button while another is held
			if ((*held) == 0 || *held & PAD_TRIGGER_L || *held & PAD_TRIGGER_R) {
				buttonLock = 0;
			}
		}
	}
}

void menu_triggerOscilloscopeEnd() {
	setSamplingRateNormal();
	PAD_SetSamplingCallback(cb);
	pressed = NULL;
	held = NULL;
	menuState = TRIG_SETUP;
}
