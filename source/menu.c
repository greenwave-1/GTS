//
// Created on 2023/10/25.
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
#include "file/file.h"
#include "stickmap_coordinates.h"

#include "submenu/oscilloscope.h"
#include "submenu/continuous.h"
#include "submenu/trigger.h"
#include "submenu/plot2d.h"
#include "submenu/gate.h"
#include "submenu/plotbutton.h"

#ifndef VERSION_NUMBER
#define VERSION_NUMBER "NOVERS_DEV"
#endif

#define MENUITEMS_LEN 9

// macro for how far the stick has to go before it counts as a movement
#define MENU_STICK_THRESHOLD 10

const float frameTime = (1000.0 / 60.0);

// enum to keep track of what menu to display, and what logic to run
static enum CURRENT_MENU currentMenu = MAIN_MENU;

// lock var for controller test
static bool lockExitControllerTest = false;
static bool startHeldAfter = false;
static u8 startHeldCounter = 0;

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
static int stickYPos = 0, stickYPrevPos = 0;
static u8 stickLockoutCounter = 0;
static bool stickLockout = false;

// menu item strings
//static const char* menuItems[MENUITEMS_LEN] = { "Controller Test", "Stick Oscilloscope", "Coordinate Viewer", "2D Plot", "Export Data", "Continuous Waveform" };
static const char* menuItems[MENUITEMS_LEN] = { "Controller Test", "Stick Oscilloscope", "Continuous Stick Oscilloscope", "Trigger Oscilloscope",
                                                "Coordinate Viewer", "2D Plot", "Button Timing Viewer", "Gate Visualizer", "Export Data"};


static bool displayInstructions = false;

static int exportReturnCode = -1;

static u32 padsConnected = 0;

static PADStatus origin[PAD_CHANMAX];
static bool originRead = false;

// buffer for strings with numbers and stuff using sprintf
static char strBuffer[100];

static bool setDrawInterlaceMode = false;

static u8 thanksPageCounter = 0;

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
	
	printStr("GCC Test Suite", currXfb);
	
	// check if port 1 is disconnected
	if ((padsConnected & 1) == 0) {
		setCursorPos(0, 38);
		printStr("Controller Disconnected!", currXfb);
		data.isDataReady = false;
		originRead = false;
	}
	
	if (data.isDataReady) {
		setCursorPos(0, 31);
		printStr("Oscilloscope Capture in memory!", currXfb);
	} else {
		exportReturnCode = -1;
	}
	setCursorPos(2, 0);
	
	// check for any buttons pressed/held
	// don't update if we are on a menu with its own callback
	// TODO: eventually I'd like each menu entry to be in its own file, would that affect this?
	// TODO: This is getting messy, there's probably a better way to do this...
	if (currentMenu != WAVEFORM && currentMenu != CONTINUOUS_WAVEFORM &&
			currentMenu != PLOT_2D && currentMenu != TRIGGER_WAVEFORM &&
			currentMenu != GATE_MEASURE && currentMenu != PLOT_BUTTON) {
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
			menu_plot2d(currXfb, &data, &pressed, &held);
			break;
		case FILE_EXPORT:
			menu_fileExport(currXfb);
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
		case TRIGGER_WAVEFORM:
			menu_triggerOscilloscope(currXfb, &pressed, &held);
			break;
		case THANKS_PAGE:
			menu_thanksPage(currXfb);
			break;
		case GATE_MEASURE:
			if (originRead == false) {
				menu_gateControllerDisconnected();
			}
			menu_gateMeasure(currXfb, &pressed, &held);
			break;
		case PLOT_BUTTON:
			menu_plotButton(currXfb, &pressed, &held);
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
	// controller test lock stuff
	else if (held == PAD_BUTTON_START && currentMenu == CONTROLLER_TEST && !startHeldAfter) {
		if (lockExitControllerTest) {
			printStr("Enabling exit, hold for 2 seconds", currXfb);
		} else {
			printStr("Disabling exit, hold for 2 seconds", currXfb);
		}
		printEllipse(startHeldCounter, 40, currXfb);
		
		startHeldCounter++;
		if (startHeldCounter > 121) {
			lockExitControllerTest = !lockExitControllerTest;
			startHeldCounter = 0;
			startHeldAfter = true;
		}
	}

	// does the user want to move back to the main menu?
	else if (held == PAD_BUTTON_B && currentMenu != MAIN_MENU &&
			!lockExitControllerTest &&
			!menu_plotButtonHasCaptureStarted()) {

		// give user feedback that they are holding the button
		printStr("Moving back to main menu", currXfb);

		// TODO: I know there's a better way to do this but I can't think of it right now...
		printEllipse(bHeldCounter, 15, currXfb);
		bHeldCounter++;
		
		
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
				case PLOT_2D:
					menu_plot2dEnd();
					break;
				case TRIGGER_WAVEFORM:
					menu_triggerOscilloscopeEnd();
					break;
				case GATE_MEASURE:
					menu_gateMeasureEnd();
					break;
				case PLOT_BUTTON:
					menu_plotButtonEnd();
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
			if (currentMenu == COORD_MAP) {
				displayInstructions = !displayInstructions;
			}
		}
		switch (currentMenu) {
			case MAIN_MENU:
				printStr("Press Start to exit.", currXfb);
				int col = 55 - (sizeof(VERSION_NUMBER));
				if (col > 25) {
					setCursorPos(22, col);
					printStr("Ver: ", currXfb);
					printStr(VERSION_NUMBER, currXfb);
				}
				break;
			case CONTROLLER_TEST:
				if (lockExitControllerTest) {
					printStr("Exiting disabled, hold Start to re-enable.", currXfb);
				} else {
					printStr("Hold B to return to main menu, hold start to disable.", currXfb);
				}
				startHeldCounter = 0;
				
				if (startHeldAfter && held ^ PAD_BUTTON_START) {
					startHeldAfter = false;
				}
				break;
			case PLOT_BUTTON:
				if (menu_plotButtonHasCaptureStarted()) {
					break;
				}
			default:
				printStr("Hold B to return to main menu.", currXfb);
				break;
		}
		bHeldCounter = 0;
	}

	return false;
}

