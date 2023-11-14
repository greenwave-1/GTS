//
// Created on 10/25/23.
//

#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "waveform.h"

// center of screen, 640x480
#define SCREEN_POS_CENTER_X 320
#define SCREEN_POS_CENTER_Y 240

// 500 values displayed at once, SCREEN_POS_CENTER_X +/- 250
#define SCREEN_TIMEPLOT_START 70
//#define SCREEN_TIMEPLOT_END 570

#define MENUITEMS_LEN 3

// enum to keep track of what menu to display, and what logic to run
static enum CURRENT_MENU currentMenu = MAIN_MENU;

// main menu counter
static u8 mainMenuSelection = 0;

// counter for how many frames b or start have been held
static u8 bHeldCounter = 0;

// data for drawing a waveform
static WaveformData data = { { 0 }, 0, false };

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
	printf("\x1b[1;0H");
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

	static WaveformDatapoint stickCoordinatesRaw;
	static WaveformDatapoint stickCoordinatesMelee;

	// get raw stick values
	stickCoordinatesRaw.ax = PAD_StickX(0), stickCoordinatesRaw.ay = PAD_StickY(0);
	stickCoordinatesRaw.cx = PAD_SubStickX(0), stickCoordinatesRaw.cy = PAD_SubStickY(0);

	// get converted stick values
	stickCoordinatesMelee = convertStickValues(&stickCoordinatesRaw);

	// print analog inputs
	printf("Stick X: (raw)   %4d   |   (melee) ", stickCoordinatesRaw.ax);
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

	printf("Stick Y: (raw)   %4d   |   (melee) ", stickCoordinatesRaw.ay);
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

	printf("C-Stick X: (raw) %4d   |   (melee) ", stickCoordinatesRaw.cx);
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
	printf("C-Stick Y: (raw) %4d   |   (melee) ", stickCoordinatesRaw.cy);
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

	static int dataScrollOffset;

	// display instructions and data for user
	printf("Press A to start read\n");

	// do we have data that we can display?
	if (data.isDataReady) {
		printf("%u samples, drawing from sample %d\n", data.endPoint + 1, dataScrollOffset + 1);

		int minX, minY;
		int maxX, maxY;

		// draw guidelines (snapback detection)
		// TODO: this needs to be replaced with a different draw guide depending on what the user selects, once that's implemented
		DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y, COLOR_GRAY, currXfb);
		DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 23, COLOR_GREEN, currXfb);
		DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y - 23, COLOR_GREEN, currXfb);

		// draw waveform
		// TODO: this needs to be gutted and replaced, this is not good code
		// y and x are drawn in separate loops because when it was together y would sometimes show over x
		// this is probably because of bad code but this just needs to be replaced anyway, ie for scaling

		if (data.endPoint < 500) {
			int prevX = data.data[0].ax;
			int prevY = data.data[0].ay;

			// initialize stat values
			minX = prevX;
			maxX = prevX;
			minY = prevY;
			maxY = prevY;

			// draw first point
			DrawBox(SCREEN_TIMEPLOT_START, SCREEN_POS_CENTER_Y - prevY, SCREEN_TIMEPLOT_START,
			        SCREEN_POS_CENTER_Y - prevY, COLOR_BLUE, currXfb);

			// y is first so that x shows on top
			for (int i = 1; i < data.endPoint; i++) {
				// check if our x1 should be the previous point or our current data
				if (prevY > data.data[i].ay) {
					DrawBox(SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - prevY,
							SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - data.data[i].ay,
							COLOR_BLUE, currXfb);
				} else {
					DrawBox(SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - data.data[i].ay,
					        SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - prevY,
					        COLOR_BLUE, currXfb);
				}

				prevY = data.data[i].ay;

				// update stat values
				if (minY > prevY) {
					minY = prevY;
				}
				if (maxY < prevY) {
					maxY = prevY;
				}
			}

			// draw first point
			DrawBox(SCREEN_TIMEPLOT_START, SCREEN_POS_CENTER_Y - prevX, SCREEN_TIMEPLOT_START,
			        SCREEN_POS_CENTER_Y - prevX, COLOR_BLUE, currXfb);
			
			// x
			for (int i = 1; i < data.endPoint; i++) {
				if (prevX > data.data[i].ax) {
					DrawBox(SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - prevX,
							SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - data.data[i].ax,
							COLOR_RED, currXfb);
				} else {
					DrawBox(SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - data.data[i].ax,
					        SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - prevX,
					        COLOR_RED, currXfb);
				}

				prevX = data.data[i].ax;
				// update stat values
				if (minX > prevX) {
					minX = prevX;
				}
				if (maxX < prevX) {
					maxX = prevX;
				}
			}
		} else {
			int prevX = data.data[dataScrollOffset].ax;
			int prevY = data.data[dataScrollOffset].ay;

			// initialize stat values
			minX = prevX;
			maxX = prevX;
			minY = prevY;
			maxY = prevY;

			// draw first point
			DrawBox(SCREEN_TIMEPLOT_START, SCREEN_POS_CENTER_Y - prevY, SCREEN_TIMEPLOT_START,
			        SCREEN_POS_CENTER_Y - prevY, COLOR_BLUE, currXfb);

			// y is first so that x shows on top
			for (int i = 1; i < 500; i++) {
				// check if our x1 should be the previous point or our current data
				if (prevY > data.data[i + dataScrollOffset].ay) {
					DrawBox(SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - prevY,
					        SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - data.data[i + dataScrollOffset].ay,
					        COLOR_BLUE, currXfb);
				} else {
					DrawBox(SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - data.data[i + dataScrollOffset].ay,
					        SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - prevY,
					        COLOR_BLUE, currXfb);
				}
				prevY = data.data[i + dataScrollOffset].ay;

				// update stat values
				if (minY > prevY) {
					minY = prevY;
				}
				if (maxY < prevY) {
					maxY = prevY;
				}
			}

			// draw first point
			DrawBox(SCREEN_TIMEPLOT_START, SCREEN_POS_CENTER_Y - prevX, SCREEN_TIMEPLOT_START,
			        SCREEN_POS_CENTER_Y - prevX, COLOR_BLUE, currXfb);

			// x
			for (int i = 0; i < 500; i++) {
				if (prevX > data.data[i + dataScrollOffset].ax) {
					DrawBox(SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - prevX,
					        SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - data.data[i + dataScrollOffset].ax,
					        COLOR_RED, currXfb);
				} else {
					DrawBox(SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - data.data[i + dataScrollOffset].ax,
					        SCREEN_TIMEPLOT_START + i, SCREEN_POS_CENTER_Y - prevX,
					        COLOR_RED, currXfb);
				}

				prevX = data.data[i + dataScrollOffset].ax;

				// update stat values
				if (minX > prevX) {
					minX = prevX;
				}
				if (maxX < prevX) {
					maxX = prevX;
				}
			}
		}

		// do we have enough data to enable scrolling?
		if (data.endPoint >= 500) {
			//printf("\x1b[23;0H");
			printf("Use DPAD left/right to scroll waveform, hold R to move faster.");
			// does the user want to scroll the waveform?
			if (held & PAD_BUTTON_RIGHT) {
				if (held & PAD_TRIGGER_R) {
					if (dataScrollOffset + 505 < data.endPoint) {
						dataScrollOffset += 5;
					}
				} else {
					if (dataScrollOffset + 501 < data.endPoint) {
						dataScrollOffset++;
					}
				}
			} else if (held & PAD_BUTTON_LEFT) {
				if (held & PAD_TRIGGER_R) {
					if (dataScrollOffset - 5 >= 0) {
						dataScrollOffset -= 5;
					}
				} else {
					if (dataScrollOffset - 1 >= 0) {
						dataScrollOffset--;
					}
				}
			}
		}
		// print min/max data
		printf( "\x1b[21;0H");
		printf("Min X: %04d | Min Y: %04d\n", minX, minY);
		printf("Max X: %04d | Max Y: %04d\n", maxX, maxY);
	}

	// only start reading if A is pressed
	// TODO: figure out if this can be removed without having to gut the current poll logic, would be better for the user to not have to do this
	if ( pressed & PAD_BUTTON_A) {
		measureWaveform(&data);
		dataScrollOffset = 0;
		assert(data.endPoint < 5000);
	}
}

