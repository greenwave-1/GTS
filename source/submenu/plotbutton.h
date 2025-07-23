//
// Created on 7/22/25.
//

#ifndef GTS_PLOTBUTTON_H
#define GTS_PLOTBUTTON_H

#include "../waveform.h"

enum PLOT_BUTTON_MENU_STATE { BUTTON_SETUP, BUTTON_POST_SETUP, BUTTON_INSTRUCTIONS };
enum PLOT_BUTTON_STATE { BUTTON_DISPLAY, BUTTON_INPUT };
enum PLOT_BUTTON_LIST { A, B, X, Y, L, La, R, Ra, Z, AX, AY, CX, CY, NO_BUTTON };

void menu_plotButton(void *currXfb, u32 *p, u32 *h);
void menu_plotButtonEnd();
bool menu_plotButtonHasCaptureStarted();

#endif //GTS_PLOTBUTTON_H
