//
// Created on 5/16/25.
//

#ifndef GTS_TRIGGER_H
#define GTS_TRIGGER_H

#include <gctypes.h>

enum TRIG_MENU_STATE { TRIG_SETUP, TRIG_POST_SETUP, TRIG_INSTRUCTIONS };
enum TRIG_STATE { TRIG_INPUT, TRIG_DISPLAY };

void menu_triggerOscilloscope(void *currXfb, u32 *p, u32 *h);
void menu_triggerOscilloscopeEnd();

#endif //GTS_TRIGGER_H
