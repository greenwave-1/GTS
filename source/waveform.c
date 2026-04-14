//
// Created on 2023/10/30.
//

#include "waveform.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// why is this required?
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// first value outside the melee deadzone
#define MELEE_DEADZONE_END 23

// bitwise or'd flags that specify what recordings are valid for a given menu
// note that the order is important, same order as enum RECORDING_TYPE
const uint8_t RECORDING_TYPE_VALID_MENUS[] = {
	// REC_CLEAR, null entry
	0,

	// normal oscilloscope
	REC_OSCILLOSCOPE_FLAG | REC_2DPLOT_FLAG,

	// continuous oscilloscope
	0,

	// trigger L
	REC_TRIGGER_L_FLAG | REC_TRIGGER_R_FLAG,

	// trigger R
	REC_TRIGGER_L_FLAG | REC_TRIGGER_R_FLAG,

	// 2d plot
	REC_OSCILLOSCOPE_FLAG | REC_2DPLOT_FLAG,

	// buttonplot
	REC_2DPLOT_FLAG | REC_BUTTONTIME_FLAG
};


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
	MeleeCoordinates ret;

	int deadzoneX, deadzoneY;
	int deadzoneCX, deadzoneCY;

	// store and scale coordinates (if necessary)
	{
		float fX = abs(sample.stickX), fY = abs(sample.stickY);
		float fCX = abs(sample.cStickX), fCY = abs(sample.cStickY);

		float mag = sqrt((fX * fX) + (fY * fY));
		float cMag = sqrt((fCX * fCX) + (fCY * fCY));

		// scale values if necessary
		if (mag > 80) {
			fX = (fX / mag) * 80;
			fY = (fY / mag) * 80;
		}

		if (cMag > 80) {
			fCX = (fCX / cMag) * 80;
			fCY = (fCY / cMag) * 80;
		}

		// store values
		ret.stickX = (int) fX, ret.stickY = (int) fY;
		deadzoneX = ret.stickX, deadzoneY = ret.stickY;

		ret.cStickX = (int) fCX, ret.cStickY = (int) fCY;
		deadzoneCX = ret.cStickX, deadzoneCY = ret.cStickY;

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
	}

	// apply deadzone prior to calc magnitude
	deadzoneX = deadzoneX < MELEE_DEADZONE_END ? 0 : deadzoneX;
	deadzoneY = deadzoneY < MELEE_DEADZONE_END ? 0 : deadzoneY;

	deadzoneCX = deadzoneCX < MELEE_DEADZONE_END ? 0 : deadzoneCX;
	deadzoneCY = deadzoneCY < MELEE_DEADZONE_END ? 0 : deadzoneCY;

	// calc magnitude
	ret.stickMagnitude = sqrt((deadzoneX * deadzoneX) + (deadzoneY * deadzoneY));
	ret.cStickMagnitude = sqrt((deadzoneCX * deadzoneCX) + (deadzoneCY * deadzoneCY));

	// get angle
	ret.stickAngle = atan2(deadzoneY, deadzoneX) * 180 / M_PI;
	ret.cStickAngle = atan2(deadzoneCY, deadzoneCX) * 180 / M_PI;
	
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
