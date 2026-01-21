//
// Created on 2023/10/30.
//

#include "waveform.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// bitwise or'd flags that specify what recordings are valid for a given menu
const uint8_t RECORDING_TYPE_VALID_MENUS[] = { 0,
											   REC_OSCILLOSCOPE_FLAG | REC_2DPLOT_FLAG,
											   REC_TRIGGER_L_FLAG | REC_TRIGGER_R_FLAG,
											   REC_TRIGGER_L_FLAG | REC_TRIGGER_R_FLAG,
											   REC_OSCILLOSCOPE_FLAG | REC_2DPLOT_FLAG,
											   REC_2DPLOT_FLAG | REC_BUTTONTIME_FLAG };


static bool init = false;
static ControllerRec *recordingData = NULL;
static ControllerRec *tempData = NULL;
static ControllerRec *continuousMenuData = NULL;

void setContinuousRecStructPtr(ControllerRec* ptr) {
	continuousMenuData = ptr;
}

bool isContinuousRecDataNull() {
	return continuousMenuData == NULL;
}

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
	if (continuousMenuData != NULL) {
		free(continuousMenuData);
		continuousMenuData = NULL;
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
	
	// record if a given axis was negative, since we don't retain that info in the converted units
	ret.stickXNegative = (sample.stickX < 0) ? true : false;
	ret.stickYNegative = (sample.stickY < 0) ? true : false;
	ret.cStickXNegative = (sample.cStickX < 0) ? true : false;
	ret.cStickYNegative = (sample.cStickY < 0) ? true : false;
	
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
	uint16_t selectedValue1 = 0, selectedValue2 = 0;
	bool value1Negative = false, value2Negative = false;
	
	switch (axis) {
		case AXIS_AX:
			selectedValue1 = coords.stickXUnit;
			value1Negative = coords.stickXNegative;
			break;
		case AXIS_AY:
			selectedValue1 = coords.stickYUnit;
			value1Negative = coords.stickYNegative;
			break;
		case AXIS_CX:
			selectedValue1 = coords.cStickXUnit;
			value1Negative = coords.cStickXNegative;
			break;
		case AXIS_CY:
			selectedValue1 = coords.cStickYUnit;
			value1Negative = coords.cStickYNegative;
			break;
		case AXIS_AXY:
			selectedValue1 = coords.stickXUnit;
			selectedValue2 = coords.stickYUnit;
			value1Negative = coords.stickXNegative;
			value2Negative = coords.stickYNegative;
			break;
		case AXIS_CXY:
			selectedValue1 = coords.cStickXUnit;
			selectedValue2 = coords.cStickYUnit;
			value1Negative = coords.cStickXNegative;
			value2Negative = coords.cStickYNegative;
			break;
		default:
			snprintf(meleeCoordString, 16, "Error!");
			return meleeCoordString;
			break;
	}
	
	// AXIS_AXY or AXIS_CXY, we want both coordinates in one string
	if (axis >= AXIS_AXY) {
		// store sign
		char retStrSign1 = ' ', retStrSign2 = ' ';
		if (value1Negative) {
			retStrSign1 = '-';
		}
		if (value2Negative) {
			retStrSign2 = '-';
		}
		
		// there are 3 permutations for possible prints, it isn't possible for both to be 1.0
		// only x is 1.0
		if (selectedValue1 == 10000) {
			snprintf(meleeCoordString, 18, "%c1.0000,%c0.%04d", retStrSign1, retStrSign2, selectedValue2);
		}
		// only y is 1.0
		else if (selectedValue2 == 10000) {
			snprintf(meleeCoordString, 18, "%c0.%04d,%c1.0000", retStrSign1, selectedValue1, retStrSign2);
		}
		// neither are 1.0
		else {
			snprintf(meleeCoordString, 18, "%c0.%04d,%c0.%04d", retStrSign1, selectedValue1, retStrSign2, selectedValue2);
		}
	}
	// we only want one value
	else {
		// store sign
		char retStrSign = ' ';
		if (value1Negative) {
			retStrSign = '-';
		}
		
		// is this a 1.0 value?
		if (selectedValue1 == 10000) {
			snprintf(meleeCoordString, 18, "%c1.0000", retStrSign);
		} else {
			snprintf(meleeCoordString, 18, "%c0.%04d", retStrSign, selectedValue1);
		}
	}
	
	return meleeCoordString;
}
