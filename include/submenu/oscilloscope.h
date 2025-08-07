//
// Created on 2025/03/18.
//

// Stick Oscilloscope submenu
// graphs stick and c-stick values over time, with specific test presents

#ifndef GTS_OSCILLOSCOPE_H
#define GTS_OSCILLOSCOPE_H

#include <gccore.h>
#include "waveform.h"

enum OSC_MENU_STATE { OSC_SETUP, OSC_POST_SETUP, OSC_INSTRUCTIONS };
enum OSC_STATE { PRE_INPUT, POST_INPUT, POST_INPUT_LOCK };

static const u8 OSCILLOSCOPE_TEST_LEN = 4;
// TODO: remove NO_TEST, since the Continuous Oscilloscope menu exists
enum OSCILLOSCOPE_TEST { SNAPBACK, PIVOT, DASHBACK, NO_TEST };

void menu_oscilloscope(void *currXfb, WaveformData *d, u32 *p, u32 *h);
void menu_oscilloscopeEnd();

#endif //GTS_OSCILLOSCOPE_H
