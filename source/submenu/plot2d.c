//
// Created on 2025/05/09.
//

#include "submenu/plot2d.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <ogc/pad.h>
#include <ogc/timesupp.h>
#include <ogc/color.h>

#include "print.h"
#include "polling.h"
#include "draw.h"
#include "images/stickmaps.h"

// close enough...
static const float FRAME_TIME = (1000.0 / 60.0);

// orange for button press samples
#define COLOR_ORANGE 0xAD1EADBA

static char strBuffer[100];

static uint32_t *pressed = NULL;
static uint32_t *held = NULL;
static bool buttonLock = false;
static uint8_t buttonPressCooldown = 0;
static uint8_t captureStartFrameCooldown = 0;

static enum PLOT_2D_MENU_STATE menuState = PLOT_SETUP;
static enum PLOT_2D_STATE plotState = PLOT_INPUT;

static ControllerRec **data = NULL, **temp = NULL;
static int prevPosX = 0, prevPosY = 0;
static int currPosX = 0, currPosY = 0;
static int prevPosDiffX = 0, prevPosDiffY = 0;
static uint32_t currMovementHeldState = 0;
static uint32_t prevMovementHeldState = 0;
static uint64_t noMovementTimer = 0;

static int noMovementStartIndex = -1;
static bool haveStartPoint = false;
static bool captureStart = false;

static MeleeCoordinates convertedCoords;
static int map2dStartIndex = 0;
static int lastDrawPoint = -1;

static bool autoCapture = false;
static int autoCaptureCounter = 0;

// enum for what image to draw in 2d plot
static enum IMAGE selectedImage = NO_IMAGE;


static sampling_callback cb;
static uint64_t prevSampleCallbackTick = 0;
static uint64_t sampleCallbackTick = 0;
static uint64_t pressedTimer = 0;
static uint8_t ellipseCounter = 0;
static bool pressLocked = false;

static void plot2dSamplingCallback() {
	// time from last call of this function calculation
	prevSampleCallbackTick = sampleCallbackTick;
	sampleCallbackTick = gettime();
	
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
	
	if ((plotState == PLOT_INPUT || autoCapture) && captureStartFrameCooldown == 0) {
		// are we already capturing data?
		if (captureStart) {
			prevPosX = currPosX;
			prevPosY = currPosY;
			currPosX = PAD_StickX(0);
			currPosY = PAD_StickY(0);
			prevPosDiffX = abs(currPosX - prevPosX);
			prevPosDiffY = abs(currPosY - prevPosY);
			prevMovementHeldState = currMovementHeldState;
			currMovementHeldState = *held;
			
			(*temp)->samples[(*temp)->sampleEnd].stickX = currPosX;
			(*temp)->samples[(*temp)->sampleEnd].stickY = currPosY;
			(*temp)->samples[(*temp)->sampleEnd].cStickX = 0;
			(*temp)->samples[(*temp)->sampleEnd].cStickY = 0;
			(*temp)->samples[(*temp)->sampleEnd].buttons = *held;
			(*temp)->samples[(*temp)->sampleEnd].timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
			(*temp)->sampleEnd++;
			
			// are we currently checking if the stick has stopped moving?
			if ((prevPosDiffX < 2 && prevPosDiffY < 2 && prevMovementHeldState == currMovementHeldState) || (*temp)->sampleEnd == WAVEFORM_SAMPLES) {
				if (noMovementStartIndex == -1) {
					noMovementStartIndex = (*temp)->sampleEnd;
				} else {
					// for some reason this can't be timeDiffUs, it breaks specifically on first run in the menu
					// if a shorter capture already exists, weird...
					noMovementTimer += ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
				}
				
				// not moving for 250 ms
				if (noMovementTimer >= 250000 || (*temp)->sampleEnd == REC_SAMPLE_MAX) {
					if (noMovementStartIndex != -1) {
						(*temp)->sampleEnd = noMovementStartIndex;
					}
					(*temp)->isRecordingReady = true;
					(*temp)->recordingType = REC_2DPLOT;
					captureStartFrameCooldown = 5;
					plotState = PLOT_DISPLAY;
				}
			} else {
				noMovementStartIndex = -1;
				noMovementTimer = 0;
			}
		
		// get our initial start point, needed to know when to start actually recording
		} else if (!haveStartPoint) {
			bool setStartPoint = false;
			// just wait for stick to return to center
			if (autoCapture) {
				// using the melee deadzone values (+-23) instead of +-10,
				// since most controllers will be configured to not go past these values
				if (abs(PAD_StickX(0)) < 23 && abs(PAD_StickY(0)) < 23) {
					setStartPoint = true;
					// needed since stick will move fast if released, triggering another capture
					captureStartFrameCooldown = 5;
				}
			// wait for A to be released before allowing data capture
			} else if (*held == 0) {
				setStartPoint = true;
			}
			if (setStartPoint) {
				prevPosX = PAD_StickX(0);
				prevPosY = PAD_StickY(0);
				(*temp)->dataExported = false;
				haveStartPoint = true;
			}
		// wait for stick to move outside ~10 units, or for buttons to be pressed to start recording
		} else {
			currPosX = PAD_StickX(0);
			currPosY = PAD_StickY(0);
			if ( abs(currPosX - prevPosX) >= 10 || abs (currPosY - prevPosY) >= 10 ||
					(*held != 0 && !autoCapture)) {
				captureStart = true;
				(*temp)->samples[0].stickX = currPosX;
				(*temp)->samples[0].stickY = currPosY;
				(*temp)->samples[0].cStickX = 0;
				(*temp)->samples[0].cStickY = 0;
				(*temp)->samples[0].buttons = *held;
				(*temp)->samples[0].timeDiffUs = 0;
				(*temp)->totalTimeUs = 0;
				(*temp)->sampleEnd = 1;
				(*temp)->isRecordingReady = false;
			}
		}
		
		if ((*temp)->isRecordingReady && captureStart) {
			// calculate total read time
			for (int i = 0; i < (*temp)->sampleEnd; i++) {
				(*temp)->totalTimeUs += (*temp)->samples[i].timeDiffUs;
			}
			
			// reset stuff
			haveStartPoint = false;
			noMovementStartIndex = -1;
			noMovementTimer = 0;
			captureStart = false;
			flipData();
			lastDrawPoint = -1;
			map2dStartIndex = 0;
		}
	} else {
		// added to reset values to look for after captureStartFrameCooldown finishes when autocapturing
		prevPosX = 0, prevPosY = 0;
	}
}

