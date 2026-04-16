//
// Created on 2025/05/09.
//

// 2d Plot submenu
// plots stick coordinates on a 2d "graph"

#ifndef GTS_PLOT2D_H
#define GTS_PLOT2D_H

#include <stdint.h>

#include "waveform.h"

enum PLOT_2D_STICKMAP_TYPE { NO_STICKMAP, BUILTIN_STICKMAP, EXTERNAL_STICKMAP };

enum PLOT_2D_MENU_STATE { PLOT_SETUP, PLOT_POST_SETUP, PLOT_INSTRUCTIONS, PLOT_FILE_PICKER };
enum PLOT_2D_STATE { PLOT_DISPLAY, PLOT_INPUT };

void menu_plot2d();
void menu_plot2dEnd();

void menu_plot2dSetAutoTrigger(bool captureState);

#endif //GTS_PLOT2D_H
