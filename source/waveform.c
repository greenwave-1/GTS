//
// Created on 2023/10/30.
//

#include "waveform.h"
#include <stdlib.h>
#include <gccore.h>
#include <math.h>
#include <ogc/lwp_watchdog.h>
#include "gecko.h"
#include "polling.h"

#ifdef DEBUGLOG
#include <string.h>
#endif

#define STICK_MOVEMENT_THRESHOLD 2
#define STICK_MOVEMENT_TIME_US 250000 // 250 ms

static bool isReadReady = false;
static u64 prevSampleCallbackTick = 0;
static u64 sampleCallbackTick = 0;

static sampling_callback cb;

// thank you extrems for pointing this out to me
void samplingCallback() {
	prevSampleCallbackTick = sampleCallbackTick;
	sampleCallbackTick = gettime();
	if (prevSampleCallbackTick == 0) {
		prevSampleCallbackTick = sampleCallbackTick;
	}
	isReadReady = true;
	
	//PAD_SetSamplingCallback(cb);
	return;
}

void measureWaveform(WaveformData *data) {
	// reset old data
	for (int i = 0; i < WAVEFORM_SAMPLES; i++) {
		data->data[i] = (WaveformDatapoint) { .ax = 0, .ay = 0, .cx = 0, .cy = 0, .buttonsHeld = 0, .timeDiffUs = 0,
						  .isAXNegative = false, .isAYNegative = false, .isCXNegative = false, .isCYNegative = false } ;
	}
	data->exported = false;
	
	setSamplingRateHigh();
	
	cb = PAD_SetSamplingCallback(samplingCallback);
	u64 temp;
	
	// we need a way to determine if the stick has stopped moving, this is a basic way to do so.
	// initial value is arbitrary, but not close enough to 0 so that the rest of the code continues to work.
	int prevPollDiffX = 10;
	int prevPollDiffY = 10;

	data->endPoint = 0;
	data->totalTimeUs = 0;
	
	prevSampleCallbackTick = 0;
	sampleCallbackTick = 0;

	// set start point
	int startPosX = PAD_StickX(0);
	int startPosY = PAD_StickY(0);

	// get data
	int currPollX = startPosX, prevPollX = startPosX;
	int currPollY = startPosY, prevPollY = startPosY;

	// wait for the stick to move roughly 10 units outside its starting position on either axis
	while ( (currPollX > startPosX - STICK_MOVEMENT_THRESHOLD && currPollX < startPosX + STICK_MOVEMENT_THRESHOLD) &&
			(currPollY > startPosY - STICK_MOVEMENT_THRESHOLD && currPollY < startPosY + STICK_MOVEMENT_THRESHOLD) ) {
		while (!isReadReady) {
			temp = gettime();
			// sleep for 10 microseconds between checks
			while (ticks_to_microsecs(gettime() - temp) > 10);
		}
		PAD_ScanPads();
		currPollX = PAD_StickX(0);
		prevPollX = currPollX;
		currPollY = PAD_StickY(0);
		prevPollY = currPollY;
		isReadReady = false;
	}

	u64 noMovementTimer = 0;
	int noMovementStartIndex = -1;
	while (true) {
		// wait for poll
		while (!isReadReady) {
			temp = gettime();
			// sleep for 10 microseconds between checks
			while (ticks_to_microsecs(gettime() - temp) > 10);
		}
		
		// update stick values
		PAD_ScanPads();

		prevPollX = currPollX;
		prevPollY = currPollY;
		currPollX = PAD_StickX(0);
		currPollY = PAD_StickY(0);
		prevPollDiffX = currPollX - prevPollX;
		prevPollDiffY = currPollY - prevPollY;

		// add data
		data->data[data->endPoint].ax = currPollX;
		data->data[data->endPoint].ay = currPollY;
		if (data->endPoint == 0) {
			data->data[0].timeDiffUs = 0;
		} else {
			data->data[data->endPoint].timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
		}
		data->endPoint++;

		// have we overrun our array?
		if (data->endPoint == WAVEFORM_SAMPLES) {
			// truncate if the stick was not moving
			if (noMovementStartIndex != -1) {
				data->endPoint = noMovementStartIndex;
			}
			break;
		}

		// only run stick position checks if we aren't doing a continuous poll
		if (!data->fullMeasure) {
			// has the stick stopped moving (as defined by STICK_MOVEMENT_THRESHOLD)
			if (prevPollDiffX < STICK_MOVEMENT_THRESHOLD && prevPollDiffX > -STICK_MOVEMENT_THRESHOLD &&
				prevPollDiffY < STICK_MOVEMENT_THRESHOLD && prevPollDiffY > -STICK_MOVEMENT_THRESHOLD) {
				if (noMovementStartIndex == -1) {
					noMovementStartIndex = data->endPoint;
				} else {
					noMovementTimer += data->data[data->endPoint - 1].timeDiffUs;
				}
				
				// break if enough time passes
				if (noMovementTimer >= STICK_MOVEMENT_TIME_US) {
					// ugly way to truncate "dead" inputs
					data->endPoint = noMovementStartIndex;
					break;
				}
			} else {
				noMovementStartIndex = -1;
				noMovementTimer = 0;
			}
		}
		isReadReady = false;
	}
	data->isDataReady = true;
	PAD_SetSamplingCallback(cb);
	
	// calculate total read time
	for (int i = 0; i < data->endPoint; i++) {
		data->totalTimeUs += data->data[i].timeDiffUs;
	}
	// polling rate gets reset by main loop, no need to do it here
}


