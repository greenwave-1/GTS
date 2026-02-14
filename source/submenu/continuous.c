//
// Created on 2025/03/17.
//

#include "submenu/continuous.h"

#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>

#include <ogc/pad.h>
#include <ogc/timesupp.h>

#include "util/print.h"
#include "util/polling.h"
#include "util/gx.h"
#include "waveform.h"

static enum CONT_MENU_STATE state = CONT_SETUP;
static enum CONT_STATE cState = INPUT;

static ControllerRec *data = NULL;
static int dataIndex = 0;

static enum CONTROLLER_STICK_AXIS selectedAxis = AXIS_AXY;

static uint16_t *pressed = NULL;
static uint16_t *held = NULL;

static uint64_t prevSampleCallbackTick = 0;
static uint64_t sampleCallbackTick = 0;
static uint64_t frameCounter = 0;

static sampling_callback cb;

static void contSamplingCallback() {
	// time from last call of this function calculation
	prevSampleCallbackTick = sampleCallbackTick;
	sampleCallbackTick = gettime();
	if (prevSampleCallbackTick == 0) {
		prevSampleCallbackTick = sampleCallbackTick;
	}
	
	readController(false);
	
	if (cState != INPUT_LOCK && state == CONT_POST_SETUP) {
		data->samples[dataIndex].stickX = PAD_StickX(0);
		data->samples[dataIndex].stickY = PAD_StickY(0);
		data->samples[dataIndex].cStickX = PAD_SubStickX(0);
		data->samples[dataIndex].cStickY = PAD_SubStickY(0);
		frameCounter += ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
		// abusing timeDiffUs here, 1 means this poll is considered a frame interval, 0 otherwise
		if (frameCounter >= FRAME_TIME_US) {
			data->samples[dataIndex].timeDiffUs = 1;
			frameCounter = 0;
		} else {
			data->samples[dataIndex].timeDiffUs = 0;
		}
		dataIndex++;
		if (dataIndex == REC_SAMPLE_MAX) {
			dataIndex = 0;
		}
		if (data->sampleEnd != REC_SAMPLE_MAX) {
			data->sampleEnd++;
		}
	}
}

static void displayInstructions() {
	
	setCursorPos(2, 0);
	setWordWrap(true);
	
	printStr("Move the currently selected stick to plot its movement on the waveform.\n\n");
	
	printStr("The ");
	printStrColor(GX_COLOR_NONE, GX_COLOR_RED_X, "red line");
	printStr(" shows the X-Axis's value, and\nthe ");
	printStrColor(GX_COLOR_NONE, GX_COLOR_BLUE_Y, "blue line");
	printStr(" shows the Y-Axis's value.\n\n");
	
	printStr("The waveform will continue to be updated until \'locked\'.");
	
	printStr("\n\nPress A");
	drawFontButton(FONT_A);
	printStr("to \'lock\' the current waveform. This prevents new data from being recorded,"
			 " and enables zooming and panning the waveform with the C-Stick");
	drawFontButton(FONT_STICK_C);
	printStr(".\n\n");
	
	printStr("The number of visible samples is shown above the recording window.");
	
	setWordWrap(false);
	
	if (isControllerConnected(CONT_PORT_1)) {
		setCursorPos(0, 31);
		printStr("Close Instructions (Z");
		drawFontButton(FONT_Z);
		printStr(")");
	}
	
	if (*pressed == PAD_TRIGGER_Z) {
		state = CONT_POST_SETUP;
	}
}

static void setup() {
	if (pressed == NULL) {
		pressed = getButtonsDownPtr();
		held = getButtonsHeldPtr();
	}
	if (data == NULL || isContinuousRecDataNull()) {
		data = malloc(sizeof(ControllerRec));
		clearRecordingArray(data);
		setContinuousRecStructPtr(data);
		data->isRecordingReady = true;
		data->recordingType = REC_OSCILLOSCOPE_CONTINUOUS;
		data->sampleEnd = 0;
	}
	setSamplingRateHigh();
	cb = PAD_SetSamplingCallback(contSamplingCallback);
	state = CONT_POST_SETUP;
	resetDrawGraph();
}

