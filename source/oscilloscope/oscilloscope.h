//
// Created on 2025/03/18.
//

#ifndef GTS_OSCILLOSCOPE_H
#define GTS_OSCILLOSCOPE_H

#include <gccore.h>
#include "../waveform.h"

static const float FRAME_TIME_MS = (1000/60.0);

enum OSC_MENU_STATE { OSC_SETUP, OSC_POST_SETUP, OSC_INSTRUCTIONS };
enum OSC_STATE { PRE_INPUT, POST_INPUT, POST_INPUT_LOCK };

static const u8 OSCILLOSCOPE_TEST_LEN = 4;
enum OSCILLOSCOPE_TEST { SNAPBACK, PIVOT, DASHBACK, NO_TEST };

void menu_oscilloscope(void *currXfb, WaveformData *d, u32 *p, u32 *h);
void menu_oscilloscopeEnd();

#endif //GTS_OSCILLOSCOPE_H
