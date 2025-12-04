//
// Created on 2025/03/17.
//

#include "submenu/continuous.h"

#include <stdint.h>
#include <malloc.h>

#include <ogc/pad.h>
#include <ogc/timesupp.h>

#include "util/print.h"
#include "util/polling.h"
#include "util/gx.h"
#include "waveform.h"

const static uint8_t SCREEN_TIMEPLOT_START = 70;

static enum CONT_MENU_STATE state = CONT_SETUP;
static enum CONT_STATE cState = INPUT;

static ControllerRec *data = NULL;
static int dataIndex = 0;

static int waveformScaleFactor = 6;
static int dataScrollOffset = 0;
static bool pressLocked = false;
static bool freeze = false;
static bool showCStick = false;

static bool buttonLock = false;
static uint16_t *pressed = NULL;
static uint16_t *held = NULL;

static uint64_t prevSampleCallbackTick = 0;
static uint64_t sampleCallbackTick = 0;
static uint64_t pressedTimer = 0;
static uint64_t frameCounter = 0;

static sampling_callback cb;

static void contSamplingCallback() {
	// time from last call of this function calculation
	prevSampleCallbackTick = sampleCallbackTick;
	sampleCallbackTick = gettime();
	if (prevSampleCallbackTick == 0) {
		prevSampleCallbackTick = sampleCallbackTick;
	}
	
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
	
	if (!freeze) {
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
	}
	data->sampleEnd = REC_SAMPLE_MAX - 1;
	setSamplingRateHigh();
	cb = PAD_SetSamplingCallback(contSamplingCallback);
	state = CONT_POST_SETUP;
}