static int ellipseCounter = 0;

void menu_continuousWaveform() {
	switch (state) {
		case CONT_SETUP:
			setup();
			break;
		case CONT_POST_SETUP:
			if (isControllerConnected(CONT_PORT_1)) {
				setCursorPos(0, 32);
				printStr("View Instructions (Z");
				drawFontButton(FONT_Z);
				printStr(")");
			}
			
			if (cState == INPUT_LOCK) {
				setCursorPos(2, 25);
				printStrColor(GX_COLOR_WHITE, GX_COLOR_BLACK, "LOCKED");
				setCursorPos(2, 35);
				printStr("Pan/Zoom (C-Stick");
				drawFontButton(FONT_STICK_C);
				printStr(")");
			}
			
			if (data->isRecordingReady) {
				if (cState != INPUT_LOCK && isControllerConnected(CONT_PORT_1)) {
					setCursorPos(2, 0);
					printStr("Reading stick.");
					printEllipse(ellipseCounter, 20);
					ellipseCounter++;
					if (ellipseCounter == 60) {
						ellipseCounter = 0;
					}
				}
				
				// draw guidelines based on selected test
				setDepthForDrawCall(-10);
				drawSolidBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128,
				             SCREEN_TIMEPLOT_START + 501, SCREEN_POS_CENTER_Y + 128, GX_COLOR_BLACK);
				setDepthForDrawCall(0);
				if (selectedAxis == AXIS_AXY) {
					drawBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128, SCREEN_TIMEPLOT_START + 501,
					        SCREEN_POS_CENTER_Y + 128, GX_COLOR_WHITE);
				} else {
					drawBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128, SCREEN_TIMEPLOT_START + 501,
					        SCREEN_POS_CENTER_Y + 128, GX_COLOR_YELLOW);
				}
				setDepthForDrawCall(-8);
				drawLine(SCREEN_TIMEPLOT_START, SCREEN_POS_CENTER_Y, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y, GX_COLOR_GRAY);
				
				setDrawGraphStickAxis(selectedAxis);
				
				setCursorPos(1, 38);
				printStr("Toggle Lock (A");
				drawFontButton(FONT_A);
				printStr(")");
				
				if (cState != INPUT_LOCK) {
					setCursorPos(2, 37);
					printStr("Toggle Stick (Y");
					drawFontButton(FONT_Y);
					printStr(")");
				}
				
				// equivalent to dataIndex + 3000 - 1
				// +3000 because index needs to be positive for modulus to work properly,
				// minus 1 because dataIndex is the _next_ index to be written to
				setDrawGraphIndexOffset(dataIndex + 2999);
				setDepthForDrawCall(-2);
				drawGraph(data, GRAPH_STICK_FULL, cState == INPUT_LOCK);
				
				int dataScrollOffset = 0, visibleDatapoints = 0;
				getGraphDisplayedInfo(&dataScrollOffset, &visibleDatapoints);
				
				setCursorPos(21,0);
				if (cState == INPUT_LOCK) {
					setCursorPos(3, 16);
					printStr("%4u Samples, (%4u/%4u)", visibleDatapoints, dataScrollOffset,
					         dataScrollOffset + visibleDatapoints);
				}
				
				// checking for input
				// graph scaling is handled in drawGraph()
				if (*pressed & PAD_BUTTON_A) {
					if (cState == INPUT_LOCK) {
						cState = INPUT;
					} else {
						cState = INPUT_LOCK;
					}
				} else if (*pressed & PAD_BUTTON_Y) {
					if (selectedAxis == AXIS_AXY) {
						selectedAxis = AXIS_CXY;
					} else {
						selectedAxis = AXIS_AXY;
					}
				}
			}
			
			if (*pressed == PAD_TRIGGER_Z) {
				state = CONT_INSTRUCTIONS;
			}
			break;
		case CONT_INSTRUCTIONS:
			displayInstructions();
			break;
	}
}

void menu_continuousEnd() {
	setSamplingRateNormal();
	PAD_SetSamplingCallback(cb);
	state = CONT_SETUP;
}
