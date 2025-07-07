//
// Created on 7/4/25.
//

#include "gate.h"
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include "../draw.h"
#include "../polling.h"
#include "../print.h"

static enum GATE_MENU_STATE menuState = GATE_SETUP;
static enum GATE_STATE state = GATE_INIT;

static u32 *pressed = NULL;
static u32 *held = NULL;
static bool buttonLock = false;
static u8 buttonPressCooldown = 0;

static sampling_callback cb;
static u64 prevSampleCallbackTick = 0;
static u64 sampleCallbackTick = 0;
static u64 pressedTimer = 0;
static bool pressLocked = false;

typedef struct GateMinMax {
	bool init;
	int8_t min;
	int8_t max;
} GateMinMax;
static GateMinMax gateMinMax[256] = { 0 };

void gateSamplingCallback() {
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
	
	if (menuState == GATE_POST_SETUP && state == GATE_POST_INIT) {
		int currX = PAD_StickX(0), currY = PAD_StickY(0);
		
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

static void setup(u32 *p, u32 *h) {
	pressed = p;
	held = h;
	//data.endPoint = TRIGGER_SAMPLES - 1;
	setSamplingRateHigh();
	cb = PAD_SetSamplingCallback(gateSamplingCallback);
	menuState = GATE_POST_SETUP;
}

static void displayInstructions(void *currXfb) {
	printStr("doin the stuff", currXfb);
	
	if (!buttonLock) {
		if (*pressed & PAD_TRIGGER_Z) {
			menuState = GATE_POST_SETUP;
			buttonLock = true;
			buttonPressCooldown = 5;
		}
	}
}

void menu_gateMeasure(void *currXfb, u32 *p, u32 *h) {
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
					}
					state = GATE_POST_INIT;
				case GATE_POST_INIT:
					// draw box around plot area
					DrawBox(SCREEN_POS_CENTER_X - 128, SCREEN_POS_CENTER_Y - 128,
					        SCREEN_POS_CENTER_X + 128, SCREEN_POS_CENTER_Y + 128,
					        COLOR_WHITE, currXfb);
					
					// draw each
					for (int i = 0; i < 256; i++) {
						if (gateMinMax[i].init) {
							DrawDot(SCREEN_POS_CENTER_X - 128 + i, SCREEN_POS_CENTER_Y - gateMinMax[i].max, COLOR_WHITE, currXfb);
							DrawDot(SCREEN_POS_CENTER_X - 128 + i, SCREEN_POS_CENTER_Y - gateMinMax[i].min, COLOR_WHITE, currXfb);
						}
					}
					
					if (!buttonLock) {
						if (*pressed & PAD_TRIGGER_Z) {
							menuState = GATE_INSTRUCTIONS;
							buttonLock = true;
							buttonPressCooldown = 5;
						}
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
