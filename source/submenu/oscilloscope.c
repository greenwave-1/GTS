//
// Created on 2025/03/18.
//

// TODO: this needs to be refactored with different sticks and XY axis considered
// TODO: currently, this is an ugly bodge

#include "submenu/oscilloscope.h"

#include <stdlib.h>
#include <stdint.h>

#include <ogc/pad.h>
#include <ogc/timesupp.h>

#include "util/print.h"
#include "util/gx.h"
#include "util/polling.h"

const static uint8_t STICK_MOVEMENT_THRESHOLD = 15;
const static uint8_t STICK_MOVEMENT_TIME_THRESHOLD_MS = 25;
const static uint8_t MEASURE_COOLDOWN_FRAMES = 5;

const static uint8_t SCREEN_TIMEPLOT_START = 70;

static enum OSC_MENU_STATE state = OSC_SETUP;
static enum OSC_STATE oState = PRE_INPUT;

// structs for storing controller data
// data: used for display once marked ready
// temp: used by the callback function while data is being collected
// structs are flipped silently by calling flipData() from waveform.h, so we don't have to change anything here
static ControllerRec **data = NULL, **temp = NULL;
static enum OSCILLOSCOPE_TEST currentTest = SNAPBACK;
static int waveformScaleFactor = 1;
static int dataScrollOffset = 0;

// stores current captured data
static ControllerSample curr;

// acts as a prepend loop
// once conditions are met, ~25ms of previous data is added to the start of the capture from here
static ControllerSample startingLoop[200];
static int startingLoopIndex = 0;

static uint8_t stickCooldown = 0;
static bool stickReturnedToOrigin = true;
static int8_t snapbackStartPosX = 0, snapbackStartPosY = 0;
static bool snapbackCrossed64 = false;
static bool recordingInProgress = false;
static enum CONTROLLER_STICK_AXIS displayedAxis = AXIS_AXY;
static enum CONTROLLER_STICK_AXIS triggeringAxis = AXIS_AX;

static uint8_t ellipseCounter = 0;
static uint64_t prevSampleCallbackTick = 0;
static uint64_t sampleCallbackTick = 0;
static uint64_t timeStickInOrigin = 0;

static uint16_t *pressed = NULL;
static uint16_t *held = NULL;

