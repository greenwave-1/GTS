//
// Created on 10/25/23.
//

#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ogc/color.h>
#include <ogc/lwp_watchdog.h>
#include "waveform.h"
#include "images/stickmaps.h"
#include "draw.h"
#include "print.h"
#include "file.h"
#include "stickmap_coordinates.h"

#include "oscilloscope/oscilloscope.h"
#include "oscilloscope/continuous.h"

#ifndef VERSION_NUMBER
#define VERSION_NUMBER "NOVERS_DEV"
#endif

#define MENUITEMS_LEN 6
#define TEST_LEN 5

// 500 values displayed at once, SCREEN_POS_CENTER_X +/- 250
#define SCREEN_TIMEPLOT_START 70

// macro for how far the stick has to go before it counts as a movement
#define MENU_STICK_THRESHOLD 10

const float frameTime = (1000.0 / 60.0);

// enum to keep track of what menu to display, and what logic to run
static enum CURRENT_MENU currentMenu = MAIN_MENU;

// enum for previous menu, we move to it when we're waiting for user input
static enum CURRENT_MENU previousMenu = MAIN_MENU;

// enum for what image to draw in 2d plot
static enum IMAGE selectedImage = SNAPBACK;
static int map2dStartIndex = 0;

static enum STICKMAP_LIST selectedStickmap = NONE;
// will be casted to whichever stickmap is selected
static int selectedStickmapSub = 0;

// main menu counter
static u8 mainMenuSelection = 0;

// counter for how many frames b or start have been held
static u8 bHeldCounter = 0;

// data for drawing a waveform
static WaveformData data = { {{ 0 }}, 0, 500, false, false };

// vars for what buttons are pressed or held
static u32 pressed = 0;
static u32 held = 0;

// var for counting how long the stick has been held away from neutral
static u8 stickheld = 0;

// menu item strings
//static const char* menuItems[MENUITEMS_LEN] = { "Controller Test", "Stick Oscilloscope", "Coordinate Viewer", "2D Plot", "Export Data", "Continuous Waveform" };
static const char* menuItems[MENUITEMS_LEN] = { "Controller Test", "Stick Oscilloscope", "Continuous Oscilloscope",
                                                "Coordinate Viewer", "2D Plot", "Export Data"};


static bool displayedWaitingInputMessage = false;

static bool displayInstructions = false;
//static bool fileIOSuccess = false;

static int lastDrawPoint = -1;
static int dataScrollOffset = 0;

static u32 padsConnected = 0;

static PADStatus origin[PAD_CHANMAX];
static bool originRead = false;

// buffer for strings with numbers and stuff
static char strBuffer[100];

static bool setDrawInterlaceMode = false;

// the "main" for the menus
// other menu functions are called from here
// this also handles moving between menus and exiting
bool menu_runMenu(void *currXfb) {
	if (!setDrawInterlaceMode) {
		if (VIDEO_GetScanMode() == VI_INTERLACE) {
			setInterlaced(true);
		}
		setDrawInterlaceMode = true;
	}
	memset(strBuffer, '\0', sizeof(strBuffer));
	resetCursor();
	// read inputs
	padsConnected = PAD_ScanPads();
	
	// read origin if a controller is connected and we don't have the origin
	if (!originRead && (padsConnected & 1) == 1 ) {
		PAD_GetOrigin(origin);
		originRead = true;
	}
	
	printStr("FossScope (Working Title)", currXfb);
	if ((padsConnected & 1) == 0) {
		setCursorPos(0, 38);
		printStr("Controller Disconnected!", currXfb);
		data.isDataReady = false;
		originRead = false;
	}
	if (data.isDataReady) {
		setCursorPos(0, 31);
		printStr("Oscilloscope Capture in memory!", currXfb);
	}
	setCursorPos(2, 0);
	
	// check for any buttons pressed/held
	// don't update if we are on a menu with its own callback
	if (currentMenu != WAVEFORM && currentMenu != CONTINUOUS_WAVEFORM) {
		pressed = PAD_ButtonsDown(0);
		held = PAD_ButtonsHeld(0);
	}

	// determine what menu we are in
	switch (currentMenu) {
		case MAIN_MENU:
			menu_mainMenu(currXfb);
			break;
		case CONTROLLER_TEST:
			menu_controllerTest(currXfb);
			break;
		case WAVEFORM:
			menu_oscilloscope(currXfb, &data, &pressed, &held);
			break;
		case PLOT_2D:
			if (displayInstructions) {
				printStr("Press X to cycle the stickmap background. Use DPAD\nleft/right to change "
					   "what the last point drawn is.\nInformation on the last chosen point is "
					   "displayed\nat the bottom. Hold R to add or remove points faster.\n"
					   "Hold L to move one point at a time.\n\nHold Y to move the \"starting sample\" with the\n"
					   "same controls as above. Information for the selected\nrange is shown on the left.", currXfb);
			} else {
				menu_2dPlot(currXfb);
			}
			break;
		case FILE_EXPORT:
			menu_fileExport(currXfb);
			break;
		case WAITING_MEASURE:
			menu_waitingMeasure(currXfb);
			break;
		case COORD_MAP:
			if (displayInstructions) {
				printStr("Press X to cycle the stickmap being tested, and Y to cycle\nwhich "
					   "category of points.\nMelee Coordinates are shown in thetop-left.\n\n"
					   "The white line represents the analog stick.\n"
					   "The yellow line represents the c-stick.\n\n"
					   "Current Stickmap: ", currXfb);
				switch (selectedStickmap) {
					case FF_WD:
						printStr("Firefox / Wavedash\n", currXfb);
						printStr(STICKMAP_FF_WD_DESC, currXfb);
						break;
					case SHIELDDROP:
						printStr("Shield Drop\n", currXfb);
						printStr(STICKMAP_SHIELDDROP_DESC, currXfb);
						break;
					case NONE:
					default:
						printStr("None\n", currXfb);
						break;
				}
			} else {
				menu_coordinateViewer(currXfb);
			}
			break;
		case CONTINUOUS_WAVEFORM:
			menu_continuousWaveform(currXfb, &pressed, &held);
			break;
		default:
			printStr("HOW DID WE END UP HERE?\n", currXfb);
			break;
	}
	if (displayInstructions) {
		setCursorPos(21, 0);
		printStr("Press Z to close instructions.", currXfb);
	}

	// move cursor to bottom left
	setCursorPos(22, 0);

	// exit the program if start is pressed
	if (pressed & PAD_BUTTON_START && currentMenu == MAIN_MENU) {
		printStr("Exiting...", currXfb);
		return true;
	}

	// does the user want to move back to the main menu?
	else if (held & PAD_BUTTON_B && currentMenu != MAIN_MENU) {
		bHeldCounter++;

		// give user feedback that they are holding the button
		printStr("Moving back to main menu", currXfb);

		// TODO: I know there's a better way to do this but I can't think of it right now...
		if (bHeldCounter > 15) {
			printStr(".", currXfb);
		}
		if (bHeldCounter > 30) {
			printStr(".", currXfb);
		}
		if (bHeldCounter > 45) {
			printStr(".", currXfb);
		}

		// has the button been held long enough?
		if (bHeldCounter > 46) {
			// special exit stuff that needs to happen for certain menus
			switch (currentMenu) {
				case WAVEFORM:
					menu_oscilloscopeEnd();
					break;
				case CONTINUOUS_WAVEFORM:
					menu_continuousEnd();
					break;
				default:
					break;
			}
			currentMenu = MAIN_MENU;
			displayInstructions = false;
			bHeldCounter = 0;
			// stop rumble if it didn't get stopped before
			PAD_ControlMotor(0, PAD_MOTOR_STOP);
		}

	} else {
		// does the user want to display instructions?
		if (pressed & PAD_TRIGGER_Z) {
			if (currentMenu == PLOT_2D || currentMenu == COORD_MAP) {
				displayInstructions = !displayInstructions;
			}
		}
		if (currentMenu != MAIN_MENU) {
			printStr("Hold B to return to main menu.", currXfb);
		} else {
			printStr("Press Start to exit.", currXfb);
			int col = 55 - (sizeof(VERSION_NUMBER));
			if (col > 25) {
				setCursorPos(22, col);
				printStr("Ver: ", currXfb);
				printStr(VERSION_NUMBER, currXfb);
			}
		}
		bHeldCounter = 0;
	}

	return false;
}