void menu_2dPlot(void *currXfb) {
	// var to keep track of the last point to draw
	static int lastDrawPoint;

	static WaveformDatapoint convertedCoords;

	// display instructions and data for user
	printf("Press A to start read\n");

	// do we have data that we can display?
	if (data.isDataReady) {
		convertedCoords = convertStickValues(&data.data[lastDrawPoint]);
		printf("%u samples, last point is: %d\n", data.endPoint + 1, lastDrawPoint + 1);
		printf("Use DPAD left/right to add/remove points from the plot,\nhold R to move faster, hold L for single point movements.");

		// print coordinates of last drawn point
		printf( "\x1b[21;0H");
		printf("Raw X: %04d | Raw Y: %04d\n", data.data[lastDrawPoint].ax, data.data[lastDrawPoint].ay);
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

		// draw plot
		// TODO: this needs to be gutted and replaced, this is not good code

		// y is negated because of how the graph is drawn
		for (int i = 0; i < lastDrawPoint; i++) {
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
	}

	// only start reading if A is pressed
	// TODO: figure out if this can be removed without having to gut the current poll logic, would be better for the user to not have to do this
	if ( pressed & PAD_BUTTON_A) {
		measureWaveform(&data);
		lastDrawPoint = data.endPoint;
		assert(data.endPoint < 5000);
	}
}