static sampling_callback cb;
static void oscilloscopeCallback() {
	// time from last call of this function calculation
	prevSampleCallbackTick = sampleCallbackTick;
	sampleCallbackTick = gettime();
	if (prevSampleCallbackTick == 0) {
		prevSampleCallbackTick = sampleCallbackTick;
	}
	
	readController(false);

	// record current data
	curr.stickX = PAD_StickX(0);
	curr.stickY = PAD_StickY(0);
	curr.cStickX = PAD_SubStickX(0);
	curr.cStickY = PAD_SubStickY(0);
	curr.timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
	
	// add data to prepend loop
	startingLoop[startingLoopIndex] = curr;
	startingLoopIndex++;
	if (startingLoopIndex == 200) {
		startingLoopIndex = 0;
	}
	
	int8_t selectedStickX = 0, selectedStickY = 0;
	getControllerSampleAxisPair(curr, displayedAxis, &selectedStickX, &selectedStickY);
	
	// are we ready to check for stick inputs?
	if (oState != POST_INPUT_LOCK) {
		// data capture is currently happening
		if (recordingInProgress) {
			// capture recorded sample
			(*temp)->samples[(*temp)->sampleEnd] = curr;
			(*temp)->totalTimeUs += ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
			(*temp)->sampleEnd++;
			
			
			// a lot of logic is shared between tests, so specific code is checked for with if()
			// are we close to the origin?
			if ((abs(selectedStickX) < STICK_MOVEMENT_THRESHOLD) && (abs(selectedStickY) < STICK_MOVEMENT_THRESHOLD)) {
				timeStickInOrigin += ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
				stickReturnedToOrigin = true;
			} else {
				stickReturnedToOrigin = false;
				timeStickInOrigin = 0;
			}
			
			// dashback has a different max
			int currTestMaxSamples = REC_SAMPLE_MAX;
			if (currentTest == DASHBACK) {
				currTestMaxSamples = 500;
			}
			
			// have we either run out of data, or has the stick stopped moving for long enough?
			if ((*temp)->sampleEnd == currTestMaxSamples || ((timeStickInOrigin / 1000)) >= STICK_MOVEMENT_TIME_THRESHOLD_MS) {
				// snapback has a fallback condition that resets the current recording
				if (currentTest == SNAPBACK) {
					// are we stopped near the origin?
					if ((abs(selectedStickX) < STICK_MOVEMENT_THRESHOLD) &&
					    (abs(selectedStickY) < STICK_MOVEMENT_THRESHOLD)) {
						(*temp)->isRecordingReady = true;
					} else {
						// go back in the loop, we're holding a position somewhere outside origin
						// this will also reset if we're out of datapoints? not sure how this'll work
						(*temp)->sampleEnd = 0;
						recordingInProgress = false;
						snapbackStartPosX = selectedStickX;
						snapbackStartPosY = selectedStickY;
						snapbackCrossed64 = false;
						oState = POST_INPUT;
					}
				}
				// everyone else...
				else {
					(*temp)->isRecordingReady = true;
				}
			}
			
			// data is marked ready, there are a couple more things we need to do before allowing it to be shown...
			if ((*temp)->isRecordingReady) {
				// determine the intended axis, based on which axis reached the highest magnitude
				int8_t xMax = 0, yMax = 0;
				int8_t currX = 0, currY = 0;
				for (int i = 0; i < (*temp)->sampleEnd - 1; i++) {
					getControllerSampleAxisPair((*temp)->samples[i], displayedAxis, &currX, &currY);
					if (abs(currX) > xMax) {
						xMax = currX;
					}
					if (abs(currY) > yMax) {
						yMax = currY;
					}
				}
				
				// we give preference to the X axis in a tie
				// this looks a bit complicated, but saves a _bunch_ of if/else checks after this
				if (xMax >= yMax) {
					if (displayedAxis == AXIS_AXY) {
						triggeringAxis = AXIS_AX;
					} else {
						triggeringAxis = AXIS_CX;
					}
				} else {
					if (displayedAxis == AXIS_AXY) {
						triggeringAxis = AXIS_AY;
					} else {
						triggeringAxis = AXIS_CY;
					}
				}
				
				// pivot requires rewriting the recording...
				if (currentTest == PIVOT) {
					// this will truncate the recording to just the pivot input, after finding it
					uint64_t timeFromOriginCross = 0;
					bool crossed64Range = false;
					int8_t inputSign = 0;
					int pivotStartIndex = 0;
					bool hasCrossedOrigin = false;
					
					// read from back of list
					for (int i = (*temp)->sampleEnd - 1; i >= 0; i--) {
						int curr = getControllerSampleAxisValue((*temp)->samples[i], triggeringAxis);
						if (!crossed64Range) {
							if (curr >= 64 || curr <= -64) {
								crossed64Range = true;
								inputSign = curr;
							}
						} else if (!hasCrossedOrigin) {
							if (inputSign * curr < 0) {
								hasCrossedOrigin = true;
							}
						} else {
							timeFromOriginCross += (*temp)->samples[i].timeDiffUs;
							if (timeFromOriginCross / 1000 >= 50) {
								pivotStartIndex = i;
								break;
							}
						}
					}
					
					(*temp)->totalTimeUs = 0;
					
					// TODO: this is wasteful for a polling function, find a way to either specify a different
					// TODO: starting point, or do this outside of the function
					// rewrite data with new starting indexdrawXFirst
					for (int i = 0; i < (*temp)->sampleEnd - 1 - pivotStartIndex; i++) {
						(*temp)->samples[i].stickX = (*temp)->samples[i + pivotStartIndex].stickX;
						(*temp)->samples[i].stickY = (*temp)->samples[i + pivotStartIndex].stickY;
						(*temp)->samples[i].cStickX = (*temp)->samples[i + pivotStartIndex].cStickX;
						(*temp)->samples[i].cStickY = (*temp)->samples[i + pivotStartIndex].cStickY;
						(*temp)->samples[i].timeDiffUs = (*temp)->samples[i + pivotStartIndex].timeDiffUs;
						(*temp)->totalTimeUs += (*temp)->samples[i + pivotStartIndex].timeDiffUs;
					}
					(*temp)->totalTimeUs -= (*temp)->samples[0].timeDiffUs;
					(*temp)->samples[0].timeDiffUs = 0;
					(*temp)->sampleEnd = (*temp)->sampleEnd - pivotStartIndex - 1;
				}
				
				recordingInProgress = false;
				oState = POST_INPUT_LOCK;
				stickCooldown = MEASURE_COOLDOWN_FRAMES;
				snapbackStartPosX = 0;
				snapbackStartPosY = 0;
				snapbackCrossed64 = false;
				(*temp)->recordingType = REC_OSCILLOSCOPE;
				
				flipData();
			}
		}
		
		// data capture has not yet occurred
		else {
			// check criteria for each test to trigger a recording
			switch (currentTest) {
				case SNAPBACK:
					// we're waiting for the stick's 'falling' action
					if (snapbackCrossed64) {
						// current value is greater, adjust our top threshold
						if (abs(selectedStickX) > abs(snapbackStartPosX) || abs(selectedStickY) > abs(snapbackStartPosY)) {
							snapbackStartPosX = selectedStickX;
							snapbackStartPosY = selectedStickY;
						}
						// has the current value moved beyond STICK_MOVEMENT_THRESHOLD from snapbackStartPos
						else if (abs(selectedStickX) + STICK_MOVEMENT_THRESHOLD <= abs(snapbackStartPosX) ||
								abs(selectedStickY) + STICK_MOVEMENT_THRESHOLD <= abs(snapbackStartPosY)) {
							recordingInProgress = true;
						}
						
					}
					// we're looking for the stick to cross +-64
					else {
						if (abs(selectedStickX) > 64 || abs(selectedStickY) > 64) {
							snapbackCrossed64 = true;
						}
					}
					
					break;
				
				case PIVOT:
				case DASHBACK:
				default:
					// we're waiting for the stick to leave center
					if ((abs(selectedStickX) > STICK_MOVEMENT_THRESHOLD) || (abs(selectedStickY) > STICK_MOVEMENT_THRESHOLD)) {
						recordingInProgress = true;
					}
					break;
			}
			
			// did we meet a condition to start recording?
			if (recordingInProgress) {
				// assign first value
				clearRecordingArray(*temp);
				(*temp)->sampleEnd = 0;
				
				int currentIndex = 0;
				
				// if pivot or dashback, prepend data
				switch (currentTest) {
					case SNAPBACK:
					case DASHBACK:
						// prepend ~25 ms of data to the recording
						int loopStartIndex = startingLoopIndex - 1;
						if (loopStartIndex == -1) {
							loopStartIndex = 200;
						}
						int loopPrependCount = 0;
						uint64_t prependedTimeUs = 0;
						// go back ~25 ms
						while (prependedTimeUs < 25000) {
							// break out of loop if data doesn't exist
							if (startingLoop[loopStartIndex].timeDiffUs == 0) {
								break;
							}
							prependedTimeUs += startingLoop[loopStartIndex].timeDiffUs;
							loopStartIndex--;
							if (loopStartIndex == -1) {
								loopStartIndex = 200;
							}
							loopPrependCount++;
						}
						for (int i = 0; i < loopPrependCount; i++) {
							(*temp)->samples[(*temp)->sampleEnd] = startingLoop[(loopStartIndex + i) % 200];
							if ((*temp)->sampleEnd == 0) {
								(*temp)->samples[0].timeDiffUs = 0;
								(*temp)->totalTimeUs = 0;
							}
							(*temp)->totalTimeUs += (*temp)->samples[(*temp)->sampleEnd].timeDiffUs;
							(*temp)->sampleEnd++;
						}
						currentIndex = (*temp)->sampleEnd;
						break;
					default:
						curr.timeDiffUs = 0;
						break;
				}
				
				(*temp)->samples[currentIndex] = curr;
				(*temp)->totalTimeUs += curr.timeDiffUs;
				(*temp)->sampleEnd = currentIndex + 1;
				(*temp)->isRecordingReady = false;
				(*temp)->dataExported = false;
				oState = PRE_INPUT;
			}
		}
	}
	// stick hasn't returned to origin
	else if (!stickReturnedToOrigin) {
		if ((abs(selectedStickX) < STICK_MOVEMENT_THRESHOLD) &&
		    (abs(selectedStickY) < STICK_MOVEMENT_THRESHOLD)) {
			stickReturnedToOrigin = true;
		}
	}
}

