//
// Created on 2025/05/09.
//

// 2d Plot submenu
// plots stick coordinates on a 2d "graph"

#ifndef GTS_PLOT2D_H
#define GTS_PLOT2D_H

#include <stdint.h>

#include "waveform.h"

enum PLOT_2D_MENU_STATE { PLOT_SETUP, PLOT_POST_SETUP, PLOT_INSTRUCTIONS };
enum PLOT_2D_STATE { PLOT_DISPLAY, PLOT_INPUT };

void menu_plot2d();
void menu_plot2dEnd();

#endif //GTS_PLOT2D_H
