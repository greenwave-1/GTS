//
// Created on 2023/10/25.
//

#include "menu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ogc/color.h>
#include <ogc/pad.h>
#include <ogc/video.h>
#include <ogc/libversion.h>

#include "waveform.h"
#include "print.h"
#include "polling.h"

// TODO: these should go away once all menus have been moved to a separate file
#include "stickmap_coordinates.h"
#include "draw.h"
#include "file/file.h"

// should I have a "parent" header that includes these?
#include "submenu/oscilloscope.h"
#include "submenu/continuous.h"
#include "submenu/trigger.h"
#include "submenu/plot2d.h"
#include "submenu/gate.h"
#include "submenu/plotbutton.h"

#ifndef VERSION_NUMBER
#define VERSION_NUMBER "NOVERS_DEV"
#endif

#ifndef COMMIT_ID
#define COMMIT_ID "BAD_ID"
#endif

#ifndef BUILD_DATE
#define BUILD_DATE "NO_DATE"
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
static uint8_t startHeldCounter = 0;

static enum STICKMAP_LIST selectedStickmap = NONE;
// will be casted to whichever stickmap is selected
static int selectedStickmapSub = 0;

// main menu counter
static enum MENU_MAIN_ENTRY_LIST mainMenuCursorPos = 0;

// counter for how many frames b or start have been held
static uint8_t bHeldCounter = 0;

// displayed struct
static ControllerRec **data = NULL;

// vars for what buttons are pressed or held
static uint16_t *pressed = NULL;
static uint16_t *held = NULL;

// var for counting how long the stick has been held away from neutral
static uint8_t stickheld = 0;
static int stickYPos = 0, stickYPrevPos = 0;
static uint8_t stickLockoutCounter = 0;
static bool stickLockout = false;

// menu item strings
//static const char* menuItems[MENUITEMS_LEN] = { "Controller Test", "Stick Oscilloscope", "Coordinate Viewer", "2D Plot", "Export Data", "Continuous Waveform" };
static const char* menuItems[MENUITEMS_LEN] = { "Controller Test", "Stick Oscilloscope", "Continuous Stick Oscilloscope", "Trigger Oscilloscope",
                                                "Coordinate Viewer", "2D Plot", "Button Timing Viewer", "Gate Visualizer", "Export Data"};


static bool displayInstructions = false;

static int exportReturnCode = -1;

static uint32_t padsConnected = 0;

// stores the controller origin values
static PADStatus origin[PAD_CHANMAX];
static bool originRead = false;

// buffer for strings with numbers and stuff using sprintf
static char strBuffer[100];

// some consumer crt tvs have alignment issues,
// this determines if certain vertical lines are doubled
static bool setDrawInterlaceMode = false;

static uint8_t thanksPageCounter = 0;

