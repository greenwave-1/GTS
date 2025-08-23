//
// Created on 2023/10/30.
//

// defines structs for holding controller data
// TODO: this needs an overhaul, this hasn't been changed significantly since the polling change

#ifndef GTS_WAVEFORM_H
#define GTS_WAVEFORM_H

#include <stdint.h>

// individual datapoint from a given controller poll
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
	uint32_t buttons;
	
	// time in microseconds from last sample
	// this will be zero for the first sample
	uint64_t timeDiffUs;
	
} ControllerSample;

// TODO: would a bitfield be better here?
typedef struct MeleeCoordinates {
	// valid values for melee units are 0 -> 10000 in multiples of 125
	uint16_t stickXUnit;
	uint16_t stickYUnit;
	uint16_t cStickXUnit;
	uint16_t cStickYUnit;
	
	// since we're storing the values as unsigned, we need these to know direction
	bool stickXNegative;
	bool stickYNegative;
	bool cStickXNegative;
	bool cStickYNegative;
	
} MeleeCoordinates;

// allocated/max size of the data array
#define REC_SAMPLE_MAX 3000

// the type of recording created
enum RECORDING_TYPE { REC_CLEAR, REC_OSCILLOSCOPE, REC_TRIGGER, REC_2DPLOT, REC_BUTTONTIME };

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

// calls malloc and inits structs
void initData();

// returns address of static pointers in waveform.c
// using double pointer so that the two structs can be swapped silently via flipData()
ControllerRec** getRecordingData();
ControllerRec** getTempData();

// resets given data
void clearRecordingArray(ControllerRec *recording);

// flips pointers in static memory
// allows menus to get a double pointer and not have to change them in each menu
void flipData();

MeleeCoordinates convertStickRawToMelee(ControllerSample sample);

#define TRIGGER_SAMPLES 500
typedef struct TriggerDatapoint {
	uint8_t triggerLAnalog;
	uint8_t triggerRAnalog;
	bool triggerLDigital;
	bool triggerRDigital;
	uint64_t timeDiffUs;
} TriggerDatapoint;

typedef struct TriggerData {
	TriggerDatapoint data[TRIGGER_SAMPLES];
	unsigned int startPoint;
	unsigned int curr;
	unsigned int endPoint;
	
	uint64_t totalTimeUs;
	
	bool isDataReady;
} TriggerData;

#define WAVEFORM_SAMPLES 3000
// individual datapoint from polling
typedef struct WaveformDatapoint {
	// analog stick
	int ax;
	int ay;
	// c stick
	int cx;
	int cy;
	// buttons held
	uint32_t buttonsHeld;
	// triggers
	TriggerDatapoint triggers;
	// time from last datapoint
	uint64_t timeDiffUs;
	// for converted values
	bool isAXNegative;
	bool isAYNegative;
	bool isCXNegative;
	bool isCYNegative;
} WaveformDatapoint;

typedef struct WaveformData {
	WaveformDatapoint data[WAVEFORM_SAMPLES];
	unsigned int endPoint;

	// total time the read took
	uint64_t totalTimeUs;
	
	bool isDataReady;
	
	// set to true for logic to ignore stick movement stuff and just fill the array
	bool fullMeasure;
	
	bool exported;

} WaveformData;


#endif //GTS_WAVEFORM_H
