//
// Created on 2025/03/17.
//

#include "continuous.h"
#include "../print.h"

#include <stdio.h>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include "../polling.h"
#include "../draw.h"
#include "../waveform.h"

char strBuffer[100];

const static u8 SCREEN_TIMEPLOT_START = 70;

static const uint32_t COLOR_RED_C = 0x846084d7;
static const uint32_t COLOR_BLUE_C = 0x6dd26d72;

static enum CONT_MENU_STATE state = CONT_SETUP;
static enum CONT_STATE cState = INPUT;

static WaveformData data = { {{ 0 }}, 0, 500, true, false, false };
static int dataIndex = 0;

static int waveformScaleFactor = 6;
static int dataScrollOffset = 0;
static bool pressLocked = false;
static bool freeze = false;
static bool showCStick = false;

static bool buttonLock = false;
static u32 *pressed;
static u32 *held;

static u64 prevSampleCallbackTick = 0;
static u64 sampleCallbackTick = 0;
static u64 pressedTimer = 0;

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
		data.data[dataIndex].ax = PAD_StickX(0);
		data.data[dataIndex].ay = PAD_StickY(0);
		data.data[dataIndex].cx = PAD_SubStickX(0);
		data.data[dataIndex].cy = PAD_SubStickY(0);
		dataIndex++;
		if (dataIndex == WAVEFORM_SAMPLES) {
			dataIndex = 0;
		}
	}
}

static void setup(u32 *p, u32 *h) {
	pressed = p;
	held = h;
	data.endPoint = WAVEFORM_SAMPLES - 1;
	setSamplingRateHigh();
	cb = PAD_SetSamplingCallback(contSamplingCallback);
	state = CONT_POST_SETUP;
}

void menu_continuousWaveform(void *currXfb, u32 *p, u32 *h) {
	switch (state) {
		case CONT_SETUP:
			setup(p, h);
			break;
		case CONT_POST_SETUP:
			setCursorPos(3, 0);
			if (!showCStick) {
				printStr("Analog Stick", currXfb);
			} else {
				printStr("C-Stick", currXfb);
			}
			if (cState == INPUT_LOCK) {
				freeze = true;
				setCursorPos(2, 28);
				printStrColor("LOCKED", currXfb, COLOR_WHITE, COLOR_BLACK);
			} else {
				freeze = false;
			}

			if (data.isDataReady) {
				// draw guidelines based on selected test
				DrawBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128, SCREEN_TIMEPLOT_START + 500,
				        SCREEN_POS_CENTER_Y + 128, COLOR_WHITE, currXfb);
				DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y, COLOR_GRAY, currXfb);
				// lots of the specific values are taken from:
				// https://github.com/PhobGCC/PhobGCC-doc/blob/main/For_Users/Phobvision_Guide_Latest.md
				
				// reset offset if its invalid
				if (dataScrollOffset > (3000 - (500 * waveformScaleFactor))) {
					dataScrollOffset = (3000 - (500 * waveformScaleFactor));
				} else if (dataScrollOffset < 0) {
					dataScrollOffset = 0;
				}
				
				int prevX = 0;
				int prevY = 0;
				
				int waveformPrevXPos = 0;
				int waveformXPos = 1;
				
				// calculate start point
				// waveformScaleFactor determines how much information is shown by only drawing every x point
				int startPoint = (dataIndex - (500 * waveformScaleFactor) - dataScrollOffset);
				if (startPoint < 0) {
					startPoint += 3000;
				}
				
				setCursorPos(20,0);
				sprintf(strBuffer, "Scaling Factor: %d\n", waveformScaleFactor);
				printStr(strBuffer, currXfb);
				if (cState == INPUT_LOCK) {
					sprintf(strBuffer, "Offset: %d", dataScrollOffset);
					printStr(strBuffer, currXfb);
				}
				
				for (int i = 0; i < 500; i++) {
					int currX, currY;
					if (!showCStick) {
						currX = data.data[(startPoint + (i * waveformScaleFactor)) % WAVEFORM_SAMPLES].ax;
						currY = data.data[(startPoint + (i * waveformScaleFactor)) % WAVEFORM_SAMPLES].ay;
					} else {
						currX = data.data[(startPoint + (i * waveformScaleFactor)) % WAVEFORM_SAMPLES].cx;
						currY = data.data[(startPoint + (i * waveformScaleFactor)) % WAVEFORM_SAMPLES].cy;
					}
					
					// y first
					DrawLine(SCREEN_TIMEPLOT_START + waveformPrevXPos, SCREEN_POS_CENTER_Y - prevY,
					         SCREEN_TIMEPLOT_START + waveformXPos, SCREEN_POS_CENTER_Y - currY,
					         COLOR_BLUE_C, currXfb);
					prevY = currY;
					// then x
					DrawLine(SCREEN_TIMEPLOT_START + waveformPrevXPos, SCREEN_POS_CENTER_Y - prevX,
					         SCREEN_TIMEPLOT_START + waveformXPos, SCREEN_POS_CENTER_Y - currX,
					         COLOR_RED_C, currXfb);
					prevX = currX;
					
					// update scaling factor
					waveformPrevXPos = waveformXPos;
					waveformXPos++;
				}
				
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
