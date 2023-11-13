//
// Created on 10/30/23.
//

#include "waveform.h"
#include <stdlib.h>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>

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
	data->minX = 0, data->minY = 0;
	data->maxX = 0; data->maxY = 0;

	// timer var
	long long unsigned int startTime = gettime();

	// set start point
	int startPosX = PAD_StickX(0);
	int startPosY = PAD_StickY(0);

	// get data
	int currPollX = startPosX, prevPollX = startPosX;
	int currPollY = startPosY, prevPollY = startPosY;

	// wait for the stick to move roughly 10 units outside of its starting position on either axis
	while ( (currPollX > startPosX - 10 && currPollX < startPosX + 10) &&
			(currPollY > startPosY - 10 && currPollY < startPosY + 10) ) {
		PAD_ScanPads();
		currPollX = PAD_StickX(0);
		prevPollX = currPollX;
		currPollY = PAD_StickY(0);
		prevPollY = currPollY;
	}

	// add the initial value
	data->data[data->endPoint].ax = currPollX;
	data->data[data->endPoint].ay = currPollY;
	data->endPoint++;

	startTime = gettime();
	// wait for 1ms to pass
	while (ticks_to_millisecs(gettime() - startTime) == 0);

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

		// update min/max values
		if (currPollX > data->maxX) {
			data->maxX = currPollX;
		}
		if (currPollX < data->minX) {
			data->minX = currPollX;
		}
		if (currPollY > data->maxY) {
			data->maxY = currPollY;
		}
		if (currPollY < data->minY) {
			data->minY = currPollY;
		}

		// has the stick stopped moving, and are we close to 0?
		if ( (prevPollDiffX < 2 && prevPollDiffX > -2 && prevPollDiffY < 2 && prevPollDiffY > -2) &&
				(currPollX < 2 && currPollX > -2 && currPollY < 2 && currPollY > -2)) {
			stickNotMovingCounter++;

			// arbitrarily break after a certain number of passes
			if (stickNotMovingCounter > 100) {
				break;
			}
		} else {
			stickNotMovingCounter = 0;
		}

		// slow down polling
		// polling at ~1ms
		while (ticks_to_millisecs(gettime() - startTime) == 0);
	}
	data->isDataReady = true;

	//return sampling rate to normal
	SI_SetSamplingRate(0);
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
