//
// Created on 10/25/23.
//

#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include "waveform.h"

#define SCREEN_POS_OFFSET 225
#define SCREEN_POS_CENTER 320
#define MENUITEMS_LEN 3

// enum to keep track of what menu to display, and what logic to run
static enum CURRENT_MENU currentMenu = MAIN_MENU;

// main menu counter
static u8 mainMenuSelection = 0;

// counter for how many frames b or start have been held
static u8 bHeldCounter = 0;

// data for drawing a waveform
static WaveformData data = { { 0 }, 0, 0, 0, 0, 0, false };

// vars for what buttons are pressed or held
static u32 pressed = 0;
static u32 held = 0;

static const char* menuItems[MENUITEMS_LEN] = { "Controller Test", "Measure Waveform", "2D Plot" };

// the "main" for the menus
// other menu functions are called from here
// this also handles moving between menus and exiting
bool menu_runMenu(void *currXfb) {
	// read inputs
	PAD_ScanPads();

	// check for any buttons pressed/held
	pressed = PAD_ButtonsDown(0);
	held = PAD_ButtonsHeld(0);

	// reset console cursor position
	printf("\x1b[2;0H");
	printf("FossScope (Working Title)\n\n");

	// determine what menu we are in
	switch (currentMenu) {
		case MAIN_MENU:
			menu_mainMenu();
			break;
		case CONTROLLER_TEST:
			menu_controllerTest();
			break;
		case WAVEFORM:
			menu_waveformMeasure(currXfb);
			break;
		case PLOT_2D:
			menu_2dPlot(currXfb);
			break;
		default:
			printf("HOW DID WE END UP HERE?\n");
			break;
	}

	// move cursor to bottom left
	printf( "\x1b[25;0H");

	// exit the program if start is pressed
	if ( pressed & PAD_BUTTON_START && currentMenu == MAIN_MENU ) {
		printf("Exiting...");
		return true;
	}

	// does the user want to move back to the main menu?
	else if ( held & PAD_BUTTON_B && currentMenu != MAIN_MENU ) {
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
			bHeldCounter = 0;
		}

	} else {
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
	if ( pressed & PAD_BUTTON_UP ) {
		if (mainMenuSelection > 0) {
			mainMenuSelection--;
		}
	} else if ( pressed & PAD_BUTTON_DOWN ) {
		if (mainMenuSelection < 2) {
			mainMenuSelection++;
		}
	}

	// does the user want to move into another menu?
	// else if to ensure that the A press is separate from any dpad stuff
	else if ( pressed & PAD_BUTTON_A ) {
		switch (mainMenuSelection) {
			case 0:
				currentMenu = CONTROLLER_TEST;
				break;
			case 1:
				currentMenu = WAVEFORM;
				break;
			case 2:
				currentMenu = PLOT_2D;
				break;
		}
	}
}