void menu_continuousWaveform() {
	switch (state) {
		case CONT_SETUP:
			setup();
			break;
		case CONT_POST_SETUP:
			setCursorPos(2, 0);
			printStr("A to freeze. Y to toggle.");
			/*
			setCursorPos(20, 0);
			printStr("Current Stick: ");
			if (!showCStick) {
				printStr("Analog Stick");
			} else {
				printStr("C-Stick");
			}
			 */
			if (cState == INPUT_LOCK) {
				freeze = true;
				setCursorPos(2, 28);
				printStrColor(GX_COLOR_WHITE, GX_COLOR_BLACK, "LOCKED");
			} else {
				freeze = false;
			}

			if (data->isRecordingReady) {
				// draw guidelines based on selected test
				if (!showCStick) {
					drawBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128, SCREEN_TIMEPLOT_START + 501,
					        SCREEN_POS_CENTER_Y + 128, GX_COLOR_WHITE);
				} else {
					drawBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128, SCREEN_TIMEPLOT_START + 501,
					        SCREEN_POS_CENTER_Y + 128, GX_COLOR_YELLOW);
				}
				
				drawLine(SCREEN_TIMEPLOT_START, SCREEN_POS_CENTER_Y, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y, GX_COLOR_GRAY);
				
				
				// lots of the specific values are taken from:
				// https://github.com/PhobGCC/PhobGCC-doc/blob/main/For_Users/Phobvision_Guide_Latest.md
				
				// reset offset if its invalid
				if (dataScrollOffset > (REC_SAMPLE_MAX - (500 * waveformScaleFactor))) {
					dataScrollOffset = (REC_SAMPLE_MAX - (500 * waveformScaleFactor));
				} else if (dataScrollOffset < 0) {
					dataScrollOffset = 0;
				}

				int waveformXPos = 1;
				
				// calculate start point
				// waveformScaleFactor determines how much information is shown by only drawing every x point
				int startPoint = (dataIndex - (500 * waveformScaleFactor) - dataScrollOffset);
				if (startPoint < 0) {
					startPoint += REC_SAMPLE_MAX;
				}
				
				if (cState == INPUT_LOCK && waveformScaleFactor != 6) {
					// draw scroll bar
					GX_SetLineWidth(24, GX_TO_ZERO);
					drawLine(SCREEN_TIMEPLOT_START, SCREEN_POS_CENTER_Y - 140,
							 SCREEN_TIMEPLOT_START + 499, SCREEN_POS_CENTER_Y - 140, GX_COLOR_GRAY);
					// calculate scroll bar position
					int scrollBarPosX = 500 - ((dataScrollOffset / (3000.0 - (500 * waveformScaleFactor))) * 500);
					drawSolidBox(SCREEN_TIMEPLOT_START + scrollBarPosX - 6, SCREEN_POS_CENTER_Y - 144,
								 SCREEN_TIMEPLOT_START + scrollBarPosX + 6, SCREEN_POS_CENTER_Y - 136,
								 GX_COLOR_WHITE);
				}
				
				setCursorPos(21,0);
				printStr("Scaling Factor: %dx", waveformScaleFactor);
				if (cState == INPUT_LOCK) {
					printStr(" | Offset: %d", dataScrollOffset);
				}
				
				int prevIndex = -1;
				int frameIntervals[500] = { 0 };
				int frameIntervalCount = 0;
				// draw graph
				updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
				
				GX_SetLineWidth(12, GX_TO_ZERO);
				
				GX_Begin(GX_LINESTRIP, GX_VTXFMT0, 500);
				
				// y first
				for (int i = 0; i < 500; i++) {
					int curr;
					if (!showCStick) {
						curr = data->samples[(startPoint + (i * waveformScaleFactor)) % REC_SAMPLE_MAX].stickY;
					} else {
						curr = data->samples[(startPoint + (i * waveformScaleFactor)) % REC_SAMPLE_MAX].cStickY;
					}
					
					GX_Position3s16(SCREEN_TIMEPLOT_START + waveformXPos, SCREEN_POS_CENTER_Y - curr, -3);
					GX_Color3u8(GX_COLOR_BLUE_Y.r, GX_COLOR_BLUE_Y.g, GX_COLOR_BLUE_Y.b);
					
					// frame interval stuff
					if (prevIndex != -1) {
						for (int j = 0; j < waveformScaleFactor; j++) {
							if (data->samples[(prevIndex + j) % REC_SAMPLE_MAX].timeDiffUs == 1) {
								if (waveformScaleFactor <= 2) {
									frameIntervals[frameIntervalCount] = waveformXPos;
									frameIntervalCount++;
									//DrawLine(SCREEN_TIMEPLOT_START + waveformXPos, (SCREEN_POS_CENTER_Y - 127),
									        //SCREEN_TIMEPLOT_START + waveformXPos, (SCREEN_POS_CENTER_Y - 112),
									        //COLOR_GRAY);
								}
							}
						}
					}
					prevIndex = (startPoint + (i * waveformScaleFactor)) % REC_SAMPLE_MAX;
					
					// update scaling factor
					waveformXPos++;
				}
				
				GX_End();
				
				// frame intervals
				
				GX_SetLineWidth(12, GX_TO_ZERO);
				GX_Begin(GX_LINES, GX_VTXFMT0, frameIntervalCount * 2);
				
				for (int i = 0; i < frameIntervalCount; i++) {
					GX_Position3s16(SCREEN_TIMEPLOT_START + frameIntervals[i], (SCREEN_POS_CENTER_Y - 127), -5);
					GX_Color3u8(GX_COLOR_GRAY.r, GX_COLOR_GRAY.g, GX_COLOR_GRAY.b);
					
					GX_Position3s16(SCREEN_TIMEPLOT_START + frameIntervals[i], (SCREEN_POS_CENTER_Y - 112), -5);
					GX_Color3u8(GX_COLOR_GRAY.r, GX_COLOR_GRAY.g, GX_COLOR_GRAY.b);
				}
				
				GX_End();
				
				GX_SetLineWidth(12, GX_TO_ZERO);
				
				waveformXPos = 1;
				// then x
				GX_Begin(GX_LINESTRIP, GX_VTXFMT0, 500);
				
				for (int i = 0; i < 500; i++) {
					int curr;
					if (!showCStick) {
						curr = data->samples[(startPoint + (i * waveformScaleFactor)) % REC_SAMPLE_MAX].stickX;
					} else {
						curr = data->samples[(startPoint + (i * waveformScaleFactor)) % REC_SAMPLE_MAX].cStickX;
					}
					
					GX_Position3s16(SCREEN_TIMEPLOT_START + waveformXPos, SCREEN_POS_CENTER_Y - curr, -3);
					GX_Color3u8(GX_COLOR_RED_X.r, GX_COLOR_RED_X.g, GX_COLOR_RED_X.b);
					
					prevIndex = (startPoint + (i * waveformScaleFactor)) % REC_SAMPLE_MAX;
					
					// update scaling factor
					waveformXPos++;
				}
				
				GX_End();
				
				if (!buttonLock){
					if (*pressed & PAD_BUTTON_A && !buttonLock) {
						if (cState == INPUT_LOCK) {
							cState = INPUT;
							waveformScaleFactor = 6;
							dataScrollOffset = 0;
						} else {
							cState = INPUT_LOCK;
						}
						buttonLock = true;
					}
					if (*pressed & PAD_BUTTON_Y && !buttonLock) {
						showCStick = !showCStick;
						buttonLock = true;
					}
					if (*pressed & PAD_BUTTON_UP && !buttonLock && cState == INPUT_LOCK) {
						waveformScaleFactor--;
						if (waveformScaleFactor < 1) {
							waveformScaleFactor = 1;
						}
						buttonLock = true;
					}
					if (*pressed & PAD_BUTTON_DOWN && !buttonLock && cState == INPUT_LOCK) {
						waveformScaleFactor++;
						if (waveformScaleFactor > 6) {
							waveformScaleFactor = 6;
						}
						buttonLock = true;
					}
					
					// bounds checks happen above, since they need to be adjusted depending on scale factor anyways
					if (*held & PAD_BUTTON_LEFT && cState == INPUT_LOCK) {
						dataScrollOffset += 25;
					} else if (*held & PAD_BUTTON_RIGHT && cState == INPUT_LOCK) {
						dataScrollOffset -= 25;
					}
				}
			}
			break;
	}
	
	if ((*held) == 0 && buttonLock) {
		buttonLock = false;
	}
}

void menu_continuousEnd() {
	setSamplingRateNormal();
	PAD_SetSamplingCallback(cb);
	state = CONT_SETUP;
}
