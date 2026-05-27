//
// Created on 2025/05/09.
//

#include "submenu/plot2d.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <ogc/pad.h>
#include <ogc/timesupp.h>

#include "util/print.h"
#include "util/polling.h"
#include "util/stickmap.h"

// orange for button press samples
#define COLOR_ORANGE 0xAD1EADBA

static uint16_t *pressed = NULL;
static uint16_t *held = NULL;
static uint8_t captureStartFrameCooldown = 0;

static enum PLOT_2D_MENU_STATE menuState = PLOT_SETUP;
static enum PLOT_2D_STATE plotState = PLOT_INPUT;
static enum PLOT_2D_STICKMAP_TYPE stickmapType = NO_STICKMAP;

static int selectedStickmap = 0;

static Stickmap **builtinStickmaps = NULL;
static int builtinStickmapsLen = 0;

static ExternalStickmap *externalJsonList = NULL;
static int externalJsonListLen = 0;
static int externalJsonIndex = 0;

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
static enum STICKMAP_WHICH_STICK whichStick = STICKMAP_A_STICK;

static bool autoCapture = false;
static int autoCaptureCounter = 0;
static bool autoCaptureStartReleased = true;

static sampling_callback cb;
static uint64_t prevSampleCallbackTick = 0;
static uint64_t sampleCallbackTick = 0;