// a lot of this comes from github.com/phobgcc/phobconfigtool
WaveformDatapoint convertStickValues(WaveformDatapoint *data) {
	WaveformDatapoint retData;

	retData.ax = data->ax, retData.ay = data->ay;
	retData.cx = data->cx, retData.cy = data->cy;
	
	// store whether x or y are negative
	retData.isAXNegative = (retData.ax < 0) ? true : false;
	retData.isAYNegative = (retData.ay < 0) ? true : false;
	retData.isCXNegative = (retData.cx < 0) ? true : false;
	retData.isCYNegative = (retData.cy < 0) ? true : false;
	
	float floatStickX = retData.ax, floatStickY = retData.ay;
	float floatCStickX = retData.cx, floatCStickY = retData.cy;

	float stickMagnitude = sqrt((retData.ax * retData.ax) + (retData.ay * retData.ay));
	float cStickMagnitude = sqrt((retData.cx * retData.cx) + (retData.cy * retData.cy));

	// magnitude must be between 0 and 80
	if (stickMagnitude > 80) {
		// scale stick value to be within range
		floatStickX = (floatStickX / stickMagnitude) * 80;
		floatStickY = (floatStickY / stickMagnitude) * 80;
	}
	if (cStickMagnitude > 80) {
		// scale stick value to be within range
		floatCStickX = (floatCStickX / cStickMagnitude) * 80;
		floatCStickY = (floatCStickY / cStickMagnitude) * 80;
	}
	
	// truncate the floats
	retData.ax = (int) floatStickX, retData.ay = (int) floatStickY;
	retData.cx = (int) floatCStickX, retData.cy = (int) floatCStickY;

	// convert to the decimal format for melee
	retData.ax = (((float) retData.ax) * 0.0125) * 10000;
	retData.ay = (((float) retData.ay) * 0.0125) * 10000;
	retData.cx = (((float) retData.cx) * 0.0125) * 10000;
	retData.cy = (((float) retData.cy) * 0.0125) * 10000;

	// get rid of any negative values
	retData.ax = abs(retData.ax), retData.ay = abs(retData.ay);
	retData.cx = abs(retData.cx), retData.cy = abs(retData.cy);

	return retData;
}


