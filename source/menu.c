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
#include "submenu/controllertest.h"

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

// some consumer crt tvs have alignment issues,
// this determines if certain vertical lines are doubled
static bool setDrawInterlaceMode = false;

static uint8_t thanksPageCounter = 0;

// the "main" for the menus
// other menu functions are called from here
// this also handles moving between menus and exiting
bool menu_runMenu() {
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
	resetCursor();
	// read inputs get origin status
	// calls PAD_ScanPads() and PAD_GetOrigin()
	attemptReadOrigin();
	
	printStr("GCC Test Suite");
	
	// check if port 1 is disconnected
	if (!isControllerConnected(CONT_PORT_1)) {
		setCursorPos(0, 38);
		printStr("Controller Disconnected!");
		(*data)->isRecordingReady = false;
	}
	
	if ((*data)->isRecordingReady) {
		switch ((*data)->recordingType) {
			case REC_OSCILLOSCOPE:
				setCursorPos(0, 31);
				printStr("Oscilloscope");
				break;
			case REC_2DPLOT:
				setCursorPos(0, 36);
				printStr("2D Plot");
				break;
			case REC_BUTTONTIME:
				setCursorPos(0, 30);
				printStr("Button Viewer");
				break;
			case REC_TRIGGER_L:
			case REC_TRIGGER_R:
				setCursorPos(0, 36);
				printStr("Trigger");
				break;
			default:
				setCursorPos(0, 36);
				printStr("Unknown");
				break;
		}
		printStr(" Capture in memory!");
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
			menu_mainMenu();
			break;
		case CONTROLLER_TEST:
			menu_controllerTest();
			break;
		case WAVEFORM:
			menu_oscilloscope();
			break;
		case PLOT_2D:
			menu_plot2d();
			break;
		case FILE_EXPORT:
			menu_fileExport();
			break;
		case COORD_MAP:
			if (displayInstructions) {
				printStr("Press X to cycle the stickmap being tested, and Y to cycle\nwhich "
					   "category of points.\nMelee Coordinates are shown in thetop-left.\n\n"
					   "The white line represents the analog stick.\n"
					   "The yellow line represents the c-stick.\n\n"
					   "Current Stickmap: ");
				switch (selectedStickmap) {
					case FF_WD:
						printStr("Firefox / Wavedash\n");
						printStr(STICKMAP_FF_WD_DESC);
						break;
					case SHIELDDROP:
						printStr("Shield Drop\n");
						printStr(STICKMAP_SHIELDDROP_DESC);
						break;
					case NONE:
					default:
						printStr("None\n");
						break;
				}
			} else {
				menu_coordinateViewer();
			}
			break;
		case CONTINUOUS_WAVEFORM:
			menu_continuousWaveform();
			break;
		case TRIGGER_WAVEFORM:
			menu_triggerOscilloscope();
			break;
		case THANKS_PAGE:
			menu_thanksPage();
			break;
		case GATE_MEASURE:
			menu_gateMeasure();
			break;
		case PLOT_BUTTON:
			menu_plotButton();
			break;
		default:
			printStr("HOW DID WE END UP HERE?\n");
			break;
	}
	if (displayInstructions) {
		setCursorPos(21, 0);
		printStr("Press Z to close instructions.");
	}

	// move cursor to bottom left
	setCursorPos(22, 0);

	// exit the program if start is pressed
	if (*pressed & PAD_BUTTON_START && currentMenu == MAIN_MENU) {
		printStr("Exiting...");
		return true;
	}
	
	// controller test lock stuff
	else if (*held == PAD_BUTTON_START && currentMenu == CONTROLLER_TEST && !startHeldAfter) {
		if (lockExitControllerTest) {
			printStr("Enabling exit, hold for 2 seconds");
		} else {
			printStr("Disabling exit, hold for 2 seconds");
		}
		printEllipse(startHeldCounter, 40);
		
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
		printStr("Moving back to main menu");
		printEllipse(bHeldCounter, 15);
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
				case CONTROLLER_TEST:
					menu_controllerTestEnd();
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
				printStr("Press Start to exit.");
				int col = 55 - (sizeof(VERSION_NUMBER));
				if (col > 25) {
					setCursorPos(22, col);
					printStr("Ver: ");
					printStr(VERSION_NUMBER);
				}
				break;
			case CONTROLLER_TEST:
				if (lockExitControllerTest) {
					printStr("Exiting disabled, hold Start to re-enable.");
				} else {
					printStr("Hold B to return to main menu, hold start to disable.");
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
				printStr("Hold B to return to main menu.");
				break;
		}
		bHeldCounter = 0;
	}

	// default case, tells main.c while loop to continue
	return false;
}