// the "main" for the menus
// other menu functions are called from here
// this also handles moving between menus and exiting
bool menu_runMenu(void *currXfb) {
	if (data == NULL) {
		data = getRecordingData();
	}
	
	if (pressed == NULL) {
		pressed = getButtonsDownPtr();
		held = getButtonsHeldPtr();
	}
	
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
		(*data)->isRecordingReady = false;
		originRead = false;
	}
	
	if ((*data)->isRecordingReady) {
		switch ((*data)->recordingType) {
			case REC_OSCILLOSCOPE:
				setCursorPos(0, 31);
				printStr("Oscilloscope", currXfb);
				break;
			case REC_2DPLOT:
				setCursorPos(0, 36);
				printStr("2D Plot", currXfb);
				break;
			case REC_BUTTONTIME:
				setCursorPos(0, 30);
				printStr("Button Viewer", currXfb);
				break;
			case REC_TRIGGER_L:
			case REC_TRIGGER_R:
				setCursorPos(0, 36);
				printStr("Trigger", currXfb);
				break;
			default:
				setCursorPos(0, 36);
				printStr("Unknown", currXfb);
				break;
		}
		printStr(" Capture in memory!", currXfb);
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
		*pressed = PAD_ButtonsDown(0);
		*held = PAD_ButtonsHeld(0);
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
			menu_oscilloscope(currXfb);
			break;
		case PLOT_2D:
			menu_plot2d(currXfb);
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
			menu_continuousWaveform(currXfb);
			break;
		case TRIGGER_WAVEFORM:
			menu_triggerOscilloscope(currXfb);
			break;
		case THANKS_PAGE:
			menu_thanksPage(currXfb);
			break;
		case GATE_MEASURE:
			if (originRead == false) {
				menu_gateControllerDisconnected();
			}
			menu_gateMeasure(currXfb);
			break;
		case PLOT_BUTTON:
			menu_plotButton(currXfb);
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
	if (*pressed & PAD_BUTTON_START && currentMenu == MAIN_MENU) {
		printStr("Exiting...", currXfb);
		return true;
	}
	
	// controller test lock stuff
	else if (*held == PAD_BUTTON_START && currentMenu == CONTROLLER_TEST && !startHeldAfter) {
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
	// this shouldn't trigger when certain menus are currently recording an input
	else if (*held == PAD_BUTTON_B && currentMenu != MAIN_MENU &&
			!lockExitControllerTest &&
			!menu_plotButtonHasCaptureStarted()) {

		// give user feedback that they are holding the button
		printStr("Moving back to main menu", currXfb);
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
		if (*pressed & PAD_TRIGGER_Z) {
			if (currentMenu == COORD_MAP) {
				displayInstructions = !displayInstructions;
			}
		}
		
		// change bottom message depending on what menu we are in
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
				
				if (startHeldAfter && *held ^ PAD_BUTTON_START) {
					startHeldAfter = false;
				}
				break;
			case PLOT_BUTTON:
				// don't print anything when exiting is disabled
				if (menu_plotButtonHasCaptureStarted()) {
					break;
				}
			default:
				printStr("Hold B to return to main menu.", currXfb);
				break;
		}
		bHeldCounter = 0;
	}

	// default case, tells main.c while loop to continue
	return false;
}

