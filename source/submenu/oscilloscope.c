//
// Created on 2025/03/18.
//

#include "submenu/oscilloscope.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <ogc/pad.h>
#include <ogc/timesupp.h>
#include <ogc/color.h>

#include "print.h"
#include "draw.h"
#include "polling.h"
#include "stickmap_coordinates.h"

const static uint8_t STICK_MOVEMENT_THRESHOLD = 5;
const static uint8_t STICK_ORIGIN_TIME_THRESHOLD_MS = 50;
const static uint8_t STICK_MOVEMENT_TIME_THRESHOLD_MS = 100;
const static uint8_t MEASURE_COOLDOWN_FRAMES = 5;

static const float FRAME_TIME_MS = (1000/60.0);

const static uint8_t SCREEN_TIMEPLOT_START = 70;

static const uint32_t COLOR_RED_C = 0x846084d7;
static const uint32_t COLOR_BLUE_C = 0x6dd26d72;

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
static char strBuffer[100];

static uint8_t stickCooldown = 0;
static int8_t snapbackStartPosX = 0, snapbackStartPosY = 0;
static int8_t snapbackPrevPosX = 0, snapbackPrevPosY = 0;
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

static uint32_t *pressed = NULL;
static uint32_t *held = NULL;
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
	
	static int8_t stickX, stickY, cStickX, cStickY;
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
	
	// read stick position if not locked
	if (oState != POST_INPUT_LOCK) {
		stickX = PAD_StickX(0);
		stickY = PAD_StickY(0);
		cStickX = PAD_SubStickX(0);
		cStickY = PAD_SubStickY(0);
		
		int8_t selectedStickX, selectedStickY;
		if (showCStick) {
			selectedStickX = cStickX;
			selectedStickY = cStickY;
		} else {
			selectedStickX = stickX;
			selectedStickY = stickY;
		}
		
		// data capture is currently happening
		if (stickMove) {
			// set values
			(*temp)->samples[(*temp)->sampleEnd].stickX = stickX;
			(*temp)->samples[(*temp)->sampleEnd].stickY = stickY;
			(*temp)->samples[(*temp)->sampleEnd].cStickX = cStickX;
			(*temp)->samples[(*temp)->sampleEnd].cStickY = cStickY;
			(*temp)->samples[(*temp)->sampleEnd].timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
			(*temp)->sampleEnd++;
			
			// handle specific logic for each test
			switch (currentTest) {
				// TODO: change this to have a buffer of a minimum amount of time, to make final graph look better
				case SNAPBACK:
					// has the stick stopped moving?
					if (abs(selectedStickX - snapbackPrevPosX) < STICK_MOVEMENT_THRESHOLD &&
					    abs(selectedStickY - snapbackPrevPosY) < STICK_MOVEMENT_THRESHOLD) {
						timeStoppedMoving += (ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick));
					} else {
						timeStoppedMoving = 0;
					}
					snapbackPrevPosX = selectedStickX;
					snapbackPrevPosY = selectedStickY;
					
					// have we either run out of data, or has the stick stopped moving for long enough?
					if ((*temp)->sampleEnd == WAVEFORM_SAMPLES || ((timeStoppedMoving / 1000)) >= STICK_MOVEMENT_TIME_THRESHOLD_MS) {
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
						}
					}
					break;
				
				case PIVOT:
					// are we close to the origin?
					// this doesn't use the selectedStick values, idk if c-stick pivots are a thing...
					if ((abs(stickX) < STICK_MOVEMENT_THRESHOLD) && (abs(stickY) < STICK_MOVEMENT_THRESHOLD)) {
						timeStickInOrigin += (ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick));
					} else {
						timeStickInOrigin = 0;
					}
					
					// have we met conditions to end recording?
					if ((*temp)->sampleEnd == WAVEFORM_SAMPLES || (timeStickInOrigin / 1000) >= STICK_ORIGIN_TIME_THRESHOLD_MS) {
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
						
						// rewrite data with new starting index
						for (int i = 0; i < (*temp)->sampleEnd - 1 - pivotStartIndex; i++) {
							(*temp)->samples[i].stickX = (*temp)->samples[i + pivotStartIndex].stickX;
							(*temp)->samples[i].stickY = (*temp)->samples[i + pivotStartIndex].stickY;
							(*temp)->samples[i].cStickX = (*temp)->samples[i + pivotStartIndex].cStickX;
							(*temp)->samples[i].cStickY = (*temp)->samples[i + pivotStartIndex].cStickY;
							(*temp)->samples[i].timeDiffUs = (*temp)->samples[i + pivotStartIndex].timeDiffUs;
						}
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
					
					if ((*temp)->sampleEnd == WAVEFORM_SAMPLES || (timeStickInOrigin / 1000) >= STICK_ORIGIN_TIME_THRESHOLD_MS) {
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
				(*temp)->recordingType = REC_OSCILLOSCOPE;
				
				// calculate total recording time
				for (int i = 0; i < (*temp)->sampleEnd; i++) {
					(*temp)->totalTimeUs += (*temp)->samples[i].timeDiffUs;
				}
				
				flipData();
			}
		}
			
		// data capture has not yet occurred
		else {
			// check criteria for each test to trigger a recording
			switch (currentTest) {
				case SNAPBACK:
					if ((selectedStickX > snapbackStartPosX + STICK_MOVEMENT_THRESHOLD || selectedStickX < snapbackStartPosX - STICK_MOVEMENT_THRESHOLD) ||
					    (selectedStickY > snapbackStartPosY + STICK_MOVEMENT_THRESHOLD) || (selectedStickY < snapbackStartPosY - STICK_MOVEMENT_THRESHOLD)) {
						stickMove = true;
					}
					break;
				
				case PIVOT:
					if ((abs(stickX) > STICK_MOVEMENT_THRESHOLD) || (abs(stickY) > STICK_MOVEMENT_THRESHOLD)) {
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
				(*temp)->samples[0].stickX = stickX;
				(*temp)->samples[0].stickY = stickY;
				(*temp)->samples[0].cStickX = cStickX;
				(*temp)->samples[0].cStickY = cStickY;
				(*temp)->samples[0].timeDiffUs = 0;
				(*temp)->totalTimeUs = 0;
				(*temp)->sampleEnd = 1;
				(*temp)->isRecordingReady = false;
				(*temp)->dataExported = false;
				oState = PRE_INPUT;
			}
		}
	}
}