void menu_controllerTest() {
	// melee stick coordinates stuff
	// a lot of this comes from github.com/phobgcc/phobconfigtool

	// get raw stick values
	int stickX = PAD_StickX(0), stickY = PAD_StickY(0);
	int cStickX = PAD_SubStickX(0), cStickY = PAD_SubStickY(0);

	int meleeStickX = stickX, meleeStickY = stickY;
	int meleeCStickX = cStickX, meleeCStickY = cStickY;

	// convert raw values to melee stick values
	{
		float floatStickX = stickX, floatStickY = stickY;
		float floatCStickX = cStickX, floatCStickY = cStickY;

		float stickMagnitude = sqrt((stickX * stickX) + (stickY * stickY));
		float cStickMagnitude = sqrt((cStickX * cStickX) + (cStickY * cStickY));

		// magnitude must be between 0 and 80
		if (stickMagnitude > 80) {
			// scale stick value to be within range
			floatStickX = (floatStickX / stickMagnitude) * 80;
			floatStickY = (floatStickY / stickMagnitude) * 80;
		}
		if (cStickMagnitude > 80) {
			// scale stick value to be within range
			floatCStickX = (floatCStickX / cStickMagnitude) * 80;
			floatCStickY = (floatCStickY / cStickMagnitude) * 80;
		}

		// truncate the floats
		meleeStickX = (int) floatStickX, meleeStickY = (int) floatStickY;
		meleeCStickX = (int) floatCStickX, meleeCStickY = (int) floatCStickY;

		// convert to the decimal format for melee
		// specifically, we are calculating the decimal part of the values
		meleeStickX = (((float) meleeStickX) * 0.0125) * 10000;
		meleeStickY = (((float) meleeStickY) * 0.0125) * 10000;
		meleeCStickX = (((float) meleeCStickX) * 0.0125) * 10000;
		meleeCStickY = (((float) meleeCStickY) * 0.0125) * 10000;

		// get rid of any negative values
		meleeStickX = abs(meleeStickX), meleeStickY = abs(meleeStickY);
		meleeCStickX = abs(meleeCStickX), meleeCStickY = abs(meleeCStickY);
	}

	// print analog inputs
	printf("Stick X: (raw)   %4d   |   (melee) ", stickX);
	// is the value negative?
	if (stickX < 0) {
		printf("-");
	}
	// is this a 1.0 value?
	if (meleeStickX == 10000) {
		printf("1.0\n");
	} else {
		printf("0.%04d\n", meleeStickX);
	}

	printf("Stick Y: (raw)   %4d   |   (melee) ", stickY);
	// is the value negative?
	if (stickY < 0) {
		printf("-");
	}
	// is this a 1.0 value?
	if (meleeStickY == 10000) {
		printf("1.0\n");
	} else {
		printf("0.%04d\n", meleeStickY);
	}

	printf("C-Stick X: (raw) %4d   |   (melee) ", cStickX);
	// is the value negative?
	if (cStickX < 0) {
		printf("-");
	}
	// is this a 1.0 value?
	if (meleeCStickX == 10000) {
		printf("1.0\n");
	} else {
		printf("0.%04d\n", meleeCStickX);
	}
	printf("C-Stick Y: (raw) %4d   |   (melee) ", cStickY);
	// is the value negative?
	if (cStickY < 0) {
		printf("-");
	}
	// is this a 1.0 value?
	if (meleeCStickY == 10000) {
		printf("1.0\n");
	} else {
		printf("0.%04d\n", meleeCStickY);
	}

	printf("Analog L: %d\n", PAD_TriggerL(0));
	printf("Analog R: %d\n", PAD_TriggerR(0));

	// print all the digital inputs
	// this is ugly, but I'm not sure of a better way to do this currently
	// this will get redone when stuff isn't just printed to console
	printf("A: ");
	if ( held & PAD_BUTTON_A ) {
		printf("Pressed");
	}
	printf("\nB: ");
	if ( held & PAD_BUTTON_B ) {
		printf("Pressed");
	}
	printf("\nX: ");
	if ( held & PAD_BUTTON_X ) {
		printf("Pressed");
	}
	printf("\nY: ");
	if ( held & PAD_BUTTON_Y ) {
		printf("Pressed");
	}
	printf("\nZ: ");
	if ( held & PAD_TRIGGER_Z ) {
		printf("Pressed");
	}
	printf("\nStart: ");
	if ( held & PAD_BUTTON_START ) {
		printf("Pressed");
	}
	printf("\nDigital L: ");
	if ( held & PAD_TRIGGER_L ) {
		printf("Pressed");
	}
	printf("\nDigital R: ");
	if ( held & PAD_TRIGGER_R ) {
		printf("Pressed");
	}
	printf("\nDPAD Up: ");
	if ( held & PAD_BUTTON_UP ) {
		printf("Pressed");
	}
	printf("\nDPAD Down: ");
	if ( held & PAD_BUTTON_DOWN ) {
		printf("Pressed");
	}
	printf("\nDPAD Left: ");
	if ( held & PAD_BUTTON_LEFT ) {
		printf("Pressed");
	}
	printf("\nDPAD Right: ");
	if ( held & PAD_BUTTON_RIGHT ) {
		printf("Pressed");
	}

}