void menu_mainMenu() {
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
			printStr("> ");
		} else {
			setCursorPos(2 + i, 2);
		}
		
		bool valid = false;
		// add indicator for menus that can view a given recording type
		if ((*data)->isRecordingReady) {
			switch (i) {
				case ENTRY_OSCILLOSCOPE:
					if (RECORDING_TYPE_VALID_MENUS[(*data)->recordingType] & REC_OSCILLOSCOPE_FLAG) {
						valid = true;
					}
					break;
				case ENTRY_TRIGGER_OSCILLOSCOPE:
					if (RECORDING_TYPE_VALID_MENUS[(*data)->recordingType] &
					    (REC_TRIGGER_L_FLAG | REC_TRIGGER_R_FLAG)) {
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
		}
		if (valid) {
			printStr("* ");
		} else {
			setCursorPos(2 + i, 4);
		}
		
		printStr(menuItems[i]);
		


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

void menu_fileExport() {
	// make sure data is actually present
	if ((*data)->isRecordingReady) {
		if ((*data)->dataExported) {
			// print status after print
			switch (exportReturnCode) {
				case 0:
					printStr("File exported successfully.");
					break;
				case 1:
					printStr("Data was marked as not ready, this shouldn't happen!");
					break;
				case 2:
					printStr("Failed to init filesystem.");
					break;
				case 3:
					printStr("Failed to create parent directory.");
					break;
				case 4:
					printStr("Failed to create file, file already exists!");
					break;
				default:
					printStr("How did we get here?");
					break;
			}
		} else {
			printStr("Attempting to export data...");
			exportReturnCode = exportData();
		}
	} else {
		printStr("No data to export, record an input first.");
	}
}

// coordinate viewer submenu
// draws melee coordinates for both sticks on a circle
// "overlays" can be toggled to show specific coordinate groups (shield drop, for example)
void menu_coordinateViewer() {
	// melee stick coordinates stuff
	// a lot of this comes from github.com/phobgcc/phobconfigtool
	
	printStr("Press Z for instructions");
	
	static ControllerSample stickRaw;
	static MeleeCoordinates stickMelee;
	
	// get raw stick values
	stickRaw.stickX = PAD_StickX(0), stickRaw.stickY = PAD_StickY(0);
	stickRaw.cStickX = PAD_SubStickX(0), stickRaw.cStickY = PAD_SubStickY(0);
	
	// get converted stick values
	stickMelee = convertStickRawToMelee(stickRaw);
	
	// print melee coordinates
	setCursorPos(4, 0);
	printStr("Stick X: ");
	// is the value negative?
	if (stickRaw.stickX < 0) {
		printStr("-");
	}
	// is this a 1.0 value?
	if (stickMelee.stickXUnit == 10000) {
		printStr("1.0\n");
	} else {
		printStr("0.%04d\n", stickMelee.stickXUnit);
	}
	
	// print melee coordinates
	printStr("Stick Y: ");
	// is the value negative?
	if (stickRaw.stickY < 0) {
		printStr("-");
	}
	// is this a 1.0 value?
	if (stickMelee.stickYUnit == 10000) {
		printStr("1.0\n");
	} else {
		printStr("0.%04d\n", stickMelee.stickYUnit);
	}
	
	// print melee coordinates
	printStr("\nC-Stick X: ");
	// is the value negative?
	if (stickRaw.cStickX < 0) {
		printStr("-");
	}
	// is this a 1.0 value?
	if (stickMelee.cStickXUnit == 10000) {
		printStr("1.0\n");
	} else {
		printStr("0.%04d\n", stickMelee.cStickXUnit);
	}
	
	// print melee coordinates
	printStr("C-Stick Y: ");
	// is the value negative?
	if (stickRaw.cStickY < 0) {
		printStr("-");
	}
	// is this a 1.0 value?
	if (stickMelee.cStickYUnit == 10000) {
		printStr("1.0\n");
	} else {
		printStr("0.%04d\n", stickMelee.cStickYUnit);
	}
	
	setCursorPos(19, 0);
	printStr("Stickmap: ");
	int stickmapRetVal = isCoordValid(selectedStickmap, stickMelee);
	switch (selectedStickmap) {
		case FF_WD:
			printStr("Firefox/Wavedash\n");
			printStr("Visible: ");
			if (selectedStickmapSub == 0) {
				printStr("ALL");
			} else {
				printStrColor(STICKMAP_FF_WD_RETCOLORS[selectedStickmapSub][0], STICKMAP_FF_WD_RETCOLORS[selectedStickmapSub][1],
							  STICKMAP_FF_WD_RETVALS[selectedStickmapSub]);
			}
			printStr("\nResult: ");
			printStrColor(STICKMAP_FF_WD_RETCOLORS[stickmapRetVal][0], STICKMAP_FF_WD_RETCOLORS[stickmapRetVal][1],
						  STICKMAP_FF_WD_RETVALS[stickmapRetVal]);
			break;
		case SHIELDDROP:
			printStr("Shield Drop\n");
			printStr("Visible: ");
			if (selectedStickmapSub == 0) {
				printStr("ALL");
			} else {
				printStrColor(STICKMAP_SHIELDDROP_RETCOLORS[selectedStickmapSub][0], STICKMAP_SHIELDDROP_RETCOLORS[selectedStickmapSub][1],
							  STICKMAP_SHIELDDROP_RETVALS[selectedStickmapSub]);
			}
			printStr("\nResult: ");
			printStrColor(STICKMAP_SHIELDDROP_RETCOLORS[stickmapRetVal][0], STICKMAP_SHIELDDROP_RETCOLORS[stickmapRetVal][1],
						  STICKMAP_SHIELDDROP_RETVALS[stickmapRetVal]);
			break;
		case NONE:
		default:
			printStr("NONE");
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
	DrawCircle(COORD_CIRCLE_CENTER_X, SCREEN_POS_CENTER_Y, 160, COLOR_MEDGRAY);;
	
	DrawStickmapOverlay(selectedStickmap, selectedStickmapSub);;

	// draw analog stick line
	DrawLine(COORD_CIRCLE_CENTER_X, SCREEN_POS_CENTER_Y, xfbCoordX, xfbCoordY, COLOR_WHITE);;
	DrawBox(xfbCoordX - 4, xfbCoordY - 4, xfbCoordX + 4, xfbCoordY + 4, COLOR_WHITE);;
	
	// draw c-stick line
	DrawLine(COORD_CIRCLE_CENTER_X, SCREEN_POS_CENTER_Y, xfbCoordCX, xfbCoordCY, COLOR_YELLOW);;
	DrawFilledBox(xfbCoordCX - 2, xfbCoordCY - 2, xfbCoordCX + 2, xfbCoordCY + 2, COLOR_YELLOW);;
	
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
void menu_thanksPage() {
	printStr("Thanks to:\n"
			 "PhobGCC team and Discord\n"
			 "DevkitPro team\n"
			 "Extrems\n"
			 "SmashScope\n"
			 "Z. B. Wells");
	
	setCursorPos(18,0);
	printStr("Compiled with: ");
	printStr(_V_STRING);
	printStr("\nBuild Date: ");
	printStr(BUILD_DATE);
	printStr("\nGTS Commit ID: ");
	printStr(COMMIT_ID);

}