static void setup(uint32_t *p, uint32_t *h) {
	setSamplingRateHigh();
	pressed = p;
	held = h;
	cb = PAD_SetSamplingCallback(plot2dSamplingCallback);
	menuState = PLOT_POST_SETUP;
	if (data == NULL) {
		data = getRecordingData();
		temp = getTempData();
	}
	plotState = PLOT_DISPLAY;
}

static void displayInstructions(void *currXfb) {
	setCursorPos(2, 0);
	printStr("Press A to prepare a recording. Recording will start with\n"
			 "any button press, or the stick moving.\n"
			 "Press X to cycle the stickmap background. Use DPAD left/right\n"
			 "to change what the last point drawn is. Information on the\n"
			 "last chosen point is shown on the left.\n\n"
			 "Hold R to add or remove points faster.\n"
	         "Hold L to move one point at a time.\n\n"
			 "Hold Y to move the \"starting sample\" with the\n"
	         "same controls as above. Information for the selected\n"
			 "range is shown on the left.\n\n"
	         "Hold Start to toggle Auto-Trigger. Enabling this removes\n"
	         "the need to press A, but disables the instruction menu and\n"
	         "only allows the stick to start a recording.\n", currXfb);
	
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

void menu_plot2d(void *currXfb, uint32_t *p, uint32_t *h) {
	switch (menuState) {
		case PLOT_SETUP:
			setup(p, h);
			break;
		case PLOT_INSTRUCTIONS:
			displayInstructions(currXfb);
			break;
		case PLOT_POST_SETUP:
			switch(plotState) {
				case PLOT_INPUT:
				case PLOT_DISPLAY:
					setCursorPos(2,0);
					if (plotState == PLOT_INPUT || autoCapture) {
						if (autoCapture) {
							printStr("Auto-Trigger enabled, w", currXfb);
						} else {
							printStr("W", currXfb);
						}
						printStr("aiting for input.", currXfb);
						printEllipse(ellipseCounter, 20, currXfb);
						ellipseCounter++;
						if (ellipseCounter == 60) {
							ellipseCounter = 0;
						}
					} else {
						printStr("Press A to start read, press Z for instructions", currXfb);
					}
					
					// check if last draw point needs to be reset
					if (lastDrawPoint == -1) {
						lastDrawPoint = (*data)->sampleEnd - 1;
					}
					if ((*data)->isRecordingReady) {
						convertedCoords = convertStickRawToMelee((*data)->samples[lastDrawPoint]);
						
						setCursorPos(5, 0);
						sprintf(strBuffer, "Total samples: %04u\n", (*data)->sampleEnd);
						printStr(strBuffer, currXfb);
						sprintf(strBuffer, "Start sample: %04u\n", map2dStartIndex + 1);
						printStr(strBuffer, currXfb);
						sprintf(strBuffer, "End sample: %04u\n", lastDrawPoint + 1);
						printStr(strBuffer, currXfb);
						
						// show button presses of last drawn point
						setCursorPos(11,0);
						printStr("Buttons Pressed:\n", currXfb);
						if ((*data)->samples[lastDrawPoint].buttons & PAD_BUTTON_A) {
							printStr("A ", currXfb);
						}
						if ((*data)->samples[lastDrawPoint].buttons & PAD_BUTTON_B) {
							printStr("B ", currXfb);
						}
						if ((*data)->samples[lastDrawPoint].buttons & PAD_BUTTON_X) {
							printStr("X ", currXfb);
						}
						if ((*data)->samples[lastDrawPoint].buttons & PAD_BUTTON_Y) {
							printStr("Y ", currXfb);
						}
						if ((*data)->samples[lastDrawPoint].buttons & PAD_TRIGGER_Z) {
							printStr("Z ", currXfb);
						}
						if ((*data)->samples[lastDrawPoint].buttons & PAD_TRIGGER_L) {
							printStr("L ", currXfb);
						}
						if ((*data)->samples[lastDrawPoint].buttons & PAD_TRIGGER_R) {
							printStr("R ", currXfb);
						}
						
						// print coordinates of last drawn point
						// raw stick coordinates
						setCursorPos(14, 0);
						sprintf(strBuffer, "Raw XY: (%04d,%04d)\n", (*data)->samples[lastDrawPoint].stickX,
						        (*data)->samples[lastDrawPoint].stickY);
						printStr(strBuffer, currXfb);
						printStr("Melee XY: (", currXfb);
						// is the value negative?
						if ((*data)->samples[lastDrawPoint].stickX < 0) {
							printStr("-", currXfb);
						} else {
							printStr("0", currXfb);
						}
						// is this a 1.0 value?
						if (convertedCoords.stickXUnit == 10000) {
							printStr("1.0000", currXfb);
						} else {
							sprintf(strBuffer, "0.%04d", convertedCoords.stickXUnit);
							printStr(strBuffer, currXfb);
						}
						printStr(",", currXfb);
						
						// is the value negative?
						if ((*data)->samples[lastDrawPoint].stickY < 0) {
							printStr("-", currXfb);
						} else {
							printStr("0", currXfb);
						}
						// is this a 1.0 value?
						if (convertedCoords.stickYUnit == 10000) {
							printStr("1.0000", currXfb);
						} else {
							sprintf(strBuffer, "0.%04d", convertedCoords.stickYUnit);
							printStr(strBuffer, currXfb);
						}
						printStr(")\n\n", currXfb);
						printStr("Stickmap: ", currXfb);
						
						// draw image below 2d plot, and print while we're at it
						switch (selectedImage) {
							case A_WAIT:
								printStr("Wait Attacks", currXfb);
								drawImage(currXfb, await_image, await_indexes, COORD_CIRCLE_CENTER_X - 127,
								          SCREEN_POS_CENTER_Y - 127);
								break;
							case CROUCH:
								printStr("Crouch", currXfb);
								drawImage(currXfb, crouch_image, crouch_indexes, COORD_CIRCLE_CENTER_X - 127,
								          SCREEN_POS_CENTER_Y - 127);
								break;
							case DEADZONE:
								printStr("Deadzones", currXfb);
								drawImage(currXfb, deadzone_image, deadzone_indexes, COORD_CIRCLE_CENTER_X - 127,
								          SCREEN_POS_CENTER_Y - 127);
								break;
							case LEDGE_L:
								printStr("Left Ledge", currXfb);
								drawImage(currXfb, ledgeL_image, ledgeL_indexes, COORD_CIRCLE_CENTER_X - 127,
								          SCREEN_POS_CENTER_Y - 127);
								break;
							case LEDGE_R:
								printStr("Right Ledge", currXfb);
								drawImage(currXfb, ledgeR_image, ledgeR_indexes, COORD_CIRCLE_CENTER_X - 127,
								          SCREEN_POS_CENTER_Y - 127);
								break;
							case MOVE_WAIT:
								printStr("Wait Movement", currXfb);
								drawImage(currXfb, movewait_image, movewait_indexes, COORD_CIRCLE_CENTER_X - 127,
								          SCREEN_POS_CENTER_Y - 127);
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
						
						uint64_t timeFromFirstSampleDraw = 0;
						int frameCounter = 0;
						// draw plot
						// y is negated because of how the graph is drawn
						// TODO: why does this need to be <= to avoid an off-by-one? step through logic later this is bugging me
						for (int i = map2dStartIndex; i <= lastDrawPoint; i++) {
							// don't add from the first value
							if (i != map2dStartIndex) {
								timeFromFirstSampleDraw += (*data)->samples[i].timeDiffUs;
							}

							// is this a frame interval?
							if ((timeFromFirstSampleDraw / 16666) > frameCounter) {
								if ((*data)->samples[i].buttons != 0) {
									DrawFilledCircle(COORD_CIRCLE_CENTER_X + (*data)->samples[i].stickX,
									                 SCREEN_POS_CENTER_Y - (*data)->samples[i].stickY, 2, COLOR_ORANGE, currXfb);
								} else {
									DrawFilledCircle(COORD_CIRCLE_CENTER_X + (*data)->samples[i].stickX,
									                 SCREEN_POS_CENTER_Y - (*data)->samples[i].stickY, 2, COLOR_WHITE, currXfb);
								}
								frameCounter++;
							// not a frame interval
							} else {
								if ((*data)->samples[i].buttons != 0) {
									DrawDot(COORD_CIRCLE_CENTER_X + (*data)->samples[i].stickX,
									        SCREEN_POS_CENTER_Y - (*data)->samples[i].stickY, COLOR_ORANGE, currXfb);
								} else {
									DrawDot(COORD_CIRCLE_CENTER_X + (*data)->samples[i].stickX,
									        SCREEN_POS_CENTER_Y - (*data)->samples[i].stickY, COLOR_WHITE, currXfb);
								}
							}
						}
						
						// highlight last sample with a box
						DrawBox( (COORD_CIRCLE_CENTER_X + (*data)->samples[lastDrawPoint].stickX) - 3,
						         (SCREEN_POS_CENTER_Y - (*data)->samples[lastDrawPoint].stickY) - 3,
						         (COORD_CIRCLE_CENTER_X + (*data)->samples[lastDrawPoint].stickX) + 3,
						         (SCREEN_POS_CENTER_Y - (*data)->samples[lastDrawPoint].stickY) + 3,
								 COLOR_WHITE, currXfb);
						
						float timeFromStartMs = timeFromFirstSampleDraw / 1000.0;
						setCursorPos(8, 0);
						sprintf(strBuffer, "Total MS: %6.2f\n", timeFromStartMs);
						printStr(strBuffer, currXfb);
						sprintf(strBuffer, "Total frames: %2.2f", timeFromStartMs / FRAME_TIME);
						printStr(strBuffer, currXfb);
						
						// cycle the stickmap shown
						if (*pressed & PAD_BUTTON_X && !buttonLock) {
							selectedImage++;
							if (selectedImage == IMAGE_LEN) {
								selectedImage = NO_IMAGE;
							}
							buttonLock = true;
							buttonPressCooldown = 5;
						}
						
						// holding L makes only individual presses work
						if (*held & PAD_TRIGGER_L) {
							if (*pressed & PAD_BUTTON_LEFT && !buttonLock) {
								// y button moves the starting point
								if (*held & PAD_BUTTON_Y) {
									// bounds check
									if (map2dStartIndex - 1 >= 0) {
										map2dStartIndex--;
									}
								} else {
									// dont let end point go past the start point
									if (lastDrawPoint - 1 >= map2dStartIndex) {
										lastDrawPoint--;
									}
								}
								buttonLock = true;
								buttonPressCooldown = 5;
							} else if (*pressed & PAD_BUTTON_RIGHT && !buttonLock) {
								// starting point
								if (*held & PAD_BUTTON_Y) {
									if (map2dStartIndex + 1 <= lastDrawPoint) {
										map2dStartIndex++;
									}
								} else {
									if (lastDrawPoint + 1 <= (*data)->sampleEnd) {
										lastDrawPoint++;
									}
								}
								buttonLock = true;
								buttonPressCooldown = 5;
							}
							// holding R moves points faster
						} else if (*held & PAD_TRIGGER_R) {
							if (*held & PAD_BUTTON_LEFT) {
								// starting point
								if (*held & PAD_BUTTON_Y) {
									if (map2dStartIndex - 5 >= 0) {
										map2dStartIndex -= 5;
									} else {
										// snap min to zero
										map2dStartIndex = 0;
									}
								} else {
									if (lastDrawPoint - 5 >= map2dStartIndex) {
										lastDrawPoint -= 5;
									} else {
										lastDrawPoint = map2dStartIndex;
									}
								}
							} else if (*held & PAD_BUTTON_RIGHT) {
								// starting point
								if (*held & PAD_BUTTON_Y) {
									if (map2dStartIndex + 5 <= lastDrawPoint) {
										map2dStartIndex += 5;
									} else {
										map2dStartIndex = lastDrawPoint;
									}
								} else {
									if (lastDrawPoint + 5 < (*data)->sampleEnd) {
										lastDrawPoint += 5;
									} else {
										lastDrawPoint = (*data)->sampleEnd - 1;
									}
								}
							}
						// not holding either trigger, normal point movement
						} else {
							if (*held & PAD_BUTTON_LEFT) {
								if (*held & PAD_BUTTON_Y) {
									if (map2dStartIndex - 1 >= 0) {
										map2dStartIndex--;
									}
								} else {
									if (lastDrawPoint - 1 >= map2dStartIndex) {
										lastDrawPoint--;
									}
								}
							} else if (*held & PAD_BUTTON_RIGHT) {
								if (*held & PAD_BUTTON_Y) {
									if (map2dStartIndex + 1 <= lastDrawPoint) {
										map2dStartIndex++;
									} else {
										map2dStartIndex = lastDrawPoint;
									}
								} else {
									if (lastDrawPoint + 1 < (*data)->sampleEnd) {
										lastDrawPoint++;
									} else {
										lastDrawPoint = (*data)->sampleEnd - 1;
									}
								}
							}
						}
						
						// make sure that the end never goes before the beginning
						// TODO: print debug message if this occurs
						if (lastDrawPoint < map2dStartIndex) {
							map2dStartIndex = lastDrawPoint;
						}
					}
					
					if (!buttonLock) {
						if (*pressed & PAD_TRIGGER_Z && !autoCapture) {
							menuState = PLOT_INSTRUCTIONS;
							buttonLock = true;
							buttonPressCooldown = 5;
						}
					}
					
					if ((*pressed & PAD_BUTTON_A && !buttonLock && !autoCapture) || captureStart) {
						plotState = PLOT_INPUT;
						(*temp)->isRecordingReady = false;
						buttonLock = true;
						buttonPressCooldown = 5;
					}
					
					// toggle auto-capture
					setCursorPos(21, 0);
					if (*held == PAD_BUTTON_START && !buttonLock && !captureStart) {
						if (autoCapture) {
							printStr("Disabling ", currXfb);
						} else {
							printStr("Enabling ", currXfb);
						}
						printStr("Auto-Trigger", currXfb);
						printEllipse(autoCaptureCounter, 40, currXfb);
						autoCaptureCounter++;
						if (autoCaptureCounter == 120) {
							autoCapture = !autoCapture;
							captureStart = false;
							haveStartPoint = false;
							buttonLock = true;
							buttonPressCooldown = 5;
							autoCaptureCounter = 0;
						}
					} else {
						printStr("Hold start to toggle Auto-Trigger.", currXfb);
						autoCaptureCounter = 0;
					}
					
					if (captureStartFrameCooldown != 0) {
						captureStartFrameCooldown--;
					}
					
					break;
				default:
					printStr("how did we get here?", currXfb);
					break;
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
	autoCaptureCounter = 0;
	autoCapture = false;
	if (!(*data)->isRecordingReady) {
		// reset stuff
		(*data)->sampleEnd = 0;
		haveStartPoint = false;
		noMovementStartIndex = -1;
		noMovementTimer = 0;
		captureStart = false;
	}
}