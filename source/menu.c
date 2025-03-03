//
// Created on 10/25/23.
//

#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ogc/color.h>
#include "waveform.h"
#include "images/stickmaps.h"
#include "images/custom_colors.h"
#include "draw_constants.h"
#include "export.h"
#include "stickmap_coordinates.h"


#define MENUITEMS_LEN 5
#define TEST_LEN 5

// 500 values displayed at once, SCREEN_POS_CENTER_X +/- 250
#define SCREEN_TIMEPLOT_START 70

// macro for how far the stick has to go before it counts as a movement
#define MENU_STICK_THRESHOLD 10

const float frameTime = (1000.0 / 60.0);

enum WAVEFORM_TEST { SNAPBACK, PIVOT, DASHBACK, FULL, NO_TEST };

static enum WAVEFORM_TEST currentTest = SNAPBACK;

// enum to keep track of what menu to display, and what logic to run
static enum CURRENT_MENU currentMenu = MAIN_MENU;

// enum for previous menu, we move to it when we're waiting for user input
static enum CURRENT_MENU previousMenu = MAIN_MENU;

// enum for what image to draw in 2d plot
static enum IMAGE selectedImage = SNAPBACK;

static enum STICKMAP_LIST stickmapList = NONE;

// main menu counter
static u8 mainMenuSelection = 0;

// counter for how many frames b or start have been held
static u8 bHeldCounter = 0;

// data for drawing a waveform
static WaveformData data = { { 0 }, 0, 500, false, false };

// vars for what buttons are pressed or held
static u32 pressed = 0;
static u32 held = 0;

// var for counting how long the stick has been held away from neutral
static u8 stickheld = 0;

// menu item strings
static const char* menuItems[MENUITEMS_LEN] = { "Controller Test", "Stick Oscilloscope", "Coordinate Viewer", "2D Plot", "Export Data" };

static bool displayedWaitingInputMessage = false;

static bool displayInstructions = false;
static bool fileIOSuccess = false;

static int lastDrawPoint = -1;
static int dataScrollOffset = 0;
static int waveformScaleFactor = 1;

static u32 padsConnected = 0;

static PADStatus origin[PAD_CHANMAX];
static bool originRead = false;

// most of this is taken from
// https://github.com/PhobGCC/PhobGCC-SW/blob/main/PhobGCC/rp2040/src/drawImage.cpp
// TODO: move this to another file, this file is getting big enough...
// TODO: this seems to use a lot of cycles, not sure if it can be optimized...
static void drawImage(void *currXfb, const unsigned char image[], const unsigned char colorIndex[8], u16 offsetX, u16 offsetY) {
	// get information on the image to be drawn
	u32 width = image[0] << 8 | image[1];
	u32 height = image[2] << 8 | image[3];

	// where image drawing ends
	// calculated in advance for use in the loop
	u32 imageEndpointX = offsetX + width;
	u32 imageEndpointY = offsetY + height;

	// ensure image won't go out of bounds
	if (imageEndpointX > 640 || imageEndpointY > 480) {
		return;
		//printf("Image with given parameters will write incorrectly\n");
	}

	u32 byte = 4;
	u8 runIndex = 0;
	// first five bits are runlength
	u8 runLength = (image[byte] >> 3) + 1;
	// last three bits are color, lookup color in index
	u8 color = colorIndex[ image[byte] & 0b111];
	// begin processing data
	for (int row = offsetY; row < imageEndpointY; row++) {
		for (int column = offsetX; column < imageEndpointX; column++) {
			// is there a pixel to actually draw? (0-4 is transparency)
			if (color >= 5) {
				DrawDot(column, row, CUSTOM_COLORS[color - 5], currXfb);
			}

			runIndex++;
			if (runIndex >= runLength) {
				runIndex = 0;
				byte++;
				runLength = (image[byte] >> 3) + 1;
				color = colorIndex[ image[byte] & 0b111];
			}
		}
	}
}

