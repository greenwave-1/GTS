//
// Created on 10/30/23.
//

#ifndef FOSSSCOPE_WAVEFORM_H
#define FOSSSCOPE_WAVEFORM_H

#include <gccore.h>

#define WAVEFORM_SAMPLES 3000
// individual datapoint from polling
typedef struct WaveformDatapoint {
	// analog stick
	int ax;
	int ay;
	// c stick
	int cx;
	int cy;
} WaveformDatapoint;

typedef struct WaveformData {
	// sampling a 1ms, roughly 5 seconds
	WaveformDatapoint data[WAVEFORM_SAMPLES];
	unsigned int endPoint;

	// stat values
	// TODO: estimate poll rate?
	u16 pollingRate;
	
	bool isDataReady;
	
	// set to true for logic to ignore stick movement stuff and just fill the array
	bool fullMeasure;

} WaveformData;

void measureWaveform(WaveformData *data);

// converts raw input values to melee coordinates
WaveformDatapoint convertStickValues(WaveformDatapoint *data);

// drawing functions from phobconfigtool
void DrawHLine (int x1, int x2, int y, int color, void *xfb);
void DrawVLine (int x, int y1, int y2, int color, void *xfb);
void DrawBox (int x1, int y1, int x2, int y2, int color, void *xfb);

// expanded drawing functions
void DrawFilledBox (int x1, int y1, int x2, int y2, int color, void *xfb);
void DrawLine (int x1, int y1, int x2, int y2, int color, void *xfb);

// generated from a python script, these are x,y magnitude, but x2
static const int STICK_BOUNDS[] = {
		160 , 0 ,
		158 , 0 ,
		158 , 2 ,
		158 , 4 ,
		158 , 6 ,
		158 , 8 ,
		158 , 10 ,
		158 , 12 ,
		158 , 14 ,
		158 , 16 ,
		158 , 18 ,
		158 , 20 ,
		158 , 22 ,
		158 , 24 ,
		156 , 26 ,
		156 , 28 ,
		156 , 30 ,
		156 , 32 ,
		156 , 34 ,
		154 , 34 ,
		154 , 36 ,
		154 , 38 ,
		154 , 40 ,
		154 , 42 ,
		152 , 42 ,
		152 , 44 ,
		152 , 46 ,
		152 , 48 ,
		150 , 50 ,
		150 , 52 ,
		150 , 54 ,
		148 , 56 ,
		148 , 58 ,
		148 , 60 ,
		146 , 60 ,
		146 , 62 ,
		146 , 64 ,
		144 , 64 ,
		144 , 66 ,
		144 , 68 ,
		142 , 70 ,
		142 , 72 ,
		140 , 72 ,
		140 , 74 ,
		140 , 76 ,
		138 , 78 ,
		138 , 80 ,
		136 , 80 ,
		136 , 82 ,
		134 , 84 ,
		134 , 86 ,
		132 , 86 ,
		132 , 88 ,
		130 , 90 ,
		130 , 92 ,
		128 , 92 ,
		128 , 94 ,
		126 , 96 ,
		126 , 98 ,
		124 , 98 ,
		124 , 100 ,
		122 , 100 ,
		122 , 102 ,
		120 , 102 ,
		120 , 104 ,
		118 , 106 ,
		116 , 108 ,
		114 , 110 ,
		114 , 112 ,
		112 , 112
};

#endif //FOSSSCOPE_WAVEFORM_H
