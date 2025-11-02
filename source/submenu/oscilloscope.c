//
// Created on 2025/03/18.
//

#include "submenu/oscilloscope.h"

#include <stdlib.h>
#include <stdint.h>

#include <ogc/pad.h>
#include <ogc/timesupp.h>

#include "print.h"
#include "gx.h"
#include "polling.h"

const static uint8_t STICK_MOVEMENT_THRESHOLD = 5;
const static uint8_t STICK_ORIGIN_TIME_THRESHOLD_MS = 50;
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
static int8_t snapbackStartPosX = 0, snapbackStartPosY = 0;
static int8_t snapbackPrevPosX = 0, snapbackPrevPosY = 0;
static bool snapbackCrossed64 = false;
static bool pressLocked = false;
static bool stickMove = false;
static bool showCStick = false;
static bool display = false;

static uint8_t ellipseCounter = 0;
static uint64_t pressedTimer = 0;
static uint64_t prevSampleCallbackTick = 0;
static uint64_t sampleCallbackTick = 0;
static uint64_t timeStickInOrigin = 0;
static uint64_t timeStoppedMoving = 0;

static uint16_t *pressed = NULL;
static uint16_t *held = NULL;
static bool buttonLock = false;
static uint8_t buttonPressCooldown = 0;

static sampling_callback cb;
static void oscilloscopeCallback() {
	// time from last call of this function calculation
	prevSampleCallbackTick = sampleCallbackTick;
	sampleCallbackTick = gettime();
	if (prevSampleCallbackTick == 0) {
		prevSampleCallbackTick = sampleCallbackTick;
	}
	
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
	
	int8_t selectedStickX, selectedStickY;
	if (showCStick) {
		selectedStickX = curr.cStickX;
		selectedStickY = curr.cStickY;
	} else {
		selectedStickX = curr.stickX;
		selectedStickY = curr.stickY;
	}
	
	// are we ready to check for stick inputs?
	if (oState != POST_INPUT_LOCK) {
		// data capture is currently happening
		if (stickMove) {
			// capture recorded sample
			(*temp)->samples[(*temp)->sampleEnd] = curr;
			(*temp)->totalTimeUs += ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
			(*temp)->sampleEnd++;
			
			// handle specific logic for each test
			switch (currentTest) {
				// TODO: change this to have a buffer of a minimum amount of time, to make final graph look better
				case SNAPBACK:
					// are we close to the origin?
					if ((abs(selectedStickX) < STICK_MOVEMENT_THRESHOLD) && (abs(selectedStickY) < STICK_MOVEMENT_THRESHOLD)) {
						timeStoppedMoving += ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
					} else {
						timeStoppedMoving = 0;
					}
					
					snapbackPrevPosX = selectedStickX;
					snapbackPrevPosY = selectedStickY;
					
					// have we either run out of data, or has the stick stopped moving for long enough?
					if ((*temp)->sampleEnd == REC_SAMPLE_MAX || ((timeStoppedMoving / 1000)) >= STICK_MOVEMENT_TIME_THRESHOLD_MS) {
						// are we stopped near the origin?
						if ((abs(selectedStickX) < STICK_MOVEMENT_THRESHOLD) && (abs(selectedStickY) < STICK_MOVEMENT_THRESHOLD)) {
							(*temp)->isRecordingReady = true;
						} else {
							// go back in the loop, we're holding a position somewhere outside origin
							// this will also reset if we're out of datapoints? not sure how this'll work
							(*temp)->sampleEnd = 0;
							stickMove = false;
							snapbackStartPosX = selectedStickX;
							snapbackStartPosY = selectedStickY;
							snapbackCrossed64 = false;
							oState = POST_INPUT;
						}
					}
					break;
				
				case PIVOT:
					// are we close to the origin?
					// this doesn't use the selectedStick values, idk if c-stick pivots are a thing...
					if ((abs(curr.stickX) < STICK_MOVEMENT_THRESHOLD) && (abs(curr.stickY) < STICK_MOVEMENT_THRESHOLD)) {
						timeStickInOrigin += (ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick));
					} else {
						timeStickInOrigin = 0;
					}
					
					// have we met conditions to end recording?
					if ((*temp)->sampleEnd == REC_SAMPLE_MAX || (timeStickInOrigin / 1000) >= STICK_ORIGIN_TIME_THRESHOLD_MS) {
						// this will truncate the recording to just the pivot input
						uint64_t timeFromOriginCross = 0;
						bool crossed64Range = false;
						int8_t inputSign = 0;
						int pivotStartIndex = 0;
						bool hasCrossedOrigin = false;
						for (int i = (*temp)->sampleEnd - 1; i >= 0; i--) {
							if (!crossed64Range) {
								if ((*temp)->samples[i].stickX >= 64 || (*temp)->samples[i].stickX <= -64) {
									crossed64Range = true;
									inputSign = (*temp)->samples[i].stickX;
								}
							} else if (!hasCrossedOrigin) {
								if (inputSign * (*temp)->samples[i].stickX < 0) {
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
						// rewrite data with new starting index
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
						
						(*temp)->isRecordingReady = true;
					}
					break;
				
				default:
					// are we close to the origin?
					if ((abs(selectedStickX) < STICK_MOVEMENT_THRESHOLD) && (abs(selectedStickY) < STICK_MOVEMENT_THRESHOLD)) {
						timeStickInOrigin += (ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick));
					} else {
						timeStickInOrigin = 0;
					}
					
					if ((*temp)->sampleEnd == REC_SAMPLE_MAX || (timeStickInOrigin / 1000) >= STICK_ORIGIN_TIME_THRESHOLD_MS) {
						(*temp)->isRecordingReady = true;
					}
					break;
			}
			
			// data is marked ready, prep it to show
			if ((*temp)->isRecordingReady) {
				stickMove = false;
				display = true;
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
						else if (abs(selectedStickX) + (STICK_MOVEMENT_THRESHOLD * 2) <= abs(snapbackStartPosX) ||
								abs(selectedStickY) + (STICK_MOVEMENT_THRESHOLD * 2) <= abs(snapbackStartPosY)) {
							stickMove = true;
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
					if ((abs(curr.stickX) > STICK_MOVEMENT_THRESHOLD) || (abs(curr.stickY) > STICK_MOVEMENT_THRESHOLD)) {
						stickMove = true;
					}
					break;
				
				default:
					if ((abs(selectedStickX) > STICK_MOVEMENT_THRESHOLD) || (abs(selectedStickY) > STICK_MOVEMENT_THRESHOLD)) {
						stickMove = true;
					}
					break;
			}
			
			if (stickMove) {
				// assign first value
				clearRecordingArray(*temp);
				(*temp)->sampleEnd = 0;
				
				int currentIndex = 0;
				
				// if pivot or dashback, prepend data
				switch (currentTest) {
					case SNAPBACK:
					case DASHBACK:
						// prepend ~50 ms of data to the recording
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
						for (int i = 0; i < loopPrependCount; i++ ) {
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
	
	if (!buttonLock) {
		if (*pressed & PAD_TRIGGER_Z) {
			state = OSC_POST_SETUP;
			buttonLock = true;
			buttonPressCooldown = 5;
		}
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
	
	// prevent pressing for a short time upon entering menu
	buttonLock = true;
	buttonPressCooldown = 5;
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
							stickCooldown--;
							if (stickCooldown == 0) {
								oState = POST_INPUT;
							}
						} else {
							setCursorPos(2, 28);
							printStrColor(GX_COLOR_WHITE, GX_COLOR_BLACK, "LOCKED");
						}
					}
				case POST_INPUT:
					if (dispData->isRecordingReady) {
						// draw guidelines based on selected test
						drawBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128,
								SCREEN_TIMEPLOT_START + 501, SCREEN_POS_CENTER_Y + 128, GX_COLOR_WHITE);
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

						int minX, minY;
						int maxX, maxY;

						// show all data if it will fit
						if (dispData->sampleEnd < 500) {
							dataScrollOffset = 0;
						// move screen to end of data input if it was further from the last capture
						} else if (dataScrollOffset > (dispData->sampleEnd) - 500) {
							dataScrollOffset = dispData->sampleEnd - 501;
						}

						int prevX = dispData->samples[dataScrollOffset].stickX;
						int prevY = dispData->samples[dataScrollOffset].stickY;

						// initialize stat values to first point
						minX = prevX;
						maxX = prevX;
						minY = prevY;
						maxY = prevY;

						int waveformXPos = 0;
						uint64_t drawnTicksUs = 0;
						
						int totalPointsToDraw = 0;
						
						// calculate how many points we're going to draw
						// we also get min and max values here
						for (int i = 0; i < 500; i++) {
							if (dataScrollOffset + i == dispData->sampleEnd) {
								break;
							}
							
							int currY, currX;
							if (!showCStick) {
								currX = dispData->samples[i].stickX;
								currY = dispData->samples[i].stickY;
							} else {
								currX = dispData->samples[i].cStickX;
								currY = dispData->samples[i].cStickY;
							}
							
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
						
						bool drawX = true;
						
						int biggestValueX = abs(maxX) >= abs(minX) ? abs(maxX) : abs(minX);
						int biggestValueY = abs(maxY) >= abs(minY) ? abs(maxY) : abs(minY);
						
						if (biggestValueX >= biggestValueY) {
							drawX = false;
						}
						
						for (int line = 0; line < 2; line++) {
							waveformXPos = 0;
							
							GX_Begin(GX_LINESTRIP, GX_VTXFMT0, totalPointsToDraw);
							
							for (int i = dataScrollOffset; i < dataScrollOffset + totalPointsToDraw; i++) {
								int curr;
								if (drawX) {
									if (!showCStick) {
										curr = dispData->samples[i].stickX;
									} else {
										curr = dispData->samples[i].cStickX;
									}
								} else {
									if (!showCStick) {
										curr = dispData->samples[i].stickY;
									} else {
										curr = dispData->samples[i].cStickY;
									}
								}
								
								GX_Position3s16(SCREEN_TIMEPLOT_START + waveformXPos, SCREEN_POS_CENTER_Y - curr, -2 + line);
								if (drawX) {
									GX_Color3u8(GX_COLOR_RED_X.r, GX_COLOR_RED_X.g, GX_COLOR_RED_X.b);
								} else {
									GX_Color3u8(GX_COLOR_BLUE_Y.r, GX_COLOR_BLUE_Y.g, GX_COLOR_BLUE_Y.b);
								}
								waveformXPos += waveformScaleFactor;
							}
							
							GX_End();
							
							drawX = !drawX;
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
						
						setCursorPos(2, 45);
						// total time is stored in microseconds, divide by 1000 for milliseconds
						if (!showCStick) {
							printStr("Shown: Stick");
						} else {
							printStr("Shown: C-Stick");
						}
						setCursorPos(3, 0);
						
						printStr("Sample start: %04u/%04u | Time shown: %04llu/%04llu ms\n", dataScrollOffset, dispData->sampleEnd, drawnTicksUs / 1000, dispData->totalTimeUs / 1000);
						
						// print test data
						setCursorPos(20, 0);
						switch (currentTest) {
							case SNAPBACK:
								printStr("Min X: %04d | Min Y: %04d   |   ", minX, minY);
								printStr("Max X: %04d | Max Y: %04d\n", maxX, maxY);
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
									// check x coordinate for +-64 (dash threshold)
									if ((abs(dispData->samples[i].stickX) >= 64) && !leftPivotRange) {
										if (pivotEndIndex == -1) {
											pivotEndIndex = i;
										}
										// pivot input must hit 80 on both sides
										if (abs(dispData->samples[i].stickX) >= 80) {
											pivotHit80 = true;
										}
									}

									// are we outside the pivot range and have already logged data of being in range
									if (pivotEndIndex != -1 && abs(dispData->samples[i].stickX) < 64) {
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
									if (abs(dispData->samples[i].stickX) >= 64 && leftPivotRange) {
										// used to ensure starting input is from the opposite side
										if (pivotStartSign == 0) {
											pivotStartSign = dispData->samples[i].stickX;
										}
										prevLeftPivotRange = true;
										if (abs(dispData->samples[i].stickX) >= 80) {
											prevPivotHit80 = true;
											break;
										}
									}
								}
								
								// phobvision doc says both sides need to hit 80 to succeed
								// multiplication is to ensure signs are correct
								if (prevPivotHit80 && pivotHit80 && (dispData->samples[pivotEndIndex].stickX * pivotStartSign < 0)) {
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

									printStr("MS: %2.2f | No turn: %2.0f%% | Pivot: %2.0f%% | Dashback: %2.0f%%",
											timeInPivotRangeMs, noTurnPercent, pivotPercent, dashbackPercent);
								} else {
									printStr("No pivot input detected.");
								}
								break;
							case DASHBACK:
								// go forward in list
								int dashbackStartIndex = -1, dashbackEndIndex = -1;
								uint64_t timeInRange = 0;
								for (int i = 0; i < dispData->sampleEnd; i++) {
									// is the stick in the range
									if ((abs(dispData->samples[i].stickX) >= 23 && abs(dispData->samples[i].stickX) < 64)) {
										timeInRange += dispData->samples[i].timeDiffUs;
										if (dashbackStartIndex == -1) {
											dashbackStartIndex = i;
										}
									} else if (dashbackStartIndex != -1) {
										dashbackEndIndex = i - 1;
										break;
									}
								}
								float dashbackPercent;
								float ucfPercent;

								if (dashbackEndIndex == -1) {
									dashbackPercent = 0;
									ucfPercent = 0;
								} else {
									// convert time in microseconds to float time in milliseconds
									float timeInRangeMs = (timeInRange / 1000.0);

									dashbackPercent = (1.0 - (timeInRangeMs / FRAME_TIME_MS_F)) * 100;

									// ucf dashback is a little more involved
									uint64_t ucfTimeInRange = timeInRange;
									for (int i = dashbackStartIndex; i <= dashbackEndIndex; i++) {
										// we're gonna assume that the previous frame polled around the origin, because i cant be bothered
										// it also makes the math easier
										uint64_t usFromPoll = 0;
										int nextPollIndex = i;
										// we need the sample that would occur around 1f after
										while (usFromPoll < FRAME_TIME_US) {
											nextPollIndex++;
											usFromPoll += dispData->samples[nextPollIndex].timeDiffUs;
										}
										// the two frames need to move more than 75 units for UCF to convert it
										if (abs(dispData->samples[i].stickX) + abs(dispData->samples[nextPollIndex].stickX) > 75) {
											ucfTimeInRange -= dispData->samples[i].timeDiffUs;
										}
									}

									float ucfTimeInRangeMs = ucfTimeInRange / 1000.0;
									if (ucfTimeInRangeMs <= 0) {
										ucfPercent = 100;
									} else {
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
								printStr("Vanilla Success: %2.0f%% | UCF Success: %2.0f%%", dashbackPercent, ucfPercent);
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
			
			if (!buttonLock) {
				if (*pressed & PAD_BUTTON_A && !buttonLock && oState != PRE_INPUT) {
					if (oState == POST_INPUT_LOCK && stickCooldown == 0) {
						oState = POST_INPUT;
					} else {
						oState = POST_INPUT_LOCK;
					}
					buttonLock = true;
					buttonPressCooldown = 5;
				}
				if (*pressed & PAD_BUTTON_X && !showCStick && !buttonLock && !stickMove) {
					currentTest++;
					// check if we overrun our test length
					if (currentTest == OSCILLOSCOPE_TEST_LEN) {
						currentTest = SNAPBACK;
					}
					buttonLock = true;
					buttonPressCooldown = 5;
				}
				if (*pressed & PAD_TRIGGER_Z && !buttonLock) {
					state = OSC_INSTRUCTIONS;
					buttonLock = true;
					buttonPressCooldown = 5;
				}
				if (*pressed & PAD_BUTTON_Y && currentTest == SNAPBACK && !buttonLock && !stickMove) {
					showCStick = !showCStick;
					buttonLock = true;
					buttonPressCooldown = 5;
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
	if (buttonLock) {
		// don't allow button press until a number of frames has passed
		if (buttonPressCooldown > 0) {
			buttonPressCooldown--;
		} else {
			if ((*held) == 0) {
				buttonLock = 0;
			}
		}
	}
}

void menu_oscilloscopeEnd() {
	setSamplingRateNormal();
	PAD_SetSamplingCallback(cb);
	pressed = NULL;
	held = NULL;
	state = OSC_SETUP;
	if (!(*temp)->isRecordingReady) {
		(*temp)->sampleEnd = 0;
		stickMove = false;
	}
}
