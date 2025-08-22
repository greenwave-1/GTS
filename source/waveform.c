//
// Created on 2023/10/30.
//

#include "waveform.h"

#include <stdlib.h>
#include <math.h>

static bool init = false;
static ControllerRec *recordingData;
static ControllerRec *tempData;

void initData() {
	if (!init) {
		recordingData = malloc(sizeof(ControllerRec));
		clearRecordingArray(recordingData);
		tempData = malloc(sizeof(ControllerRec));
		clearRecordingArray(tempData);
		
		init = true;
	}
}

ControllerRec** getRecordingData() {
	return &recordingData;
}

ControllerRec** getTempData() {
	return &tempData;
}

void clearRecordingArray(ControllerRec *recording) {
	// iterate over array
	for (int i = 0; i < REC_SAMPLE_MAX; i++) {
		recording->samples[i].stickX = 0;
		recording->samples[i].stickY = 0;
		recording->samples[i].cStickX = 0;
		recording->samples[i].cStickY = 0;
		recording->samples[i].triggerL = 0;
		recording->samples[i].triggerR = 0;
		recording->samples[i].buttons = 0;
		recording->samples[i].timeDiffUs = 0;
	}
	// set related values
	recording->sampleEnd = 0;
	recording->recordingType = REC_CLEAR;
	recording->isRecordingReady = false;
	recording->dataExported = false;
}

void flipData() {
	// switch pointers
	ControllerRec *temp = tempData;
	tempData = recordingData;
	recordingData = temp;
	
	// mark "old" data as not ready for display
	tempData->isRecordingReady = false;
}

// a lot of this comes from github.com/phobgcc/phobconfigtool
MeleeCoordinates convertStickRawToMelee(ControllerSample sample) {
	
	float floatStickX = abs(sample.stickX), floatStickY = abs(sample.stickY);
	float floatCStickX = abs(sample.cStickX), floatCStickY = abs(sample.cStickY);
	
	float stickMagnitude = sqrt((sample.stickX * sample.stickX) + (sample.stickY * sample.stickY));
	float cStickMagnitude = sqrt((sample.cStickX * sample.cStickX) + (sample.cStickY * sample.cStickY));
	
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
	
	MeleeCoordinates ret;
	
	// truncate the floats
	ret.stickXUnit = (int) floatStickX, ret.stickYUnit = (int) floatStickY;
	ret.cStickXUnit = (int) floatCStickX, ret.cStickYUnit = (int) floatCStickY;
	
	// convert to the decimal format for melee
	ret.stickXUnit = (((float) ret.stickXUnit) * 0.0125) * 10000;
	ret.stickYUnit = (((float) ret.stickYUnit) * 0.0125) * 10000;
	ret.cStickXUnit = (((float) ret.cStickXUnit) * 0.0125) * 10000;
	ret.cStickYUnit = (((float) ret.cStickYUnit) * 0.0125) * 10000;
	
	return ret;
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