// the "main" for the menus
// other menu functions are called from here
// this also handles moving between menus and exiting
bool menu_runMenu(void *currXfb) {
	// read inputs
	padsConnected = PAD_ScanPads();
	
	// read origin if a controller is connected and we don't have the origin
	if (!originRead && (padsConnected & 1) == 1 ) {
		PAD_GetOrigin(origin);
		originRead = true;
	}

	// reset console cursor position
	printf("\x1b[3;0H");
	printf("FossScope (Working Title)");
	if ((padsConnected & 1) == 0) {
		printf("                             Controller Disconnected!");
		data.isDataReady = false;
		originRead = false;
	}
	if (data.isDataReady) {
		printf("                      Oscilloscope capture in memory!");
	}
	printf("\n\n");
	
	
	// check for any buttons pressed/held
	pressed = PAD_ButtonsDown(0);
	held = PAD_ButtonsHeld(0);

	// determine what menu we are in
	switch (currentMenu) {
		case MAIN_MENU:
			menu_mainMenu();
			break;
		case CONTROLLER_TEST:
			menu_controllerTest(currXfb);
			break;
		case WAVEFORM:
			if (displayInstructions) {
				printf("Press X to cycle the current test, results will show above the waveform.\n"
					   "Use DPAD left/right to scroll waveform when it is larger than the\n"
					   "displayed area, hold R to move faster.\n\n");
				printf("CURRENT TEST: ");
				switch (currentTest) {
					case SNAPBACK:
						printf("SNAPBACK\nCheck the min/max value on a given axis depending on where your\n"
						        "stick started. If you moved the stick left, check the Max value on a given\n"
						        "axis. Snapback can occur when the max value is at or above 23. If right,\n"
						        "then at or below -23.");
						break;
					case PIVOT:
						printf("PIVOT\nFor a successful pivot, you want the stick's position to stay\n"
						       "above/below +64/-64 for ~16.6ms (1 frame). Less, and you might get nothing,\n"
						       "more, and you might get a dashback. You also need the stick to hit 80/-80 on\n"
							   "both sides. Check the PhobVision docs for more info.");
						break;
					case DASHBACK:
						printf("DASHBACK\nA (vanilla) dashback will be successful when the stick doesn't get\n"
						"polled between 23 and 64, or -23 and -64. Less time in this range is better.");
						break;
					case FULL:
						printf("FULL MEASURE\nNot an actual test.\nWill fill the input buffer always,"
							   " useful for longer inputs.");
						break;
					default:
						printf("NO TEST SELECTED");
						break;
				}
			} else {
				menu_waveformMeasure(currXfb);
			}
			break;
		case PLOT_2D:
			if (displayInstructions) {
				printf("Press X to cycle the stickmap background. Use DPAD left/right to change\n"
					   "what the last point drawn is. Information on the last chosen point is\n"
					   "displayed at the bottom. Hold R to add or remove points faster.\n"
					   "Hold L to move one point at a time.");
			} else {
				menu_2dPlot(currXfb);
			}
			break;
		case FILE_EXPORT:
			menu_fileExport();
			break;
		case WAITING_MEASURE:
			menu_waitingMeasure();
			break;
		case COORD_MAP:
			if (displayInstructions) {
				printf("Press X to cycle the stickmap being tested, Melee Coordinates are shown\n"
					   "in the top-left.\nThe white line with an unfilled box represents the analog stick.\n"
					   "The yellow line with a filled box represents the c-stick.\n\n"
					   "Current Stickmap: ");
				switch (stickmapList) {
					case FF_WD:
						printf("Firefox / Wavedash\n");
						printf("%s", STICKMAP_FF_WD_DESC);
						break;
					case SHIELDDROP:
						printf("Shield Drop\n");
						printf("%s", STICKMAP_SHIELDDROP_DESC);
						break;
					case NONE:
					default:
						printf("None");
						break;
				}
			} else {
				menu_coordinateViewer(currXfb);
			}
			break;
		default:
			printf("HOW DID WE END UP HERE?\n");
			break;
	}

	// move cursor to bottom left
	printf( "\x1b[27;0H");

	// exit the program if start is pressed
	if (pressed & PAD_BUTTON_START && currentMenu == MAIN_MENU) {
		printf("Exiting...");
		return true;
	}

	// does the user want to move back to the main menu?
	else if (held & PAD_BUTTON_B && currentMenu != MAIN_MENU) {
		bHeldCounter++;

		// give user feedback that they are holding the button
		printf("Moving back to main menu");

		// TODO: I know there's a better way to do this but I can't think of it right now...
		if (bHeldCounter > 15) {
			printf(".");
		}
		if (bHeldCounter > 30) {
			printf(".");
		}
		if (bHeldCounter > 45) {
			printf(".");
		}

		// has the button been held long enough?
		if (bHeldCounter > 46) {
			currentMenu = MAIN_MENU;
			displayInstructions = false;
			bHeldCounter = 0;
		}

	} else {
		// does the user want to display instructions?
		if (pressed & PAD_TRIGGER_Z) {
			if (currentMenu == WAVEFORM || currentMenu == PLOT_2D || currentMenu == COORD_MAP) {
				displayInstructions = !displayInstructions;
			}
		}
		if (currentMenu != MAIN_MENU) {
			printf("Hold B to return to main menu.");
		} else {
			printf("Press Start to exit.");
		}
		bHeldCounter = 0;
	}

	return false;
}