static void displayInstructions() {
	setCursorPos(2, 0);
	printStr("Press X to cycle the current test, results will show above the\n"
			"waveform. Press Y to cycle between Analog Stick and C-Stick.\n"
			"Use DPAD left/right to scroll waveform when it is\n"
			"larger than the displayed area, hold R to move faster.");
	printStr("\n\nCURRENT TEST: ");
	switch (currentTest) {
		case SNAPBACK:
			printStr("SNAPBACK\n"
					"Check the min/max value on a given axis depending on where\n"
					"the stick started. The range in which Melee will not register\n"
					"a directional input is -22 to +22. Anything outside that range\n"
					"risks the game registering a non-neutral stick input.");
			break;
		case PIVOT:
			printStr("PIVOT\n"
					"For a successful pivot, the stick's position should stay\n"
					"above/below +64/-64 for ~16.6ms (1 frame). Less, and a pivot\n"
					"may not occur; more, and a dashback may occur. The stick\n"
					"should also hit 80/-80 on both sides.\n");
			break;
		case DASHBACK:
			printStr("DASHBACK\n"
					"A (vanilla) dashback will be successful when the stick doesn't\n"
					"get polled between 23 and 64, or -23 and -64.\n"
					"Less time in this range is better.");
			break;
		default:
			printStr("NO TEST SELECTED");
			break;
	}
	
	setCursorPos(21, 0);
	printStr("Press Z to close instructions.");
	
	if (*pressed & PAD_TRIGGER_Z) {
		state = OSC_POST_SETUP;
	}
}