void menu_waveformMeasure(void *currXfb) {
	// TODO: I would bet that there's an off-by-one in here somewhere...

	static int horizontalScroll;

	// display instructions and data for user
	printf("Press A to start read\n");

	// do we have data that we can display?
	if (data.isDataReady) {
		printf("%u samples, drawing from sample %d\n", data.endPoint + 1, horizontalScroll + 1);

		// draw guidelines
		DrawHLine(50, 550, SCREEN_POS_OFFSET, COLOR_GRAY, currXfb);
		DrawHLine(50, 550, SCREEN_POS_OFFSET + 23, COLOR_GREEN, currXfb);
		DrawHLine(50, 550, SCREEN_POS_OFFSET - 23, COLOR_GREEN, currXfb);

		// draw waveform
		// TODO: this needs to be gutted and replaced, this is not good code
		// y and x are drawn in separate loops because when it was together y would sometimes show over x
		// this is probably because of bad code but this just needs to be replaced anyway, ie for scaling

		if (data.endPoint < 500) {
			int prevX = data.data[0].ax;
			int prevY = data.data[0].ay;

			// y is first so that x shows on top
			for (int i = 0; i < data.endPoint; i++) {
				// check if our x1 should be the previous point or our current data
				if (prevY > data.data[i].ay) {
					DrawBox(50 + i, (prevY * -1) + SCREEN_POS_OFFSET, 50 + i + 1,
					        (data.data[i].ay * -1) + SCREEN_POS_OFFSET + 1, COLOR_BLUE, currXfb);
				} else {
					DrawBox(50 + i, (data.data[i].ay * -1) + SCREEN_POS_OFFSET, 50 + i + 1,
					        (prevY * -1) + SCREEN_POS_OFFSET + 1, COLOR_BLUE, currXfb);
				}
				prevY = data.data[i].ay;
			}

			// x
			for (int i = 0; i < data.endPoint; i++) {
				if (prevX > data.data[i].ax) {
					DrawBox(50 + i, (prevX * -1) + SCREEN_POS_OFFSET, 50 + i + 1,
					        (data.data[i].ax * -1) + SCREEN_POS_OFFSET + 1, COLOR_RED, currXfb);
				} else {
					DrawBox(50 + i, (data.data[i].ax * -1) + SCREEN_POS_OFFSET, 50 + i + 1,
					        (prevX * -1) + SCREEN_POS_OFFSET + 1, COLOR_RED, currXfb);
				}

				prevX = data.data[i].ax;
			}
		} else {
			int prevX = data.data[0].ax;
			int prevY = data.data[0].ay;

			// y is first so that x shows on top
			for (int i = 0; i < 500; i++) {
				// check if our x1 should be the previous point or our current data
				if (prevY > data.data[i + horizontalScroll].ay) {
					DrawBox(50 + i, (prevY * -1) + SCREEN_POS_OFFSET, 50 + i + 1,
					        (data.data[i + horizontalScroll].ay * -1) + SCREEN_POS_OFFSET + 1, COLOR_BLUE, currXfb);
				} else {
					DrawBox(50 + i, (data.data[i + horizontalScroll].ay * -1) + SCREEN_POS_OFFSET, 50 + i + 1,
					        (prevY * -1) + SCREEN_POS_OFFSET + 1, COLOR_BLUE, currXfb);
				}
				prevY = data.data[i + horizontalScroll].ay;
			}

			// x
			for (int i = 0; i < 500; i++) {
				if (prevX > data.data[i + horizontalScroll].ax) {
					DrawBox(50 + i, (prevX * -1) + SCREEN_POS_OFFSET, 50 + i + 1,
					        (data.data[i + horizontalScroll].ax * -1) + SCREEN_POS_OFFSET + 1, COLOR_RED, currXfb);
				} else {
					DrawBox(50 + i, (data.data[i + horizontalScroll].ax * -1) + SCREEN_POS_OFFSET, 50 + i + 1,
					        (prevX * -1) + SCREEN_POS_OFFSET + 1, COLOR_RED, currXfb);
				}

				prevX = data.data[i + horizontalScroll].ax;
			}
		}

		// do we have enough data to enable scrolling?
		if (data.endPoint >= 500) {
			printf("\x1b[23;0H");
			printf("Use DPAD left/right to scroll waveform, hold R to move faster.");
			// does the user want to scroll the waveform?
			if (held & PAD_BUTTON_RIGHT) {
				if (held & PAD_TRIGGER_R) {
					if (horizontalScroll + 505 < data.endPoint) {
						horizontalScroll += 5;
					}
				} else {
					if (horizontalScroll + 501 < data.endPoint) {
						horizontalScroll++;
					}
				}
			} else if (held & PAD_BUTTON_LEFT) {
				if (held & PAD_TRIGGER_R) {
					if (horizontalScroll - 5 >= 0) {
						horizontalScroll -= 5;
					}
				} else {
					if (horizontalScroll - 1 >= 0) {
						horizontalScroll--;
					}
				}
			}
		}
	}

	// print min/max data
	printf( "\x1b[20;0H");
	printf("Min X: %04d | Min Y: %04d\n", data.minX, data.minY);
	printf("Max X: %04d | Max Y: %04d\n", data.maxX, data.maxY);

	// only start reading if A is pressed
	// TODO: figure out if this can be removed without having to gut the current poll logic, would be better for the user to not have to do this
	if ( pressed & PAD_BUTTON_A) {
		measureWaveform(&data);
		horizontalScroll = 0;
		assert(data.endPoint < 5000);
	}
}

void menu_2dPlot(void *currXfb) {
	// display instructions and data for user
	printf("Press A to start read\n");

	// do we have data that we can display?
	if (data.isDataReady) {
		printf("%u samples\n", data.endPoint + 1);

		// draw plot
		// TODO: this needs to be gutted and replaced, this is not good code

		// y is negated because of how the graph is drawn
		for (int i = 0; i < data.endPoint; i++) {
			DrawBox(SCREEN_POS_CENTER + data.data[i].ax, SCREEN_POS_OFFSET - data.data[i].ay,
			        SCREEN_POS_CENTER + data.data[i].ax, SCREEN_POS_OFFSET - data.data[i].ay, COLOR_WHITE, currXfb);
		}
	}

	// only start reading if A is pressed
	// TODO: figure out if this can be removed without having to gut the current poll logic, would be better for the user to not have to do this
	if ( pressed & PAD_BUTTON_A) {
		measureWaveform(&data);
		assert(data.endPoint < 5000);
	}
}
