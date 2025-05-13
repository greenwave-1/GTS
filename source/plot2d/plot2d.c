//
// Created on 2025/05/09.
//

#include "plot2d.h"

#include <stdio.h>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include "../print.h"
#include "../polling.h"
#include "../draw.h"
#include "../images/stickmaps.h"

// close enough...
#define FRAME_TIME (1000.0 / 60.0)

static char strBuffer[100];

static u32 *pressed = NULL;
static u32 *held = NULL;
static bool buttonLock = false;
static u8 buttonPressCooldown = 0;

static enum PLOT_MENU_STATE menuState = PLOT_SETUP;
static enum PLOT_STATE plotState = PLOT_INPUT;

static WaveformData *data = NULL;
static WaveformDatapoint convertedCoords;
static int map2dStartIndex = 0;
static int lastDrawPoint = -1;
static int *indexPointer = NULL;

// enum for what image to draw in 2d plot
static enum IMAGE selectedImage = NO_IMAGE;


static sampling_callback cb;
static u64 prevSampleCallbackTick = 0;
static u64 sampleCallbackTick = 0;
static u64 pressedTimer = 0;
static u8 ellipseCounter = 0;
static bool pressLocked = false;

static bool stickMoved = false;
static int startingX, startingY;

static void plot2dSamplingCallback() {
	// time from last call of this function calculation
	prevSampleCallbackTick = sampleCallbackTick;
	sampleCallbackTick = gettime();
	if (prevSampleCallbackTick == 0) {
		prevSampleCallbackTick = sampleCallbackTick;
	}
	
	//static s8 x, y, cx, cy;
	PAD_ScanPads();
	
	// keep buttons in a "pressed" state long enough for code to see it
	// TODO: I don't like this implementation
	if (!pressLocked) {
		*pressed = PAD_ButtonsDown(0);
		if ((*pressed) != 0) {
			pressLocked = true;
			pressedTimer = gettime();
		}
	} else {
		if (ticks_to_millisecs(gettime() - pressedTimer) > 32) {
			pressLocked = false;
		}
	}
	
	*held = PAD_ButtonsHeld(0);
	
	if (plotState == PLOT_INPUT) {
	
	}
}

static void setup(WaveformData *d, u32 *p, u32 *h) {
	setSamplingRateHigh();
	pressed = p;
	held = h;
	cb = PAD_SetSamplingCallback(plot2dSamplingCallback);
	menuState = PLOT_POST_SETUP;
	if (data == NULL) {
		data = d;
	}
	if (d->isDataReady) {
		plotState = PLOT_DISPLAY;
	}
}

void printInstructions(void *currXfb) {
	printStr("Press X to cycle the stickmap background. Use DPAD\n"
			 "left/right to change what the last point drawn is.\n"
			 "Information on the last chosen point is displayed\n"
			 "at the bottom. Hold R to add or remove points faster.\n"
	         "Hold L to move one point at a time.\n\n"
			 "Hold Y to move the \"starting sample\" with the\n"
	         "same controls as above. Information for the selected\n"
			 "range is shown on the left.", currXfb);
	
	setCursorPos(21, 0);
	printStr("Press Z to close instructions.", currXfb);
	
	if (!buttonLock) {
		if (*pressed & PAD_TRIGGER_Z) {
			menuState = PLOT_POST_SETUP;
			buttonLock = true;
			buttonPressCooldown = 5;
		}
	}
}

