//
// Created on 2023/10/25.
//

#include "menu.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <ogc/pad.h>
#include <ogc/video.h>
#include <ogc/libversion.h>

#include "waveform.h"
#include "util/gx.h"
#include "util/print.h"
#include "util/polling.h"

// TODO: these should go away once all menus have been moved to a separate file
#include "util/file.h"

#ifndef NO_DATE_CHECK
#include "util/datetime.h"
static bool dateChecked = false;
static enum DATE_CHECK_LIST date;
#endif

// should I have a "parent" header that includes these?
#include "submenu/oscilloscope.h"
#include "submenu/continuous.h"
#include "submenu/trigger.h"
#include "submenu/plot2d.h"
#include "submenu/gate.h"
#include "submenu/plotbutton.h"
#include "submenu/controllertest.h"
#include "submenu/coordinateviewer.h"

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

// enum to keep track of what menu to display, and what logic to run
static enum CURRENT_MENU currentMenu = MAIN_MENU;

// lock var for controller test
static bool lockExitEnabled = false;
static bool startHeldAfter = false;
static uint8_t startHeldCounter = 0;

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
static const char* menuItems[MENUITEMS_LEN] = { "Controller Test", "Stick Oscilloscope", "Continuous Stick Oscilloscope", "Trigger Oscilloscope",
                                                "Coordinate Viewer", "2D Plot", "Button Timing Viewer", "Gate Visualizer", "Export Data"};

static bool displayInstructions = false;

