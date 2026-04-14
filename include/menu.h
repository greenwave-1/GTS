//
// Created on 2023/10/25.
//

// Handles the main menu and other basic menus that I haven't moved yet

#ifndef GTS_MENU_H
#define GTS_MENU_H

// enum for keeping track of the currently displayed menu
enum CURRENT_MENU { MAIN_MENU, CONTROLLER_TEST, WAVEFORM, PLOT_2D,
		PLOT_BUTTON, IMAGE_TEST, FILE_EXPORT, COORD_MAP,
		CONTINUOUS_WAVEFORM, TRIGGER_WAVEFORM, GATE_MEASURE, 
		THANKS_PAGE, BLACKOUT, ERR };

// enum for the individual menu entries on the main menu
// used for iterating over stuff/readability
enum MENU_MAIN_ENTRY_LIST { ENTRY_CONT_TEST, ENTRY_OSCILLOSCOPE, ENTRY_CONT_OSCILLOSCOPE,
		ENTRY_TRIGGER_OSCILLOSCOPE, ENTRY_COORD_VIEWER, ENTRY_2D_PLOT,
		ENTRY_BUTTON_PLOT, ENTRY_GATE_VIS, ENTRY_DATA_EXPORT };

// any cleanup jobs that need to be done...
void menu_deInit();

// "main" function
bool menu_runMenu();

// functions for drawing the individual menus
void menu_mainMenu();
void menu_fileExport();
void menu_thanksPage();

void menu_setCurrentMenu(enum CURRENT_MENU menu);

// mainly used for errordisplay
enum CURRENT_MENU menu_getCurrentMenu();

char* menu_getMenuString(enum CURRENT_MENU menu);

void menu_drawHeader();

#endif //GTS_MENU_H