void menu_mainMenu(void *currXfb) {
	stickYPrevPos = stickYPos;
	stickYPos = PAD_StickY(0);
	int stickDiff = abs(stickYPos - stickYPrevPos);
	
	// flags which tell whether the stick is held in an up or down position
	u8 up = stickYPos > MENU_STICK_THRESHOLD;
	u8 down = stickYPos < -MENU_STICK_THRESHOLD;
	
	// check if we're outside the deadzone, and the stick hasn't moved past a threshold since last frame
	if (stickDiff < MENU_STICK_THRESHOLD && (up || down)) {
		// 120 frames == ~2 seconds
		if (stickLockoutCounter < 120) {
			stickLockoutCounter++;
		} else if (stickLockoutCounter == 120) {
			stickLockout = true;
		}
	} else {
		stickLockout = false;
		stickLockoutCounter = 0;
	}

	// only move the stick if it wasn't already held for the last 10 ticks
	u8 movable = stickheld % 10 == 0 && !stickLockout;
	
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
				currentMenu = TRIGGER_WAVEFORM;
				break;
			case 4:
				currentMenu = COORD_MAP;
				break;
			case 5:
				currentMenu = PLOT_2D;
				break;
			case 6:
				currentMenu = PLOT_BUTTON;
				break;
			case 7:
				currentMenu = GATE_MEASURE;
				break;
			case 8:
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
	
	// thanks page logic: if Z is held on port 4 for 2 seconds on the main menu
	if (PAD_ButtonsHeld(3) == PAD_TRIGGER_Z) {
		thanksPageCounter++;
		if (thanksPageCounter == 120) {
			currentMenu = THANKS_PAGE;
			thanksPageCounter = 0;
		}
	} else {
		thanksPageCounter = 0;
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
		
		if (!(held & PAD_TRIGGER_L)) {
			setCursorPos(18, 2);
			sprintf(strBuffer, "L Origin: %d", origin[0].triggerL);
			printStr(strBuffer, currXfb);
		}
		if (!(held & PAD_TRIGGER_R)) {
			setCursorPos(18, 44);
			sprintf(strBuffer, "R Origin: %d", origin[0].triggerR);
			printStr(strBuffer, currXfb);
		}
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

void menu_fileExport(void *currXfb) {
	// run if we have a result
	//if (exportReturnCode >= 0) {
	if (data.isDataReady) {
		if (data.exported) {
			switch (exportReturnCode) {
				case 0:
					printStr("File exported successfully.", currXfb);
					break;
				case 1:
					printStr("Data was marked as not ready, this shouldn't happen!", currXfb);
					break;
				case 2:
					printStr("Failed to init filesystem.", currXfb);
					break;
				case 3:
					printStr("Failed to create parent directory.", currXfb);
					break;
				case 4:
					printStr("Failed to create file, file already exists!", currXfb);
					break;
				default:
					printStr("How did we get here?", currXfb);
					break;
			}
		} else {
			printStr("Attempting to export data...", currXfb);
			exportReturnCode = exportData(&data);
		}
	} else {
		printStr("No data to export, record an input first.", currXfb);
	}
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

void menu_thanksPage(void *currXfb) {
	printStr("Thanks to:\n"
			 "PhobGCC team and Discord\n"
			 "DevkitPro team\n"
			 "Extrems\n"
			 "SmashScope\n"
			 "Z. B. Wells", currXfb);
}
