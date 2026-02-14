//
// Created on 7/4/25.
//

#include "submenu/gate.h"

#include <stdint.h>

#include <ogc/pad.h>
#include <ogc/timesupp.h>

#include "util/gx.h"
#include "util/polling.h"
#include "util/print.h"

static enum GATE_MENU_STATE menuState = GATE_SETUP;
static enum GATE_STATE state = GATE_INIT;

static uint16_t *pressed = NULL;
static uint16_t *held = NULL;
static uint8_t yPressFrameCounter = 0;
static bool yHeldAfterReset = false;

static sampling_callback cb;
static uint64_t prevSampleCallbackTick = 0;
static uint64_t sampleCallbackTick = 0;

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
	
	readController(false);
	
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

static void setup() {
	if (pressed == NULL) {
		pressed = getButtonsDownPtr();
		held = getButtonsHeldPtr();
	}
	setSamplingRateHigh();
	cb = PAD_SetSamplingCallback(gateSamplingCallback);
	menuState = GATE_POST_SETUP;
}

static void displayInstructions() {
	setCursorPos(2, 0);
	setWordWrap(true);
	printStr("Slowly move the Analog Stick");
	drawFontButton(FONT_STICK_A);
	printStr("or the C-Stick");
	drawFontButton(FONT_STICK_C);
	printStr("\naround its gate to measure.\n\n"
			 "As data is captured, points will begin to show the gate\'s "
			 "outline.\n\n"
			 "Phob visualizations can have two quirks: \n"
			 "- Small gaps around the cardinals, and user-defined\n notches\n"
			 "- Points may appear inside the gate, close to\n the origin\n\n"
			 "Neither of these are an issue, and can be ignored.\n\n"
			 "Press X");
	drawFontButton(FONT_Y);
	printStr("to toggle which stick is being visualized.\n"
			 "Hold Y");
	drawFontButton(FONT_X);
	printStr("to reset the current visualization.");
	setWordWrap(false);
	
	if (isControllerConnected(CONT_PORT_1)) {
		setCursorPos(0, 31);
		printStr("Close Instructions (Z");
		drawFontButton(FONT_Z);
		printStr(")");
	}
	
	if (*pressed & PAD_TRIGGER_Z) {
		menuState = GATE_POST_SETUP;
	}
}


void menu_gateMeasure() {
	// reset measurement if controller is disconnected
	if (!isControllerConnected(CONT_PORT_1)) {
		state = GATE_INIT;
	}
	
	switch (menuState) {
		case GATE_SETUP:
			setup();
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
					if (isControllerConnected(CONT_PORT_1)) {
						setCursorPos(0, 32);
						printStr("View Instructions (Z");
						drawFontButton(FONT_Z);
						printStr(")");
					}
					setCursorPos(4, 0);
					printStr("Stick (Y");
					drawFontButton(FONT_Y);
					printStr("):");
					setCursorPos(5, 2);
					if (showC) {
						printStr("C-Stick");
					} else {
						printStr("Analog Stick");
					}
					setCursorPos(7, 0);
					if (yPressFrameCounter != 0) {
						printStr("Resetting");
						printEllipse(yPressFrameCounter, 30);
					} else {
						printStr("Reset Data (X");
						drawFontButton(FONT_X);
						printStr(")");
					}
					
					updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
					
					// draw box around plot area
					drawSolidBox(COORD_CIRCLE_CENTER_X - 128, SCREEN_POS_CENTER_Y - 128,
					             COORD_CIRCLE_CENTER_X + 128, SCREEN_POS_CENTER_Y + 128,
					             GX_COLOR_BLACK);
					if (showC) {
						drawBox(COORD_CIRCLE_CENTER_X - 128, SCREEN_POS_CENTER_Y - 128,
						        COORD_CIRCLE_CENTER_X + 128, SCREEN_POS_CENTER_Y + 128,
						        GX_COLOR_YELLOW);
					} else {
						drawBox(COORD_CIRCLE_CENTER_X - 128, SCREEN_POS_CENTER_Y - 128,
						        COORD_CIRCLE_CENTER_X + 128, SCREEN_POS_CENTER_Y + 128,
						        GX_COLOR_WHITE);
					}
					
					// draw each
					int totalPoints = 0;
					int validPointIndexes[256] = {0};
					
					// figure out how many points we need to draw
					for (int i = 0; i < 256; i++) {
						if (gateMinMax[i].init) {
							validPointIndexes[totalPoints] = i;
							totalPoints++;
						}
					}
					
					GX_SetPointSize(12, GX_TO_ZERO);
					// totalPoints * 2 because each entry in the array contains both min and max
					GX_Begin(GX_POINTS, VTXFMT_PRIMITIVES_INT, totalPoints * 2);
					
					// TODO: there's a better way to do this, right?
					if (showC) {
						for (int i = 0; i < totalPoints; i++) {
							GX_Position3s16(COORD_CIRCLE_CENTER_X - 128 + validPointIndexes[i], SCREEN_POS_CENTER_Y - gateMinMax[validPointIndexes[i]].cMax, -5);
							GX_Color4u8(GX_COLOR_SILVER.r, GX_COLOR_SILVER.g, GX_COLOR_SILVER.b, GX_COLOR_SILVER.a);
							
							GX_Position3s16(COORD_CIRCLE_CENTER_X - 128 + validPointIndexes[i], SCREEN_POS_CENTER_Y - gateMinMax[validPointIndexes[i]].cMin, -5);
							GX_Color4u8(GX_COLOR_SILVER.r, GX_COLOR_SILVER.g, GX_COLOR_SILVER.b, GX_COLOR_SILVER.a);
						}
					} else {
						for (int i = 0; i < totalPoints; i++) {
							GX_Position3s16(COORD_CIRCLE_CENTER_X - 128 + validPointIndexes[i], SCREEN_POS_CENTER_Y - gateMinMax[validPointIndexes[i]].max, -5);
							GX_Color4u8(GX_COLOR_SILVER.r, GX_COLOR_SILVER.g, GX_COLOR_SILVER.b, GX_COLOR_SILVER.a);
							
							GX_Position3s16(COORD_CIRCLE_CENTER_X - 128 + validPointIndexes[i], SCREEN_POS_CENTER_Y - gateMinMax[validPointIndexes[i]].min, -5);
							GX_Color4u8(GX_COLOR_SILVER.r, GX_COLOR_SILVER.g, GX_COLOR_SILVER.b, GX_COLOR_SILVER.a);
						}
					}
					GX_End();
					
					
					if (*pressed & PAD_TRIGGER_Z) {
						menuState = GATE_INSTRUCTIONS;
					} else if (*pressed & PAD_BUTTON_Y) {
						showC = !showC;
					}
					
					if (*held & PAD_BUTTON_X) {
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
			displayInstructions();
			break;
	}
}

void menu_gateMeasureEnd() {
	setSamplingRateNormal();
	PAD_SetSamplingCallback(cb);
	pressed = NULL;
	held = NULL;
	menuState = GATE_SETUP;
	//state = GATE_INIT;
}

void menu_gateMeasureResetData() {
	state = GATE_INIT;
}
