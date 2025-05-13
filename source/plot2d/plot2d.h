//
// Created on 2025/05/09.
//

#ifndef GTS_PLOT2D_H
#define GTS_PLOT2D_H

#include "../waveform.h"

enum PLOT_MENU_STATE { PLOT_SETUP, PLOT_POST_SETUP, PLOT_INSTRUCTIONS };
enum PLOT_STATE { PLOT_DISPLAY, PLOT_INPUT };

void menu_plot2d(void *currXfb, WaveformData *d, u32 *p, u32 *h);
void menu_plot2dEnd();

#endif //GTS_PLOT2D_H