static void displayInstructions(void *currXfb) {
	setCursorPos(2, 0);
	printStr("Press X to cycle the current test, results will show above the\n"
			"waveform. Press Y to cycle between Analog Stick and C-Stick.\n"
			"Use DPAD left/right to scroll waveform when it is\n"
			"larger than the displayed area, hold R to move faster.", currXfb);
	printStr("\n\nCURRENT TEST: ", currXfb);
	switch (currentTest) {
		case SNAPBACK:
			printStr("SNAPBACK\n"
					"Check the min/max value on a given axis depending on where\n"
					"the stick started. The range in which Melee will not register\n"
					"a directional input is -22 to +22. Anything outside that range\n"
					"risks the game registering a non-neutral stick input.", currXfb);
			break;
		case PIVOT:
			printStr("PIVOT\n"
					"For a successful pivot, the stick's position should stay\n"
					"above/below +64/-64 for ~16.6ms (1 frame). Less, and a pivot\n"
					"may not occur; more, and a dashback may occur. The stick\n"
					"should also hit 80/-80 on both sides.\n", currXfb);
			break;
		case DASHBACK:
			printStr("DASHBACK\n"
					"A (vanilla) dashback will be successful when the stick doesn't\n"
					"get polled between 23 and 64, or -23 and -64.\n"
					"Less time in this range is better.", currXfb);
			break;
		default:
			printStr("NO TEST SELECTED", currXfb);
			break;
	}
	
	setCursorPos(21, 0);
	printStr("Press Z to close instructions.", currXfb);
	
	if (!buttonLock) {
		if (*pressed & PAD_TRIGGER_Z) {
			state = OSC_POST_SETUP;
			buttonLock = true;
			buttonPressCooldown = 5;
		}
	}
}

