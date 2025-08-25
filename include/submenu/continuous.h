//
// Created on 2025/03/17.
//

// Continuous Oscilloscope submenu
// continuously displays stick and c-stick coordinates over time

#ifndef GTS_CONTINUOUS_H
#define GTS_CONTINUOUS_H

#include <stdint.h>

enum CONT_MENU_STATE { CONT_SETUP, CONT_POST_SETUP };
enum CONT_STATE { INPUT, INPUT_LOCK };

void menu_continuousWaveform(void *currXfb, uint16_t *p, uint16_t *h);
void menu_continuousEnd();



#endif //GTS_CONTINUOUS_H
