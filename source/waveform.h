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

	bool isDataReady;

} WaveformData;

void measureWaveform(WaveformData *data);

// drawing functions from phobconfigtool
void DrawHLine (int x1, int x2, int y, int color, void *xfb);
void DrawVLine (int x, int y1, int y2, int color, void *xfb);
void DrawBox (int x1, int y1, int x2, int y2, int color, void *xfb);

#endif //FOSSSCOPE_WAVEFORM_H
