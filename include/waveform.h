//
// Created on 2023/10/30.
//

// defines structs for holding controller data

#ifndef GTS_WAVEFORM_H
#define GTS_WAVEFORM_H

#include <stdint.h>

// TODO: this file should be renamed, along with "ControllerRec". I can't think of something better right now...

// individual datapoint from a given controller poll
// TODO: split the data for the sticks
//  (make a generic "Stick recording" struct and store two of them per ControllerSample
typedef struct ControllerSample {
	// all analog values
	// analog stick
	int8_t stickX;
	int8_t stickY;
	
	// c-stick
	int8_t cStickX;
	int8_t cStickY;
	
	// analog triggers
	uint8_t triggerL;
	uint8_t triggerR;
	
	// all digital buttons
	uint16_t buttons;
	
	// time in microseconds from last sample
	// this will be zero for the first sample
	uint64_t timeDiffUs;
	
} ControllerSample;

// TODO: same todo as above
typedef struct MeleeCoordinates {
	int8_t stickX;
	int8_t stickY;

	int8_t cStickX;
	int8_t cStickY;

	double stickMagnitude;
	double cStickMagnitude;

	double stickAngle;
	double cStickAngle;

} MeleeCoordinates;

// allocated/max size of the data array
#define REC_SAMPLE_MAX 3000

// the type of recording created
enum RECORDING_TYPE {
	REC_CLEAR, REC_OSCILLOSCOPE, REC_OSCILLOSCOPE_CONTINUOUS,
	REC_TRIGGER_L, REC_TRIGGER_R,
	REC_2DPLOT, REC_BUTTONTIME
};

#define REC_CLEAR_FLAG 0
#define REC_OSCILLOSCOPE_FLAG 1
#define REC_TRIGGER_L_FLAG 2
#define REC_TRIGGER_R_FLAG 4
#define REC_2DPLOT_FLAG 8
#define REC_BUTTONTIME_FLAG 16

// array that contains what menus are valid for a given recording
extern const uint8_t RECORDING_TYPE_VALID_MENUS[7];

// recording structure
// holds all datapoints (ControllerSample) captured, along with what type of recording (RECORDING_TYPE) and total datapoints
typedef struct ControllerRec {
	// array of datapoints
	ControllerSample samples[REC_SAMPLE_MAX];
	
	// the total number of samples (IE: the last capture index)
	int sampleEnd;
	
	// total time in microseconds that a given recording lasts
	uint64_t totalTimeUs;
	
	// recording type
	enum RECORDING_TYPE recordingType;
	
	// flag for if contents are considered "ready"
	bool isRecordingReady;
	
	// flag for exporting
	bool dataExported;
	
} ControllerRec;

// gives waveform.c a pointer to continuous's ControllerRec
// used for free() in freeControllerRecStructs()
// same as above
void setContinuousRecStructPtr(ControllerRec* ptr);

// calls malloc and inits structs
void initControllerRecStructs();

// free memory
void freeControllerRecStructs();

// returns address of static pointers in waveform.c
// using double pointer so that the two structs can be swapped silently via flipData()
ControllerRec** getRecordingData();
ControllerRec** getTempData();

// single pointer since we don't do any swapping weirdness
ControllerRec* getContinuousData();

// resets given data
void clearRecordingArray(ControllerRec *recording);

// flips pointers in static memory
// allows menus to get a double pointer and not have to change them in each menu
void flipData();

enum CONTROLLER_STICK_AXIS {
	AXIS_AX = 0x01, AXIS_AY = 0x02,
	AXIS_CX = 0x04, AXIS_CY = 0x08,
	AXIS_AXY = (0x10 | AXIS_AX | AXIS_AY),
	AXIS_CXY = (0x20 | AXIS_CX | AXIS_CY)
};

int8_t getControllerSampleAxisValue(ControllerSample sample, enum CONTROLLER_STICK_AXIS axis);
int8_t getControllerSampleXValue(ControllerSample sample, enum CONTROLLER_STICK_AXIS axis);
int8_t getControllerSampleYValue(ControllerSample sample, enum CONTROLLER_STICK_AXIS axis);
void getControllerSampleAxisPair(ControllerSample sample, enum CONTROLLER_STICK_AXIS axis, int8_t* retX, int8_t* retY);
MeleeCoordinates convertStickRawToMelee(ControllerSample sample);


char* getMeleeCoordinateString(MeleeCoordinates coords, enum CONTROLLER_STICK_AXIS axis);

#endif //GTS_WAVEFORM_H