static int exportReturnCode = -1;

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

	resetCursor();
	// read inputs get origin status
	// calls PAD_ScanPads() and PAD_GetOrigin()
	attemptReadOrigin();
	
	#ifndef NO_DATE_CHECK
	if (!dateChecked) {
		date = checkDate();
		dateChecked = true;
	}
	
	// in case a future check doesn't want to print the text normally...
	switch (date) {
		case DATE_NICE:
		case DATE_CMAS:
			drawDateSpecial(date);
			printStrColor(GX_COLOR_NONE, GX_COLOR_WHITE, "GCC Test Suite");
			break;
		case DATE_PM:
			drawDateSpecial(date);
		case DATE_NONE:
		default:
			printStrColor(GX_COLOR_BLACK, GX_COLOR_WHITE, "GCC Test Suite");
			break;
	}
	#else
	printStr("GCC Test Suite");
	#endif
	
	// check if port 1 is disconnected
	if (!isControllerConnected(CONT_PORT_1)) {
		setCursorPos(0, 38);
		printStr("Controller Disconnected!");
		(*data)->isRecordingReady = false;
	}
	
	// TODO: is there a better way to have an indicator but also have more screen space?
	if ((*data)->isRecordingReady && currentMenu == MAIN_MENU) {
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
	} else if (!(*data)->isRecordingReady) {
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
			menu_coordViewSetLockState(lockExitEnabled);
			menu_coordView();
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
			printStr("currentMenu is invalid value, how did this happen?\n");
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
	else if (*held == PAD_BUTTON_START && (currentMenu == CONTROLLER_TEST || currentMenu == COORD_MAP) && !startHeldAfter) {
		if (lockExitEnabled) {
			printStr("Enabling exit, hold for 2 seconds");
		} else {
			printStr("Disabling exit, hold for 2 seconds");
		}
		printEllipse(startHeldCounter, 40);
		
		startHeldCounter++;
		if (startHeldCounter > 121) {
			lockExitEnabled = !lockExitEnabled;
			startHeldCounter = 0;
			startHeldAfter = true;
		}
	}

	// does the user want to move back to the main menu?
	// this shouldn't trigger when certain menus are currently recording an input
	else if (*held == PAD_BUTTON_B && currentMenu != MAIN_MENU &&
			!lockExitEnabled &&
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
				case COORD_MAP:
					menu_coordViewEnd();
					break;
				case THANKS_PAGE:
				default:
					break;
			}
			currentMenu = MAIN_MENU;
			displayInstructions = false;
			bHeldCounter = 0;
			// stop rumble if it didn't get stopped before
			PAD_ControlMotor(0, PAD_MOTOR_STOP);
			PAD_ControlMotor(3, PAD_MOTOR_STOP);
		}

	} else {
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
			case COORD_MAP:
			case CONTROLLER_TEST:
				if (lockExitEnabled) {
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

static int colSelection = 0;
static const int colNum = 5;
static const int colHeight = 24;
static const uint8_t colSet[][10] = {
		{ 0x5b, 0xce, 0xfa },
		{ 0xf5, 0xa9, 0xb8 },
		{ 0xff, 0xff, 0xff },
		{ 0xf5, 0xa9, 0xb8 },
		{ 0x5b, 0xce, 0xfa },
		
		{ 0xe2, 0x8c, 0x00 },
		{ 0xec, 0xcd, 0x00 },
		{ 0xff, 0xff, 0xff },
		{ 0x62, 0xae, 0xdc },
		{ 0x20, 0x38, 0x56 }
};

static void menu_mainMenuDraw() {
	const int startX = 400, startY = 80;
	
	updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
	
	// texture
	if (colSelection == 0) {
		// background quad
		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		
		GX_Position3s16(startX - 3, startY - 6, -2);
		GX_Color3u8(0xff, 0xff, 0xff);
		
		GX_Position3s16(startX + 203, startY - 6, -2);
		GX_Color3u8(0xff, 0xff, 0xff);
		
		GX_Position3s16(startX + 203, startY + 126, -2);
		GX_Color3u8(0xff, 0xff, 0xff);
		
		GX_Position3s16(startX - 3, startY + 126, -2);
		GX_Color3u8(0xff, 0xff, 0xff);
		
		GX_End();
		
		updateVtxDesc(VTX_TEX_NOCOLOR, GX_MODULATE);
		changeLoadedTexmap(TEXMAP_P);
		
		int width, height;
		getCurrentTexmapDims(&width, &height);
		
		int heightOffset = (height - 120) / 2;
		
		GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
		
		GX_Position3s16(startX, startY - heightOffset, -1);
		GX_TexCoord2s16(0, 0);
		
		GX_Position3s16(startX + width, startY - heightOffset, -1);
		GX_TexCoord2s16(width, 0);
		
		GX_Position3s16(startX + width, (startY - heightOffset) + height, -1);
		GX_TexCoord2s16(width, height);
		
		GX_Position3s16(startX, (startY - heightOffset) + height, -1);
		GX_TexCoord2s16(0, height);
		
		GX_End();
	}
	// raw quads
	else {
		// background quad
		GX_Begin(GX_QUADS, GX_VTXFMT0, 24);
		
		GX_Position3s16(startX - 2, startY - 2, -2);
		GX_Color3u8(0xff, 0xff, 0xff);
		
		GX_Position3s16(startX + 202, startY - 2, -2);
		GX_Color3u8(0xff, 0xff, 0xff);
		
		GX_Position3s16(startX + 202, startY + 122, -2);
		GX_Color3u8(0xff, 0xff, 0xff);
		
		GX_Position3s16(startX - 2, startY + 122, -2);
		GX_Color3u8(0xff, 0xff, 0xff);
		
		int indexStart = (colSelection - 1) * 5;
		
		for (int i = 0; i < colNum; i++) {
			// colSelection - 1 because 1 = index 0
			GX_Position3s16(startX, startY + (colHeight * i), -1);
			GX_Color3u8(colSet[indexStart + i][0], colSet[indexStart + i][1], colSet[indexStart + i][2]);
			
			GX_Position3s16(startX + 200, startY + (colHeight * i), -1);
			GX_Color3u8(colSet[indexStart + i][0], colSet[indexStart + i][1], colSet[indexStart + i][2]);
			
			GX_Position3s16(startX + 200, startY + (colHeight * (i + 1)), -1);
			GX_Color3u8(colSet[indexStart + i][0], colSet[indexStart + i][1], colSet[indexStart + i][2]);
			
			GX_Position3s16(startX, startY + (colHeight * (i + 1)), -1);
			GX_Color3u8(colSet[indexStart + i][0], colSet[indexStart + i][1], colSet[indexStart + i][2]);
		}
		
		GX_End();
		
		if (colSelection == 1) {
			// this is in reference to a close friend of mine, not me
			setCursorPos(10, 45);
			printStr("G + A <3");
		}
	}
	
	// cycle through options
	if (PAD_ButtonsDown(1) == PAD_BUTTON_A) {
		colSelection++;
		colSelection %= 3;
	}
}

static bool mainMenuDraw = false;

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
	
	if (mainMenuDraw) {
		menu_mainMenuDraw();
	}
	
	if (PAD_ButtonsDown(1) == PAD_TRIGGER_Z) {
		mainMenuDraw = !mainMenuDraw;
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

// self-explanatory
void menu_thanksPage() {
	printStr("Thanks to:\n"
			 "PhobGCC team and Discord\n"
			 "DevkitPro team\n"
			 "Extrems\n"
			 "SmashScope\n"
			 "bkacjios / m-overlay\n"
			 "Z. B. Wells");
	
	setCursorPos(17,0);
	#ifdef HW_RVL
	printStr("Hardware: Wii");
	#elifdef HW_DOL
	printStr("Hardware: GameCube");
	#endif
	printStr("\nCompiled with: ");
	printStr(_V_STRING);
	printStr("\nBuild Date: ");
	printStr(BUILD_DATE);
	printStr("\nGTS Commit ID: ");
	printStr(COMMIT_ID);
}

