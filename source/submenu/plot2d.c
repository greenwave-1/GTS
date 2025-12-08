//
// Created on 2025/05/09.
//

#include "submenu/plot2d.h"

#include <stdlib.h>
#include <stdint.h>

#include <ogc/pad.h>
#include <ogc/timesupp.h>

#include "util/print.h"
#include "util/polling.h"

// orange for button press samples
#define COLOR_ORANGE 0xAD1EADBA

// used to make sure diagonals don't count as an input
const static uint16_t DPAD_MASK = PAD_BUTTON_UP | PAD_BUTTON_DOWN | PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT;
// these should not affect buttonLock logic
const static uint16_t PRESS_MASK = PAD_TRIGGER_L | PAD_TRIGGER_R | PAD_BUTTON_X;

static uint16_t *pressed = NULL;
static uint16_t *held = NULL;
static bool buttonLock = false;
static uint8_t buttonPressCooldown = 0;
static uint8_t captureStartFrameCooldown = 0;

static enum PLOT_2D_MENU_STATE menuState = PLOT_SETUP;
static enum PLOT_2D_STATE plotState = PLOT_INPUT;

// structs for storing controller data
// data: used for display once marked ready
// temp: used by the callback function while data is being collected
// structs are flipped silently by calling flipData() from waveform.h, so we don't have to change anything here
static ControllerRec **data = NULL, **temp = NULL;
static int startPosX = 0, startPosY = 0;
static int prevPosX = 0, prevPosY = 0;
static int currPosX = 0, currPosY = 0;
static int prevPosDiffX = 0, prevPosDiffY = 0;
static uint16_t currMovementHeldState = 0;
static uint16_t prevMovementHeldState = 0;
static uint64_t noMovementTimer = 0;

static int noMovementStartIndex = -1;
static bool haveStartPoint = false;
static bool captureStart = false;

static MeleeCoordinates convertedCoords;
static int map2dStartIndex = 0;
static int lastDrawPoint = -1;
static bool showCStick = false;

static bool autoCapture = false;
static int autoCaptureCounter = 0;

// enum for what image to draw in 2d plot
static enum IMAGE selectedImage = NO_IMAGE;
static enum IMAGE selectedImageCopy = NO_IMAGE;

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
	
	prevPosX = currPosX;
	prevPosY = currPosY;
	
	if (!showCStick) {
		currPosX = PAD_StickX(0);
		currPosY = PAD_StickY(0);
	} else {
		currPosX = PAD_SubStickX(0);
		currPosY = PAD_SubStickY(0);
	}
	
	if ((plotState == PLOT_INPUT || autoCapture) && captureStartFrameCooldown == 0) {
		// are we already capturing data?
		if (captureStart) {
			prevPosDiffX = abs(currPosX - prevPosX);
			prevPosDiffY = abs(currPosY - prevPosY);
			prevMovementHeldState = currMovementHeldState;
			currMovementHeldState = *held;
			
			(*temp)->samples[(*temp)->sampleEnd].stickX = PAD_StickX(0);
			(*temp)->samples[(*temp)->sampleEnd].stickY = PAD_StickY(0);
			(*temp)->samples[(*temp)->sampleEnd].cStickX = PAD_SubStickX(0);
			(*temp)->samples[(*temp)->sampleEnd].cStickY = PAD_SubStickY(0);
			(*temp)->samples[(*temp)->sampleEnd].buttons = *held;
			(*temp)->samples[(*temp)->sampleEnd].timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
			(*temp)->totalTimeUs += ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
			(*temp)->sampleEnd++;
			
			// are we currently checking if the stick has stopped moving?
			if ((prevPosDiffX < 2 && prevPosDiffY < 2 && prevMovementHeldState == currMovementHeldState) || (*temp)->sampleEnd == REC_SAMPLE_MAX) {
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
				if (abs(PAD_StickX(0)) < 23 && abs(PAD_StickY(0)) < 23 &&
						abs(PAD_SubStickX(0)) < 23 && abs(PAD_SubStickY(0)) < 23) {
					setStartPoint = true;
					// needed since stick will move fast if released, triggering another capture
					captureStartFrameCooldown = 5;
				}
			// wait for A to be released before allowing data capture
			} else if (*held == 0) {
				setStartPoint = true;
			}
			
			if (setStartPoint) {
				if (!showCStick) {
					startPosX = PAD_StickX(0);
					startPosY = PAD_StickY(0);
				} else {
					startPosX = PAD_SubStickX(0);
					startPosY = PAD_SubStickY(0);
				}
				haveStartPoint = true;
			}
		// wait for stick to move outside ~10 units, or for buttons to be pressed to start recording
		} else {
			if ( abs(currPosX - startPosX) >= 10 || abs (currPosY - startPosY) >= 10 ||
					(*held != 0 && !autoCapture)) {
				captureStart = true;
				clearRecordingArray(*temp);
				(*temp)->samples[0].stickX = PAD_StickX(0);
				(*temp)->samples[0].stickY = PAD_StickY(0);
				(*temp)->samples[0].cStickX = PAD_SubStickX(0);
				(*temp)->samples[0].cStickY = PAD_SubStickY(0);
				(*temp)->samples[0].buttons = *held;
				(*temp)->samples[0].timeDiffUs = 0;
				(*temp)->sampleEnd = 1;
				(*temp)->isRecordingReady = false;
				(*temp)->dataExported = false;
			}
		}
		
		if ((*temp)->isRecordingReady && captureStart) {
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
		startPosX = 0, startPosY = 0;
	}
}