void menu_mainMenu() {
	// stop rumble if it didn't get stopped before
	PAD_ControlMotor(0, PAD_MOTOR_STOP);
	int stickY = PAD_StickY(0);

	// flags which tell whether the stick is held in an up or down position
	u8 up = stickY > MENU_STICK_THRESHOLD;
	u8 down = stickY < -MENU_STICK_THRESHOLD;

	// only move the stick if it wasn't already held for the last 10 ticks
	u8 movable = stickheld % 10 == 0;
	
	// iterate over the menu items array as defined in menu.c
	for (int i = 0; i < MENUITEMS_LEN; i++) {
		// is the item we're about to print the currently selected menu?
		if (mainMenuSelection == i) {
			printf(" > ");
		} else {
			printf("   ");
		}

		//iterate over an individual string from the array
		const int len = strlen(menuItems[i]);
		for (int j = 0; j < len; j++) {
			printf("%c", menuItems[i][j]);
		}
		printf("\n");
	}

	// does the user move the cursor?
	if (pressed & PAD_BUTTON_UP || (up && movable)) {
		if (mainMenuSelection > 0) {
			mainMenuSelection--;
		}
	} else if (pressed & PAD_BUTTON_DOWN || (down && movable)) {
		if (mainMenuSelection < MENUITEMS_LEN - 1) {
			mainMenuSelection++;
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
				currentMenu = COORD_MAP;
				break;
			case 3:
				currentMenu = PLOT_2D;
				break;
			case 4:
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
	printf("\x1b[24;0H");
	printf("Stick Raw (X,Y): (%04d,%04d)", stickCoordinatesRaw.ax, stickCoordinatesRaw.ay);
	printf("\x1b[24;40H");
	printf("C-Stick Raw (X,Y): (%04d,%04d)", stickCoordinatesRaw.cx, stickCoordinatesRaw.cy);
	
	// print melee coordinates
	printf("\x1b[25;0H");
	printf("Stick Melee (X,Y): (");
	// is the value negative?
	if (stickCoordinatesRaw.ax < 0) {
		printf("-");
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.ax == 10000) {
		printf("1.0");
	} else {
		printf("0.%04d", stickCoordinatesMelee.ax);
	}
	printf(",");
	
	// is the value negative?
	if (stickCoordinatesRaw.ay < 0) {
		printf("-");
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.ay == 10000) {
		printf("1.0");
	} else {
		printf("0.%04d", stickCoordinatesMelee.ay);
	}
	printf(")");
	
	printf("\x1b[25;40H");
	printf("C-Stick Melee (X,Y): (");
	// is the value negative?
	if (stickCoordinatesRaw.cx < 0) {
		printf("-");
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.cx == 10000) {
		printf("1.0");
	} else {
		printf("0.%04d", stickCoordinatesMelee.cx);
	}
	printf(",");
	// is the value negative?
	if (stickCoordinatesRaw.cy < 0) {
		printf("-");
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.cy == 10000) {
		printf("1.0");
	} else {
		printf("0.%04d", stickCoordinatesMelee.cy);
	}
	printf(")");
	
	if (originRead) {
		printf("\x1b[26;0H");
		printf("Stick Origin (X,Y): %04d, %04d", origin[0].stickX, origin[0].stickY);
		printf("\x1b[26;40H");
		printf("C-Stick Origin (X,Y): %04d, %04d", origin[0].substickX, origin[0].substickY);
	}
	

	// visual stuff
	// Buttons

    // A
	if (held & PAD_BUTTON_A) {
		printf("\x1b[30;0m\x1b[47;1m");
		DrawFilledBox(CONT_TEST_BUTTON_A_X1, CONT_TEST_BUTTON_A_Y1,
					  CONT_TEST_BUTTON_A_X1 + CONT_TEST_BUTTON_A_SIZE, CONT_TEST_BUTTON_A_Y1 + CONT_TEST_BUTTON_A_SIZE,
					  COLOR_WHITE, currXfb);
	} else {
		DrawBox(CONT_TEST_BUTTON_A_X1, CONT_TEST_BUTTON_A_Y1,
		        CONT_TEST_BUTTON_A_X1 + CONT_TEST_BUTTON_A_SIZE, CONT_TEST_BUTTON_A_Y1 + CONT_TEST_BUTTON_A_SIZE,
		        COLOR_WHITE, currXfb);
    }
	printf("\x1b[10;54H");
	printf("A");
	printf("\x1b[37;1m\x1b[40;0m");
	
    // B
	if (held & PAD_BUTTON_B) {
		printf("\x1b[30;0m\x1b[47;1m");
		DrawFilledBox(CONT_TEST_BUTTON_B_X1, CONT_TEST_BUTTON_B_Y1,
		              CONT_TEST_BUTTON_B_X1 + CONT_TEST_BUTTON_B_SIZE, CONT_TEST_BUTTON_B_Y1 + CONT_TEST_BUTTON_B_SIZE,
		              COLOR_WHITE, currXfb);
	} else {
		DrawBox(CONT_TEST_BUTTON_B_X1, CONT_TEST_BUTTON_B_Y1,
		        CONT_TEST_BUTTON_B_X1 + CONT_TEST_BUTTON_B_SIZE, CONT_TEST_BUTTON_B_Y1 + CONT_TEST_BUTTON_B_SIZE,
		        COLOR_WHITE, currXfb);
	}
	printf("\x1b[11;49H");
	printf("B");
	printf("\x1b[37;1m\x1b[40;0m");

	// X
	if (held & PAD_BUTTON_X) {
		printf("\x1b[30;0m\x1b[47;1m");
		DrawFilledBox(CONT_TEST_BUTTON_Z_X1, CONT_TEST_BUTTON_X_Y1,
		              CONT_TEST_BUTTON_Z_X1 + CONT_TEST_BUTTON_XY_SHORT, CONT_TEST_BUTTON_X_Y1 + CONT_TEST_BUTTON_XY_LONG,
		              COLOR_WHITE, currXfb);
	} else {
		DrawBox(CONT_TEST_BUTTON_Z_X1, CONT_TEST_BUTTON_X_Y1,
		        CONT_TEST_BUTTON_Z_X1 + CONT_TEST_BUTTON_XY_SHORT, CONT_TEST_BUTTON_X_Y1 + CONT_TEST_BUTTON_XY_LONG,
		        COLOR_WHITE, currXfb);
	}
	printf("\x1b[10;59H");
	printf("X");
	printf("\x1b[37;1m\x1b[40;0m");

	// Y
	if (held & PAD_BUTTON_Y) {
		printf("\x1b[30;0m\x1b[47;1m");
		DrawFilledBox(CONT_TEST_BUTTON_A_X1, CONT_TEST_BUTTON_Y_Y1,
		              CONT_TEST_BUTTON_A_X1 + CONT_TEST_BUTTON_XY_LONG, CONT_TEST_BUTTON_Y_Y1 + CONT_TEST_BUTTON_XY_SHORT,
		              COLOR_WHITE, currXfb);
	} else {
		DrawBox(CONT_TEST_BUTTON_A_X1, CONT_TEST_BUTTON_Y_Y1,
		        CONT_TEST_BUTTON_A_X1 + CONT_TEST_BUTTON_XY_LONG, CONT_TEST_BUTTON_Y_Y1 + CONT_TEST_BUTTON_XY_SHORT,
		        COLOR_WHITE, currXfb);
	}
	printf("\x1b[8;54H");
	printf("Y");
	printf("\x1b[37;1m\x1b[40;0m");

    // Z
	if (held & PAD_TRIGGER_Z) {
		printf("\x1b[30;0m\x1b[47;1m");
		DrawFilledBox(CONT_TEST_BUTTON_Z_X1, CONT_TEST_BUTTON_Z_Y1,
		              CONT_TEST_BUTTON_Z_X1 + CONT_TEST_BUTTON_XY_SHORT, CONT_TEST_BUTTON_Z_Y1 + CONT_TEST_BUTTON_XY_SHORT,
		              COLOR_WHITE, currXfb);
		
		// also enable rumble if z is held
		PAD_ControlMotor(0, PAD_MOTOR_RUMBLE);
	} else {
		DrawBox(CONT_TEST_BUTTON_Z_X1, CONT_TEST_BUTTON_Z_Y1,
		        CONT_TEST_BUTTON_Z_X1 + CONT_TEST_BUTTON_XY_SHORT, CONT_TEST_BUTTON_Z_Y1 + CONT_TEST_BUTTON_XY_SHORT,
		        COLOR_WHITE, currXfb);
		
		// stop rumble if z is not held
		PAD_ControlMotor(0, PAD_MOTOR_STOP);
	}
	printf("\x1b[8;59H");
	printf("Z");
	printf("\x1b[37;1m\x1b[40;0m");

	// Start
	if (held & PAD_BUTTON_START) {
		printf("\x1b[30;0m\x1b[47;1m");
		DrawFilledBox(CONT_TEST_BUTTON_START_X1, CONT_TEST_BUTTON_START_Y1,
		              CONT_TEST_BUTTON_START_X1 + CONT_TEST_BUTTON_START_LEN, CONT_TEST_BUTTON_START_Y1 + CONT_TEST_BUTTON_START_WIDTH,
		              COLOR_WHITE, currXfb);
	} else {
		DrawBox(CONT_TEST_BUTTON_START_X1, CONT_TEST_BUTTON_START_Y1,
		        CONT_TEST_BUTTON_START_X1 + CONT_TEST_BUTTON_START_LEN, CONT_TEST_BUTTON_START_Y1 + CONT_TEST_BUTTON_START_WIDTH,
		        COLOR_WHITE, currXfb);
	}
	printf("\x1b[8;38H");
	printf("START");
	printf("\x1b[37;1m\x1b[40;0m");
	
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


	printf( "\x1b[21;5H");
	printf("Analog L: %d", PAD_TriggerL(0));
	if (held & PAD_TRIGGER_L) {
		printf("\x1b[22;5H");
		printf("Digital L Pressed");
	}
	
	printf( "\x1b[21;60H");
	printf("Analog R: %d", PAD_TriggerR(0));
	if (held & PAD_TRIGGER_R) {
		printf("\x1b[22;56H");
		printf("Digital R Pressed");
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
	DrawCircle(CONT_TEST_STICK_CENTER_X, CONT_TEST_STICK_CENTER_Y, CONT_TEST_STICK_RAD,
			   COLOR_GRAY, currXfb); // perimeter
	DrawLine(CONT_TEST_STICK_CENTER_X, CONT_TEST_STICK_CENTER_Y,
			 xfbCoordX, xfbCoordY, COLOR_SILVER, currXfb); // line from center
	DrawFilledCircle(xfbCoordX, xfbCoordY, CONT_TEST_STICK_RAD / 2, 5, COLOR_WHITE, currXfb); // smaller circle
	
	// c-stick
	DrawCircle(CONT_TEST_CSTICK_CENTER_X, CONT_TEST_CSTICK_CENTER_Y, CONT_TEST_STICK_RAD,
	           COLOR_GRAY, currXfb); // perimeter
	DrawLine(CONT_TEST_CSTICK_CENTER_X, CONT_TEST_CSTICK_CENTER_Y,
	         xfbCoordCX, xfbCoordCY, COLOR_MEDGRAY, currXfb); // line from center
	DrawFilledCircle(xfbCoordCX, xfbCoordCY, CONT_TEST_STICK_RAD / 2, 1, COLOR_YELLOW, currXfb);
}

void menu_waveformMeasure(void *currXfb) {
	// TODO: I would bet that there's an off-by-one in here somewhere...

	// display instructions and data for user
	printf("Press A to start read, press Z for instructions\n");

	// do we have data that we can display?
	if (data.isDataReady) {
		int minX, minY;
		int maxX, maxY;

		// draw guidelines based on selected test
		DrawBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 128, COLOR_WHITE, currXfb);
		DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y, COLOR_GRAY, currXfb);
		// lots of the specific values are taken from:
		// https://github.com/PhobGCC/PhobGCC-doc/blob/main/For_Users/Phobvision_Guide_Latest.md
		switch (currentTest) {
			case PIVOT:
				DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 64, COLOR_GREEN, currXfb);
				DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y - 64, COLOR_GREEN, currXfb);
				printf( "\x1b[11;0H");
				printf("+64");
				printf( "\x1b[19;0H");
				printf("-64");
				break;
			case FULL:
			case DASHBACK:
				DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 64, COLOR_GREEN, currXfb);
				DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y - 64, COLOR_GREEN, currXfb);
				printf( "\x1b[11;0H");
				printf("+64");
				printf( "\x1b[19;0H");
				printf("-64");
			case SNAPBACK:
				DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 23, COLOR_GREEN, currXfb);
				DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y - 23, COLOR_GREEN, currXfb);
				printf( "\x1b[14;0H");
				printf("+23");
				printf( "\x1b[17;0H");
				printf("-23");
			default:
				break;
		}


		// draw waveform
		// i think this is better than what was here before?
		if (data.endPoint < 500) {
			dataScrollOffset = 0;
		}
		
		int prevX = data.data[dataScrollOffset].ax;
		int prevY = data.data[dataScrollOffset].ay;
		
		// initialize stat values to first point
		minX = prevX;
		maxX = prevX;
		minY = prevY;
		maxY = prevY;
		
		int waveformPrevXPos = 0;
		int waveformXPos = waveformScaleFactor;
		u64 drawnTicksUs = 0;
		
		// draw 500 datapoints from the scroll offset
		for (int i = dataScrollOffset + 1; i < dataScrollOffset + 500; i++) {
			// make sure we haven't gone outside our bounds
			if (i == data.endPoint || waveformXPos >= 500) {
				break;
			}
			
			// y first
			DrawLine(SCREEN_TIMEPLOT_START + waveformPrevXPos, SCREEN_POS_CENTER_Y - prevY,
			         SCREEN_TIMEPLOT_START + waveformXPos, SCREEN_POS_CENTER_Y - data.data[i].ay,
					 COLOR_BLUE, currXfb);
			prevY = data.data[i].ay;
			// then x
			DrawLine(SCREEN_TIMEPLOT_START + waveformPrevXPos, SCREEN_POS_CENTER_Y - prevX,
			         SCREEN_TIMEPLOT_START + waveformXPos, SCREEN_POS_CENTER_Y - data.data[i].ax,
			         COLOR_RED, currXfb);
			prevX = data.data[i].ax;
			
			// update stat values
			if (minX > prevX) {
				minX = prevX;
			}
			if (maxX < prevX) {
				maxX = prevX;
			}
			if (minY > prevY) {
				minY = prevY;
			}
			if (maxY < prevY) {
				maxY = prevY;
			}
			
			// adding time from drawn points, to show how long the current view is
			drawnTicksUs += data.data[i].timeDiffUs;
			
			// update scaling factor
			waveformPrevXPos = waveformXPos;
			waveformXPos += waveformScaleFactor;
		}

		// do we have enough data to enable scrolling?
		// TODO: enable scrolling when scaled
		if (data.endPoint >= 500 ) {
			// does the user want to scroll the waveform?
			if (held & PAD_BUTTON_RIGHT) {
				if (held & PAD_TRIGGER_R) {
					if (dataScrollOffset + 510 < data.endPoint) {
						dataScrollOffset += 10;
					}
				} else {
					if (dataScrollOffset + 501 < data.endPoint) {
						dataScrollOffset++;
					}
				}
			} else if (held & PAD_BUTTON_LEFT) {
				if (held & PAD_TRIGGER_R) {
					if (dataScrollOffset - 10 >= 0) {
						dataScrollOffset -= 10;
					}
				} else {
					if (dataScrollOffset - 1 >= 0) {
						dataScrollOffset--;
					}
				}
			}
		}
		
		printf( "\x1b[6;0H");
		// total time is stored in microseconds, divide by 1000 for milliseconds
		printf("%u samples, %0.3f ms, starting sample: %d, visible time: %0.3f ms\n", data.endPoint + 1, (data.totalTimeUs / ((float) 1000)), dataScrollOffset + 1, (drawnTicksUs / ((float) 1000)));

		// print test data
		printf( "\x1b[24;0H");
		switch (currentTest) {
			case SNAPBACK:
				printf("Min X: %04d | Min Y: %04d   |   ", minX, minY);
				printf("Max X: %04d | Max Y: %04d\n", maxX, maxY);
				break;
			case PIVOT:
				bool pivotHit80 = false;
				bool prevPivotHit80 = false;
				bool leftPivotRange = false;
				bool prevLeftPivotRange = false;
				int pivotStartIndex = -1, pivotEndIndex = -1;
				int pivotStartSign = 0;
				// start from the back of the list
				for (int i = data.endPoint; i >= 0; i--) {
					// check x coordinate for +-64 (dash threshold)
					if (data.data[i].ax >= 64 || data.data[i].ax <= -64) {
						if (pivotEndIndex == -1) {
							pivotEndIndex = i;
						}
						// pivot input must hit 80 on both sides
						if (data.data[i].ax >= 80 || data.data[i].ax <= -80) {
							pivotHit80 = true;
						}
					}

					// are we outside the pivot range and have already logged data of being in range
					if (pivotEndIndex != -1 && data.data[i].ax < 64 && data.data[i].ax > -64) {
						leftPivotRange = true;
						if (pivotStartIndex == -1) {
							// need the "previous" poll since this one is out of the range
							pivotStartIndex = i + 1;
						}
						if (prevLeftPivotRange || !pivotHit80) {
							break;
						}
					}

					// look for the initial input
					if ( (data.data[i].ax >= 64 || data.data[i].ax <= -64) && leftPivotRange) {
						// used to ensure starting input is from the opposite side
						if (pivotStartSign == 0) {
							pivotStartSign = data.data[i].ax;
						}
						prevLeftPivotRange = true;
						if (data.data[i].ax >= 80 || data.data[i].ax <= -80) {
							prevPivotHit80 = true;
							break;
						}
					}
				}

				// phobvision doc says both sides need to hit 80 to succeed
				// multiplication is to ensure signs are correct
				if (prevPivotHit80 && pivotHit80 && (data.data[pivotEndIndex].ax * pivotStartSign < 0)) {
					float noTurnPercent = 0;
					float pivotPercent = 0;
					float dashbackPercent = 0;

					u64 timeInPivotRangeUs = 0;
					for (int i = pivotStartIndex; i <= pivotEndIndex; i++) {
						timeInPivotRangeUs += data.data[i].timeDiffUs;
					}
					
					// convert time to float in milliseconds
					float timeInPivotRangeMs = (timeInPivotRangeUs / 1000.0);
					
					// TODO: i think the calculation can be simplified here...
					// how many milliseconds could a poll occur that would cause a miss
					float diffFrameTimePoll = frameTime - timeInPivotRangeMs;
					
					// negative time difference, dashback
					if (diffFrameTimePoll < 0) {
						dashbackPercent = ((diffFrameTimePoll * -1) / frameTime) * 100;
						if (dashbackPercent > 100) {
							dashbackPercent = 100;
						}
						pivotPercent = 100 - dashbackPercent;
					// positive or 0 time diff, no turn
					} else {
						noTurnPercent = (diffFrameTimePoll / frameTime) * 100;
						if (noTurnPercent > 100) {
							noTurnPercent = 100;
						}
						pivotPercent = 100 - noTurnPercent;
					}
					
					printf("MS in range: %2.2f | No turn: %2.0f%% | Empty Pivot: %2.0f%% | Dashback: %2.0f%%",
						   timeInPivotRangeMs, noTurnPercent, pivotPercent, dashbackPercent);
					//printf("\nUS Total: %llu, Start index: %d, End index: %d", timeInPivotRangeUs, pivotStartIndex, pivotEndIndex);
				} else {
					printf("No pivot input detected.");
				}
				break;
			case DASHBACK:
				// go forward in list
				int dashbackStartIndex = -1, dashbackEndIndex = -1;
				u64 timeInRange = 0;
				for (int i = 0; i < data.endPoint; i++) {
					// is the stick in the range
					if ((data.data[i].ax >= 23 && data.data[i].ax < 64) || (data.data[i].ax <= -23 && data.data[i].ax > -64)) {
						timeInRange += data.data[i].timeDiffUs;
						if (dashbackStartIndex == -1) {
							dashbackStartIndex = i;
						}
					} else if (dashbackStartIndex != -1) {
						dashbackEndIndex = i - 1;
						break;
					}
				}
				
				// convert time in microseconds to float time in milliseconds
				float timeInRangeMs = (timeInRange / 1000.0);
				
				float dashbackPercent = (1.0 - (timeInRangeMs / frameTime)) * 100;
				float ucfPercent;
				
				// ucf dashback is a little more involved
				u64 ucfTimeInRange = timeInRange;
				for (int i = dashbackStartIndex; i <= dashbackEndIndex; i++) {
					// we're gonna assume that the previous frame polled around the origin, because i cant be bothered
					// it also makes the math easier
					u64 usFromPoll = 0;
					int nextPollIndex = i;
					// we need the sample that would occur around 1f after
					while (usFromPoll < 16666) {
						nextPollIndex++;
						usFromPoll += data.data[nextPollIndex].timeDiffUs;
					}
					// the two frames need to move more than 75 units for UCF to convert it
					if (data.data[i].ax + data.data[nextPollIndex].ax > 75 || data.data[i].ax + data.data[nextPollIndex].ax < -75) {
						ucfTimeInRange -= data.data[i].timeDiffUs;
					}
				}
				
				float ucfTimeInRangeMs = ucfTimeInRange / 1000.0;
				if (ucfTimeInRangeMs <= 0) {
					ucfPercent = 100;
				} else {
					ucfPercent = (1.0 - (ucfTimeInRangeMs / frameTime)) * 100;
				}

				// this shouldn't happen in theory, maybe on box?
				if (dashbackPercent > 100) {
					dashbackPercent = 100;
				}
				if (ucfPercent > 100) {
					ucfPercent = 100;
				}
				// this definitely can happen though
				if (dashbackPercent < 0) {
					dashbackPercent = 0;
				}
				if (ucfPercent < 0) {
					ucfPercent = 0;
				}
				printf("Vanilla Success: %2.0f%% | UCF Success: %2.0f%%", dashbackPercent, ucfPercent);
				break;
			case FULL:
			case NO_TEST:
				break;
			default:
				printf("Error?");
				break;

		}
	}
	printf( "\x1b[26;0H");
	printf("Current test: ");
	switch (currentTest) {
		case SNAPBACK:
			printf("Snapback");
			break;
		case PIVOT:
			printf("Pivot");
			break;
		case DASHBACK:
			printf("Dashback");
			break;
		case NO_TEST:
			printf("None");
			break;
		case FULL:
			printf("Full Measure");
			break;
		default:
			printf("Error");
			break;
	}
	printf("\n");

	// only start reading if A is pressed
	// TODO: figure out if this can be removed without having to gut the current poll logic, would be better for the user to not have to do this
	if (pressed & PAD_BUTTON_A) {
		if (currentTest == FULL) {
			data.fullMeasure = true;
		} else {
			data.fullMeasure = false;
		}
		previousMenu = WAVEFORM;
		currentMenu = WAITING_MEASURE;
	// does the user want to change the test?
	} else if (pressed & PAD_BUTTON_X) {
		currentTest++;
		// hacky way to disable pivot tests for now
		if (currentTest == PIVOT) {
			//currentTest++;
		}
		// check if we overrun our test length
		if (currentTest == TEST_LEN) {
			currentTest = SNAPBACK;
		}
	// adjust scaling factor
	//} else if (pressed & PAD_BUTTON_Y) {
	//	waveformScaleFactor++;
	//	if (waveformScaleFactor > 5) {
	//		waveformScaleFactor = 1;
	//	}
	}
}

