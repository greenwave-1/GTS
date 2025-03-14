//
// Created on 10/25/23.
//

#ifndef FOSSSCOPE_MENU_H
#define FOSSSCOPE_MENU_H

#include <stdbool.h>

// enum for keeping track of the currently displayed menu
enum CURRENT_MENU { MAIN_MENU, CONTROLLER_TEST, WAVEFORM, PLOT_2D, IMAGE_TEST, FILE_EXPORT, WAITING_MEASURE, COORD_MAP, ERR };

// functions for drawing the individual menus
bool menu_runMenu(void *currXfb);
void menu_mainMenu(void *currXfb);
void menu_controllerTest(void *currXfb);
void menu_waveformMeasure(void *currXfb);
void menu_2dPlot(void *currXfb);
void menu_fileExport(void *currXfb);
void menu_waitingMeasure(void *currXfb);
void menu_coordinateViewer(void *currXfb);


#endif //FOSSSCOPE_MENU_H