static void setup() {
	setSamplingRateHigh();
	if (pressed == NULL) {
		pressed = getButtonsDownPtr();
		held = getButtonsHeldPtr();
	}
	cb = PAD_SetSamplingCallback(plot2dSamplingCallback);
	menuState = PLOT_POST_SETUP;
	if (data == NULL) {
		data = getRecordingData();
		temp = getTempData();
	}
	plotState = PLOT_DISPLAY;
	
	// check if existing recording is valid for this menu
	if (!(RECORDING_TYPE_VALID_MENUS[(*data)->recordingType] & REC_2DPLOT_FLAG)) {
		clearRecordingArray(*data);
	}
	
	// prevent pressing for a short time upon entering menu
	buttonLock = true;
	showCStick = false;
	buttonPressCooldown = 5;
}

static void displayInstructions() {
	setCursorPos(2, 0);
	printStr("Press A to prepare a recording. Recording will start with\n"
			 "any button press, or the stick moving.\n\n"
			 "Press X to cycle the stickmap background. Use DPAD left/right\n"
			 "to change what the last point drawn is. Information on the\n"
			 "last chosen point is shown on the left.\n\n"
			 "Hold R to go faster, or L to move one point at a time.\n\n"
			 "Hold Y to move the \"starting sample\" with the same controls as\n"
			 "above. Info for the selected range is shown on the left.\n\n"
	         "Hold Start to toggle Auto-Trigger. Enabling this removes\n"
	         "the need to press A, but disables the instruction menu (Z),\n"
			 "and only allows the stick to start a recording.\n");
	
	setCursorPos(21, 0);
	printStr("Press Z to close instructions.");
	
	if (!buttonLock) {
		if (*held & PAD_TRIGGER_Z) {
			menuState = PLOT_POST_SETUP;
			buttonLock = true;
			buttonPressCooldown = 5;
		}
	}
}

