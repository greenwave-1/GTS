//
// Created on 3/18/25.
//

#ifndef FOSSSCOPE_R2_OSCILLOSCOPE_H
#define FOSSSCOPE_R2_OSCILLOSCOPE_H

#include <gccore.h>

static const float FRAME_TIME_MS = (1000/60.0);

enum MENU_STATE { OSC_SETUP, OSC_POST_SETUP, OSC_INSTRUCTIONS };
enum OSCILLOSCOPE_STATE { PRE_INPUT, POST_INPUT, POST_INPUT_LOCK };

static const u8 OSCILLOSCOPE_TEST_LEN = 4;
enum OSCILLOSCOPE_TEST { SNAPBACK, PIVOT, DASHBACK, NO_TEST };

void menu_oscilloscope(void *currXfb, u32 *p, u32 *h);
void menu_oscilloscopeEnd();
sampling_callback getOscilloscopeCallbackRef();

#endif //FOSSSCOPE_R2_OSCILLOSCOPE_H
