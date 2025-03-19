//
// Created on 3/17/25.
//

#include "continuous.h"
#include "../print.h"

#include <stdio.h>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include "../polling.h"
#include "../draw.h"
#include "../waveform.h"

const static u8 SCREEN_TIMEPLOT_START = 70;

static const uint32_t COLOR_RED_C = 0x846084d7;
static const uint32_t COLOR_BLUE_C = 0x6dd26d72;

static enum CONT_MENU_STATE state = CONT_SETUP;

static WaveformData data = { {{ 0 }}, 0, 500, true, false, false };
static int dataIndex = 0;

static int waveformScaleFactor = 6;
static int dataScrollOffset = 0;
static bool freeze = false;

static u64 prevSampleCallbackTick = 0;
static u64 sampleCallbackTick = 0;

static sampling_callback cb;

static void contSamplingCallback() {
	// time from last call of this function calculation
	prevSampleCallbackTick = sampleCallbackTick;
	sampleCallbackTick = gettime();
	if (prevSampleCallbackTick == 0) {
		prevSampleCallbackTick = sampleCallbackTick;
	}
	
	PAD_ScanPads();
	
	if (!freeze) {
		data.data[dataIndex].ax = PAD_StickX(0);
		data.data[dataIndex].ay = PAD_StickY(0);
		dataIndex++;
		if (dataIndex == WAVEFORM_SAMPLES) {
			dataIndex = 0;
		}
	}
}

static void setup() {
	data.endPoint = WAVEFORM_SAMPLES - 1;
	setSamplingRateHigh();
	cb = PAD_SetSamplingCallback(contSamplingCallback);
	state = CONT_POST_SETUP;
}

void menu_continuousWaveform(void *currXfb) {
	switch (state) {
		case CONT_SETUP:
			setup();
			break;
		case CONT_POST_SETUP:
			// draw guidelines based on selected test
			DrawBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 128, COLOR_WHITE, currXfb);
			DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y, COLOR_GRAY, currXfb);
			if (data.isDataReady) {
				// draw guidelines based on selected test
				DrawBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128, SCREEN_TIMEPLOT_START + 500,
				        SCREEN_POS_CENTER_Y + 128, COLOR_WHITE, currXfb);
				DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y, COLOR_GRAY, currXfb);
				// lots of the specific values are taken from:
				// https://github.com/PhobGCC/PhobGCC-doc/blob/main/For_Users/Phobvision_Guide_Latest.md
				
				if (data.endPoint < 500) {
					dataScrollOffset = 0;
				}
				
				int prevX = data.data[dataScrollOffset].ax;
				int prevY = data.data[dataScrollOffset].ay;
				
				int waveformPrevXPos = 0;
				int waveformXPos = 1;
				
				// draw 500 datapoints,
				// draw every dataScrollOffset point
				for (int i = 0; i < 500; i++) {
					// make sure we haven't gone outside our bounds
					if (i == data.endPoint || waveformXPos >= 500) {
						break;
					}
					// y first
					DrawLine(SCREEN_TIMEPLOT_START + waveformPrevXPos, SCREEN_POS_CENTER_Y - prevY,
					         SCREEN_TIMEPLOT_START + waveformXPos, SCREEN_POS_CENTER_Y - data.data[i * waveformScaleFactor].ay,
					         COLOR_BLUE_C, currXfb);
					prevY = data.data[i * waveformScaleFactor].ay;
					// then x
					DrawLine(SCREEN_TIMEPLOT_START + waveformPrevXPos, SCREEN_POS_CENTER_Y - prevX,
					         SCREEN_TIMEPLOT_START + waveformXPos, SCREEN_POS_CENTER_Y - data.data[i * waveformScaleFactor].ax,
					         COLOR_RED_C, currXfb);
					prevX = data.data[i * waveformScaleFactor].ax;
					
					// update scaling factor
					waveformPrevXPos = waveformXPos;
					waveformXPos++;
				}
			}
			break;
	}
}

void menu_continuousEnd() {
	setSamplingRateNormal();
	PAD_SetSamplingCallback(cb);
	state = CONT_SETUP;
}
