//
// Created on 10/25/23.
//

#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "waveform.h"
#include "images/stickmaps.h"
#include "images/custom_colors.h"

// center of screen, 640x480
#define SCREEN_POS_CENTER_X 320
#define SCREEN_POS_CENTER_Y 240

#define MENUITEMS_LEN 3
#define TEST_LEN 5

// 500 values displayed at once, SCREEN_POS_CENTER_X +/- 250
#define SCREEN_TIMEPLOT_START 70

enum WAVEFORM_TEST { SNAPBACK, PIVOT, DASHBACK, FULL, NO_TEST };

static enum WAVEFORM_TEST currentTest = SNAPBACK;

// enum to keep track of what menu to display, and what logic to run
static enum CURRENT_MENU currentMenu = MAIN_MENU;

// enum for what image to draw in 2d plot
static enum IMAGE selectedImage = SNAPBACK;

// main menu counter
static u8 mainMenuSelection = 0;

// counter for how many frames b or start have been held
static u8 bHeldCounter = 0;

// data for drawing a waveform
static WaveformData data = { { 0 }, 0, false, false };

// vars for what buttons are pressed or held
static u32 pressed = 0;
static u32 held = 0;

// menu item strings
static const char* menuItems[MENUITEMS_LEN] = { "Controller Test", "Measure Waveform", "2D Plot" };

static bool displayInstructions = false;

static int lastDrawPoint = -1;
static int dataScrollOffset = 0;


