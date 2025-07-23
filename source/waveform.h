//
// Created on 2023/10/30.
//

// defines structs for holding controller data
// TODO: this needs an overhaul, this hasn't been changed significantly since the polling change

#ifndef GTS_WAVEFORM_H
#define GTS_WAVEFORM_H

#include <gccore.h>

#define TRIGGER_SAMPLES 500
typedef struct TriggerDatapoint {
	u8 triggerLAnalog;
	u8 triggerRAnalog;
	bool triggerLDigital;
	bool triggerRDigital;
	u64 timeDiffUs;
} TriggerDatapoint;

typedef struct TriggerData {
	TriggerDatapoint data[TRIGGER_SAMPLES];
	unsigned int startPoint;
	unsigned int curr;
	unsigned int endPoint;
	
	u64 totalTimeUs;
	
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
	u32 buttonsHeld;
	// triggers
	TriggerDatapoint triggers;
	// time from last datapoint
	u64 timeDiffUs;
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
	u64 totalTimeUs;
	
	bool isDataReady;
	
	// set to true for logic to ignore stick movement stuff and just fill the array
	bool fullMeasure;
	
	bool exported;

} WaveformData;

//enum CONTROLLER_STICKS_XY { A_STICK_X, A_STICK_Y, C_STICK_X, C_STICK_Y };

// converts raw input values to melee coordinates
WaveformDatapoint convertStickValues(WaveformDatapoint *data);

//char* meleeCoord(WaveformDatapoint data, enum CONTROLLER_STICKS_XY axis);
//char* meleeCoord(int coord);


#endif //GTS_WAVEFORM_H