void menu_plot2d(void *currXfb, WaveformData *d, u32 *p, u32 *h) {
	switch (menuState) {
		case PLOT_SETUP:
			setup(d, p, h);
			break;
		case PLOT_INSTRUCTIONS:
			printInstructions(currXfb);
			break;
		case PLOT_POST_SETUP:
			switch(plotState) {
				case PLOT_DISPLAY:
					printStr("Press A to start read, press Z for instructions", currXfb);
					
					// check if last draw point needs to be reset
					if (lastDrawPoint == -1) {
						lastDrawPoint = d->endPoint - 1;
					}
					convertedCoords = convertStickValues(&d->data[lastDrawPoint]);
					
					setCursorPos(5, 0);
					sprintf(strBuffer, "Total samples: %04u\n", d->endPoint);
					printStr(strBuffer, currXfb);
					sprintf(strBuffer, "Start sample: %04u\n", map2dStartIndex + 1);
					printStr(strBuffer, currXfb);
					sprintf(strBuffer, "End sample: %04u\n", lastDrawPoint + 1);
					printStr(strBuffer, currXfb);
					
					u64 timeFromStart = 0;
					for (int i = map2dStartIndex + 1; i <= lastDrawPoint; i++) {
						timeFromStart += d->data[i].timeDiffUs;
					}
					float timeFromStartMs = timeFromStart / 1000.0;
					sprintf(strBuffer, "Total MS: %6.2f\n", timeFromStartMs);
					printStr(strBuffer, currXfb);
					sprintf(strBuffer, "Total frames: %2.2f", timeFromStartMs / FRAME_TIME);
					printStr(strBuffer, currXfb);
					
					// print coordinates of last drawn point
					// raw stick coordinates
					setCursorPos(19, 0);
					sprintf(strBuffer, "Raw XY: (%04d,%04d)\n", d->data[lastDrawPoint].ax, d->data[lastDrawPoint].ay);
					printStr(strBuffer, currXfb);
					printStr("Melee XY: (", currXfb);
					// is the value negative?
					if (d->data[lastDrawPoint].ax < 0) {
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
					if (d->data[lastDrawPoint].ay < 0) {
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
							DrawDot(COORD_CIRCLE_CENTER_X + d->data[i].ax, SCREEN_POS_CENTER_Y - d->data[i].ay, COLOR_WHITE, currXfb);
						} else {
							DrawDot(COORD_CIRCLE_CENTER_X + d->data[i].ax, SCREEN_POS_CENTER_Y - d->data[i].ay, COLOR_GRAY, currXfb);
						}
					}
					
					// using a pointer so that the logic can be used for both moving the beginning and end
					if (*held & PAD_BUTTON_Y) {
						indexPointer = &map2dStartIndex;
					} else {
						indexPointer = &lastDrawPoint;
					}
					
					// cycle the stickmap shown
					if (*pressed & PAD_BUTTON_X && !buttonLock) {
						selectedImage++;
						if (selectedImage == IMAGE_LEN) {
							selectedImage = NO_IMAGE;
						}
						buttonLock = true;
						buttonPressCooldown = 5;
					// set up for reading data
					}
					if (*pressed & PAD_BUTTON_A && !buttonLock) {
						plotState = PLOT_INPUT;
						d->isDataReady = false;
						buttonLock = true;
						buttonPressCooldown = 5;
					}
					
					// holding L makes only individual presses work
					if (*held & PAD_TRIGGER_L) {
						if (*pressed & PAD_BUTTON_LEFT && !buttonLock) {
							if (*indexPointer - 1 >= 0) {
								(*indexPointer)--;
							}
							buttonLock = true;
							buttonPressCooldown = 5;
						} else if (*pressed & PAD_BUTTON_RIGHT && !buttonLock) {
							// this logic can't be generalized easily
							if (indexPointer == &map2dStartIndex) {
								if (map2dStartIndex + 1 < lastDrawPoint) {
									map2dStartIndex++;
								} else {
									map2dStartIndex = lastDrawPoint;
								}
							} else {
								if (lastDrawPoint + 1 < d->endPoint) {
									lastDrawPoint++;
								} else {
									lastDrawPoint = d->endPoint - 1;
								}
							}
							buttonLock = true;
							buttonPressCooldown = 5;
						}
					// holding R moves points faster
					} else if (*held & PAD_TRIGGER_R) {
						if (*held & PAD_BUTTON_LEFT) {
							if (*indexPointer - 5 >= 0) {
								(*indexPointer) -= 5;
							} else {
								(*indexPointer) = 0;
							}
						} else if (*held & PAD_BUTTON_RIGHT) {
							// this logic can't be generalized easily
							if (indexPointer == &map2dStartIndex) {
								if (map2dStartIndex + 5 < lastDrawPoint) {
									map2dStartIndex += 5;
								} else {
									map2dStartIndex = lastDrawPoint;
								}
							} else {
								if (lastDrawPoint + 5 < d->endPoint) {
									lastDrawPoint += 5;
								} else {
									lastDrawPoint = d->endPoint - 1;
								}
							}
						}
					// not holding either trigger, normal point movement
					} else {
						if (*held & PAD_BUTTON_LEFT) {
							if (*indexPointer - 1 >= 0) {
								(*indexPointer)--;
							}
						} else if (*held & PAD_BUTTON_RIGHT) {
							// this logic can't be generalized easily
							if (indexPointer == &map2dStartIndex) {
								if (map2dStartIndex+ + 1 < lastDrawPoint) {
									map2dStartIndex++;
								} else {
									map2dStartIndex = lastDrawPoint;
								}
							} else {
								if (lastDrawPoint + 1 < d->endPoint) {
									lastDrawPoint++;
								} else {
									lastDrawPoint = d->endPoint - 1;
								}
							}
						}
					}
					
					// make sure that the end never goes before the beginning
					if (lastDrawPoint < map2dStartIndex) {
						map2dStartIndex = lastDrawPoint;
					}
					
					break;
				case PLOT_INPUT:
					// nothing happens here other than showing the message about waiting for an input
					// the sampling callback function will change the plotState enum when an input is done
					setCursorPos(2,0);
					printStr("Waiting for input.", currXfb);
					if (ellipseCounter > 20) {
						printStr(".", currXfb);
					}
					if (ellipseCounter > 40) {
						printStr(".", currXfb);
					}
					ellipseCounter++;
					if (ellipseCounter == 60) {
						ellipseCounter = 0;
					}
					break;
				default:
					printStr("how did we get here?", currXfb);
					break;
			}
			
			if (!buttonLock) {
				if (*pressed & PAD_TRIGGER_Z) {
					menuState = PLOT_INSTRUCTIONS;
					buttonLock = true;
					buttonPressCooldown = 5;
				}
			}
			break;
		default:
			printStr("how did we get here? menuState", currXfb);
			break;
	}
	
	if (buttonLock) {
		// don't allow button press until a number of frames has passed
		if (buttonPressCooldown > 0) {
			buttonPressCooldown--;
		} else {
			// allow L and R to be held and not prevent buttonLock from being reset
			// this is needed _only_ because we have a check for a pressed button while another is held
			if ((*held) == 0 || *held & PAD_TRIGGER_L || *held & PAD_TRIGGER_R) {
				buttonLock = 0;
			}
		}
	}
}

void menu_plot2dEnd() {
	setSamplingRateNormal();
	PAD_SetSamplingCallback(cb);
	pressed = NULL;
	held = NULL;
	menuState = PLOT_SETUP;
	lastDrawPoint = -1;
}