void menu_2dPlot(void *currXfb) {
	static WaveformDatapoint convertedCoords;

	// display instructions and data for user
	printf("Press A to start read, press Z for instructions\n");

	// do we have data that we can display?
	if (data.isDataReady) {
		convertedCoords = convertStickValues(&data.data[lastDrawPoint]);
		printf("%04u samples, last point is: %04d, ", data.endPoint + 1, lastDrawPoint + 1);
		printf("Microsecs from last poll: %05llu", data.data[lastDrawPoint].timeDiffUs);
		// TODO: move instructions under different prompt, so I don't have to keep messing with text placement

		// print coordinates of last drawn point
		printf( "\x1b[24;0H");
		printf("Raw X: %04d | Raw Y: %04d   |   ", data.data[lastDrawPoint].ax, data.data[lastDrawPoint].ay);
		printf("Melee X: ");

		// is the value negative?
		if (data.data[lastDrawPoint].ax < 0) {
			printf("-");
		}
		// is this a 1.0 value?
		if (convertedCoords.ax == 10000) {
			printf("1.0");
		} else {
			printf("0.%04d", convertedCoords.ax);
		}
		printf(" | Melee Y: ");
		// is the value negative?
		if (data.data[lastDrawPoint].ay < 0) {
			printf("-");
		}
		// is this a 1.0 value?
		if (convertedCoords.ay == 10000) {
			printf("1.0\n");
		} else {
			printf("0.%04d\n", convertedCoords.ay);
		}
		printf("Currently selected stickmap: ");

		// draw image below 2d plot, and print while we're at it
		switch (selectedImage) {
			case A_WAIT:
				printf("Wait Attacks");
				drawImage(currXfb, await_image, await_indexes, SCREEN_POS_CENTER_X - 127, SCREEN_POS_CENTER_Y - 127);
				break;
			case CROUCH:
				printf("Crouch");
				drawImage(currXfb, crouch_image, crouch_indexes, SCREEN_POS_CENTER_X - 127, SCREEN_POS_CENTER_Y - 127);
				break;
			case DEADZONE:
				printf("Deadzones");
				drawImage(currXfb, deadzone_image, deadzone_indexes, SCREEN_POS_CENTER_X - 127, SCREEN_POS_CENTER_Y - 127);
				break;
			case LEDGE_L:
				printf("Left Ledge");
				drawImage(currXfb, ledgeL_image, ledgeL_indexes, SCREEN_POS_CENTER_X - 127, SCREEN_POS_CENTER_Y - 127);
				break;
			case LEDGE_R:
				printf("Right Ledge");
				drawImage(currXfb, ledgeR_image, ledgeR_indexes, SCREEN_POS_CENTER_X - 127, SCREEN_POS_CENTER_Y - 127);
				break;
			case MOVE_WAIT:
				printf("Wait Movement");
				drawImage(currXfb, movewait_image, movewait_indexes, SCREEN_POS_CENTER_X - 127, SCREEN_POS_CENTER_Y - 127);
				break;
			case NO_IMAGE:
				printf("None");
			default:
				break;
		}
		printf("\n");

		// draw plot
		// y is negated because of how the graph is drawn
		// TODO: why does this need to be <= to avoid an off-by-one? step through logic later this is bugging me
		for (int i = 0; i <= lastDrawPoint; i++) {
			DrawBox(SCREEN_POS_CENTER_X + data.data[i].ax, SCREEN_POS_CENTER_Y - data.data[i].ay,
					SCREEN_POS_CENTER_X + data.data[i].ax, SCREEN_POS_CENTER_Y - data.data[i].ay,
					COLOR_WHITE, currXfb);
		}

		// does the user want to change what data is drawn?
		// single movements with L
		if (held & PAD_TRIGGER_L) {
			if (pressed & PAD_BUTTON_RIGHT) {
				if (lastDrawPoint + 1 < data.endPoint) {
					lastDrawPoint++;
				}
			} else if (pressed & PAD_BUTTON_LEFT) {
				if (lastDrawPoint - 1 >= 0) {
					lastDrawPoint--;
				}
			}
		} else if (held & PAD_BUTTON_RIGHT) {
			if (held & PAD_TRIGGER_R) {
				if (lastDrawPoint + 5 < data.endPoint) {
					lastDrawPoint += 5;
				}
			} else {
				if (lastDrawPoint + 1 < data.endPoint) {
					lastDrawPoint++;
				}
			}
		} else if (held & PAD_BUTTON_LEFT) {
			if (held & PAD_TRIGGER_R) {
				if (lastDrawPoint - 5 >= 0) {
					lastDrawPoint -= 5;
				}
			} else {
				if (lastDrawPoint - 1 >= 0) {
					lastDrawPoint--;
				}
			}
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


void menu_fileExport() {
	printf("todo");
	//fileIOSuccess = exportData(&data, false);
}


void menu_waitingMeasure() {
	if (!displayedWaitingInputMessage) {
		printf("\nWaiting for user input...");
		displayedWaitingInputMessage = true;
		return;
	}
	measureWaveform(&data);
	dataScrollOffset = 0;
	lastDrawPoint = data.endPoint;
	assert(data.endPoint < 5000);
	currentMenu = previousMenu;
	displayedWaitingInputMessage = false;
}


void menu_coordinateViewer(void *currXfb) {
	// melee stick coordinates stuff
	// a lot of this comes from github.com/phobgcc/phobconfigtool
	
	printf("Press Z for instructions\n\n");
	
	static WaveformDatapoint stickCoordinatesRaw;
	static WaveformDatapoint stickCoordinatesMelee;
	
	// get raw stick values
	stickCoordinatesRaw.ax = PAD_StickX(0), stickCoordinatesRaw.ay = PAD_StickY(0);
	stickCoordinatesRaw.cx = PAD_SubStickX(0), stickCoordinatesRaw.cy = PAD_SubStickY(0);
	
	// get converted stick values
	stickCoordinatesMelee = convertStickValues(&stickCoordinatesRaw);
	
	// print melee coordinates
	printf("Stick X: ");
	// is the value negative?
	if (stickCoordinatesRaw.ax < 0) {
		printf("-");
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.ax == 10000) {
		printf("1.0\n");
	} else {
		printf("0.%04d\n", stickCoordinatesMelee.ax);
	}
	
	// print melee coordinates
	printf("Stick Y: ");
	// is the value negative?
	if (stickCoordinatesRaw.ay < 0) {
		printf("-");
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.ay == 10000) {
		printf("1.0\n");
	} else {
		printf("0.%04d\n", stickCoordinatesMelee.ay);
	}
	
	// print melee coordinates
	printf("\nC-Stick X: ");
	// is the value negative?
	if (stickCoordinatesRaw.cx < 0) {
		printf("-");
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.cx == 10000) {
		printf("1.0\n");
	} else {
		printf("0.%04d\n", stickCoordinatesMelee.cx);
	}
	
	// print melee coordinates
	printf("C-Stick Y: ");
	// is the value negative?
	if (stickCoordinatesRaw.cy < 0) {
		printf("-");
	}
	// is this a 1.0 value?
	if (stickCoordinatesMelee.cy == 10000) {
		printf("1.0\n");
	} else {
		printf("0.%04d\n", stickCoordinatesMelee.cy);
	}
	
	printf("\x1b[23;0H");
	printf("\nStickmap: ");
	int stickmapRetVal = isCoordValid(stickmapList, stickCoordinatesMelee);
	switch (stickmapList) {
		case FF_WD:
			printf("Firefox/Wavedash\n");
			printf("Result: %s%s", STICKMAP_FF_WD_RETCOLORS[stickmapRetVal], STICKMAP_FF_WD_RETVALS[stickmapRetVal]);
			break;
		case SHIELDDROP:
			printf("Shield Drop\n");
			printf("Result: %s%s", STICKMAP_SHIELDDROP_RETCOLORS[stickmapRetVal], STICKMAP_SHIELDDROP_RETVALS[stickmapRetVal]);
			break;
		case NONE:
		default:
			printf("NONE");
			break;
	}
	printf("\x1b[37;1m\x1b[40;0m");
	
	
	// calculate screen coordinates for stick position drawing
	int xfbCoordX = (stickCoordinatesMelee.ax / 125) * 2;
	if (stickCoordinatesRaw.ax < 0) {
		xfbCoordX *= -1;
	}
	xfbCoordX += SCREEN_POS_CENTER_X;
	
	int xfbCoordY = (stickCoordinatesMelee.ay / 125) * 2;
	if (stickCoordinatesRaw.ay > 0) {
		xfbCoordY *= -1;
	}
	xfbCoordY += SCREEN_POS_CENTER_Y;
	
	int xfbCoordCX = (stickCoordinatesMelee.cx / 125) * 2;
	if (stickCoordinatesRaw.cx < 0) {
		xfbCoordCX *= -1;
	}
	xfbCoordCX += SCREEN_POS_CENTER_X;
	
	int xfbCoordCY = (stickCoordinatesMelee.cy / 125) * 2;
	if (stickCoordinatesRaw.cy > 0) {
		xfbCoordCY *= -1;
	}
	xfbCoordCY += SCREEN_POS_CENTER_Y;
	
	// draw stickbox bounds
	DrawCircle(SCREEN_POS_CENTER_X, SCREEN_POS_CENTER_Y, 160, COLOR_MEDGRAY, currXfb);
	
	// draw stickmap area
	// TODO
	switch (stickmapList) {
		case FF_WD:
			//DrawFilledBox(toStickmap(STICKMAP_FF_WD_COORD_SAFE[0][0]) + SCREEN_POS_CENTER_X - 2, toStickmap(STICKMAP_FF_WD_COORD_SAFE[0][1]) + SCREEN_POS_CENTER_Y - 2,
			//              toStickmap(STICKMAP_FF_WD_COORD_SAFE[0][0]) + SCREEN_POS_CENTER_X + 2, toStickmap(STICKMAP_FF_WD_COORD_SAFE[0][1]) + SCREEN_POS_CENTER_Y + 2,
			//			  COLOR_LIME, currXfb);
			//DrawLine(SCREEN_POS_CENTER_X, SCREEN_POS_CENTER_Y, convertMeleeCoordToStickmap(STICKMAP_FF_WD_COORD_SAFE[0][0]) + SCREEN_POS_CENTER_X, convertMeleeCoordToStickmap(STICKMAP_FF_WD_COORD_SAFE[0][1]) + SCREEN_POS_CENTER_Y, COLOR_GREEN, currXfb);
			break;
		case SHIELDDROP:
			break;
		case NONE:
		default:
			break;
	}

	// draw analog stick line
	DrawLine(SCREEN_POS_CENTER_X, SCREEN_POS_CENTER_Y, xfbCoordX, xfbCoordY, COLOR_WHITE, currXfb);
	DrawBox(xfbCoordX - 4, xfbCoordY - 4, xfbCoordX + 4, xfbCoordY + 4, COLOR_WHITE, currXfb);
	
	// draw c-stick line
	DrawLine(SCREEN_POS_CENTER_X, SCREEN_POS_CENTER_Y, xfbCoordCX, xfbCoordCY, COLOR_YELLOW, currXfb);
	DrawFilledBox(xfbCoordCX - 2, xfbCoordCY - 2, xfbCoordCX + 2, xfbCoordCY + 2, COLOR_YELLOW, currXfb);
	
	if (pressed & PAD_BUTTON_X) {
		stickmapList++;
		if (stickmapList == 3) {
			stickmapList = 0;
		}
	}
}