void menu_mainMenu(void *currXfb) {
	int stickY = PAD_StickY(0);

	// flags which tell whether the stick is held in an up or down position
	u8 up = stickY > MENU_STICK_THRESHOLD;
	u8 down = stickY < -MENU_STICK_THRESHOLD;

	// only move the stick if it wasn't already held for the last 10 ticks
	u8 movable = stickheld % 10 == 0;
	
	// iterate over the menu items array as defined in menu.c
	for (int i = 0; i < MENUITEMS_LEN; i++) {
		setCursorPos(2 + i, 0);
		// is the item we're about to print the currently selected menu?
		if (mainMenuSelection == i) {
			printStr("> ", currXfb);
		} else {
			setCursorPos(2 + i, 2);
		}
		printStr(menuItems[i], currXfb);

	}

	// does the user move the cursor?
	if (pressed & PAD_BUTTON_UP || (up && movable)) {
		if (mainMenuSelection > 0) {
			mainMenuSelection--;
		} else {
			mainMenuSelection = MENUITEMS_LEN - 1;
		}
	} else if (pressed & PAD_BUTTON_DOWN || (down && movable)) {
		if (mainMenuSelection < MENUITEMS_LEN - 1) {
			mainMenuSelection++;
		} else {
			mainMenuSelection = 0;
		}
	}

	// does the user want to move into another menu?
	// else if to ensure that the A press is separate from any dpad stuff
	else if (pressed & PAD_BUTTON_A) {
		switch (mainMenuSelection) {
			case 0:
				currentMenu = CONTROLLER_TEST;
				break;
			case 1:
				currentMenu = WAVEFORM;
				break;
			case 2:
				currentMenu = CONTINUOUS_WAVEFORM;
				break;
			case 3:
				currentMenu = COORD_MAP;
				break;
			case 4:
				currentMenu = PLOT_2D;
				break;
			case 5:
				currentMenu = FILE_EXPORT;
				break;
		}
	}

	// increase or reset counter for how long stick has been held
	if (up || down) {
		stickheld++;
	} else {
		stickheld = 0;
	}
}

