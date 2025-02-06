//
// Created on 10/30/23.
//

#include "waveform.h"
#include <stdlib.h>
#include <gccore.h>
#include <math.h>
#include <ogc/lwp_watchdog.h>


#define STICK_MOVEMENT_THRESHOLD 5
#define STICK_ORIGIN_THRESHOLD 3

//
void measureWaveform(WaveformData *data) {
	// set sampling rate high
	SI_SetSamplingRate(1);

	// we need a way to determine if the stick has stopped moving, this is a basic way to do so.
	// initial value is arbitrary, but not close enough to 0 so that the rest of the code continues to work.
	int prevPollDiffX = 10;
	int prevPollDiffY = 10;
	unsigned int stickNotMovingCounter = 0;

	data->endPoint = 0;

	// timer var
	long long unsigned int startTime = gettime();

	// set start point
	int startPosX = PAD_StickX(0);
	int startPosY = PAD_StickY(0);

	// get data
	int currPollX = startPosX, prevPollX = startPosX;
	int currPollY = startPosY, prevPollY = startPosY;

	// wait for the stick to move roughly 10 units outside its starting position on either axis
	while ( (currPollX > startPosX - STICK_MOVEMENT_THRESHOLD && currPollX < startPosX + STICK_MOVEMENT_THRESHOLD) &&
			(currPollY > startPosY - STICK_MOVEMENT_THRESHOLD && currPollY < startPosY + STICK_MOVEMENT_THRESHOLD) ) {
		PAD_ScanPads();
		currPollX = PAD_StickX(0);
		prevPollX = currPollX;
		currPollY = PAD_StickY(0);
		prevPollY = currPollY;
	}

	startTime = gettime();
	// add the initial value
	data->data[data->endPoint].ax = currPollX;
	data->data[data->endPoint].ay = currPollY;
	data->endPoint++;

	// wait for 1ms to pass
	while (ticks_to_millisecs(gettime() - startTime) < 1);

	while (true) {
		// get time for polling
		startTime = gettime();

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
		data->endPoint++;

		// have we overrun our array?
		if (data->endPoint == WAVEFORM_SAMPLES - 1) {
			break;
		}

		// only run stick position checks if we aren't doing a continuous poll
		if (!data->fullMeasure) {
			// has the stick stopped moving (as defined by STICK_MOVEMENT_THRESHOLD)
			if (prevPollDiffX < STICK_MOVEMENT_THRESHOLD && prevPollDiffX > -STICK_MOVEMENT_THRESHOLD &&
				prevPollDiffY < STICK_MOVEMENT_THRESHOLD && prevPollDiffY > -STICK_MOVEMENT_THRESHOLD) {

				// is the stick close to origin?
				if (currPollX < STICK_ORIGIN_THRESHOLD && currPollX > -STICK_ORIGIN_THRESHOLD &&
					currPollY < STICK_ORIGIN_THRESHOLD && currPollY > -STICK_ORIGIN_THRESHOLD) {
					// accelerate our counter if we're not moving _and_ at origin
					stickNotMovingCounter += 25;
				}
				stickNotMovingCounter++;

				// break if the stick continues to not move
				// TODO: this should be tweaked
				if (stickNotMovingCounter > 500) {
					break;
				}
			} else {
				stickNotMovingCounter = 0;
			}
		}

		// slow down polling
		// polling every ~1ms
		while (ticks_to_millisecs(gettime() - startTime) < 1);
	}
	data->isDataReady = true;

	//return sampling rate to normal
	SI_SetSamplingRate(0);
}

// a lot of this comes from github.com/phobgcc/phobconfigtool
WaveformDatapoint convertStickValues(WaveformDatapoint *data) {
	WaveformDatapoint retData;

	retData.ax = data->ax, retData.ay = data->ay;
	retData.cx = data->cx, retData.cy = data->cy;

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


// taken from github.com/phobgcc/phobconfigtool
// should probably replace this with something gl based at some point
/*
* takes in values to draw a horizontal line of a given color
*/
void DrawHLine (int x1, int x2, int y, int color, void *xfb) {
	int i;
	y = 320 * y;
	x1 >>= 1;
	x2 >>= 1;
	for (i = x1; i <= x2; i++) {
		u32 *tmpfb = xfb;
		tmpfb[y+i] = color;
	}
}

/*
* takes in values to draw a vertical line of a given color
*/
void DrawVLine (int x, int y1, int y2, int color, void *xfb) {
	int i;
	x >>= 1;
	for (i = y1; i <= y2; i++) {
		u32 *tmpfb = xfb;
		tmpfb[x + (640 * i) / 2] = color;
	}
}

/*
* takes in values to draw a box of a given color
*/
void DrawBox (int x1, int y1, int x2, int y2, int color, void *xfb) {
	DrawHLine (x1, x2, y1, color, xfb);
	DrawHLine (x1, x2, y2, color, xfb);
	DrawVLine (x1, y1, y2, color, xfb);
	DrawVLine (x2, y1, y2, color, xfb);
}
