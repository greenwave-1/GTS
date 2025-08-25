//
// Created on 7/4/25.
//

#include "submenu/gate.h"

#include <stdint.h>

#include <ogc/pad.h>
#include <ogc/timesupp.h>
#include <ogc/color.h>

#include "draw.h"
#include "polling.h"
#include "print.h"

static enum GATE_MENU_STATE menuState = GATE_SETUP;
static enum GATE_STATE state = GATE_INIT;

static uint16_t *pressed = NULL;
static uint16_t *held = NULL;
static bool buttonLock = false;
static uint8_t buttonPressCooldown = 0;
static uint8_t yPressFrameCounter = 0;
static bool yHeldAfterReset = false;

static sampling_callback cb;
static uint64_t prevSampleCallbackTick = 0;
static uint64_t sampleCallbackTick = 0;
static uint64_t pressedTimer = 0;
static bool pressLocked = false;

typedef struct GateMinMax {
	bool init;
	int8_t min;
	int8_t max;
	int8_t cMin;
	int8_t cMax;
} GateMinMax;
static GateMinMax gateMinMax[256] = { 0 };
static bool showC = false;

void gateSamplingCallback() {
	// time from last call of this function calculation
	prevSampleCallbackTick = sampleCallbackTick;
	sampleCallbackTick = gettime();
	
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
	
	if (menuState == GATE_POST_SETUP && state == GATE_POST_INIT) {
		int currX = PAD_StickX(0), currY = PAD_StickY(0);
		int currCX = PAD_SubStickX(0), currCY = PAD_SubStickY(0);
		
		if (showC) {
			if (!gateMinMax[currCX + 128].init) {
				gateMinMax[currCX + 128].init = true;
			}
			if (gateMinMax[currCX + 128].cMin > currCY) {
				gateMinMax[currCX + 128].cMin = currCY;
			}
			if (gateMinMax[currCX + 128].cMax < currCY) {
				gateMinMax[currCX + 128].cMax = currCY;
			}
		} else {
			if (!gateMinMax[currX + 128].init) {
				gateMinMax[currX + 128].init = true;
			}
			
			if (gateMinMax[currX + 128].min > currY) {
				gateMinMax[currX + 128].min = currY;
			}
			if (gateMinMax[currX + 128].max < currY) {
				gateMinMax[currX + 128].max = currY;
			}
		}
	}
	
}

static void setup(uint16_t *p, uint16_t *h) {
	pressed = p;
	held = h;
	//data.endPoint = TRIGGER_SAMPLES - 1;
	setSamplingRateHigh();
	cb = PAD_SetSamplingCallback(gateSamplingCallback);
	menuState = GATE_POST_SETUP;
}

static void displayInstructions(void *currXfb) {
	setCursorPos(2, 0);
	printStr("Move the stick around the gate to measure.\n"
			 "Press X to toggle which stick gate is measured.\n"
			 "Hold Y to reset current measure.\n"
			 "Measure will also be reset after returning to Main Menu.", currXfb);
	
	setCursorPos(21, 0);
	printStr("Press Z to close instructions.", currXfb);
	
	if (!buttonLock) {
		if (*pressed & PAD_TRIGGER_Z) {
			menuState = GATE_POST_SETUP;
			buttonLock = true;
			buttonPressCooldown = 5;
		}
	}
}

void menu_gateControllerDisconnected() {
	if (menuState == GATE_POST_SETUP) {
		state = GATE_INIT;
	}
}

void menu_gateMeasure(void *currXfb, uint16_t *p, uint16_t *h) {
	switch (menuState) {
		case GATE_SETUP:
			setup(p, h);
			break;
		case GATE_POST_SETUP:
			switch (state) {
				case GATE_INIT:
					// reset data
					for (int i = 0; i < 256; i++) {
						gateMinMax[i].init = false;
						gateMinMax[i].min = 0;
						gateMinMax[i].max = 0;
						gateMinMax[i].cMin = 0;
						gateMinMax[i].cMax = 0;
					}
					showC = false;
					yPressFrameCounter = 0;
					state = GATE_POST_INIT;
				case GATE_POST_INIT:
					setCursorPos(2, 0);
					printStr("Press Z for Instructions", currXfb);
					setCursorPos(21,0);
					printStr("Current stick: ", currXfb);
					if (showC) {
						printStr("C-Stick", currXfb);
					} else {
						printStr("Analog Stick", currXfb);
					}
					if (yPressFrameCounter != 0) {
						setCursorPos(20, 0);
						printStr("Resetting", currXfb);
						printEllipse(yPressFrameCounter, 30, currXfb);
					}
					// draw box around plot area
					DrawBox(SCREEN_POS_CENTER_X - 128, SCREEN_POS_CENTER_Y - 128,
					        SCREEN_POS_CENTER_X + 128, SCREEN_POS_CENTER_Y + 128,
					        COLOR_WHITE, currXfb);
					
					// draw each
					if (showC) {
						for (int i = 0; i < 256; i++) {
							if (gateMinMax[i].init) {
								DrawDot(SCREEN_POS_CENTER_X - 128 + i, SCREEN_POS_CENTER_Y - gateMinMax[i].cMax, COLOR_YELLOW, currXfb);
								DrawDot(SCREEN_POS_CENTER_X - 128 + i, SCREEN_POS_CENTER_Y - gateMinMax[i].cMin, COLOR_YELLOW, currXfb);
							}
						}
					} else {
						for (int i = 0; i < 256; i++) {
							if (gateMinMax[i].init) {
								DrawDot(SCREEN_POS_CENTER_X - 128 + i, SCREEN_POS_CENTER_Y - gateMinMax[i].max, COLOR_WHITE, currXfb);
								DrawDot(SCREEN_POS_CENTER_X - 128 + i, SCREEN_POS_CENTER_Y - gateMinMax[i].min, COLOR_WHITE, currXfb);
							}
						}
					}
					
					if (!buttonLock) {
						if (*pressed & PAD_TRIGGER_Z) {
							menuState = GATE_INSTRUCTIONS;
							buttonLock = true;
							buttonPressCooldown = 5;
						}
						if (*pressed & PAD_BUTTON_X) {
							showC = !showC;
							buttonLock = true;
							buttonPressCooldown = 5;
						}
					}
					if (*held & PAD_BUTTON_Y) {
						if (!yHeldAfterReset) {
							yPressFrameCounter++;
							if (yPressFrameCounter == 91) {
								state = GATE_INIT;
								yHeldAfterReset = true;
							}
						}
					} else {
						yPressFrameCounter = 0;
						yHeldAfterReset = false;
					}
					break;
			}
			break;
		case GATE_INSTRUCTIONS:
			displayInstructions(currXfb);
			break;
	}
	
	if (buttonLock) {
		// don't allow button press until a number of frames has passed
		if (buttonPressCooldown > 0) {
			buttonPressCooldown--;
		} else {
			// allow L and R to be held and not prevent buttonLock from being reset
			// this is needed _only_ because we have a check for a pressed button while another is held
			if ((*held) == 0) {
				buttonLock = 0;
			}
		}
	}
}

void menu_gateMeasureEnd() {
	setSamplingRateNormal();
	PAD_SetSamplingCallback(cb);
	pressed = NULL;
	held = NULL;
	menuState = GATE_SETUP;
	state = GATE_INIT;
}
