//
// Created on 2023/10/25.
//

// Handles the main menu and other basic menus that I haven't moved yet

#ifndef GTS_MENU_H
#define GTS_MENU_H

#include <stdbool.h>

// enum for keeping track of the currently displayed menu
enum CURRENT_MENU { MAIN_MENU, CONTROLLER_TEST, WAVEFORM, PLOT_2D,
		PLOT_BUTTON, IMAGE_TEST, FILE_EXPORT, COORD_MAP,
		CONTINUOUS_WAVEFORM, TRIGGER_WAVEFORM, GATE_MEASURE, 
		THANKS_PAGE, ERR };

// functions for drawing the individual menus
bool menu_runMenu(void *currXfb);
void menu_mainMenu(void *currXfb);
void menu_controllerTest(void *currXfb);
void menu_fileExport(void *currXfb);
void menu_coordinateViewer(void *currXfb);
void menu_thanksPage(void *currXfb);


#endif //GTS_MENU_H