static void plot2dSamplingCallback() {
	// time from last call of this function calculation
	prevSampleCallbackTick = sampleCallbackTick;
	sampleCallbackTick = gettime();

	readController(false);

	prevPosX = currPosX;
	prevPosY = currPosY;

	switch (whichStick) {
		case STICKMAP_C_STICK:
			currPosX = PAD_SubStickX(0);
			currPosY = PAD_SubStickY(0);
			break;
		case STICKMAP_A_STICK:
		default:
			// just in case something goes wrong...
			currPosX = PAD_StickX(0);
			currPosY = PAD_StickY(0);
			break;
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
				switch (whichStick) {
					case STICKMAP_C_STICK:
						startPosX = PAD_SubStickX(0);
						startPosY = PAD_SubStickY(0);
						break;
					case STICKMAP_A_STICK:
					default:
						// just in case something goes wrong...
						startPosX = PAD_StickX(0);
						startPosY = PAD_StickY(0);
						break;
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

	if (builtinStickmaps == NULL) {
		builtinStickmaps = getBuiltinStickmap(STICKMAP_PLOT2D, &builtinStickmapsLen);
	}

	if (externalJsonList == NULL) {
		externalJsonList = getExternalJsonList(&externalJsonListLen);
	}

	if (data == NULL) {
		data = getRecordingData();
		temp = getTempData();
	}

	cb = PAD_SetSamplingCallback(plot2dSamplingCallback);
	menuState = PLOT_POST_SETUP;
	plotState = PLOT_DISPLAY;

	autoCaptureStartReleased = true;

	// check if existing recording is valid for this menu
	if (!(RECORDING_TYPE_VALID_MENUS[(*data)->recordingType] & REC_2DPLOT_FLAG)) {
		clearRecordingArray(*data);
	}

	whichStick = STICKMAP_A_STICK;
	resetScrollingPrint();
}

static void displayInstructions() {
	startScrollingPrint(40, 70, 600, 400, GX_COLOR_WHITE);
	setWordWrap(true);

	printStr("Press A");
	drawFontButton(FONT_A);
	printStr("to prepare a recording. Recording will start when "
			 "any button is pressed, or when the stick moves.\n\n");

	printStr("Use");
	fontButtonSetDpadDirections(FONT_DPAD_UP | FONT_DPAD_DOWN);
	drawFontButton(FONT_DPAD);
	printStr("to change stickmap background. Use");
	fontButtonSetDpadDirections(FONT_DPAD_LEFT | FONT_DPAD_RIGHT);
	drawFontButton(FONT_DPAD);
	printStr("to change the graph\'s end point. Information on "
			 "the last drawn point is shown on the left.\n\n");

	printStr("Where applicable, press L");
	drawFontButton(FONT_L);
	printStr(" and Z");
	drawFontButton(FONT_Z);
	printStr("together to show a more detailed description of "
			 "the last sample's location.\n\n");

	printStr("Hold R");
	drawFontButton(FONT_R);
	printStr("to go faster, or L");
	drawFontButton(FONT_L);
	printStr("to move one point at a time.\n\n");

	printStr("Hold X");
	drawFontButton(FONT_X);
	printStr("to move the \"starting sample\" with the same controls "
			 "as above. Info for the selected range is shown on the left.\n\n");

	printStr("Press Y");
	drawFontButton(FONT_Y);
	printStr("to toggle which stick is captured.\n\n");

	printStr("Hold Start");
	drawFontButton(FONT_START);
	printStr("to toggle Auto-Trigger. Enabling this removes "
			 "the need to press A");
	drawFontButton(FONT_A);
	printStr(", but disables the instruction menu (Z");
	drawFontButton(FONT_Z);
	printStr("), showing the zone description (L");
	drawFontButton(FONT_L);
	printStr("+Z");
	drawFontButton(FONT_Z);
	printStr("), and only allows the stick to start a recording.");

	setWordWrap(false);
	endScrollingPrint();

	if (isControllerConnected(CONT_PORT_1)) {
		setCursorPos(0, 31);
		printStr("Close Instructions (Z");
		drawFontButton(FONT_Z);
		printStr(")");
	}

	if (*pressed == PAD_TRIGGER_Z && *held == PAD_TRIGGER_Z) {
		menuState = PLOT_POST_SETUP;
	}
}

static int dpadFlashIncrement = 0;
static bool showDesc = false;
static bool makingTextures = false;
static bool stickmapChanged = false;
static uint8_t ellipseCounter = 0;

void menu_plot2d() {
	// which stickmap array are we dealing with?
	Stickmap **displayList = NULL;
	int displayListLen = 0;
	switch (stickmapType) {
		case BUILTIN_STICKMAP:
			displayList = builtinStickmaps;
			displayListLen = builtinStickmapsLen;
			break;
		case EXTERNAL_STICKMAP:
			if (externalJsonList != NULL && externalJsonIndex != -1) {
				displayList = externalJsonList[externalJsonIndex].stickmapArr;
				displayListLen = externalJsonList[externalJsonIndex].stickmapArrLen;
				break;
			}
		case NO_STICKMAP:
		default:
			// fallthrough condition for cases above NO_STICKMAP
			stickmapType = NO_STICKMAP;
			break;
	}

	// doublecheck bounds
	if (selectedStickmap > displayListLen) {
		selectedStickmap = 0;
		stickmapChanged = true;
	}

	// load texture if needed
	if (stickmapChanged && stickmapType != NO_STICKMAP) {
		loadStickmapTexture(&displayList[selectedStickmap]->texture);
		//if (displayList[selectedStickmap]->whichStick != STICKMAP_NOT_SPECIFIED &&
				//displayList[selectedStickmap]->whichStick != whichStick) {
			//whichStick = displayList[selectedStickmap]->whichStick;
		//}
		stickmapChanged = false;
	}

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

			if (isControllerConnected(CONT_PORT_1)) {
				if (plotState != PLOT_INPUT) {
					if (!autoCapture) {
						setCursorPos(0, 32);
						printStr("View Instructions (Z");
						drawFontButton(FONT_Z);
						printStr(")");
						setCursorPos(1, 35);
						printStr("Load JSON (L");
						drawFontButton(FONT_L);
						printStr("+A");
						drawFontButton(FONT_A);
						printStr(")");
					}
					setCursorPos(2, 37);
					printStr("Toggle Stick (Y");
					drawFontButton(FONT_Y);
					printStr(")");
				}
			}

			switch(plotState) {
				case PLOT_INPUT:
				case PLOT_DISPLAY:
					setCursorPos(2,0);
					if (plotState == PLOT_INPUT || autoCapture) {
						printStr("Waiting for input.");
						printEllipse(ellipseCounter, 20);
						ellipseCounter++;
						if (ellipseCounter == 60) {
							ellipseCounter = 0;
						}
					} else {
						printStr("Press A");
						drawFontButton(FONT_A);
						printStr("to prepare a recording");
					}

					// check if last draw point needs to be reset
					if (lastDrawPoint == -1) {
						lastDrawPoint = dispData->sampleEnd - 1;
					}

					// draw box around plot area
					GXColor boxColor = GX_COLOR_WHITE;
					if (whichStick == STICKMAP_C_STICK) {
						boxColor = GX_COLOR_YELLOW;
					}
					drawBox(COORD_CIRCLE_CENTER_X - 128, SCREEN_POS_CENTER_Y - 128,
							COORD_CIRCLE_CENTER_X + 128, SCREEN_POS_CENTER_Y + 128,
							boxColor);


					setDepthForDrawCall(-10);
					drawSolidBox(COORD_CIRCLE_CENTER_X - 128, SCREEN_POS_CENTER_Y - 128,
								COORD_CIRCLE_CENTER_X + 128, SCREEN_POS_CENTER_Y + 128,
								GX_COLOR_BLACK);

					// selected stickmap
					setCursorPos(4, 0);
					printStr("Stickmap ");
					if (stickmapType != NO_STICKMAP) {
						printStr("%2d/%2d", selectedStickmap + 1, displayListLen);
					}
					setCursorPos(4, 14);
					printStr(" (");
					fontButtonSetDpadDirections(FONT_DPAD_UP | FONT_DPAD_DOWN);
					drawFontButton(FONT_DPAD);
					printStr("):  ");
					setCursorPos(5, 2);

					switch (stickmapType) {
						case BUILTIN_STICKMAP:
						case EXTERNAL_STICKMAP:
							if (strlen(displayList[selectedStickmap]->name) > 20) {
								printStr("%.*s...", 18, displayList[selectedStickmap]->name);
							} else {
								printStr(displayList[selectedStickmap]->name);
							}
							break;
						default:
							printStr("None");
							break;
					}

					// draw image
					if (stickmapType != NO_STICKMAP) {
						updateVtxDesc(VTX_TEXTURES, GX_MODULATE);
						changeLoadedTexmap(TEXMAP_STICKMAP);
						setDepthForDrawCall(-8);
						drawTextureFull(COORD_CIRCLE_CENTER_X - 128, SCREEN_POS_CENTER_Y - 128, (GXColor) { 200, 200, 200, 255 } );
					}

					if (dispData->isRecordingReady) {
						convertedCoords = convertStickRawToMelee(dispData->samples[lastDrawPoint]);

						// print coordinates of last drawn point
						// raw stick coordinates
						setCursorPos(14, 0);
						printStr("Raw XY:");
						setCursorPos(15, 2);
						switch (whichStick) {
							case STICKMAP_C_STICK:
								printStr("(%4d,%4d)\n", dispData->samples[lastDrawPoint].cStickX,
								         dispData->samples[lastDrawPoint].cStickY);
								printStr("Melee XY:\n  (%s)", getMeleeCoordinateString(convertedCoords, AXIS_CXY));
								break;
							case STICKMAP_A_STICK:
							default:
								printStr("(%4d,%4d)\n", dispData->samples[lastDrawPoint].stickX,
								         dispData->samples[lastDrawPoint].stickY);
								printStr("Melee XY:\n  (%s)", getMeleeCoordinateString(convertedCoords, AXIS_AXY));
								break;
						}

						// show button presses of last drawn point
						setCursorPos(18, 0);
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

						int subcatIndex = -1, descIndex = -1;
						setCursorPos(20, 0);
						if (stickmapType != NO_STICKMAP) {
							printStr("Zone (L");
							drawFontButton(FONT_L);
							printStr("+Z");
							drawFontButton(FONT_Z);
							printStr("): ");
							subcatIndex = getCoordSubcategory(convertedCoords, whichStick, displayList[selectedStickmap]);

							if (subcatIndex != -1) {
								for (int i = displayList[selectedStickmap]->subcategoryDescListLen - 1; i >= 0 ; i--) {
									if (subcatIndex >=
										displayList[selectedStickmap]->subcategoryDescList[i].listIndexStart) {
										descIndex = i;
										break;
									}
								}

								if (strlen(displayList[selectedStickmap]->subcategoryDescList[descIndex].name) > 34) {
									printStr("%d - %.*s...", subcatIndex + 1, 30, displayList[selectedStickmap]->subcategoryDescList[descIndex].name);
								} else {
									printStr(displayList[selectedStickmap]->subcategoryDescList[descIndex].name);
								}

							} else {
								printStr("None");
							}
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
								GX_Begin(GX_POINTS, VTXFMT_PRIMITIVES_INT, 1);
								switch (whichStick) {
									case STICKMAP_C_STICK:
										GX_Position3s16(COORD_CIRCLE_CENTER_X + dispData->samples[dataIndex].cStickX,
										                SCREEN_POS_CENTER_Y - dispData->samples[dataIndex].cStickY, -4);
										break;
									case STICKMAP_A_STICK:
									default:
										GX_Position3s16(COORD_CIRCLE_CENTER_X + dispData->samples[dataIndex].stickX,
										                SCREEN_POS_CENTER_Y - dispData->samples[dataIndex].stickY, -4);
										break;
								}

								if (dispData->samples[dataIndex].buttons != 0) {
									GX_Color4u8(GX_COLOR_ORANGE.r, GX_COLOR_ORANGE.g, GX_COLOR_ORANGE.b, GX_COLOR_ORANGE.a);
								} else {
									GX_Color4u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b, GX_COLOR_WHITE.b);
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

								GX_Begin(GX_POINTS, VTXFMT_PRIMITIVES_INT, pointsToDraw);

								int endPoint = dataIndex + pointsToDraw;
								while (dataIndex < endPoint) {
									switch (whichStick) {
										case STICKMAP_C_STICK:
											GX_Position3s16(COORD_CIRCLE_CENTER_X + dispData->samples[dataIndex].cStickX,
											                SCREEN_POS_CENTER_Y - dispData->samples[dataIndex].cStickY, -4);
											break;
										case STICKMAP_A_STICK:
										default:
											GX_Position3s16(COORD_CIRCLE_CENTER_X + dispData->samples[dataIndex].stickX,
											                SCREEN_POS_CENTER_Y - dispData->samples[dataIndex].stickY, -4);
											break;
									}
									if (dispData->samples[dataIndex].buttons != 0) {
										GX_Color4u8(GX_COLOR_ORANGE.r, GX_COLOR_ORANGE.g, GX_COLOR_ORANGE.b, GX_COLOR_WHITE.b);
									} else {
										GX_Color4u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b, GX_COLOR_WHITE.b);
									}
									dataIndex++;
								}
							}
							GX_End();
						}

						// highlight last sample with a box
						setDepthForDrawCall(-3);
						switch (whichStick) {
							case STICKMAP_C_STICK:
								drawBox((COORD_CIRCLE_CENTER_X + dispData->samples[lastDrawPoint].cStickX) - 3,
								        (SCREEN_POS_CENTER_Y - dispData->samples[lastDrawPoint].cStickY) - 3,
								        (COORD_CIRCLE_CENTER_X + dispData->samples[lastDrawPoint].cStickX) + 3,
								        (SCREEN_POS_CENTER_Y - dispData->samples[lastDrawPoint].cStickY) + 3,
								        GX_COLOR_WHITE);
								break;
							case STICKMAP_A_STICK:
							default:
								drawBox((COORD_CIRCLE_CENTER_X + dispData->samples[lastDrawPoint].stickX) - 3,
								        (SCREEN_POS_CENTER_Y - dispData->samples[lastDrawPoint].stickY) - 3,
								        (COORD_CIRCLE_CENTER_X + dispData->samples[lastDrawPoint].stickX) + 3,
								        (SCREEN_POS_CENTER_Y - dispData->samples[lastDrawPoint].stickY) + 3,
								        GX_COLOR_WHITE);
								break;
						}

						setCursorPos(3, 30);
						printStr("Total samples: %4u", dispData->sampleEnd);
						setCursorPos(7, 0);
						printStr("Graph start (");
						drawFontButton(FONT_X);
						printStr("+");
						fontButtonSetDpadDirections(FONT_DPAD_LEFT | FONT_DPAD_RIGHT);
						drawFontButton(FONT_DPAD);
						printStr("):\n %4u\n", map2dStartIndex + 1);

						printStr("Graph End(");
						fontButtonSetDpadDirections(FONT_DPAD_LEFT | FONT_DPAD_RIGHT);
						drawFontButton(FONT_DPAD);
						printStr("):\n %4u\n", lastDrawPoint + 1);

						double timeFromStartMs = timeFromFirstSampleDraw / 1000.0;
						printStr("Visible MS, frames:\n");
						printStr(" %7.2f ms, %5.2ff", timeFromStartMs, timeFromStartMs / FRAME_TIME_MS_F);

						if (showDesc) {
							if (stickmapType != NO_STICKMAP) {
								// dim background
								// TODO: this doesn't work as expected
								//  need to change scrollingprint to not draw in (0, 0) -> (640,480)
								//setAlphaForDrawCall(128);
								//setDepthForDrawCall(0);
								//drawSolidBox(0, 0, 640, 480, GX_COLOR_BLACK);
								startScrollingPrint(100, 100, 540, 350, GX_COLOR_WHITE);
								// clear screen in new bounds
								setDepthForDrawCall(0);
								drawSolidBox(0, 0, 640, 4800, GX_COLOR_BLACK);
								setWordWrap(true);

								printStr("Current List:\n - ");
								if (externalJsonIndex == -1) {
									printStr("Built-in\n\n");
								} else {
									printStr("%s\n\n", externalJsonList[externalJsonIndex].fileName);
								}
								printStr("Current Stickmap (");
								fontButtonSetDpadDirections(FONT_DPAD_UP | FONT_DPAD_DOWN);
								drawFontButton(FONT_DPAD);
								printStr("):\n - ");
								printStr("%s\n", displayList[selectedStickmap]->name);
								printStr("\nDescription:\n - ");
								printStr("%s\n\n", displayList[selectedStickmap]->desc);
								printStr("Zones:\n");
								for (int i = 0; i < displayList[selectedStickmap]->subcategoryDescListLen; i++) {
									printStr("%d - %s", i + 1, displayList[selectedStickmap]->subcategoryDescList[i].name);

									if (displayList[selectedStickmap]->subcategoryDescList[i].desc != NULL) {
										printStr(": %s", displayList[selectedStickmap]->subcategoryDescList[i].desc);
									}
									printStr("\n\n");
								}

								setWordWrap(false);
								endScrollingPrint();
							} else {
								// just in case...
								showDesc = false;
							}
						}

						// holding L makes only individual presses work
						if (*held & PAD_TRIGGER_L) {
							if (*pressed & PAD_BUTTON_LEFT) {
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
							} else if (*pressed & PAD_BUTTON_RIGHT) {
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

					// cycle the stickmap shown
					// we want up/down to be pressed, and nothing else held
					// checking *pressed here lets this be a one-shot until the button is released
					if (*pressed == PAD_BUTTON_UP && *held == PAD_BUTTON_UP) {
						stickmapChanged = true;
						if (stickmapType == NO_STICKMAP) {
							stickmapType = BUILTIN_STICKMAP;
						} else {
							selectedStickmap++;
							selectedStickmap %= displayListLen;
						}
					} else if (*pressed == PAD_BUTTON_DOWN && *held == PAD_BUTTON_DOWN) {
						stickmapChanged = true;
						if (stickmapType == NO_STICKMAP) {
							stickmapType = BUILTIN_STICKMAP;
						} else {
							selectedStickmap--;
							if (selectedStickmap == -1) {
								selectedStickmap = displayListLen - 1;
							}
						}
					}

					if (!autoCapture && plotState != PLOT_INPUT) {
						if (*pressed == PAD_TRIGGER_Z && *held == PAD_TRIGGER_Z) {
							menuState = PLOT_INSTRUCTIONS;
						} else if (*pressed & PAD_BUTTON_Y) {
							// cycle between analog and c-stick
							// TODO: rewrite for new texture system
							if (whichStick == STICKMAP_A_STICK) {
								whichStick = STICKMAP_C_STICK;
							} else {
								whichStick = STICKMAP_A_STICK;
							}
						} else if (*pressed == PAD_TRIGGER_Z && *held == (PAD_TRIGGER_Z | PAD_TRIGGER_L)) {
							showDesc = !showDesc;
						}
					}

					if ((*pressed == PAD_BUTTON_A && *held == PAD_BUTTON_A && !autoCapture) || captureStart) {
						plotState = PLOT_INPUT;
						(*temp)->isRecordingReady = false;
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
		case PLOT_FILE_PICKER:
			if (externalJsonIndex == -1) {
				if (isControllerConnected(CONT_PORT_1)) {
					setCursorPos(1, 27);
					printStr("Close File Picker (L");
					drawFontButton(FONT_L);
					printStr("+A");
					drawFontButton(FONT_A);
					printStr(")");
				}
				externalJsonIndex = drawJsonFilePicker(externalJsonList);
				if (externalJsonIndex != -1 && !externalJsonList[externalJsonIndex].generatedTextures) {
					makingTextures = true;
					generateStickmapTextureAsync(externalJsonList[externalJsonIndex].stickmapArr, externalJsonList[externalJsonIndex].stickmapArrLen);
					externalJsonList[externalJsonIndex].generatedTextures = true;
				}
			} else if (makingTextures) {
				setCursorPos(10, 11);
				printStr("Generating textures, please wait ");
				printSpinningLineInterval(10);
				if (isExternalStickmapReady()) {
					makingTextures = false;
				}
			} else {
				menuState = PLOT_POST_SETUP;
				stickmapChanged = true;
				selectedStickmap = 0;
				stickmapType = EXTERNAL_STICKMAP;
			}
			break;
		default:
			printStr("how did we get here? menuState");
			break;
	}

	if (!autoCapture && menuState != PLOT_SETUP) {
		// L + A -> toggle file selection
		if (*held == (PAD_BUTTON_A | PAD_TRIGGER_L) && (*pressed & (PAD_BUTTON_A | PAD_TRIGGER_L))) {
			if (menuState == PLOT_POST_SETUP) {
				menuState = PLOT_FILE_PICKER;
				selectedStickmap = 0;
				//selectedStickmapSub = 0;
				externalJsonIndex = -1;
				stickmapType = NO_STICKMAP;
			} else if (menuState == PLOT_FILE_PICKER) {
				menuState = PLOT_POST_SETUP;
				externalJsonIndex = -1;
				selectedStickmap = 0;
				stickmapType = NO_STICKMAP;
			}
		}
	}
	fontButtonFlashIncrement(&dpadFlashIncrement, 30);
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
	showDesc = false;
	if (!(*temp)->isRecordingReady) {
		// reset stuff
		(*temp)->sampleEnd = 0;
		haveStartPoint = false;
		noMovementStartIndex = -1;
		noMovementTimer = 0;
		captureStart = false;
	}
}

void menu_plot2dSetAutoTrigger(bool captureState) {
	if (autoCapture != captureState) {
		autoCapture = captureState;
		captureStart = false;
		haveStartPoint = false;
	}
}