// only run once
static void setup() {
	setSamplingRateHigh();
	if (pressed == NULL) {
		pressed = getButtonsDownPtr();
		held = getButtonsHeldPtr();
	}
	cb = PAD_SetSamplingCallback(oscilloscopeCallback);
	state = OSC_POST_SETUP;
	if(data == NULL) {
		data = getRecordingData();
		temp = getTempData();
	}
	if ((*data)->isRecordingReady && oState == PRE_INPUT) {
		oState = POST_INPUT_LOCK;
	}

	// check if existing recording is valid for this menu
	if (!(RECORDING_TYPE_VALID_MENUS[(*data)->recordingType] & REC_OSCILLOSCOPE_FLAG)) {
		clearRecordingArray(*data);
		oState = PRE_INPUT;
	}
}

// function called from outside
void menu_oscilloscope() {
	switch (state) {
		case OSC_SETUP:
			setup();
			break;
		case OSC_POST_SETUP:
			// we're getting the address of the object itself here, not the address of the pointer,
			// which means we will always point to the same object, regardless of a flip
			ControllerRec *dispData = *data;
			
			switch (oState) {
				case PRE_INPUT:
					printStr("Waiting for input.");
					printEllipse(ellipseCounter, 20);
					ellipseCounter++;
					if (ellipseCounter == 60) {
						ellipseCounter = 0;
					}
				case POST_INPUT_LOCK:
					// TODO: this is dumb, do this a better way to fit better
					if (oState == POST_INPUT_LOCK) {
						// dont allow new input until cooldown elapses
						if (stickCooldown != 0) {
							if (stickReturnedToOrigin) {
								stickCooldown--;
							}
							if (stickCooldown == 0) {
								oState = POST_INPUT;
							}
						} else {
							setCursorPos(2, 28);
							printStrColor(GX_COLOR_WHITE, GX_COLOR_BLACK, "LOCKED");
						}
					}
				case POST_INPUT:
					// draw guidelines based on selected test
					drawSolidBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128,
					             SCREEN_TIMEPLOT_START + 501, SCREEN_POS_CENTER_Y + 128, GX_COLOR_BLACK);
					if (displayedAxis == AXIS_AXY) {
						drawBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128,
						        SCREEN_TIMEPLOT_START + 501, SCREEN_POS_CENTER_Y + 128, GX_COLOR_WHITE);
					} else {
						drawBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128,
						        SCREEN_TIMEPLOT_START + 501, SCREEN_POS_CENTER_Y + 128, GX_COLOR_YELLOW);
					}
					drawLine(SCREEN_TIMEPLOT_START, SCREEN_POS_CENTER_Y,
							 SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y, GX_COLOR_GRAY);
					// lots of the specific values are taken from:
					// https://github.com/PhobGCC/PhobGCC-doc/blob/main/For_Users/Phobvision_Guide_Latest.md
					switch (currentTest) {
						case PIVOT:
							drawLine(SCREEN_TIMEPLOT_START, SCREEN_POS_CENTER_Y + 64,
									 SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 64, GX_COLOR_DARKGREEN);
							drawLine(SCREEN_TIMEPLOT_START, SCREEN_POS_CENTER_Y - 64,
									  SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y - 64, GX_COLOR_DARKGREEN);
							setCursorPos(8, 0);
							printStr("+64");
							setCursorPos(15, 0);
							printStr("-64");
							break;
						case DASHBACK:
							drawLine(SCREEN_TIMEPLOT_START,  SCREEN_POS_CENTER_Y + 64,
									  SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 64, GX_COLOR_DARKGREEN);
							drawLine(SCREEN_TIMEPLOT_START,  SCREEN_POS_CENTER_Y - 64,
									  SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y - 64, GX_COLOR_DARKGREEN);
							setCursorPos(8, 0);
							printStr("+64");
							setCursorPos(15, 0);
							printStr("-64");
						case SNAPBACK:
							drawLine(SCREEN_TIMEPLOT_START, SCREEN_POS_CENTER_Y + 23,
									  SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 23, GX_COLOR_DARKGREEN);
							drawLine(SCREEN_TIMEPLOT_START, SCREEN_POS_CENTER_Y - 23,
									  SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y - 23, GX_COLOR_DARKGREEN);
							setCursorPos(10, 0);
							printStr("+23");
							setCursorPos(13, 0);
							printStr("-23");
						default:
							break;
					}
					
					if (dispData->isRecordingReady) {
						// show all data if it will fit
						if (dispData->sampleEnd < 500) {
							dataScrollOffset = 0;
						// move screen to end of data input if it was further from the last capture
						} else if (dataScrollOffset > (dispData->sampleEnd) - 500) {
							dataScrollOffset = dispData->sampleEnd - 501;
						}
						
						// initialize stat values to first point
						int8_t minX, minY;
						int8_t maxX, maxY;
						
						getControllerSampleAxisPair(dispData->samples[dataScrollOffset], displayedAxis, &minX, &minY);
						maxX = minX;
						maxY = minY;

						int waveformXPos = 0;
						uint64_t drawnTicksUs = 0;
						
						int totalPointsToDraw = 0;
						
						// calculate how many points we're going to draw
						// we also get min and max values here
						for (int i = 0; i < 500; i++) {
							if (dataScrollOffset + i == dispData->sampleEnd) {
								break;
							}
							
							int8_t currX, currY;
							getControllerSampleAxisPair(dispData->samples[i], displayedAxis, &currX, &currY);
							
							if (minX > currX) {
								minX = currX;
							}
							if (maxX < currX) {
								maxX = currX;
							}
							if (minY > currY) {
								minY = currY;
							}
							if (maxY < currY) {
								maxY = currY;
							}
							
							// adding time from drawn points, to show how long the current view is
							drawnTicksUs += dispData->samples[i].timeDiffUs;
							
							totalPointsToDraw++;
						}
						
						updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
						
						bool drawXFirst = false;
						
						int magnitudeX = abs(maxX) >= abs(minX) ? abs(maxX) : abs(minX);
						int magnitudeY = abs(maxY) >= abs(minY) ? abs(maxY) : abs(minY);
						
						// this is slightly unintuitive
						// this will be false because we draw the axis with the larger magnitude _second_
						// which lets it show over the other axis
						// Also, we don't use triggeringAxis here since the magnitude is screen-local
						if (magnitudeY > magnitudeX) {
							drawXFirst = true;
						}
						
						for (int line = 0; line < 2; line++) {
							waveformXPos = 0;
							
							GX_Begin(GX_LINESTRIP, GX_VTXFMT0, totalPointsToDraw);
							
							for (int i = dataScrollOffset; i < dataScrollOffset + totalPointsToDraw; i++) {
								int8_t currX, currY;
								getControllerSampleAxisPair(dispData->samples[i], displayedAxis, &currX, &currY);
								
								if (drawXFirst) {
									GX_Position3s16(SCREEN_TIMEPLOT_START + waveformXPos, SCREEN_POS_CENTER_Y - currX, -2 + line);
									GX_Color3u8(GX_COLOR_RED_X.r, GX_COLOR_RED_X.g, GX_COLOR_RED_X.b);
								} else {
									GX_Position3s16(SCREEN_TIMEPLOT_START + waveformXPos, SCREEN_POS_CENTER_Y - currY, -2 + line);
									GX_Color3u8(GX_COLOR_BLUE_Y.r, GX_COLOR_BLUE_Y.g, GX_COLOR_BLUE_Y.b);
								}
								waveformXPos += waveformScaleFactor;
							}
							
							GX_End();
							
							drawXFirst = !drawXFirst;
						}
						
						// do we have enough data to enable scrolling?
						// TODO: enable scrolling when scaled
						if (dispData->sampleEnd >= 500 ) {
							// does the user want to scroll the waveform?
							if (*held & PAD_BUTTON_RIGHT) {
								if (*held & PAD_TRIGGER_R) {
									if (dataScrollOffset + 510 < dispData->sampleEnd) {
										dataScrollOffset += 10;
									}
								} else {
									if (dataScrollOffset + 501 < dispData->sampleEnd) {
										dataScrollOffset++;
									}
								}
							} else if (*held & PAD_BUTTON_LEFT) {
								if (*held & PAD_TRIGGER_R) {
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
						
						/*
						setCursorPos(2, 45);
						if (!showCStick) {
							printStr("Shown: Stick");
						} else {
							printStr("Shown: C-Stick");
						}
						 */
						setCursorPos(3, 0);
						
						// total time is stored in microseconds, divide by 1000 for milliseconds
						printStr("Sample start: %4u/%4u | Time shown: %4llu/%4llu ms\n", dataScrollOffset, dispData->sampleEnd, drawnTicksUs / 1000, dispData->totalTimeUs / 1000);
						
						// print test data
						setCursorPos(20, 0);
						
						// some test logic always uses the 'triggeringAxis',
						// so data would be wrong if we showed it when the other axis is shown
						enum CONTROLLER_STICK_AXIS workingAxis = triggeringAxis;
						if (displayedAxis == AXIS_AXY) {
							if (triggeringAxis == AXIS_CX) {
								// default value is fine
							}
							if (triggeringAxis == AXIS_CY) {
								workingAxis = AXIS_AY;
							}
						} else if (displayedAxis == AXIS_CXY) {
							if (triggeringAxis == AXIS_AX) {
								workingAxis = AXIS_CX;
							}
							if (triggeringAxis == AXIS_AY) {
								workingAxis = AXIS_CY;
							}
						}
						
						switch (currentTest) {
							case SNAPBACK:
								// highlight axis with biggest magnitude
								// if tied, preference will go to X
								// we highlight X if this is false
								if (!drawXFirst) {
									printStrBox(GX_COLOR_WHITE, "X Min, Max: (%4d,%4d)", minX, maxX);
									printStr("  |  Y Min, Max: (%4d,%4d)", minY, maxY);
								} else {
									printStr("X Min, Max: (%4d,%4d)  |  ", minX, maxX);
									printStrBox(GX_COLOR_WHITE, "Y Min, Max: (%4d,%4d)", minY, maxY);
								}
								break;
							case PIVOT:
								bool pivotHit80 = false;
								bool prevPivotHit80 = false;
								bool leftPivotRange = false;
								bool prevLeftPivotRange = false;
								int pivotStartIndex = -1, pivotEndIndex = -1;
								int pivotStartSign = 0;
								// start from the back of the list
								for (int i = dispData->sampleEnd - 1; i >= 0; i--) {
									int8_t curr = getControllerSampleAxisValue(dispData->samples[i], workingAxis);
									// check current coordinate for +-64 (dash threshold)
									if ((abs(curr) >= 64) && !leftPivotRange) {
										if (pivotEndIndex == -1) {
											pivotEndIndex = i;
										}
										// pivot input must hit 80 on both sides
										if (abs(curr) >= 80) {
											pivotHit80 = true;
										}
										// check if we didn't poll between the dash thresholds
										// (dolphin w/ bad pode)?
										if (pivotEndIndex != -1 && !leftPivotRange &&
										    (curr * getControllerSampleAxisValue(dispData->samples[pivotEndIndex], workingAxis) < 0)) {
											leftPivotRange = true;
											pivotStartIndex = i;
										}
									}
									
									// are we outside the pivot range and have already logged data of being in range
									if (pivotEndIndex != -1 && abs(curr) < 64) {
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
									if (abs(curr) >= 64 && leftPivotRange) {
										// used to ensure starting input is from the opposite side
										if (pivotStartSign == 0) {
											pivotStartSign = curr;
										}
										prevLeftPivotRange = true;
										if (abs(curr) >= 80) {
											prevPivotHit80 = true;
											break;
										}
									}
								}
								
								// phobvision doc says both sides need to hit 80 to succeed
								// multiplication is to ensure signs are correct
								int8_t pivotLastValue = getControllerSampleAxisValue(dispData->samples[pivotEndIndex], workingAxis);

								if (prevPivotHit80 && pivotHit80 && (pivotLastValue * pivotStartSign < 0)) {
									float noTurnPercent = 0;
									float pivotPercent = 0;
									float dashbackPercent = 0;

									uint64_t timeInPivotRangeUs = 0;
									for (int i = pivotStartIndex; i <= pivotEndIndex; i++) {
										timeInPivotRangeUs += dispData->samples[i].timeDiffUs;
									}

									// convert time to float in milliseconds
									float timeInPivotRangeMs = (timeInPivotRangeUs / 1000.0);

									// TODO: i think the calculation can be simplified here...
									// how many milliseconds could a poll occur that would cause a miss
									float diffFrameTimePoll = FRAME_TIME_MS_F - timeInPivotRangeMs;

									// negative time difference, dashback
									if (diffFrameTimePoll < 0) {
										dashbackPercent = ((diffFrameTimePoll * -1) / FRAME_TIME_MS_F) * 100;
										if (dashbackPercent > 100) {
											dashbackPercent = 100;
										}
										pivotPercent = 100 - dashbackPercent;
										// positive or 0 time diff, no turn
									} else {
										noTurnPercent = (diffFrameTimePoll / FRAME_TIME_MS_F) * 100;
										if (noTurnPercent > 100) {
											noTurnPercent = 100;
										}
										pivotPercent = 100 - noTurnPercent;
									}

									printStr("MS: %6.2f | No turn: %3.0f%% | Pivot: %3.0f%% | Dashback: %3.0f%%",
											timeInPivotRangeMs, noTurnPercent, pivotPercent, dashbackPercent);
								} else {
									printStr("MS:    0.0 | No turn:   0%% | Pivot:   0%% | Dashback: 100%%");
									//printStr("prev %d piv %d last %d start %d", prevPivotHit80, pivotHit80, pivotLastValue, pivotStartSign);
								}
								break;
							case DASHBACK:
								// go forward in list
								int dashbackStartIndex = -1, dashbackEndIndex = -1;
								uint64_t timeInRange = 0;
								bool stickInDashRange = false;
								
								// iterate over the list, find the start and end point where the X axis is in the
								// slow-turn range [23,63]
								// we also check if the X axis reaches the dash range (64+)
								for (int i = 0; i < dispData->sampleEnd; i++) {
									int8_t curr = getControllerSampleAxisValue(dispData->samples[i], workingAxis);
									// is the stick in the dash range?
									// note that this is independent from the other if
									if (abs(curr) >= 65) {
										// mark that we did cross that threshold
										stickInDashRange = true;
										// did we miss the slow-turn range?
										if (dashbackStartIndex == -1) {
											break;
										}
									}
									
									// is the stick in the slow-turn range
									if ((abs(curr) >= 23 && abs(curr) < 64)) {
										timeInRange += dispData->samples[i].timeDiffUs;
										// set this as the first sample that is in slow-turn range
										if (dashbackStartIndex == -1) {
											dashbackStartIndex = i;
										}
									} else if (dashbackStartIndex != -1) {
										dashbackEndIndex = i - 1;
										// stick has left the slow-turn range, there's no need to continue
										// checking the list
										break;
									}
								}
								
								float dashbackPercent = 0.0;
								float ucfPercent = 0.0;
								
								if (dashbackEndIndex == -1) {
									// stick did not get polled in slow-turn range, but did get to dash range,
									// so this should pass???
									if (stickInDashRange) {
										dashbackPercent = 100;
										ucfPercent = 100;
									}
									// stick did not hit dash range, fail
									else {
										dashbackPercent = 0.0;
										ucfPercent = 0.0;
									}
								} else {
									// convert time in microseconds to float time in milliseconds
									float timeInRangeMs = (timeInRange / 1000.0);

									// vanilla dashback is just the amount of time the stick was in the slow-turn range
									// over the total time for one frame
									// (subtracted from 1.0 because we want to show the _success_ rate)
									dashbackPercent = (1.0 - (timeInRangeMs / FRAME_TIME_MS_F)) * 100;

									// ucf dashback is a little more involved
									
									// we iterate over each sample in the slow-turn range, calculate what the next
									// frame would be based on that sample, and check if it meets the requirements
									// for the dash-intention check (total units moved across those two frame >75)
									uint64_t ucfTimeInRange = timeInRange;
									for (int i = dashbackStartIndex; i <= dashbackEndIndex; i++) {
										// for ucf testing, we need to find the samples that occur on both the
										// previous frame and the frame after
										uint64_t usFromPoll = 0;
										int nextPollIndex = i, prevPollIndex = i;
										
										// first find poll that would occur around 1f before
										// if this fails to find a poll due to running out of data, we'll assume
										// the poll happened around the origin
										while (usFromPoll < FRAME_TIME_US && prevPollIndex >= 0) {
											usFromPoll += dispData->samples[prevPollIndex].timeDiffUs;
											prevPollIndex--;
										}
										
										// reset this since we reuse it...
										usFromPoll = 0;
										
										// now find the poll that would occur around 1f after
										while (usFromPoll < FRAME_TIME_US && nextPollIndex != dispData->sampleEnd) {
											nextPollIndex++;
											usFromPoll += dispData->samples[nextPollIndex].timeDiffUs;
										}
										
										// exit if we've hit the end of the list somehow
										// idk how this would happen except _maybe_ on box
										if (usFromPoll < FRAME_TIME_US) {
											break;
										}
										
										// ucf dash intention check
										// the two frames need to move more than 75 units for UCF to convert it.
										// there are two cases we handle here:
										// - one where we found a poll ~1f before
										// - one where we do _not_ find a poll ~1f before
										// in the first case, we calculate how many total units the stick moved from
										// that previous poll, and in the second case we assume the poll was
										// around the origin, so we can just use the second poll's value as the total
										int8_t dashIntentionCount = 0;
										
										// first case: we need to actually find the total units moved
										if (prevPollIndex >= 0) {
											dashIntentionCount = abs(
													getControllerSampleAxisValue(dispData->samples[prevPollIndex], workingAxis) -
													getControllerSampleAxisValue(dispData->samples[i], workingAxis) );
											
											dashIntentionCount += abs(
													getControllerSampleAxisValue(dispData->samples[i], workingAxis) -
													getControllerSampleAxisValue(dispData->samples[nextPollIndex], workingAxis) );
										}
										// second case: we assume previous poll is at origin (0,0), and just use
										// the value in nextPollIndex
										else {
											dashIntentionCount = abs(getControllerSampleAxisValue(dispData->samples[nextPollIndex], workingAxis));
										}
										
										// did the stick move enough units to trigger the dash intention check?
										if (dashIntentionCount > 75) {
											// since we're iterating over each sample in the slow-turn range,
											// if the dash intention check passes, we subtract its time from
											// the total time that a slow-turn _could_ occur.
											// in theory, after this loops finishes, ucfTimeInRange will only contain
											// the time when the dash intention check could fail.
											ucfTimeInRange -= dispData->samples[i].timeDiffUs;
										}
									}

									float ucfTimeInRangeMs = ucfTimeInRange / 1000.0;
									
									// this means all of the samples in the dash range passed the intention check,
									// so we give it 100%
									if (ucfTimeInRangeMs <= 0) {
										ucfPercent = 100;
									} else {
										// one or more samples didn't pass the intention check, do normal calculation
										ucfPercent = (1.0 - (ucfTimeInRangeMs / FRAME_TIME_MS_F)) * 100;
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
								}
								
								printStr("Vanilla Success: %2.0f%% | UCF Success: %2.0f%%",
								         dashbackPercent, ucfPercent);
								break;
							default:
								printStr("Error?");
								break;

						}
					} else {
						oState = PRE_INPUT;
					}
					break;
				default:
					printStr("How did we get here?");
					break;
			}
			
			setCursorPos(21,0);
			printStr("Current test: ");
			switch (currentTest) {
				case SNAPBACK:
					printStr("Snapback");
					break;
				case PIVOT:
					printStr("Pivot");
					break;
				case DASHBACK:
					printStr("Dashback");
					break;
				default:
					printStr("Error");
					break;
			}
			
			
			if (*pressed & PAD_BUTTON_A && oState != PRE_INPUT) {
				if (oState == POST_INPUT_LOCK && stickCooldown == 0) {
					oState = POST_INPUT;
				} else {
					oState = POST_INPUT_LOCK;
				}
			} else if (*pressed & PAD_BUTTON_X && !recordingInProgress) {
				currentTest++;
				// check if we overrun our test length
				if (currentTest == OSCILLOSCOPE_TEST_LEN) {
					currentTest = SNAPBACK;
				}
			} else if (*pressed & PAD_TRIGGER_Z) {
				state = OSC_INSTRUCTIONS;
			} else if (*pressed & PAD_BUTTON_Y && !recordingInProgress) {
				if (displayedAxis == AXIS_AXY) {
					displayedAxis = AXIS_CXY;
				} else {
					displayedAxis = AXIS_AXY;
				}
			}
			
			// adjust scaling factor
			//} else if (pressed & PAD_BUTTON_Y) {
			//	waveformScaleFactor++;
			//	if (waveformScaleFactor > 5) {
			//		waveformScaleFactor = 1;
			//	}
			break;
		case OSC_INSTRUCTIONS:
			displayInstructions();
			break;
		default:
			printStr("How did we get here?");
			break;
	}
}

void menu_oscilloscopeEnd() {
	setSamplingRateNormal();
	PAD_SetSamplingCallback(cb);
	pressed = NULL;
	held = NULL;
	state = OSC_SETUP;
	stickReturnedToOrigin = true;
	if (!(*temp)->isRecordingReady) {
		(*temp)->sampleEnd = 0;
		recordingInProgress = false;
	}
}