void menu_mainMenu(void *currXfb) {
	stickYPrevPos = stickYPos;
	stickYPos = PAD_StickY(0);
	int stickDiff = abs(stickYPos - stickYPrevPos);
	
	// flags which tell whether the stick is held in an up or down position
	uint8_t up = stickYPos > MENU_STICK_THRESHOLD;
	uint8_t down = stickYPos < -MENU_STICK_THRESHOLD;
	
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
	uint8_t movable = stickheld % 10 == 0 && !stickLockout;
	
	// iterate over the menu items array as defined in menu.c
	for (int i = 0; i < MENUITEMS_LEN; i++) {
		setCursorPos(2 + i, 0);
		// is the item we're about to print the currently selected menu?
		if (mainMenuCursorPos == i) {
			printStr("> ", currXfb);
		} else {
			setCursorPos(2 + i, 2);
		}
		
		bool valid = false;
		// add indicator for menus that can view a given recording type
		switch (i) {
			case ENTRY_OSCILLOSCOPE:
				if (RECORDING_TYPE_VALID_MENUS[(*data)->recordingType] & REC_OSCILLOSCOPE_FLAG) {
					valid = true;
				}
				break;
			case ENTRY_TRIGGER_OSCILLOSCOPE:
				if (RECORDING_TYPE_VALID_MENUS[(*data)->recordingType] & (REC_TRIGGER_L_FLAG | REC_TRIGGER_R_FLAG)) {
					valid = true;
				}
				break;
			case ENTRY_2D_PLOT:
				if (RECORDING_TYPE_VALID_MENUS[(*data)->recordingType] & REC_2DPLOT_FLAG) {
					valid = true;
				}
				break;
			case ENTRY_BUTTON_PLOT:
				if (RECORDING_TYPE_VALID_MENUS[(*data)->recordingType] & REC_BUTTONTIME_FLAG) {
					valid = true;
				}
				break;
			default:
				break;
		}
		if (valid) {
			printStr("* ", currXfb);
		} else {
			setCursorPos(2 + i, 4);
		}
		
		printStr(menuItems[i], currXfb);
		


	}

	// does the user move the cursor?
	if (*pressed & PAD_BUTTON_UP || (up && movable)) {
		if (mainMenuCursorPos > 0) {
			mainMenuCursorPos--;
		} else {
			mainMenuCursorPos = MENUITEMS_LEN - 1;
		}
	} else if (*pressed & PAD_BUTTON_DOWN || (down && movable)) {
		if (mainMenuCursorPos < MENUITEMS_LEN - 1) {
			mainMenuCursorPos++;
		} else {
			mainMenuCursorPos = 0;
		}
	}

	// does the user want to move into another menu?
	// else if to ensure that the A press is separate from any dpad stuff
	// TODO: maybe reorder the enum so that the number and enum match up?
	else if (*pressed & PAD_BUTTON_A) {
		switch (mainMenuCursorPos) {
			case ENTRY_CONT_TEST:
				currentMenu = CONTROLLER_TEST;
				break;
			case ENTRY_OSCILLOSCOPE:
				currentMenu = WAVEFORM;
				break;
			case ENTRY_CONT_OSCILLOSCOPE:
				currentMenu = CONTINUOUS_WAVEFORM;
				break;
			case ENTRY_TRIGGER_OSCILLOSCOPE:
				currentMenu = TRIGGER_WAVEFORM;
				break;
			case ENTRY_COORD_VIEWER:
				currentMenu = COORD_MAP;
				break;
			case ENTRY_2D_PLOT:
				currentMenu = PLOT_2D;
				break;
			case ENTRY_BUTTON_PLOT:
				currentMenu = PLOT_BUTTON;
				break;
			case ENTRY_GATE_VIS:
				currentMenu = GATE_MEASURE;
				break;
			case ENTRY_DATA_EXPORT:
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

// controller test submenu
// basic visual button and stick test
// also shows coordinates (raw and melee converted), and origin values
void menu_controllerTest(void *currXfb) {
	// melee stick coordinates stuff
	// a lot of this comes from github.com/phobgcc/phobconfigtool

	static ControllerSample stickRaw;
	static MeleeCoordinates stickMelee;

	// get raw stick values
	stickRaw.stickX = PAD_StickX(0), stickRaw.stickY = PAD_StickY(0);
	stickRaw.cStickX = PAD_SubStickX(0), stickRaw.cStickY = PAD_SubStickY(0);

	// get converted stick values
	stickMelee = convertStickRawToMelee(stickRaw);
	
	// print raw stick coordinates
	setCursorPos(19, 0);
	sprintf(strBuffer, "Raw XY: (%04d,%04d)", stickRaw.stickX, stickRaw.stickY);
	printStr(strBuffer, currXfb);
	setCursorPos(19, 38);
	sprintf(strBuffer, "C-Raw XY: (%04d,%04d)", stickRaw.cStickX, stickRaw.cStickY);
	printStr(strBuffer, currXfb);
	
	// print melee coordinates
	setCursorPos(20, 0);
	printStr("Melee: (", currXfb);
	// is the value negative?
	if (stickRaw.stickX < 0) {
		printStr("-", currXfb);
	} else {
		printStr("0", currXfb);
	}
	// is this a 1.0 value?
	if (stickMelee.stickXUnit == 10000) {
		printStr("1.0000", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d", stickMelee.stickXUnit);
		printStr(strBuffer, currXfb);
	}
	printStr(",", currXfb);
	
	// is the value negative?
	if (stickRaw.stickY < 0) {
		printStr("-", currXfb);
	} else {
		printStr("0", currXfb);
	}
	// is this a 1.0 value?
	if (stickMelee.stickYUnit == 10000) {
		printStr("1.0000", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d", stickMelee.stickYUnit);
		printStr(strBuffer, currXfb);
	}
	printStr(")", currXfb);
	
	setCursorPos(20, 33);
	sprintf(strBuffer, "C-Melee: (");
	printStr(strBuffer, currXfb);
	// is the value negative?
	if (stickRaw.cStickX < 0) {
		printStr("-", currXfb);
	} else {
		printStr("0", currXfb);
	}
	// is this a 1.0 value?
	if (stickMelee.cStickXUnit == 10000) {
		printStr("1.0000", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d", stickMelee.cStickXUnit);
		printStr(strBuffer, currXfb);
	}
	printStr(",", currXfb);
	
	// is the value negative?
	if (stickRaw.cStickY < 0) {
		printStr("-", currXfb);
	} else {
		printStr("0", currXfb);
	}
	// is this a 1.0 value?
	if (stickMelee.cStickYUnit == 10000) {
		printStr("1.0000", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d", stickMelee.cStickYUnit);
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
		
		if (!(*held & PAD_TRIGGER_L)) {
			setCursorPos(18, 2);
			sprintf(strBuffer, "L Origin: %d", origin[0].triggerL);
			printStr(strBuffer, currXfb);
		}
		if (!(*held & PAD_TRIGGER_R)) {
			setCursorPos(18, 44);
			sprintf(strBuffer, "R Origin: %d", origin[0].triggerR);
			printStr(strBuffer, currXfb);
		}
	}
	

	// visual stuff
	// Buttons

    // A
	if (*held & PAD_BUTTON_A) {
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
	if (*held & PAD_BUTTON_B) {
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
	if (*held & PAD_BUTTON_X) {
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
	if (*held & PAD_BUTTON_Y) {
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
	if (*held & PAD_TRIGGER_Z) {
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
		// stop rumble if z is not *held
		PAD_ControlMotor(0, PAD_MOTOR_STOP);
	}

	// Start
	if (*held & PAD_BUTTON_START) {
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
	if (*held & PAD_BUTTON_UP) {
		DrawFilledBox(CONT_TEST_DPAD_UP_X1, CONT_TEST_DPAD_UP_Y1,
		        CONT_TEST_DPAD_UP_X1 + CONT_TEST_DPAD_SHORT, CONT_TEST_DPAD_UP_Y1 + CONT_TEST_DPAD_LONG,
		        COLOR_WHITE, currXfb);
	} else {
		DrawBox(CONT_TEST_DPAD_UP_X1, CONT_TEST_DPAD_UP_Y1,
		        CONT_TEST_DPAD_UP_X1 + CONT_TEST_DPAD_SHORT, CONT_TEST_DPAD_UP_Y1 + CONT_TEST_DPAD_LONG,
		        COLOR_WHITE, currXfb);
	}
	
	// down
	if (*held & PAD_BUTTON_DOWN) {
		DrawFilledBox(CONT_TEST_DPAD_UP_X1, CONT_TEST_DPAD_DOWN_Y1,
		              CONT_TEST_DPAD_UP_X1 + CONT_TEST_DPAD_SHORT, CONT_TEST_DPAD_DOWN_Y1 + CONT_TEST_DPAD_LONG,
		              COLOR_WHITE, currXfb);
	} else {
		DrawBox(CONT_TEST_DPAD_UP_X1, CONT_TEST_DPAD_DOWN_Y1,
		        CONT_TEST_DPAD_UP_X1 + CONT_TEST_DPAD_SHORT, CONT_TEST_DPAD_DOWN_Y1 + CONT_TEST_DPAD_LONG,
		        COLOR_WHITE, currXfb);
	}
	
	
	//left
	if (*held & PAD_BUTTON_LEFT) {
		DrawFilledBox(CONT_TEST_DPAD_LEFT_X1, CONT_TEST_DPAD_LEFT_Y1,
		              CONT_TEST_DPAD_LEFT_X1 + CONT_TEST_DPAD_LONG, CONT_TEST_DPAD_LEFT_Y1 + CONT_TEST_DPAD_SHORT,
		              COLOR_WHITE, currXfb);
	} else {
		DrawBox(CONT_TEST_DPAD_LEFT_X1, CONT_TEST_DPAD_LEFT_Y1,
		        CONT_TEST_DPAD_LEFT_X1 + CONT_TEST_DPAD_LONG, CONT_TEST_DPAD_LEFT_Y1 + CONT_TEST_DPAD_SHORT,
		        COLOR_WHITE, currXfb);
	}
	
	// right
	if (*held & PAD_BUTTON_RIGHT) {
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
	if (*held & PAD_TRIGGER_L) {
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
	if (*held & PAD_TRIGGER_L) {
		setCursorPos(18, 2);
		printStr("Digital L Pressed", currXfb);
	}
	
	// Analog R Slider
	DrawBox(CONT_TEST_TRIGGER_R_X1, CONT_TEST_TRIGGER_Y1,
	        CONT_TEST_TRIGGER_R_X1 + CONT_TEST_TRIGGER_WIDTH + 1, CONT_TEST_TRIGGER_Y1 + CONT_TEST_TRIGGER_LEN + 1,
	        COLOR_WHITE, currXfb);
	if (*held & PAD_TRIGGER_R) {
		DrawFilledBox(CONT_TEST_TRIGGER_R_X1 + 2, CONT_TEST_TRIGGER_Y1 + 1 + (255 - PAD_TriggerR(0)),
		              CONT_TEST_TRIGGER_R_X1 + CONT_TEST_TRIGGER_WIDTH, CONT_TEST_TRIGGER_Y1 + CONT_TEST_TRIGGER_LEN,
		              COLOR_BLUE, currXfb);
	} else {
		DrawFilledBox(CONT_TEST_TRIGGER_R_X1 + 2, CONT_TEST_TRIGGER_Y1 + 1 + (255 - PAD_TriggerR(0)),
		              CONT_TEST_TRIGGER_R_X1 + CONT_TEST_TRIGGER_WIDTH, CONT_TEST_TRIGGER_Y1 + CONT_TEST_TRIGGER_LEN,
		              COLOR_RED, currXfb);
	}
	
	setCursorPos(17,44);
	sprintf(strBuffer, "Analog R: %d", PAD_TriggerR(0));
	printStr(strBuffer, currXfb);
	if (*held & PAD_TRIGGER_R) {
		setCursorPos(18, 40);
		printStr("Digital R Pressed", currXfb);
	}

	// Analog Stick
	// calculate screen coordinates for stick position drawing
	int xfbCoordX = (stickMelee.stickXUnit / 250);
	if (stickRaw.stickX < 0) {
		xfbCoordX *= -1;
	}
	xfbCoordX += CONT_TEST_STICK_CENTER_X;
	
	int xfbCoordY = (stickMelee.stickYUnit / 250);
	if (stickRaw.stickY > 0) {
		xfbCoordY *= -1;
	}
	xfbCoordY += CONT_TEST_STICK_CENTER_Y;
	
	int xfbCoordCX = (stickMelee.cStickXUnit / 250);
	if (stickRaw.cStickX < 0) {
		xfbCoordCX *= -1;
	}
	xfbCoordCX += CONT_TEST_CSTICK_CENTER_X;
	
	int xfbCoordCY = (stickMelee.cStickYUnit / 250);
	if (stickRaw.cStickY > 0) {
		xfbCoordCY *= -1;
	}
	xfbCoordCY += CONT_TEST_CSTICK_CENTER_Y;
	
	// analog stick
	DrawOctagonalGate(CONT_TEST_STICK_CENTER_X, CONT_TEST_STICK_CENTER_Y, 2, COLOR_GRAY, currXfb); // perimeter
	DrawLine(CONT_TEST_STICK_CENTER_X, CONT_TEST_STICK_CENTER_Y,
	         CONT_TEST_STICK_CENTER_X + (stickRaw.stickX / 2), CONT_TEST_STICK_CENTER_Y - (stickRaw.stickY / 2),
			 COLOR_SILVER, currXfb); // line from center
	for (int i = CONT_TEST_STICK_RAD / 2; i > 0; i -= 5) {
		DrawCircle(CONT_TEST_STICK_CENTER_X + (stickRaw.stickX / 2), CONT_TEST_STICK_CENTER_Y - (stickRaw.stickY / 2), i, COLOR_WHITE, currXfb);
	} // smaller circle
	
	// c-stick
	DrawOctagonalGate(CONT_TEST_CSTICK_CENTER_X, CONT_TEST_CSTICK_CENTER_Y, 2, COLOR_GRAY, currXfb); // perimeter
	DrawLine(CONT_TEST_CSTICK_CENTER_X, CONT_TEST_CSTICK_CENTER_Y,
	         CONT_TEST_CSTICK_CENTER_X + (stickRaw.cStickX / 2), CONT_TEST_CSTICK_CENTER_Y - (stickRaw.cStickY / 2),
	         COLOR_MEDGRAY, currXfb); // line from center
	DrawFilledCircle(CONT_TEST_CSTICK_CENTER_X + (stickRaw.cStickX / 2), CONT_TEST_CSTICK_CENTER_Y - (stickRaw.cStickY / 2),
	                 CONT_TEST_STICK_RAD / 2, COLOR_YELLOW, currXfb); // smaller circle
}

void menu_fileExport(void *currXfb) {
	// make sure data is actually present
	if ((*data)->isRecordingReady) {
		if ((*data)->dataExported) {
			// print status after print
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
			exportReturnCode = exportData();
		}
	} else {
		printStr("No data to export, record an input first.", currXfb);
	}
}

// coordinate viewer submenu
// draws melee coordinates for both sticks on a circle
// "overlays" can be toggled to show specific coordinate groups (shield drop, for example)
void menu_coordinateViewer(void *currXfb) {
	// melee stick coordinates stuff
	// a lot of this comes from github.com/phobgcc/phobconfigtool
	
	printStr("Press Z for instructions", currXfb);
	
	static ControllerSample stickRaw;
	static MeleeCoordinates stickMelee;
	
	// get raw stick values
	stickRaw.stickX = PAD_StickX(0), stickRaw.stickY = PAD_StickY(0);
	stickRaw.cStickX = PAD_SubStickX(0), stickRaw.cStickY = PAD_SubStickY(0);
	
	// get converted stick values
	stickMelee = convertStickRawToMelee(stickRaw);
	
	// print melee coordinates
	setCursorPos(4, 0);
	printStr("Stick X: ", currXfb);
	// is the value negative?
	if (stickRaw.stickX < 0) {
		printStr("-", currXfb);
	}
	// is this a 1.0 value?
	if (stickMelee.stickXUnit == 10000) {
		printStr("1.0\n", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d\n", stickMelee.stickXUnit);
		printStr(strBuffer, currXfb);
	}
	
	// print melee coordinates
	printStr("Stick Y: ", currXfb);
	// is the value negative?
	if (stickRaw.stickY < 0) {
		printStr("-", currXfb);
	}
	// is this a 1.0 value?
	if (stickMelee.stickYUnit == 10000) {
		printStr("1.0\n", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d\n", stickMelee.stickYUnit);
		printStr(strBuffer, currXfb);
	}
	
	// print melee coordinates
	printStr("\nC-Stick X: ", currXfb);
	// is the value negative?
	if (stickRaw.cStickX < 0) {
		printStr("-", currXfb);
	}
	// is this a 1.0 value?
	if (stickMelee.cStickXUnit == 10000) {
		printStr("1.0\n", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d\n", stickMelee.cStickXUnit);
		printStr(strBuffer, currXfb);
	}
	
	// print melee coordinates
	printStr("C-Stick Y: ", currXfb);
	// is the value negative?
	if (stickRaw.cStickY < 0) {
		printStr("-", currXfb);
	}
	// is this a 1.0 value?
	if (stickMelee.cStickYUnit == 10000) {
		printStr("1.0\n", currXfb);
	} else {
		sprintf(strBuffer, "0.%04d\n", stickMelee.cStickYUnit);
		printStr(strBuffer, currXfb);
	}
	
	setCursorPos(19, 0);
	printStr("Stickmap: ", currXfb);
	int stickmapRetVal = isCoordValid(selectedStickmap, stickMelee);
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
	int xfbCoordX = (stickMelee.stickXUnit / 125) * 2;
	if (stickRaw.stickX < 0) {
		xfbCoordX *= -1;
	}
	xfbCoordX += COORD_CIRCLE_CENTER_X;
	
	int xfbCoordY = (stickMelee.stickYUnit / 125) * 2;
	if (stickRaw.stickY > 0) {
		xfbCoordY *= -1;
	}
	xfbCoordY += SCREEN_POS_CENTER_Y;
	
	int xfbCoordCX = (stickMelee.cStickXUnit / 125) * 2;
	if (stickRaw.cStickX < 0) {
		xfbCoordCX *= -1;
	}
	xfbCoordCX += COORD_CIRCLE_CENTER_X;
	
	int xfbCoordCY = (stickMelee.cStickYUnit / 125) * 2;
	if (stickRaw.cStickY > 0) {
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
	
	if (*pressed & PAD_BUTTON_X) {
		selectedStickmap++;
		selectedStickmapSub = 0;
		if (selectedStickmap == 3) {
			selectedStickmap = 0;
		}
	}
	if (*pressed & PAD_BUTTON_Y) {
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

// self-explanatory
void menu_thanksPage(void *currXfb) {
	printStr("Thanks to:\n"
			 "PhobGCC team and Discord\n"
			 "DevkitPro team\n"
			 "Extrems\n"
			 "SmashScope\n"
			 "Z. B. Wells", currXfb);
	
	setCursorPos(18,0);
	printStr("Compiled with: ", currXfb);
	printStr(_V_STRING, currXfb);
	printStr("\nBuild Date: ", currXfb);
	printStr(BUILD_DATE, currXfb);
	printStr("\nGTS Commit ID: ", currXfb);
	printStr(COMMIT_ID, currXfb);

}
