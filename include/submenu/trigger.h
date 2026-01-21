//
// Created on 5/16/25.
//

// Trigger oscilloscope submenu
// graphs analog trigger values over time

#ifndef GTS_TRIGGER_H
#define GTS_TRIGGER_H

#include <stdint.h>

enum TRIG_MENU_STATE { TRIG_SETUP, TRIG_POST_SETUP, TRIG_INSTRUCTIONS };
enum TRIG_STATE { TRIG_INPUT, TRIG_DISPLAY, TRIG_DISPLAY_LOCK };

void menu_triggerOscilloscope();
void menu_triggerOscilloscopeEnd();

#endif //GTS_TRIGGER_H
