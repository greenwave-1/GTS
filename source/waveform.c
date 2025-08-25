//
// Created on 2023/10/30.
//

#include "waveform.h"

#include <stdlib.h>
#include <math.h>

static bool init = false;
static ControllerRec *recordingData = NULL;
static ControllerRec *tempData = NULL;

// allocate memory for and initialize recording structs
void initControllerRecStructs() {
	if (!init) {
		recordingData = malloc(sizeof(ControllerRec));
		clearRecordingArray(recordingData);
		tempData = malloc(sizeof(ControllerRec));
		clearRecordingArray(tempData);
		
		init = true;
	}
}

void freeControllerRecStructs() {
	if (recordingData != NULL) {
		free(recordingData);
		recordingData = NULL;
	}
	if (tempData != NULL) {
		free(tempData);
		tempData = NULL;
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
	recording->totalTimeUs = 0;
	recording->recordingType = REC_CLEAR;
	recording->isRecordingReady = false;
	recording->dataExported = false;
}

// change what static pointers are pointing to
// this allow a double pointer only get an address once, and still be able to swap values
void flipData() {
	// switch pointers
	ControllerRec *temp = tempData;
	tempData = recordingData;
	recordingData = temp;
	
	// mark "old" data as not ready for display
	tempData->isRecordingReady = false;
	tempData->dataExported = false;
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
	
	ret.stickXNegative = (sample.stickX < 0) ? true : false;
	ret.stickYNegative = (sample.stickY < 0) ? true : false;
	ret.cStickXNegative = (sample.cStickX < 0) ? true : false;
	ret.cStickYNegative = (sample.cStickY < 0) ? true : false;
	
	return ret;
}