// most of this is taken from
// https://github.com/PhobGCC/PhobGCC-SW/blob/main/PhobGCC/rp2040/src/drawImage.cpp
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
				DrawBox(column, row, column, row, CUSTOM_COLORS[color - 5], currXfb);
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
			if (displayInstructions) {
				printf("Press X to cycle the current test, results will show above the waveform.\n"
					   "Use DPAD left/right to scroll waveform when it is larger than the\n"
					   "displayed area, hold R to move faster.\n\n"
					   "MODES:\n"
					   "Snapback: Check the min/max value on a given axis depending on where your\n"
					   "stick started. If you moved the stick left, check the Max value on a given\n"
					   "axis. Snapback can occur when the max value is at or above 23. If right,\n"
					   "then at or below -23\n\n"
					   "Pivot: For a successful pivot, you want the stick's position to stay\n"
					   "above/below +64/-64 for ~16.6ms (1 frame). Less, and you might get nothing,\n"
					   "more, and you might get a dashback. You also need the stick to hit 80/-80 on\n"
					   "both sides. Check the PhobVision docs for more info.\n\n"
					   "Dashback: A (vanilla) dashback will be successful when the stick doesn't get\n"
					   "polled between 23 and 64, or -23 and -64. Less time in this range is better,\n\n"
					   "Full Measure: Will fill the input buffer always, useful for longer inputs.\n");
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
		default:
			printf("HOW DID WE END UP HERE?\n");
			break;
	}

	// move cursor to bottom left
	printf( "\x1b[25;0H");

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
			if (currentMenu == WAVEFORM || currentMenu == PLOT_2D) {
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
	if (pressed & PAD_BUTTON_UP) {
		if (mainMenuSelection > 0) {
			mainMenuSelection--;
		}
	} else if (pressed & PAD_BUTTON_DOWN) {
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
	if (held & PAD_BUTTON_A) {
		printf("Pressed");
	}
	printf("\nB: ");
	if (held & PAD_BUTTON_B) {
		printf("Pressed");
	}
	printf("\nX: ");
	if (held & PAD_BUTTON_X) {
		printf("Pressed");
	}
	printf("\nY: ");
	if (held & PAD_BUTTON_Y) {
		printf("Pressed");
	}
	printf("\nZ: ");
	if (held & PAD_TRIGGER_Z) {
		printf("Pressed");
	}
	printf("\nStart: ");
	if (held & PAD_BUTTON_START) {
		printf("Pressed");
	}
	printf("\nDigital L: ");
	if (held & PAD_TRIGGER_L) {
		printf("Pressed");
	}
	printf("\nDigital R: ");
	if (held & PAD_TRIGGER_R) {
		printf("Pressed");
	}
	printf("\nDPAD Up: ");
	if (held & PAD_BUTTON_UP) {
		printf("Pressed");
	}
	printf("\nDPAD Down: ");
	if (held & PAD_BUTTON_DOWN) {
		printf("Pressed");
	}
	printf("\nDPAD Left: ");
	if (held & PAD_BUTTON_LEFT) {
		printf("Pressed");
	}
	printf("\nDPAD Right: ");
	if (held & PAD_BUTTON_RIGHT) {
		printf("Pressed");
	}

}

void menu_waveformMeasure(void *currXfb) {
	// TODO: I would bet that there's an off-by-one in here somewhere...

	// display instructions and data for user
	printf("Press A to start read, press Z for instructions\n");

	// do we have data that we can display?
	if (data.isDataReady) {

		printf("%u samples, drawing from sample %d\n", data.endPoint + 1, dataScrollOffset + 1);

		int minX, minY;
		int maxX, maxY;

		// draw guidelines based on selected test
		DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y, COLOR_GRAY, currXfb);
		// lots of the specific values are taken from:
		// https://github.com/PhobGCC/PhobGCC-doc/blob/main/For_Users/Phobvision_Guide_Latest.md
		switch (currentTest) {
			case PIVOT:
				DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 64, COLOR_GREEN, currXfb);
				DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y - 64, COLOR_GREEN, currXfb);
				printf( "\x1b[9;0H");
				printf("+64");
				printf( "\x1b[17;0H");
				printf("-64");
				break;
			case FULL:
			case DASHBACK:
				DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 64, COLOR_GREEN, currXfb);
				DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y - 64, COLOR_GREEN, currXfb);
				printf( "\x1b[9;0H");
				printf("+64");
				printf( "\x1b[17;0H");
				printf("-64");
			case SNAPBACK:
				DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 23, COLOR_GREEN, currXfb);
				DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y - 23, COLOR_GREEN, currXfb);
				printf( "\x1b[12;0H");
				printf("+23");
				printf( "\x1b[15;0H");
				printf("-23");
			default:
				break;
		}


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

		// print test data
		printf( "\x1b[5;0H");
		u16 pollCount = 0;
		switch (currentTest) {
			case SNAPBACK:
				printf("Min X: %04d | Min Y: %04d   |   ", minX, minY);
				printf("Max X: %04d | Max Y: %04d\n", maxX, maxY);
				break;
			case PIVOT:
				bool pivotHit80 = false;
				bool beforePivotHit80 = false;
				bool leftPivotRange = false;
				bool beforeLeftPivotRange = false;
				// start from the back of the list
				for (int i = data.endPoint; i >= 0; i--) {
					// check x coordinate for +-64
					if (data.data[i].ax >= 64 || data.data[i].ax <= -64) {
						if (data.data[i].ax >= 80 || data.data[i].ax <= -80) {
							pivotHit80 = true;
						}
						pollCount++;
					}

					// are we outside the pivot range and have already logged data of being in range
					if (pollCount > 0 && data.data[i].ax < 64 && data.data[i].ax > -64) {
						leftPivotRange = true;
						if (beforeLeftPivotRange || !pivotHit80) {
							break;
						}
					}

					// look for the initial input
					if ( (data.data[i].ax >= 64 || data.data[i].ax <= -64) && leftPivotRange) {
						beforeLeftPivotRange = true;
						if (data.data[i].ax >= 80 || data.data[i].ax <= -80) {
							beforePivotHit80 = true;
							break;
						}
					}
				}

				// phobvision doc says both sides need to hit 80 to succeed
				if (beforePivotHit80 && pivotHit80) {
					float noTurnPercent = 0;
					float pivotPercent = 0;
					float dashbackPercent = 0;

					// (16.6 - polls) / 16.6
					// gets amount of time that a no turn could occur, the get percentage
					noTurnPercent = (((1000.0 / 60.0) - (pollCount * 2)) / (1000.0 / 60.0)) * 100;
					if (noTurnPercent < 0) {
						noTurnPercent = 0;
					}

					// no turn could occur, calculate normally
					if ((pollCount * 2)< 17) {
						pivotPercent = ((float) (pollCount * 2) / (1000.0 / 60.0)) * 100;
					} else {
						// 33.3 - polls
						// opposite of the case above, we want the game to poll the second frame on a value below +-64
						pivotPercent = (1000.0 / 30.0) - (pollCount * 2) ;
						// get percentage
						pivotPercent = (pivotPercent / (1000.0 / 60.0)) * 100;
						if (pivotPercent < 0) {
							pivotPercent = 0;
						}

						// (polls - 16.6) / 16.6
						// amount of time that a dashback would be registered, provided polls >= 17
						dashbackPercent = (((float) (pollCount * 2) - (1000.0 / 60.0)) / (1000.0 / 60.0)) * 100;
						if (dashbackPercent > 100) {
							dashbackPercent = 100;
						}
					}

					printf("Polls in pivot range: %u, No turn: %2.0f%% | Empty Pivot: %2.0f%% | Dashback: %2.0f%%",
						   pollCount, noTurnPercent, pivotPercent, dashbackPercent);
				} else {
					printf("No pivot input detected.");
				}
				break;
			case DASHBACK:
				// go forward in list
				for (int i = 0; i < data.endPoint; i++) {
					// is the stick in the range
					if ((data.data[i].ax >= 23 && data.data[i].ax < 64) || (data.data[i].ax <= -23 && data.data[i].ax > -64)) {
						pollCount++;
					} else if (pollCount > 0) {
						break;
					}
				}

				float dashbackPercent = (((1000.0 / 60.0) - (pollCount * 2)) / (1000.0 / 60.0)) * 100;
				float ucfPercent = (((1000.0 / 30.0) - (pollCount * 2)) / (1000.0 / 60.0)) * 100;

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
				printf("Polls in fail range: %u | Vanilla Success: %2.0f%% | UCF Success: %2.0f%%", pollCount, dashbackPercent, ucfPercent);
				break;
			case FULL:
			case NO_TEST:
				break;
			default:
				printf("Error?");
				break;

		}
	}
	printf( "\x1b[23;0H");
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
		measureWaveform(&data);
		dataScrollOffset = 0;
		lastDrawPoint = data.endPoint;
		assert(data.endPoint < 5000);
	// does the user want to change the test?
	} else if (pressed & PAD_BUTTON_X) {
		currentTest++;
		if (currentTest == TEST_LEN) {
			currentTest = SNAPBACK;
		}
	}
}

void menu_2dPlot(void *currXfb) {
	static WaveformDatapoint convertedCoords;

	// display instructions and data for user
	printf("Press A to start read, press Z for instructions\n");

	// do we have data that we can display?
	if (data.isDataReady) {
		convertedCoords = convertStickValues(&data.data[lastDrawPoint]);
		printf("%u samples, last point is: %d\n", data.endPoint + 1, lastDrawPoint + 1);
		// TODO: move instructions under different prompt, so I don't have to keep messing with text placement

		// print coordinates of last drawn point
		printf( "\x1b[22;0H");
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
		measureWaveform(&data);
		dataScrollOffset = 0;
		lastDrawPoint = data.endPoint;
		assert(data.endPoint < 5000);
	}
}
