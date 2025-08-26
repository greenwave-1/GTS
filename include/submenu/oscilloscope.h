//
// Created on 2025/03/18.
//

// Stick Oscilloscope submenu
// graphs stick and c-stick values over time, with specific test presents

#ifndef GTS_OSCILLOSCOPE_H
#define GTS_OSCILLOSCOPE_H

#include <stdint.h>

#include "waveform.h"

enum OSC_MENU_STATE { OSC_SETUP, OSC_POST_SETUP, OSC_INSTRUCTIONS };
enum OSC_STATE { PRE_INPUT, POST_INPUT, POST_INPUT_LOCK };

static const uint8_t OSCILLOSCOPE_TEST_LEN = 3;
// TODO: remove NO_TEST, since the Continuous Oscilloscope menu exists
enum OSCILLOSCOPE_TEST { SNAPBACK, PIVOT, DASHBACK };

void menu_oscilloscope(void *currXfb);
void menu_oscilloscopeEnd();

#endif //GTS_OSCILLOSCOPE_H