void menu_plot2d() {
	switch (menuState) {
		case PLOT_SETUP:
			setup();
			break;
		case PLOT_INSTRUCTIONS:
			displayInstructions();
			break;
		case PLOT_POST_SETUP:
			// we're getting the address of the object itself here, not the address of the pointer,
			// which means we will always point to the same object, regardless of a flip
			ControllerRec *dispData = *data;
			
			switch(plotState) {
				case PLOT_INPUT:
				case PLOT_DISPLAY:
					setCursorPos(2,0);
					if (plotState == PLOT_INPUT || autoCapture) {
						if (autoCapture) {
							printStr("Auto-Trigger enabled, w");
						} else {
							printStr("W");
						}
						printStr("aiting for input.");
						printEllipse(ellipseCounter, 20);
						ellipseCounter++;
						if (ellipseCounter == 60) {
							ellipseCounter = 0;
						}
					} else {
						printStr("Press A to prepare a recording, press Z for instructions");
					}
					
					// check if last draw point needs to be reset
					if (lastDrawPoint == -1) {
						lastDrawPoint = dispData->sampleEnd - 1;
					}
					
					setCursorPos(4, 0);
					printStr("Stick:    ");
					if (!showCStick) {
						printStr("Analog Stick");
					} else {
						printStr("C-Stick");
					}
					
					if (dispData->isRecordingReady) {
						convertedCoords = convertStickRawToMelee(dispData->samples[lastDrawPoint]);
						
						// print coordinates of last drawn point
						// raw stick coordinates
						setCursorPos(5, 0);
						printStr("Raw XY:   (");
						if (!showCStick) {
							printStr("%4d,%4d)\n", dispData->samples[lastDrawPoint].stickX,
							         dispData->samples[lastDrawPoint].stickY);
							printStr("Melee XY: (%s)", getMeleeCoordinateString(convertedCoords, AXIS_XY));
						} else {
							printStr("%4d,%4d)\n", dispData->samples[lastDrawPoint].cStickX,
							         dispData->samples[lastDrawPoint].cStickY);
							printStr("Melee XY: (%s)", getMeleeCoordinateString(convertedCoords, AXIS_CXY));
						}
						
						// show button presses of last drawn point
						setCursorPos(8, 0);
						printStr("Buttons Pressed:\n");
						if (dispData->samples[lastDrawPoint].buttons & PAD_BUTTON_A) {
							printStr("A ");
						}
						if (dispData->samples[lastDrawPoint].buttons & PAD_BUTTON_B) {
							printStr("B ");
						}
						if (dispData->samples[lastDrawPoint].buttons & PAD_BUTTON_X) {
							printStr("X ");
						}
						if (dispData->samples[lastDrawPoint].buttons & PAD_BUTTON_Y) {
							printStr("Y ");
						}
						if (dispData->samples[lastDrawPoint].buttons & PAD_TRIGGER_Z) {
							printStr("Z ");
						}
						if (dispData->samples[lastDrawPoint].buttons & PAD_TRIGGER_L) {
							printStr("L ");
						}
						if (dispData->samples[lastDrawPoint].buttons & PAD_TRIGGER_R) {
							printStr("R ");
						}
						
						drawSolidBox(COORD_CIRCLE_CENTER_X - 128, SCREEN_POS_CENTER_Y - 128,
						             COORD_CIRCLE_CENTER_X + 128, SCREEN_POS_CENTER_Y + 128,
						             GX_COLOR_BLACK);
						
						updateVtxDesc(VTX_TEX_NOCOLOR, GX_MODULATE);
						changeLoadedTexmap(TEXMAP_STICKMAPS);
						changeStickmapTexture((int) selectedImage);
						
						// draw image
						if (selectedImage != NO_IMAGE) {
							GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
							
							GX_Position3s16(COORD_CIRCLE_CENTER_X - 128, SCREEN_POS_CENTER_Y - 128, -5);
							GX_TexCoord2s16(0, 0);
							
							GX_Position3s16(COORD_CIRCLE_CENTER_X + 127, SCREEN_POS_CENTER_Y - 128, -5);
							GX_TexCoord2s16(255, 0);
							
							GX_Position3s16(COORD_CIRCLE_CENTER_X + 127, SCREEN_POS_CENTER_Y + 127, -5);
							GX_TexCoord2s16(255, 255);
							
							GX_Position3s16(COORD_CIRCLE_CENTER_X - 128, SCREEN_POS_CENTER_Y + 127, -5);
							GX_TexCoord2s16(0, 255);
							
							GX_End();
						}
						
						setCursorPos(16, 0);
						printStr("Stickmap: ");
						switch (selectedImage) {
							case A_WAIT:
								printStr("Wait Attacks");
								break;
							case CROUCH:
								printStr("Crouch");
								break;
							case DEADZONE:
								printStr("Deadzones");
								break;
							case LEDGE_L:
								printStr("Left Ledge");
								break;
							case LEDGE_R:
								printStr("Right Ledge");
								break;
							case MOVE_WAIT:
								printStr("Wait Movement");
								break;
							case NO_IMAGE:
								printStr("None");
							default:
								break;
						}
						
						setCursorPos(17, 0);
						printStr("Zone: %s", "TEMP");
						
						// draw box around plot area
						if (!showCStick) {
							drawBox(COORD_CIRCLE_CENTER_X - 128, SCREEN_POS_CENTER_Y - 128,
							        COORD_CIRCLE_CENTER_X + 128, SCREEN_POS_CENTER_Y + 128,
							        GX_COLOR_WHITE);
						} else {
							drawBox(COORD_CIRCLE_CENTER_X - 128, SCREEN_POS_CENTER_Y - 128,
							        COORD_CIRCLE_CENTER_X + 128, SCREEN_POS_CENTER_Y + 128,
							        GX_COLOR_YELLOW);
						}
						
						
						// we need to calculate vertices ahead of time
						
						uint64_t timeFromFirstSampleDraw = 0;
						
						int frameIntervalIndex = 0;
						int frameIntervalList[3000] = { -1 };
						
						// this is <= because lastDrawPoint is zero indexed
						for (int i = map2dStartIndex; i <= lastDrawPoint; i++) {
							// don't add from the first value
							if (i != map2dStartIndex) {
								timeFromFirstSampleDraw += dispData->samples[i].timeDiffUs;
							}
							if ((timeFromFirstSampleDraw / FRAME_TIME_US) > frameIntervalIndex) {
								frameIntervalList[frameIntervalIndex] = i;
								frameIntervalIndex++;
							}
						}
						
						int dataIndex = map2dStartIndex;
						int currFrameInterval = 0;
						
						updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
						
						// this is <= because lastDrawPoint is zero indexed
						while (dataIndex <= lastDrawPoint) {
							// is our current datapoint a frame interval?
							if (dataIndex == frameIntervalList[currFrameInterval]) {
								GX_SetPointSize(32, GX_TO_ZERO);
								GX_Begin(GX_POINTS, GX_VTXFMT0, 1);
								if (!showCStick) {
									GX_Position3s16(COORD_CIRCLE_CENTER_X + dispData->samples[dataIndex].stickX,
									                SCREEN_POS_CENTER_Y - dispData->samples[dataIndex].stickY, -4);
								} else {
									GX_Position3s16(COORD_CIRCLE_CENTER_X + dispData->samples[dataIndex].cStickX,
									                SCREEN_POS_CENTER_Y - dispData->samples[dataIndex].cStickY, -4);
								}
								if (dispData->samples[dataIndex].buttons != 0) {
									GX_Color3u8(GX_COLOR_ORANGE.r, GX_COLOR_ORANGE.g, GX_COLOR_ORANGE.b);
								} else {
									GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
								}
								currFrameInterval++;
								dataIndex++;
							}
							// samples between frame interval
							else {
								GX_SetPointSize(8, GX_TO_ZERO);
								
								int pointsToDraw;
								if (currFrameInterval != frameIntervalIndex) {
									pointsToDraw = frameIntervalList[currFrameInterval] - dataIndex;
								} else {
									pointsToDraw = lastDrawPoint - dataIndex + 1;
								}
								
								GX_Begin(GX_POINTS, GX_VTXFMT0, pointsToDraw);
								
								int endPoint = dataIndex + pointsToDraw;
								while (dataIndex < endPoint) {
									if (!showCStick) {
										GX_Position3s16(COORD_CIRCLE_CENTER_X + dispData->samples[dataIndex].stickX,
										                SCREEN_POS_CENTER_Y - dispData->samples[dataIndex].stickY, -4);
									} else {
										GX_Position3s16(COORD_CIRCLE_CENTER_X + dispData->samples[dataIndex].cStickX,
										                SCREEN_POS_CENTER_Y - dispData->samples[dataIndex].cStickY, -4);
									}
									if (dispData->samples[dataIndex].buttons != 0) {
										GX_Color3u8(GX_COLOR_ORANGE.r, GX_COLOR_ORANGE.g, GX_COLOR_ORANGE.b);
									} else {
										GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
									}
									dataIndex++;
								}
							}
							GX_End();
						}
						
						// highlight last sample with a box
						if (!showCStick) {
							drawBox((COORD_CIRCLE_CENTER_X + dispData->samples[lastDrawPoint].stickX) - 3,
							        (SCREEN_POS_CENTER_Y - dispData->samples[lastDrawPoint].stickY) - 3,
							        (COORD_CIRCLE_CENTER_X + dispData->samples[lastDrawPoint].stickX) + 3,
							        (SCREEN_POS_CENTER_Y - dispData->samples[lastDrawPoint].stickY) + 3,
							        GX_COLOR_WHITE);
						} else {
							drawBox((COORD_CIRCLE_CENTER_X + dispData->samples[lastDrawPoint].cStickX) - 3,
							        (SCREEN_POS_CENTER_Y - dispData->samples[lastDrawPoint].cStickY) - 3,
							        (COORD_CIRCLE_CENTER_X + dispData->samples[lastDrawPoint].cStickX) + 3,
							        (SCREEN_POS_CENTER_Y - dispData->samples[lastDrawPoint].cStickY) + 3,
							        GX_COLOR_WHITE);
						}
						
						setCursorPos(11, 0);
						printStr("Total samples: %11u\n", dispData->sampleEnd);
						printStr("Graph start/end: %4u/%4u", map2dStartIndex + 1, lastDrawPoint + 1);
						
						double timeFromStartMs = timeFromFirstSampleDraw / 1000.0;
						setCursorPos(13, 0);
						printStr("Total MS: %16.2f\n", timeFromStartMs);
						printStr("Total frames:%13.2f", timeFromStartMs / FRAME_TIME_MS_F);
						

						
						// cycle the stickmap shown
						if ((*held & DPAD_MASK) == PAD_BUTTON_UP && !buttonLock) {
							selectedImage++;
							if (!showCStick) {
								selectedImage %= IMAGE_LEN;
							} else {
								// only first 3 options are valid for c-stick
								selectedImage %= 3;
							}
							buttonLock = true;
							buttonPressCooldown = 5;
						} else if ((*held & DPAD_MASK) == PAD_BUTTON_DOWN && !buttonLock) {
							selectedImage--;
							if (selectedImage == -1) {
								if (!showCStick) {
									selectedImage = IMAGE_LEN - 1;
								} else {
									selectedImage = 2;
								}
							}
							buttonLock = true;
							buttonPressCooldown = 5;
						}
						
						// holding L makes only individual presses work
						if (*held & PAD_TRIGGER_L) {
							if (*held & PAD_BUTTON_LEFT && !buttonLock) {
								// x button moves the starting point
								if (*held & PAD_BUTTON_X) {
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
							} else if (*held & PAD_BUTTON_RIGHT && !buttonLock) {
								// starting point
								if (*held & PAD_BUTTON_X) {
									if (map2dStartIndex + 1 <= lastDrawPoint) {
										map2dStartIndex++;
									}
								} else {
									if (lastDrawPoint + 1 <= dispData->sampleEnd) {
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
								if (*held & PAD_BUTTON_X) {
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
								if (*held & PAD_BUTTON_X) {
									if (map2dStartIndex + 5 <= lastDrawPoint) {
										map2dStartIndex += 5;
									} else {
										map2dStartIndex = lastDrawPoint;
									}
								} else {
									if (lastDrawPoint + 5 < dispData->sampleEnd) {
										lastDrawPoint += 5;
									} else {
										lastDrawPoint = dispData->sampleEnd - 1;
									}
								}
							}
						// not holding either trigger, normal point movement
						} else {
							if (*held & PAD_BUTTON_LEFT) {
								if (*held & PAD_BUTTON_X) {
									if (map2dStartIndex - 1 >= 0) {
										map2dStartIndex--;
									}
								} else {
									if (lastDrawPoint - 1 >= map2dStartIndex) {
										lastDrawPoint--;
									}
								}
							} else if (*held & PAD_BUTTON_RIGHT) {
								if (*held & PAD_BUTTON_X) {
									if (map2dStartIndex + 1 <= lastDrawPoint) {
										map2dStartIndex++;
									} else {
										map2dStartIndex = lastDrawPoint;
									}
								} else {
									if (lastDrawPoint + 1 < dispData->sampleEnd) {
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
						if (*held & PAD_TRIGGER_Z && !autoCapture) {
							menuState = PLOT_INSTRUCTIONS;
							buttonLock = true;
							buttonPressCooldown = 5;
						} else if (*held & PAD_BUTTON_Y && plotState != PLOT_INPUT) {
							// cycle between analog and c-stick
							showCStick = !showCStick;
							
							// store and swap stickmaps
							enum IMAGE temp = selectedImageCopy;
							selectedImageCopy = selectedImage;
							selectedImage = temp;
							
							buttonLock = true;
							buttonPressCooldown = 5;
						}
					}
					
					if ((*held & PAD_BUTTON_A && !buttonLock && !autoCapture) || captureStart) {
						plotState = PLOT_INPUT;
						(*temp)->isRecordingReady = false;
						buttonLock = true;
						buttonPressCooldown = 5;
					}
					
					// toggle auto-capture
					setCursorPos(21, 0);
					if (*held == PAD_BUTTON_START && !buttonLock && !captureStart) {
						if (autoCapture) {
							printStr("Disabling ");
						} else {
							printStr("Enabling ");
						}
						printStr("Auto-Trigger");
						printEllipse(autoCaptureCounter, 40);
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
						printStr("Hold start to toggle Auto-Trigger.");
						autoCaptureCounter = 0;
					}
					
					if (captureStartFrameCooldown != 0) {
						captureStartFrameCooldown--;
					}
					
					break;
				default:
					printStr("how did we get here?");
					break;
			}
			break;
		default:
			printStr("how did we get here? menuState");
			break;
	}
	
	if (buttonLock) {
		// don't allow button press until a number of frames has passed
		if (buttonPressCooldown > 0 && ((*held) & (~PRESS_MASK)) == 0) {
			buttonPressCooldown--;
		} else if (buttonPressCooldown == 0) {
			// allow L, R, and X to be held and not prevent buttonLock from being reset
			// this is needed _only_ because we have a check for a pressed button while another is held
			if ((*held) == 0 || *held & PRESS_MASK) {
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
	if (!(*temp)->isRecordingReady) {
		// reset stuff
		(*temp)->sampleEnd = 0;
		haveStartPoint = false;
		noMovementStartIndex = -1;
		noMovementTimer = 0;
		captureStart = false;
	}
}