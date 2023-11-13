//
// Created on 10/25/23.
//

#ifndef FOSSSCOPE_MENU_H
#define FOSSSCOPE_MENU_H

#include <stdbool.h>

// enum for keeping track of the currently displayed menu
enum CURRENT_MENU { MAIN_MENU, CONTROLLER_TEST, WAVEFORM, PLACEHOLDER };

// functions for drawing the individual menus
bool menu_runMenu(void *currXfb);
void menu_mainMenu();
void menu_controllerTest();
void menu_waveformMeasure(void *currXfb);
void menu_2dPlot(void *currXfb);

#endif //FOSSSCOPE_MENU_H
