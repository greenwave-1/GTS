//
// Created on 10/30/23.
//

#include "waveform.h"
#include <stdlib.h>
#include <gccore.h>
#include <math.h>
#include <ogc/lwp_watchdog.h>
#include "gecko.h"
#include "polling.h"

#ifdef DEBUG
#include <string.h>
#endif

#define STICK_MOVEMENT_THRESHOLD 5
#define STICK_ORIGIN_THRESHOLD 3


void measureWaveform(WaveformData *data) {
	// set SI Polling rate
	// poll 17 times per frame, every 31 horizontal lines
	//SI_SetXY(31,17); // normal 985, vblank 920
	
	//SI_SetXY(11, 24); // normal 698, then 604, then 667, then back
	setSamplingRateHigh();
	
	
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


// TODO: move all of this to another file

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


void DrawFilledBox (int x1, int y1, int x2, int y2, int color, void *xfb) {
	for (int i = x1; i < x2 + 1; i++) {
		DrawVLine(i, y1, y2, color, xfb);
	}
}


// draw a line given two coordinates, using Bresenham's line-drawing algorithm
void DrawLine(int x1, int y1, int x2, int y2, int color, void *xfb) {
	int distanceX = x2 - x1, distanceY = y2 - y1;
	u32 *tmpfb = xfb;
	// used for when one coordinate goes negative
	int xDir = 1, yDir = 1;
	
	// make sure our magnitude is positive, and note if the drawing direction should be negative
	if (distanceX < 0) {
		xDir = -1;
		distanceX *= -1;
	}
	if (distanceY < 0) {
		yDir = -1;
		distanceY *= -1;
	}
	
	// x will be our primary axis
	if (distanceX > distanceY) {
		int delta = 2 * distanceY - distanceX;
		int currY = y1;
		
		for (int i = 0; i < distanceX; i++) {
			tmpfb[((x1 + (i * xDir)) + (currY * 640)) / 2] = color;
			if (delta > 0) {
				currY += (1 * yDir);
				delta -= (2 * distanceX);
			}
			delta += (2 * distanceY);
		}
	// y will be our primary axis
	} else {
		int delta = 2 * distanceX - distanceY;
		int currX = x1;
		
		for (int i = 0; i < distanceY; i++) {
			tmpfb[(((y1 + (i * yDir)) * 640) + currX) / 2] = color;
			//tmpfb[((x1 + i) + (currY * 640)) / 2] = color;
			if (delta > 0) {
				currX += (1 * xDir);
				delta -= (2 * distanceY);
			}
			delta += (2 * distanceX);
		}
	}
}


void DrawDot (int x, int y, int color, void *xfb) {
	x >>= 1;
	u32 *tmpfb = xfb;
	tmpfb[x + (640 * y) / 2] = color;
}


// mostly taken from https://www.geeksforgeeks.org/mid-point-circle-drawing-algorithm/
void DrawCircle (int cx, int cy, int r, int color, void *xfb) {
	u32 *tmpfb = xfb;

	int x = r, y = 0;
	
	if (r > 0) {
		DrawDot(cx + x, cy - y, color, xfb);
		DrawDot(cx - x, cy + y, color, xfb);
		DrawDot(cx + y, cy + x, color, xfb);
		DrawDot(cx - y, cy + x, color, xfb);
	}
	
	int delta = 1 - r;
	
	while (x > y) {
		y++;
		
		if (delta <= 0) {
			delta = delta + 2*y + 1;
		} else {
			x--;
			delta = delta + 2 * y - 2 * x + 1;
		}
		
		if (x < y) {
			break;
		}
		
		DrawDot(cx + x, cy + y, color, xfb);
		DrawDot(cx - x, cy - y, color, xfb);
		DrawDot(cx + x, cy - y, color, xfb);
		DrawDot(cx - x, cy + y, color, xfb);
		
		if (x != y) {
			DrawDot(cx + y, cy + x, color, xfb);
			DrawDot(cx - y, cy + x, color, xfb);
			DrawDot(cx + y, cy - x, color, xfb);
			DrawDot(cx - y, cy - x, color, xfb);
		}
	}
}


void DrawFilledCircle(int cx, int cy, int r, int interval, int color, void *xfb) {
	for (int i = r; i > 0; i -= interval) {
		DrawCircle(cx, cy, i, color, xfb);
	}
}
