//
// Created on 2025/03/17.
//

// Continuous Oscilloscope submenu
// continuously displays stick and c-stick coordinates over time

#ifndef GTS_CONTINUOUS_H
#define GTS_CONTINUOUS_H

#include <gctypes.h>

enum CONT_MENU_STATE { CONT_SETUP, CONT_POST_SETUP };
enum CONT_STATE { INPUT, INPUT_LOCK };

void menu_continuousWaveform(void *currXfb, u32 *p, u32 *h);
void menu_continuousEnd();



#endif //GTS_CONTINUOUS_H
