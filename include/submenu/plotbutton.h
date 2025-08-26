//
// Created on 7/22/25.
//

// Button Timing Viewer submenu
// plots buttons and binary stick states over time

#ifndef GTS_PLOTBUTTON_H
#define GTS_PLOTBUTTON_H

#include <stdint.h>

#include "waveform.h"

enum PLOT_BUTTON_MENU_STATE { BUTTON_SETUP, BUTTON_POST_SETUP, BUTTON_INSTRUCTIONS };
enum PLOT_BUTTON_STATE { BUTTON_DISPLAY, BUTTON_INPUT };

void menu_plotButton(void *currXfb);
void menu_plotButtonEnd();
bool menu_plotButtonHasCaptureStarted();

#endif //GTS_PLOTBUTTON_H