// only run once
static void setup(uint32_t *p, uint32_t *h) {
	setSamplingRateHigh();
	pressed = p;
	held = h;
	cb = PAD_SetSamplingCallback(oscilloscopeCallback);
	state = OSC_POST_SETUP;
	if(data == NULL) {
		data = getRecordingData();
		temp = getTempData();
	}
	if ((*data)->isRecordingReady && oState == PRE_INPUT) {
		oState = POST_INPUT_LOCK;
	}
	// don't use data from trigger menu
	// _technically_ this doesn't need to happen, but trigger recording is basically useless here
	if ((*data)->recordingType == REC_TRIGGER) {
		clearRecordingArray(*data);
		oState = PRE_INPUT;
	}
}

// function called from outside
void menu_oscilloscope(void *currXfb, uint32_t *p, uint32_t *h) {
	// we're getting the address of the object itself here, not the address of the pointer,
	// which means we will always point to the same object, regardless of a flip
	ControllerRec *dispData = *data;
	
	switch (state) {
		case OSC_SETUP:
			setup(p, h);
			break;
		case OSC_POST_SETUP:
			switch (oState) {
				case PRE_INPUT:
					printStr("Waiting for input.", currXfb);
					printEllipse(ellipseCounter, 20, currXfb);
					ellipseCounter++;
					if (ellipseCounter == 60) {
						ellipseCounter = 0;
					}
					
					setCursorPos(21,0);
					printStr("Current test: ", currXfb);
					switch (currentTest) {
						case SNAPBACK:
							printStr("Snapback", currXfb);
							break;
						case PIVOT:
							printStr("Pivot", currXfb);
							break;
						case DASHBACK:
							printStr("Dashback", currXfb);
							break;
						default:
							printStr("Error", currXfb);
							break;
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
							printStrColor("LOCKED", currXfb, COLOR_WHITE, COLOR_BLACK);
						}
					}
				case POST_INPUT:
					if (dispData->isRecordingReady) {
						// draw guidelines based on selected test
						DrawBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 128, COLOR_WHITE, currXfb);
						DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y, COLOR_GRAY, currXfb);
						// lots of the specific values are taken from:
						// https://github.com/PhobGCC/PhobGCC-doc/blob/main/For_Users/Phobvision_Guide_Latest.md
						switch (currentTest) {
							case PIVOT:
								DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 64, COLOR_GREEN, currXfb);
								DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y - 64, COLOR_GREEN, currXfb);
								setCursorPos(8, 0);
								printStr("+64", currXfb);
								setCursorPos(15, 0);
								printStr("-64", currXfb);
								break;
							case DASHBACK:
								DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 64, COLOR_GREEN, currXfb);
								DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y - 64, COLOR_GREEN, currXfb);
								setCursorPos(8, 0);
								printStr("+64", currXfb);
								setCursorPos(15, 0);
								printStr("-64", currXfb);
							case SNAPBACK:
								DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 23, COLOR_GREEN, currXfb);
								DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y - 23, COLOR_GREEN, currXfb);
								setCursorPos(10, 0);
								printStr("+23", currXfb);
								setCursorPos(13, 0);
								printStr("-23", currXfb);
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

						int waveformPrevXPos = 0;
						int waveformXPos = waveformScaleFactor;
						uint64_t drawnTicksUs = 0;

						// draw 500 datapoints from the scroll offset
						for (int i = dataScrollOffset + 1; i < dataScrollOffset + 500; i++) {
							// make sure we haven't gone outside our bounds
							if (i == dispData->sampleEnd || waveformXPos >= 500) {
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

							// y first
							DrawLine(SCREEN_TIMEPLOT_START + waveformPrevXPos, SCREEN_POS_CENTER_Y - prevY,
									SCREEN_TIMEPLOT_START + waveformXPos, SCREEN_POS_CENTER_Y - currY,
									COLOR_BLUE_C, currXfb);
							prevY = currY;
							// then x
							DrawLine(SCREEN_TIMEPLOT_START + waveformPrevXPos, SCREEN_POS_CENTER_Y - prevX,
									SCREEN_TIMEPLOT_START + waveformXPos, SCREEN_POS_CENTER_Y - currX,
									COLOR_RED_C, currXfb);
							prevX = currX;

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
							drawnTicksUs += dispData->samples[i].timeDiffUs;

							// update scaling factor
							waveformPrevXPos = waveformXPos;
							waveformXPos += waveformScaleFactor;
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
							printStr("Shown: Stick", currXfb);
						} else {
							printStr("Shown: C-Stick", currXfb);
						}
						setCursorPos(3, 0);
						
						sprintf(strBuffer, "Sample start: %04u/%04u | Time shown: %04llu/%04llu ms\n", dataScrollOffset, dispData->sampleEnd, drawnTicksUs / 1000, dispData->totalTimeUs / 1000);
						printStr(strBuffer, currXfb);
						
						// print test data
						setCursorPos(20, 0);
						switch (currentTest) {
							case SNAPBACK:
								sprintf(strBuffer, "Min X: %04d | Min Y: %04d   |   ", minX, minY);
								printStr(strBuffer, currXfb);
								sprintf(strBuffer, "Max X: %04d | Max Y: %04d\n", maxX, maxY);
								printStr(strBuffer, currXfb);
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
									if (abs((dispData->samples[i].stickX) >= 64) && !leftPivotRange) {
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
									float diffFrameTimePoll = FRAME_TIME_MS - timeInPivotRangeMs;

									// negative time difference, dashback
									if (diffFrameTimePoll < 0) {
										dashbackPercent = ((diffFrameTimePoll * -1) / FRAME_TIME_MS) * 100;
										if (dashbackPercent > 100) {
											dashbackPercent = 100;
										}
										pivotPercent = 100 - dashbackPercent;
										// positive or 0 time diff, no turn
									} else {
										noTurnPercent = (diffFrameTimePoll / FRAME_TIME_MS) * 100;
										if (noTurnPercent > 100) {
											noTurnPercent = 100;
										}
										pivotPercent = 100 - noTurnPercent;
									}

									sprintf(strBuffer, "MS: %2.2f | No turn: %2.0f%% | Pivot: %2.0f%% | Dashback: %2.0f%%",
											timeInPivotRangeMs, noTurnPercent, pivotPercent, dashbackPercent);
									printStr(strBuffer, currXfb);
									//printf("\nUS Total: %llu, Start index: %d, End index: %d", timeInPivotRangeUs, pivotStartIndex, pivotEndIndex);
								} else {
									printStr("No pivot input detected.", currXfb);
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

									dashbackPercent = (1.0 - (timeInRangeMs / FRAME_TIME_MS)) * 100;

									// ucf dashback is a little more involved
									uint64_t ucfTimeInRange = timeInRange;
									for (int i = dashbackStartIndex; i <= dashbackEndIndex; i++) {
										// we're gonna assume that the previous frame polled around the origin, because i cant be bothered
										// it also makes the math easier
										uint64_t usFromPoll = 0;
										int nextPollIndex = i;
										// we need the sample that would occur around 1f after
										while (usFromPoll < 16666) {
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
										ucfPercent = (1.0 - (ucfTimeInRangeMs / FRAME_TIME_MS)) * 100;
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
								sprintf(strBuffer, "Vanilla Success: %2.0f%% | UCF Success: %2.0f%%", dashbackPercent, ucfPercent);
								printStr(strBuffer, currXfb);
								break;
							default:
								printStr("Error?", currXfb);
								break;

						}
						setCursorPos(21,0);
						printStr("Current test: ", currXfb);
						switch (currentTest) {
							case SNAPBACK:
								printStr("Snapback", currXfb);
								break;
							case PIVOT:
								printStr("Pivot", currXfb);
								break;
							case DASHBACK:
								printStr("Dashback", currXfb);
								break;
							default:
								printStr("Error", currXfb);
								break;
						}
					} else {
						oState = PRE_INPUT;
					}
					break;
				default:
					printStr("How did we get here?", currXfb);
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
			displayInstructions(currXfb);
			break;
		default:
			printStr("How did we get here?", currXfb);
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