void menu_controllerTest(void *currXfb) {
	// melee stick coordinates stuff
	// a lot of this comes from github.com/phobgcc/phobconfigtool

	static WaveformDatapoint stickCoordinatesRaw;
	static WaveformDatapoint stickCoordinatesMelee;

	// get raw stick values
	stickCoordinatesRaw.ax = PAD_StickX(0), stickCoordinatesRaw.ay = PAD_StickY(0);
	stickCoordinatesRaw.cx = PAD_SubStickX(0), stickCoordinatesRaw.cy = PAD_SubStickY(0);

	// get converted stick values
	stickCoordinatesMelee = convertStickValues(&stickCoordinatesRaw);
	
	// print raw stick coordinates
	setCursorPos(19, 0);
	sprintf(strBuffer, "Raw XY: (%04d,%04d)", stickCoordinatesRaw.ax, stickCoordinatesRaw.ay);
	printStr(strBuffer, currXfb);
	setCursorPos(19, 38);
	sprintf(strBuffer, "C-Raw XY: (%04d,%04d)", stickCoordinatesRaw.cx, stickCoordinatesRaw.cy);
	printStr(strBuffer, currXfb);
	
	// print melee coordinates
	setCursorPos(20, 0);
	printStr("Melee: (", currXfb);
	// is the value negative?
	if (stickCoordinatesRaw.ax < 0) {
		printStr("-", currXfb);
	} else {
		printStr("0", currXfb);
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.ax == 10000) {
		printStr("1.0000", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d", stickCoordinatesMelee.ax);
		printStr(strBuffer, currXfb);
	}
	printStr(",", currXfb);
	
	// is the value negative?
	if (stickCoordinatesRaw.ay < 0) {
		printStr("-", currXfb);
	} else {
		printStr("0", currXfb);
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.ay == 10000) {
		printStr("1.0000", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d", stickCoordinatesMelee.ay);
		printStr(strBuffer, currXfb);
	}
	printStr(")", currXfb);
	
	setCursorPos(20, 33);
	sprintf(strBuffer, "C-Melee: (");
	printStr(strBuffer, currXfb);
	// is the value negative?
	if (stickCoordinatesRaw.cx < 0) {
		printStr("-", currXfb);
	} else {
		printStr("0", currXfb);
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.cx == 10000) {
		printStr("1.0000", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d", stickCoordinatesMelee.cx);
		printStr(strBuffer, currXfb);
	}
	printStr(",", currXfb);
	
	// is the value negative?
	if (stickCoordinatesRaw.cy < 0) {
		printStr("-", currXfb);
	} else {
		printStr("0", currXfb);
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.cy == 10000) {
		printStr("1.0000", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d", stickCoordinatesMelee.cy);
		printStr(strBuffer, currXfb);
	}
	printStr(")", currXfb);
	
	if (originRead) {
		setCursorPos(21, 0);
		sprintf(strBuffer, "Origin XY: (%04d,%04d)", origin[0].stickX, origin[0].stickY);
		printStr(strBuffer, currXfb);
		setCursorPos(21, 35);
		sprintf(strBuffer, "C-Origin XY: (%04d,%04d)", origin[0].substickX, origin[0].substickY);
		printStr(strBuffer, currXfb);
	}
	

	// visual stuff
	// Buttons

    // A
	if (held & PAD_BUTTON_A) {
		DrawFilledBox(CONT_TEST_BUTTON_A_X1, CONT_TEST_BUTTON_A_Y1,
					  CONT_TEST_BUTTON_A_X1 + CONT_TEST_BUTTON_A_SIZE, CONT_TEST_BUTTON_A_Y1 + CONT_TEST_BUTTON_A_SIZE,
					  COLOR_WHITE, currXfb);
		drawCharDirect(currXfb, CONT_TEST_BUTTON_A_X1 + 12, CONT_TEST_BUTTON_A_Y1 + 8, COLOR_BLACK, 'A');
	} else {
		DrawBox(CONT_TEST_BUTTON_A_X1, CONT_TEST_BUTTON_A_Y1,
		        CONT_TEST_BUTTON_A_X1 + CONT_TEST_BUTTON_A_SIZE, CONT_TEST_BUTTON_A_Y1 + CONT_TEST_BUTTON_A_SIZE,
		        COLOR_WHITE, currXfb);
		drawCharDirect(currXfb, CONT_TEST_BUTTON_A_X1 + 12, CONT_TEST_BUTTON_A_Y1 + 8, COLOR_WHITE, 'A');
    }
	
    // B
	if (held & PAD_BUTTON_B) {
		DrawFilledBox(CONT_TEST_BUTTON_B_X1, CONT_TEST_BUTTON_B_Y1,
		              CONT_TEST_BUTTON_B_X1 + CONT_TEST_BUTTON_B_SIZE, CONT_TEST_BUTTON_B_Y1 + CONT_TEST_BUTTON_B_SIZE,
		              COLOR_WHITE, currXfb);
		drawCharDirect(currXfb, CONT_TEST_BUTTON_B_X1 + 8, CONT_TEST_BUTTON_B_Y1 + 5, COLOR_BLACK, 'B');
	} else {
		DrawBox(CONT_TEST_BUTTON_B_X1, CONT_TEST_BUTTON_B_Y1,
		        CONT_TEST_BUTTON_B_X1 + CONT_TEST_BUTTON_B_SIZE, CONT_TEST_BUTTON_B_Y1 + CONT_TEST_BUTTON_B_SIZE,
		        COLOR_WHITE, currXfb);
		drawCharDirect(currXfb, CONT_TEST_BUTTON_B_X1 + 8, CONT_TEST_BUTTON_B_Y1 + 5, COLOR_WHITE, 'B');
	}

	// X
	if (held & PAD_BUTTON_X) {
		DrawFilledBox(CONT_TEST_BUTTON_Z_X1, CONT_TEST_BUTTON_X_Y1,
		              CONT_TEST_BUTTON_Z_X1 + CONT_TEST_BUTTON_XY_SHORT, CONT_TEST_BUTTON_X_Y1 + CONT_TEST_BUTTON_XY_LONG,
		              COLOR_WHITE, currXfb);
		drawCharDirect(currXfb, CONT_TEST_BUTTON_Z_X1 + 7, CONT_TEST_BUTTON_X_Y1 + 8, COLOR_BLACK, 'X');
	} else {
		DrawBox(CONT_TEST_BUTTON_Z_X1, CONT_TEST_BUTTON_X_Y1,
		        CONT_TEST_BUTTON_Z_X1 + CONT_TEST_BUTTON_XY_SHORT, CONT_TEST_BUTTON_X_Y1 + CONT_TEST_BUTTON_XY_LONG,
		        COLOR_WHITE, currXfb);
		drawCharDirect(currXfb, CONT_TEST_BUTTON_Z_X1 + 7, CONT_TEST_BUTTON_X_Y1 + 8, COLOR_WHITE, 'X');
	}

	// Y
	if (held & PAD_BUTTON_Y) {
		DrawFilledBox(CONT_TEST_BUTTON_A_X1, CONT_TEST_BUTTON_Y_Y1,
		              CONT_TEST_BUTTON_A_X1 + CONT_TEST_BUTTON_XY_LONG, CONT_TEST_BUTTON_Y_Y1 + CONT_TEST_BUTTON_XY_SHORT,
		              COLOR_WHITE, currXfb);
		drawCharDirect(currXfb, CONT_TEST_BUTTON_A_X1 + 12, CONT_TEST_BUTTON_Y_Y1 + 4, COLOR_BLACK, 'Y');
	} else {
		DrawBox(CONT_TEST_BUTTON_A_X1, CONT_TEST_BUTTON_Y_Y1,
		        CONT_TEST_BUTTON_A_X1 + CONT_TEST_BUTTON_XY_LONG, CONT_TEST_BUTTON_Y_Y1 + CONT_TEST_BUTTON_XY_SHORT,
		        COLOR_WHITE, currXfb);
		drawCharDirect(currXfb, CONT_TEST_BUTTON_A_X1 + 12, CONT_TEST_BUTTON_Y_Y1 + 4, COLOR_WHITE, 'Y');
	}

    // Z
	if (held & PAD_TRIGGER_Z) {
		DrawFilledBox(CONT_TEST_BUTTON_Z_X1, CONT_TEST_BUTTON_Z_Y1,
		              CONT_TEST_BUTTON_Z_X1 + CONT_TEST_BUTTON_XY_SHORT, CONT_TEST_BUTTON_Z_Y1 + CONT_TEST_BUTTON_XY_SHORT,
		              COLOR_WHITE, currXfb);
		drawCharDirect(currXfb, CONT_TEST_BUTTON_Z_X1 + 7, CONT_TEST_BUTTON_Z_Y1 + 4, COLOR_BLACK, 'Z');
		// also enable rumble if z is held
		PAD_ControlMotor(0, PAD_MOTOR_RUMBLE);
	} else {
		DrawBox(CONT_TEST_BUTTON_Z_X1, CONT_TEST_BUTTON_Z_Y1,
		        CONT_TEST_BUTTON_Z_X1 + CONT_TEST_BUTTON_XY_SHORT, CONT_TEST_BUTTON_Z_Y1 + CONT_TEST_BUTTON_XY_SHORT,
		        COLOR_WHITE, currXfb);
		drawCharDirect(currXfb, CONT_TEST_BUTTON_Z_X1 + 7, CONT_TEST_BUTTON_Z_Y1 + 4, COLOR_WHITE, 'Z');
		// stop rumble if z is not held
		PAD_ControlMotor(0, PAD_MOTOR_STOP);
	}

	// Start
	if (held & PAD_BUTTON_START) {
		DrawFilledBox(CONT_TEST_BUTTON_START_X1, CONT_TEST_BUTTON_START_Y1,
		              CONT_TEST_BUTTON_START_X1 + CONT_TEST_BUTTON_START_LEN, CONT_TEST_BUTTON_START_Y1 + CONT_TEST_BUTTON_START_WIDTH,
		              COLOR_WHITE, currXfb);
		// TODO: this is ugly
		drawCharDirect(currXfb, CONT_TEST_BUTTON_START_X1 + 7, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_BLACK, 'S');
		drawCharDirect(currXfb, CONT_TEST_BUTTON_START_X1 + 17, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_BLACK, 'T');
		drawCharDirect(currXfb, CONT_TEST_BUTTON_START_X1 + 27, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_BLACK, 'A');
		drawCharDirect(currXfb, CONT_TEST_BUTTON_START_X1 + 37, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_BLACK, 'R');
		drawCharDirect(currXfb, CONT_TEST_BUTTON_START_X1 + 47, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_BLACK, 'T');
	} else {
		DrawBox(CONT_TEST_BUTTON_START_X1, CONT_TEST_BUTTON_START_Y1,
		        CONT_TEST_BUTTON_START_X1 + CONT_TEST_BUTTON_START_LEN, CONT_TEST_BUTTON_START_Y1 + CONT_TEST_BUTTON_START_WIDTH,
		        COLOR_WHITE, currXfb);
		drawCharDirect(currXfb, CONT_TEST_BUTTON_START_X1 + 7, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_WHITE, 'S');
		drawCharDirect(currXfb, CONT_TEST_BUTTON_START_X1 + 17, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_WHITE, 'T');
		drawCharDirect(currXfb, CONT_TEST_BUTTON_START_X1 + 27, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_WHITE, 'A');
		drawCharDirect(currXfb, CONT_TEST_BUTTON_START_X1 + 37, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_WHITE, 'R');
		drawCharDirect(currXfb, CONT_TEST_BUTTON_START_X1 + 47, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_WHITE, 'T');
	}
	
	// DPad
	// up
	if (held & PAD_BUTTON_UP) {
		DrawFilledBox(CONT_TEST_DPAD_UP_X1, CONT_TEST_DPAD_UP_Y1,
		        CONT_TEST_DPAD_UP_X1 + CONT_TEST_DPAD_SHORT, CONT_TEST_DPAD_UP_Y1 + CONT_TEST_DPAD_LONG,
		        COLOR_WHITE, currXfb);
	} else {
		DrawBox(CONT_TEST_DPAD_UP_X1, CONT_TEST_DPAD_UP_Y1,
		        CONT_TEST_DPAD_UP_X1 + CONT_TEST_DPAD_SHORT, CONT_TEST_DPAD_UP_Y1 + CONT_TEST_DPAD_LONG,
		        COLOR_WHITE, currXfb);
	}
	
	// down
	if (held & PAD_BUTTON_DOWN) {
		DrawFilledBox(CONT_TEST_DPAD_UP_X1, CONT_TEST_DPAD_DOWN_Y1,
		              CONT_TEST_DPAD_UP_X1 + CONT_TEST_DPAD_SHORT, CONT_TEST_DPAD_DOWN_Y1 + CONT_TEST_DPAD_LONG,
		              COLOR_WHITE, currXfb);
	} else {
		DrawBox(CONT_TEST_DPAD_UP_X1, CONT_TEST_DPAD_DOWN_Y1,
		        CONT_TEST_DPAD_UP_X1 + CONT_TEST_DPAD_SHORT, CONT_TEST_DPAD_DOWN_Y1 + CONT_TEST_DPAD_LONG,
		        COLOR_WHITE, currXfb);
	}
	
	
	//left
	if (held & PAD_BUTTON_LEFT) {
		DrawFilledBox(CONT_TEST_DPAD_LEFT_X1, CONT_TEST_DPAD_LEFT_Y1,
		              CONT_TEST_DPAD_LEFT_X1 + CONT_TEST_DPAD_LONG, CONT_TEST_DPAD_LEFT_Y1 + CONT_TEST_DPAD_SHORT,
		              COLOR_WHITE, currXfb);
	} else {
		DrawBox(CONT_TEST_DPAD_LEFT_X1, CONT_TEST_DPAD_LEFT_Y1,
		        CONT_TEST_DPAD_LEFT_X1 + CONT_TEST_DPAD_LONG, CONT_TEST_DPAD_LEFT_Y1 + CONT_TEST_DPAD_SHORT,
		        COLOR_WHITE, currXfb);
	}
	
	// right
	if (held & PAD_BUTTON_RIGHT) {
		DrawFilledBox(CONT_TEST_DPAD_RIGHT_X1, CONT_TEST_DPAD_LEFT_Y1,
					  CONT_TEST_DPAD_RIGHT_X1 + CONT_TEST_DPAD_LONG, CONT_TEST_DPAD_LEFT_Y1 + CONT_TEST_DPAD_SHORT,
		              COLOR_WHITE, currXfb);
	} else {
		DrawBox(CONT_TEST_DPAD_RIGHT_X1, CONT_TEST_DPAD_LEFT_Y1,
				CONT_TEST_DPAD_RIGHT_X1 + CONT_TEST_DPAD_LONG, CONT_TEST_DPAD_LEFT_Y1 + CONT_TEST_DPAD_SHORT,
		        COLOR_WHITE, currXfb);
	}
	
	 
	// Analog L Slider
	//DrawBox(53, 69, 66, 326, COLOR_WHITE, currXfb);
	DrawBox(CONT_TEST_TRIGGER_L_X1, CONT_TEST_TRIGGER_Y1,
			CONT_TEST_TRIGGER_L_X1 + CONT_TEST_TRIGGER_WIDTH + 1, CONT_TEST_TRIGGER_Y1 + CONT_TEST_TRIGGER_LEN + 1,
			COLOR_WHITE, currXfb);
	if (held & PAD_TRIGGER_L) {
		DrawFilledBox(CONT_TEST_TRIGGER_L_X1 + 2, CONT_TEST_TRIGGER_Y1 + 1 + (255 - PAD_TriggerL(0)),
		              CONT_TEST_TRIGGER_L_X1 + CONT_TEST_TRIGGER_WIDTH, CONT_TEST_TRIGGER_Y1 + CONT_TEST_TRIGGER_LEN,
		              COLOR_BLUE, currXfb);
	} else {
		DrawFilledBox(CONT_TEST_TRIGGER_L_X1 + 2, CONT_TEST_TRIGGER_Y1 + 1 + (255 - PAD_TriggerL(0)),
				CONT_TEST_TRIGGER_L_X1 + CONT_TEST_TRIGGER_WIDTH, CONT_TEST_TRIGGER_Y1 + CONT_TEST_TRIGGER_LEN,
				COLOR_RED, currXfb);
	}
	
	
	setCursorPos(17,2);
	sprintf(strBuffer, "Analog L: %d", PAD_TriggerL(0));
	printStr(strBuffer, currXfb);
	if (held & PAD_TRIGGER_L) {
		setCursorPos(18, 2);
		printStr("Digital L Pressed", currXfb);
	}
	
	setCursorPos(17,44);
	sprintf(strBuffer, "Analog R: %d", PAD_TriggerR(0));
	printStr(strBuffer, currXfb);
	if (held & PAD_TRIGGER_R) {
		setCursorPos(18, 40);
		printStr("Digital R Pressed", currXfb);
	}
	
	// Analog R Slider
	DrawBox(CONT_TEST_TRIGGER_R_X1, CONT_TEST_TRIGGER_Y1,
	        CONT_TEST_TRIGGER_R_X1 + CONT_TEST_TRIGGER_WIDTH + 1, CONT_TEST_TRIGGER_Y1 + CONT_TEST_TRIGGER_LEN + 1,
	        COLOR_WHITE, currXfb);
	if (held & PAD_TRIGGER_R) {
		DrawFilledBox(CONT_TEST_TRIGGER_R_X1 + 2, CONT_TEST_TRIGGER_Y1 + 1 + (255 - PAD_TriggerR(0)),
		              CONT_TEST_TRIGGER_R_X1 + CONT_TEST_TRIGGER_WIDTH, CONT_TEST_TRIGGER_Y1 + CONT_TEST_TRIGGER_LEN,
		              COLOR_BLUE, currXfb);
	} else {
		DrawFilledBox(CONT_TEST_TRIGGER_R_X1 + 2, CONT_TEST_TRIGGER_Y1 + 1 + (255 - PAD_TriggerR(0)),
		              CONT_TEST_TRIGGER_R_X1 + CONT_TEST_TRIGGER_WIDTH, CONT_TEST_TRIGGER_Y1 + CONT_TEST_TRIGGER_LEN,
		              COLOR_RED, currXfb);
	}

	// Analog Stick
	// calculate screen coordinates for stick position drawing
	int xfbCoordX = (stickCoordinatesMelee.ax / 250);
	if (stickCoordinatesRaw.ax < 0) {
		xfbCoordX *= -1;
	}
	xfbCoordX += CONT_TEST_STICK_CENTER_X;
	
	int xfbCoordY = (stickCoordinatesMelee.ay / 250);
	if (stickCoordinatesRaw.ay > 0) {
		xfbCoordY *= -1;
	}
	xfbCoordY += CONT_TEST_STICK_CENTER_Y;
	
	int xfbCoordCX = (stickCoordinatesMelee.cx / 250);
	if (stickCoordinatesRaw.cx < 0) {
		xfbCoordCX *= -1;
	}
	xfbCoordCX += CONT_TEST_CSTICK_CENTER_X;
	
	int xfbCoordCY = (stickCoordinatesMelee.cy / 250);
	if (stickCoordinatesRaw.cy > 0) {
		xfbCoordCY *= -1;
	}
	xfbCoordCY += CONT_TEST_CSTICK_CENTER_Y;
	
	// analog stick
	DrawOctagonalGate(CONT_TEST_STICK_CENTER_X, CONT_TEST_STICK_CENTER_Y, 2, COLOR_GRAY, currXfb); // perimeter
	DrawLine(CONT_TEST_STICK_CENTER_X, CONT_TEST_STICK_CENTER_Y,
	         CONT_TEST_STICK_CENTER_X + (stickCoordinatesRaw.ax / 2), CONT_TEST_STICK_CENTER_Y - (stickCoordinatesRaw.ay / 2),
			 COLOR_SILVER, currXfb); // line from center
	for (int i = CONT_TEST_STICK_RAD / 2; i > 0; i -= 5) {
		DrawCircle(CONT_TEST_STICK_CENTER_X + (stickCoordinatesRaw.ax / 2), CONT_TEST_STICK_CENTER_Y - (stickCoordinatesRaw.ay / 2), i, COLOR_WHITE, currXfb);
	} // smaller circle
	
	// c-stick
	DrawOctagonalGate(CONT_TEST_CSTICK_CENTER_X, CONT_TEST_CSTICK_CENTER_Y, 2, COLOR_GRAY, currXfb); // perimeter
	DrawLine(CONT_TEST_CSTICK_CENTER_X, CONT_TEST_CSTICK_CENTER_Y,
	         CONT_TEST_CSTICK_CENTER_X + (stickCoordinatesRaw.cx / 2), CONT_TEST_CSTICK_CENTER_Y - (stickCoordinatesRaw.cy / 2),
	         COLOR_MEDGRAY, currXfb); // line from center
	DrawFilledCircle(CONT_TEST_CSTICK_CENTER_X + (stickCoordinatesRaw.cx / 2), CONT_TEST_CSTICK_CENTER_Y - (stickCoordinatesRaw.cy / 2),
	                 CONT_TEST_STICK_RAD / 2, COLOR_YELLOW, currXfb); // smaller circle
}

void menu_2dPlot(void *currXfb) {
	static WaveformDatapoint convertedCoords;

	// display instructions and data for user
	printStr("Press A to start read, press Z for instructions", currXfb);

	// do we have data that we can display?
	if (data.isDataReady) {
		if (lastDrawPoint == -1) {
			lastDrawPoint = data.endPoint - 1;
		}
		convertedCoords = convertStickValues(&data.data[lastDrawPoint]);
		// TODO: move instructions under different prompt, so I don't have to keep messing with text placement
		
		setCursorPos(5, 0);
		sprintf(strBuffer, "Total samples: %04u\n", data.endPoint);
		printStr(strBuffer, currXfb);
		sprintf(strBuffer, "Start sample: %04u\n", map2dStartIndex + 1);
		printStr(strBuffer, currXfb);
		sprintf(strBuffer, "End sample: %04u\n", lastDrawPoint + 1);
		printStr(strBuffer, currXfb);
		
		u64 timeFromStart = 0;
		for (int i = map2dStartIndex; i <= lastDrawPoint; i++) {
			timeFromStart += data.data[i].timeDiffUs;
		}
		float timeFromStartMs = timeFromStart / 1000.0;
		sprintf(strBuffer, "Total MS: %6.2f\n", timeFromStartMs);
		printStr(strBuffer, currXfb);
		sprintf(strBuffer, "Total frames: %2.2f", timeFromStartMs / frameTime);
		printStr(strBuffer, currXfb);
		
		// print coordinates of last drawn point
		// raw stick coordinates
		setCursorPos(19, 0);
		sprintf(strBuffer, "Raw XY: (%04d,%04d)\n", data.data[lastDrawPoint].ax, data.data[lastDrawPoint].ay);
		printStr(strBuffer, currXfb);
		printStr("Melee XY: (", currXfb);
		// is the value negative?
		if (data.data[lastDrawPoint].ax < 0) {
			printStr("-", currXfb);
		} else {
			printStr("0", currXfb);
		}
		// is this a 1.0 value?
		if (convertedCoords.ax == 10000) {
			printStr("1.0000", currXfb);
		} else {
			sprintf(strBuffer, "0.%04d", convertedCoords.ax);
			printStr(strBuffer, currXfb);
		}
		printStr(",", currXfb);
		
		// is the value negative?
		if (data.data[lastDrawPoint].ay < 0) {
			printStr("-", currXfb);
		} else {
			printStr("0", currXfb);
		}
		// is this a 1.0 value?
		if (convertedCoords.ay == 10000) {
			printStr("1.0000", currXfb);
		} else {
			sprintf(strBuffer, "0.%04d", convertedCoords.ay);
			printStr(strBuffer, currXfb);
		}
		printStr(")\n", currXfb);
		printStr("Stickmap: ", currXfb);

		// draw image below 2d plot, and print while we're at it
		switch (selectedImage) {
			case A_WAIT:
				printStr("Wait Attacks", currXfb);
				drawImage(currXfb, await_image, await_indexes, COORD_CIRCLE_CENTER_X - 127, SCREEN_POS_CENTER_Y - 127);
				break;
			case CROUCH:
				printStr("Crouch", currXfb);
				drawImage(currXfb, crouch_image, crouch_indexes, COORD_CIRCLE_CENTER_X - 127, SCREEN_POS_CENTER_Y - 127);
				break;
			case DEADZONE:
				printStr("Deadzones", currXfb);
				drawImage(currXfb, deadzone_image, deadzone_indexes, COORD_CIRCLE_CENTER_X - 127, SCREEN_POS_CENTER_Y - 127);
				break;
			case LEDGE_L:
				printStr("Left Ledge", currXfb);
				drawImage(currXfb, ledgeL_image, ledgeL_indexes, COORD_CIRCLE_CENTER_X - 127, SCREEN_POS_CENTER_Y - 127);
				break;
			case LEDGE_R:
				printStr("Right Ledge", currXfb);
				drawImage(currXfb, ledgeR_image, ledgeR_indexes, COORD_CIRCLE_CENTER_X - 127, SCREEN_POS_CENTER_Y - 127);
				break;
			case MOVE_WAIT:
				printStr("Wait Movement", currXfb);
				drawImage(currXfb, movewait_image, movewait_indexes, COORD_CIRCLE_CENTER_X - 127, SCREEN_POS_CENTER_Y - 127);
				break;
			case NO_IMAGE:
				printStr("None", currXfb);
			default:
				break;
		}
		
		// draw box around plot area
		DrawBox(COORD_CIRCLE_CENTER_X - 128, SCREEN_POS_CENTER_Y - 128,
				COORD_CIRCLE_CENTER_X + 128, SCREEN_POS_CENTER_Y + 128,
				COLOR_WHITE, currXfb);
		
		
		// draw plot
		// y is negated because of how the graph is drawn
		// TODO: why does this need to be <= to avoid an off-by-one? step through logic later this is bugging me
		for (int i = 0; i <= lastDrawPoint; i++) {
			if (i >= map2dStartIndex) {
				DrawDot(COORD_CIRCLE_CENTER_X + data.data[i].ax, SCREEN_POS_CENTER_Y - data.data[i].ay, COLOR_WHITE, currXfb);
			} else {
				DrawDot(COORD_CIRCLE_CENTER_X + data.data[i].ax, SCREEN_POS_CENTER_Y - data.data[i].ay, COLOR_GRAY, currXfb);
			}
		}

		// TODO: refactor this, its a mess
		// does the user want to change what data is drawn?
		// single movements with L
		if (held & PAD_TRIGGER_L) {
			if (pressed & PAD_BUTTON_RIGHT) {
				if (held & PAD_BUTTON_Y) {
					if (map2dStartIndex + 1 < lastDrawPoint) {
						map2dStartIndex++;
					} else {
						map2dStartIndex = lastDrawPoint;
					}
				} else {
					if (lastDrawPoint + 1 < data.endPoint) {
						lastDrawPoint++;
					} else {
						lastDrawPoint = data.endPoint - 1;
					}
				}
			} else if (pressed & PAD_BUTTON_LEFT) {
				if (held & PAD_BUTTON_Y) {
					if (map2dStartIndex - 1 >= 0) {
						map2dStartIndex--;
					}
				} else {
					if (lastDrawPoint - 1 >= 0) {
						lastDrawPoint--;
					}
				}
			}
		} else if (held & PAD_BUTTON_RIGHT) {
			if (held & PAD_TRIGGER_R) {
				if (held & PAD_BUTTON_Y) {
					if (map2dStartIndex + 5 < lastDrawPoint) {
						map2dStartIndex += 5;
					} else {
						map2dStartIndex = lastDrawPoint;
					}
				} else {
					if (lastDrawPoint + 5 < data.endPoint) {
						lastDrawPoint += 5;
					} else {
						lastDrawPoint = data.endPoint - 1;
					}
				}
			} else {
				if (held & PAD_BUTTON_Y) {
					if (map2dStartIndex + 1 < lastDrawPoint) {
						map2dStartIndex++;
					}
				} else {
					if (lastDrawPoint + 1 < data.endPoint) {
						lastDrawPoint++;
					}
				}
			}
		} else if (held & PAD_BUTTON_LEFT) {
			if (held & PAD_TRIGGER_R) {
				if (held & PAD_BUTTON_Y) {
					if (map2dStartIndex - 5 >= 0) {
						map2dStartIndex -= 5;
					} else {
						map2dStartIndex = 0;
					}
				} else {
					if (lastDrawPoint - 5 >= 0) {
						lastDrawPoint -= 5;
					} else {
						lastDrawPoint = 0;
						map2dStartIndex = 0;
					}
				}
			} else {
				if (held & PAD_BUTTON_Y) {
					if (map2dStartIndex - 1 >= 0) {
						map2dStartIndex--;
					}
				} else {
					if (lastDrawPoint - 1 >= 0) {
						lastDrawPoint--;
					}
				}
			}
		}
		
		// hacky fix to make sure start isn't after end
		// TODO: this should be fixed when the above is refactored
		if (lastDrawPoint <= map2dStartIndex && lastDrawPoint != 0) {
			map2dStartIndex = lastDrawPoint - 1;
		}

		// does the user want to change what stickmap is displayed?
		if (pressed & PAD_BUTTON_X) {
			selectedImage++;
			if (selectedImage == IMAGE_LEN) {
				selectedImage = NO_IMAGE;
			}
		}
	}

	// only start reading if A is pressed
	// TODO: figure out if this can be removed without having to gut the current poll logic, would be better for the user to not have to do this
	if (pressed & PAD_BUTTON_A) {
		data.fullMeasure = false;
		previousMenu = PLOT_2D;
		currentMenu = WAITING_MEASURE;
	}
}


void menu_fileExport(void *currXfb) {
	printStr("todo", currXfb);
	return;
	
	if (data.isDataReady) {
		if (!data.exported) {
			data.exported = exportData(&data, false);
			if (!data.exported) {
				printf("Failed to export data");
				while (!(held & PAD_BUTTON_B)) {
					PAD_ScanPads();
					held = PAD_ButtonsHeld(0);
					VIDEO_WaitVSync();
				}
			}
		} else {
			printf("Data already exported");
		}
	} else {
		printf("No data to export, record an input using Oscilloscope or Plot");
	}
}


void menu_waitingMeasure(void *currXfb) {
	if (!displayedWaitingInputMessage) {
		printStr("\nWaiting for user input...", currXfb);
		displayedWaitingInputMessage = true;
		return;
	}
	measureWaveform(&data);
	dataScrollOffset = 0;
	lastDrawPoint = data.endPoint - 1;
	map2dStartIndex = 0;
	assert(data.endPoint < 5000);
	currentMenu = previousMenu;
	displayedWaitingInputMessage = false;
}


void menu_coordinateViewer(void *currXfb) {
	// melee stick coordinates stuff
	// a lot of this comes from github.com/phobgcc/phobconfigtool
	
	printStr("Press Z for instructions", currXfb);
	
	static WaveformDatapoint stickCoordinatesRaw;
	static WaveformDatapoint stickCoordinatesMelee;
	
	// get raw stick values
	stickCoordinatesRaw.ax = PAD_StickX(0), stickCoordinatesRaw.ay = PAD_StickY(0);
	stickCoordinatesRaw.cx = PAD_SubStickX(0), stickCoordinatesRaw.cy = PAD_SubStickY(0);
	
	// get converted stick values
	stickCoordinatesMelee = convertStickValues(&stickCoordinatesRaw);
	
	// print melee coordinates
	setCursorPos(4, 0);
	printStr("Stick X: ", currXfb);
	// is the value negative?
	if (stickCoordinatesRaw.ax < 0) {
		printStr("-", currXfb);
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.ax == 10000) {
		printStr("1.0\n", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d\n", stickCoordinatesMelee.ax);
		printStr(strBuffer, currXfb);
	}
	
	// print melee coordinates
	printStr("Stick Y: ", currXfb);
	// is the value negative?
	if (stickCoordinatesRaw.ay < 0) {
		printStr("-", currXfb);
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.ay == 10000) {
		printStr("1.0\n", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d\n", stickCoordinatesMelee.ay);
		printStr(strBuffer, currXfb);
	}
	
	// print melee coordinates
	printStr("\nC-Stick X: ", currXfb);
	// is the value negative?
	if (stickCoordinatesRaw.cx < 0) {
		printStr("-", currXfb);
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.cx == 10000) {
		printStr("1.0\n", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d\n", stickCoordinatesMelee.cx);
		printStr(strBuffer, currXfb);
	}
	
	// print melee coordinates
	printStr("C-Stick Y: ", currXfb);
	// is the value negative?
	if (stickCoordinatesRaw.cy < 0) {
		printStr("-", currXfb);
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.cy == 10000) {
		printStr("1.0\n", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d\n", stickCoordinatesMelee.cy);
		printStr(strBuffer, currXfb);
	}
	
	setCursorPos(19, 0);
	printStr("Stickmap: ", currXfb);
	int stickmapRetVal = isCoordValid(selectedStickmap, stickCoordinatesMelee);
	switch (selectedStickmap) {
		case FF_WD:
			printStr("Firefox/Wavedash\n", currXfb);
			printStr("Visible: ", currXfb);
			if (selectedStickmapSub == 0) {
				printStr("ALL", currXfb);
			} else {
				printStrColor(STICKMAP_FF_WD_RETVALS[selectedStickmapSub], currXfb,
							  STICKMAP_FF_WD_RETCOLORS[selectedStickmapSub][0], STICKMAP_FF_WD_RETCOLORS[selectedStickmapSub][1]);
				//sprintf(strBuffer, "%s\n", );
				//printStr(strBuffer, currXfb);
			}
			printStr("\nResult: ", currXfb);
			printStrColor(STICKMAP_FF_WD_RETVALS[stickmapRetVal], currXfb,
						  STICKMAP_FF_WD_RETCOLORS[stickmapRetVal][0], STICKMAP_FF_WD_RETCOLORS[stickmapRetVal][1]);
			break;
		case SHIELDDROP:
			printStr("Shield Drop\n", currXfb);
			printStr("Visible: ", currXfb);
			if (selectedStickmapSub == 0) {
				printStr("ALL", currXfb);
			} else {
				//sprintf(strBuffer, "%s\n", STICKMAP_SHIELDDROP_RETVALS[selectedStickmapSub]);
				//printStr(strBuffer, currXfb);
				printStrColor(STICKMAP_SHIELDDROP_RETVALS[selectedStickmapSub], currXfb,
				              STICKMAP_SHIELDDROP_RETCOLORS[selectedStickmapSub][0], STICKMAP_SHIELDDROP_RETCOLORS[selectedStickmapSub][1]);
			}
			printStr("\nResult: ", currXfb);
			printStrColor(STICKMAP_SHIELDDROP_RETVALS[stickmapRetVal], currXfb,
						  STICKMAP_SHIELDDROP_RETCOLORS[stickmapRetVal][0], STICKMAP_SHIELDDROP_RETCOLORS[stickmapRetVal][1]);
			break;
		case NONE:
		default:
			printStr("NONE", currXfb);
			break;
	}
	
	
	// calculate screen coordinates for stick position drawing
	int xfbCoordX = (stickCoordinatesMelee.ax / 125) * 2;
	if (stickCoordinatesRaw.ax < 0) {
		xfbCoordX *= -1;
	}
	xfbCoordX += COORD_CIRCLE_CENTER_X;
	
	int xfbCoordY = (stickCoordinatesMelee.ay / 125) * 2;
	if (stickCoordinatesRaw.ay > 0) {
		xfbCoordY *= -1;
	}
	xfbCoordY += SCREEN_POS_CENTER_Y;
	
	int xfbCoordCX = (stickCoordinatesMelee.cx / 125) * 2;
	if (stickCoordinatesRaw.cx < 0) {
		xfbCoordCX *= -1;
	}
	xfbCoordCX += COORD_CIRCLE_CENTER_X;
	
	int xfbCoordCY = (stickCoordinatesMelee.cy / 125) * 2;
	if (stickCoordinatesRaw.cy > 0) {
		xfbCoordCY *= -1;
	}
	xfbCoordCY += SCREEN_POS_CENTER_Y;
	
	// draw stickbox bounds
	DrawCircle(COORD_CIRCLE_CENTER_X, SCREEN_POS_CENTER_Y, 160, COLOR_MEDGRAY, currXfb);
	
	DrawStickmapOverlay(selectedStickmap, selectedStickmapSub, currXfb);

	// draw analog stick line
	DrawLine(COORD_CIRCLE_CENTER_X, SCREEN_POS_CENTER_Y, xfbCoordX, xfbCoordY, COLOR_WHITE, currXfb);
	DrawBox(xfbCoordX - 4, xfbCoordY - 4, xfbCoordX + 4, xfbCoordY + 4, COLOR_WHITE, currXfb);
	
	// draw c-stick line
	DrawLine(COORD_CIRCLE_CENTER_X, SCREEN_POS_CENTER_Y, xfbCoordCX, xfbCoordCY, COLOR_YELLOW, currXfb);
	DrawFilledBox(xfbCoordCX - 2, xfbCoordCY - 2, xfbCoordCX + 2, xfbCoordCY + 2, COLOR_YELLOW, currXfb);
	
	if (pressed & PAD_BUTTON_X) {
		selectedStickmap++;
		selectedStickmapSub = 0;
		if (selectedStickmap == 3) {
			selectedStickmap = 0;
		}
	}
	if (pressed & PAD_BUTTON_Y) {
		selectedStickmapSub++;
		switch (selectedStickmap) {
			case (FF_WD):
				if (selectedStickmapSub == STICKMAP_FF_WD_ENUM_LEN) {
					selectedStickmapSub = 0;
				}
				break;
			case (SHIELDDROP):
				if (selectedStickmapSub == STICKMAP_SHIELDDROP_ENUM_LEN) {
					selectedStickmapSub = 0;
				}
				break;
			case (NONE):
			default:
				selectedStickmapSub = 0;
				break;
		}
	}
}
