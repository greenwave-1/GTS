//
// Created on 2023/10/30.
//

#include "waveform.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// bitwise or'd flags that specify what recordings are valid for a given menu
// note that the order is important, same order as enum RECORDING_TYPE
const uint8_t RECORDING_TYPE_VALID_MENUS[] = { 0, // REC_CLEAR, null entry
											   REC_OSCILLOSCOPE_FLAG | REC_2DPLOT_FLAG, // normal oscilloscope
											   0, // continuous oscilloscope
											   REC_TRIGGER_L_FLAG | REC_TRIGGER_R_FLAG, // trigger L
											   REC_TRIGGER_L_FLAG | REC_TRIGGER_R_FLAG, // trigger R
											   REC_OSCILLOSCOPE_FLAG | REC_2DPLOT_FLAG, // 2d plot
											   REC_2DPLOT_FLAG | REC_BUTTONTIME_FLAG }; // buttonplot


static bool init = false;
static ControllerRec *recordingData = NULL;
static ControllerRec *tempData = NULL;
static ControllerRec *continuousMenuData = NULL;

// allocate memory for and initialize recording structs
void initControllerRecStructs() {
	if (!init) {
		// data for most menus that do high-speed recording
		recordingData = malloc(sizeof(ControllerRec));
		clearRecordingArray(recordingData);
		tempData = malloc(sizeof(ControllerRec));
		clearRecordingArray(tempData);
		
		// continuous oscilloscope's data
		continuousMenuData = malloc(sizeof(ControllerRec));
		clearRecordingArray(continuousMenuData);
		
		init = true;
	}
}

void freeControllerRecStructs() {
	free(recordingData);
	recordingData = NULL;
	
	free(tempData);
	tempData = NULL;
	
	free(continuousMenuData);
	continuousMenuData = NULL;
}

ControllerRec** getRecordingData() {
	return &recordingData;
}

ControllerRec** getTempData() {
	return &tempData;
}

ControllerRec* getContinuousData() {
	return continuousMenuData;
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
	ret.stickX = (int) floatStickX, ret.stickY = (int) floatStickY;
	ret.cStickX = (int) floatCStickX, ret.cStickY = (int) floatCStickY;
	
	// set sign
	if (sample.stickX < 0) {
		ret.stickX *= -1;
	}
	if (sample.stickY < 0) {
		ret.stickY *= -1;
	}
	if (sample.cStickX < 0) {
		ret.cStickX *= -1;
	}
	if (sample.cStickY < 0) {
		ret.cStickY *= -1;
	}
	
	return ret;
}

// simple helper function to avoid stupid if-else nonsense in polling functions
int8_t getControllerSampleAxisValue(ControllerSample sample, enum CONTROLLER_STICK_AXIS axis) {
	switch (axis) {
		case AXIS_AX:
			return sample.stickX;
		case AXIS_AY:
			return sample.stickY;
		case AXIS_CX:
			return sample.cStickX;
		case AXIS_CY:
			return sample.cStickY;
		case AXIS_AXY:
		case AXIS_CXY:
		default:
			return 0;
	}
}

int8_t getControllerSampleXValue(ControllerSample sample, enum CONTROLLER_STICK_AXIS axis) {
	switch (axis) {
		case AXIS_AXY:
			return sample.stickX;
		case AXIS_CXY:
			return sample.cStickX;
		case AXIS_AX:
		case AXIS_AY:
		case AXIS_CX:
		case AXIS_CY:
		default:
			return 0;
	}
}

int8_t getControllerSampleYValue(ControllerSample sample, enum CONTROLLER_STICK_AXIS axis) {
	switch (axis) {
		case AXIS_AXY:
			return sample.stickY;
		case AXIS_CXY:
			return sample.cStickY;
		case AXIS_AX:
		case AXIS_AY:
		case AXIS_CX:
		case AXIS_CY:
		default:
			return 0;
	}
}

// same as above, but return a given axis pair instead of a single value
void getControllerSampleAxisPair(ControllerSample sample, enum CONTROLLER_STICK_AXIS axis, int8_t *retX, int8_t *retY) {
	switch (axis) {
		case AXIS_AXY:
			*retX = sample.stickX;
			*retY = sample.stickY;
			break;
		case AXIS_CXY:
			*retX = sample.cStickX;
			*retY = sample.cStickY;
			break;
		case AXIS_AX:
		case AXIS_AY:
		case AXIS_CX:
		case AXIS_CY:
		default:
			break;
	}
}

static char meleeCoordString[20];
char* getMeleeCoordinateString(MeleeCoordinates coords, enum CONTROLLER_STICK_AXIS axis) {
	// terminate string just in case...
	meleeCoordString[0] = '\0';
	
	int8_t coordX = 0, coordY = 0;
	
	// analog stick?
	if (axis & AXIS_AXY) {
		coordX = coords.stickX;
		coordY = coords.stickY;
	}
	// c-stick?
	else if (axis & AXIS_CXY) {
		coordX = coords.cStickX;
		coordY = coords.cStickY;
	}
	
	// convert to melee units
	float valueX = (coordX * 125) / 10000.0, valueY = (coordY * 125) / 10000.0;
	
	// do we need two strings?
	if (axis > 0x10) {
		snprintf(meleeCoordString, 18, "%7.4f,%7.4f", valueX, valueY);
	} else {
		float targetValue = 0.0;
		switch (axis) {
			case AXIS_AX:
			case AXIS_CX:
				targetValue = valueX;
				break;
			case AXIS_AY:
			case AXIS_CY:
				targetValue = valueY;
				break;
			default:
				break;
			
		}
		snprintf(meleeCoordString, 18, "%7.4f", targetValue);
	}
	
	return meleeCoordString;
}
