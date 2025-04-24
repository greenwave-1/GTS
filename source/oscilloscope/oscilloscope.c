//
// Created on 3/18/25.
//

#include "oscilloscope.h"
#include <ogc/lwp_watchdog.h>
#include <stdlib.h>
#include "../print.h"
#include "../draw.h"
#include "../polling.h"
#include "../stickmap_coordinates.h"
//#include "../waveform.h"

const static u8 STICK_MOVEMENT_THRESHOLD = 5;
const static u8 STICK_ORIGIN_TIME_THRESHOLD_MS = 50;
const static u8 STICK_MOVEMENT_TIME_THRESHOLD_MS = 100;
const static u8 MEASURE_COOLDOWN_FRAMES = 5;

const static u8 SCREEN_TIMEPLOT_START = 70;

static const uint32_t COLOR_RED_C = 0x846084d7;
static const uint32_t COLOR_BLUE_C = 0x6dd26d72;

static enum OSC_MENU_STATE state = OSC_SETUP;
static enum OSCILLOSCOPE_STATE oState = PRE_INPUT;

static WaveformData *data = NULL; // = { {{ 0 }}, 0, 500, false, false };
static enum OSCILLOSCOPE_TEST currentTest = SNAPBACK;
static int waveformScaleFactor = 1;
static int dataScrollOffset = 0;
static char strBuffer[100];

static u8 stickCooldown = 0;
static s8 snapbackStartPosX = 0, snapbackStartPosY = 0;
static s8 snapbackPrevPosX = 0, snapbackPrevPosY = 0;
static bool pressLocked = false;
static bool stickMove = false;
static bool showCStick = false;
static bool display = false;

static u8 ellipseCounter = 0;
static u64 pressedTimer = 0;
static u64 prevSampleCallbackTick = 0;
static u64 sampleCallbackTick = 0;
static u64 timeStickInOrigin = 0;
static u64 timeStoppedMoving = 0;

static u32 *pressed;
static u32 *held;
static bool buttonLock = false;
static u8 buttonPressCooldown = 0;

static sampling_callback cb;
static void oscilloscopeCallback() {
	// time from last call of this function calculation
	prevSampleCallbackTick = sampleCallbackTick;
	sampleCallbackTick = gettime();
	if (prevSampleCallbackTick == 0) {
		prevSampleCallbackTick = sampleCallbackTick;
	}

	static s8 x, y, cx, cy;
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
		x = PAD_StickX(0);
		y = PAD_StickY(0);
		cx = PAD_SubStickX(0);
		cy = PAD_SubStickY(0);
		
		// handle stick recording differently based on the selected test
		switch (currentTest) {
			case SNAPBACK:
				// we're already recording an input
				if (stickMove) {
					data->data[data->endPoint].ax = x;
					data->data[data->endPoint].ay = y;
					data->data[data->endPoint].cx = cx;
					data->data[data->endPoint].cy = cy;
					data->data[data->endPoint].timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
					data->endPoint++;
					
					// has the stick stopped moving?
					if (!showCStick) {
						if (abs(x - snapbackPrevPosX) < STICK_MOVEMENT_THRESHOLD &&
						    abs(y - snapbackPrevPosY) < STICK_MOVEMENT_THRESHOLD ) {
							timeStoppedMoving += (ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick));
						} else {
							timeStoppedMoving = 0;
						}
						snapbackPrevPosX = x;
						snapbackPrevPosY = y;
					} else {
						if (abs(cx - snapbackPrevPosX) < STICK_MOVEMENT_THRESHOLD &&
						    abs(cy - snapbackPrevPosY) < STICK_MOVEMENT_THRESHOLD) {
							timeStoppedMoving += (ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick));
						} else {
							timeStoppedMoving = 0;
						}
						snapbackPrevPosX = cx;
						snapbackPrevPosY = cy;
					}
					
					// have we either run out of data, or has the stick stopped moving for long enough?
					if (data->endPoint == WAVEFORM_SAMPLES || ((timeStoppedMoving / 1000)) >= STICK_MOVEMENT_TIME_THRESHOLD_MS) {
						if (!showCStick) {
							// are we stopped near the origin?
							if ((x < STICK_MOVEMENT_THRESHOLD && x > -STICK_MOVEMENT_THRESHOLD) &&
							    (y < STICK_MOVEMENT_THRESHOLD && y > -STICK_MOVEMENT_THRESHOLD)) {
								// normal procedure, make data ready
								data->isDataReady = true;
								stickMove = false;
								display = true;
								oState = POST_INPUT_LOCK;
								stickCooldown = MEASURE_COOLDOWN_FRAMES;
								snapbackStartPosX = 0;
								snapbackStartPosY = 0;
							} else {
								// go back in the loop, we're holding a position somewhere outside origin
								// this will also reset if we're out of datapoints? not sure how this'll work
								data->endPoint = 0;
								stickMove = false;
								snapbackStartPosX = x;
								snapbackStartPosY = y;
							}
						} else {
							if ((cx < STICK_MOVEMENT_THRESHOLD && cx > -STICK_MOVEMENT_THRESHOLD) &&
							    (cy < STICK_MOVEMENT_THRESHOLD && cy > -STICK_MOVEMENT_THRESHOLD)) {
								data->isDataReady = true;
								stickMove = false;
								display = true;
								oState = POST_INPUT_LOCK;
								stickCooldown = MEASURE_COOLDOWN_FRAMES;
								snapbackStartPosX = 0;
								snapbackStartPosY = 0;
							} else {
								data->endPoint = 0;
								stickMove = false;
								snapbackStartPosX = cx;
								snapbackStartPosY = cy;
							}
						}
					}
					
				// we've not recorded an input yet
				} else {
					// does the stick move outside the threshold?
					if (!showCStick) {
						if ((x > snapbackStartPosX + STICK_MOVEMENT_THRESHOLD || x < snapbackStartPosX - STICK_MOVEMENT_THRESHOLD) ||
						    (y > snapbackStartPosY + STICK_MOVEMENT_THRESHOLD) || (y < snapbackStartPosY - STICK_MOVEMENT_THRESHOLD)) {
							// reset data
							for (int i = 1; i < WAVEFORM_SAMPLES; i++) {
								data->data[i].ax = 0;
								data->data[i].ay = 0;
								data->data[i].cx = 0;
								data->data[i].cy = 0;
								data->data[i].timeDiffUs = 0;
							}
							stickMove = true;
							data->data[0].ax = x;
							data->data[0].ay = y;
							data->data[0].cx = cx;
							data->data[0].cy = cy;
							data->data[0].timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
							data->endPoint = 1;
							oState = PRE_INPUT;
						}
					} else {
						if ((cx > snapbackStartPosX + STICK_MOVEMENT_THRESHOLD || cx < snapbackStartPosX - STICK_MOVEMENT_THRESHOLD) ||
						    (cy > snapbackStartPosY + STICK_MOVEMENT_THRESHOLD) || (cy < snapbackStartPosY - STICK_MOVEMENT_THRESHOLD)) {
							// reset data
							for (int i = 1; i < WAVEFORM_SAMPLES; i++) {
								data->data[i].ax = 0;
								data->data[i].ay = 0;
								data->data[i].cx = 0;
								data->data[i].cy = 0;
								data->data[i].timeDiffUs = 0;
							}
							stickMove = true;
							data->data[0].ax = x;
							data->data[0].ay = y;
							data->data[0].cx = cx;
							data->data[0].cy = cy;
							data->data[0].timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
							data->endPoint = 1;
							oState = PRE_INPUT;
						}
					}
				}
				break;
			case PIVOT:
				// we're already recording an input
				if (stickMove) {
					data->data[data->endPoint].ax = x;
					data->data[data->endPoint].ay = y;
					data->data[data->endPoint].cx = cx;
					data->data[data->endPoint].cy = cy;
					data->data[data->endPoint].timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
					data->endPoint++;
					// are we close to the origin?
					if ((x < STICK_MOVEMENT_THRESHOLD && x > -STICK_MOVEMENT_THRESHOLD) &&
					    (y < STICK_MOVEMENT_THRESHOLD && y > -STICK_MOVEMENT_THRESHOLD)) {
						timeStickInOrigin += (ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick));
					} else {
						timeStickInOrigin = 0;
					}
					if (data->endPoint == WAVEFORM_SAMPLES || (timeStickInOrigin / 1000) >= STICK_ORIGIN_TIME_THRESHOLD_MS) {
						// TODO: replace this with something proper, wip is already there with the switch case
						// this will truncate the recording to just the pivot input
						u64 timeFromOriginCross = 0;
						bool crossed64Range = false;
						s8 inputSign = 0;
						int pivotStartIndex = 0;
						bool hasCrossedOrigin = false;
						for (int i = data->endPoint - 1; i >= 0; i--) {
							if (!crossed64Range) {
								if (data->data[i].ax >= 64 || data->data[i].ax <= -64) {
									crossed64Range = true;
									inputSign = data->data[i].ax;
								}
							} else if (!hasCrossedOrigin) {
								if (inputSign * data->data[i].ax < 0) {
									hasCrossedOrigin = true;
								}
							} else {
								timeFromOriginCross += data->data[i].timeDiffUs;
								if (timeFromOriginCross / 1000 >= 50) {
									pivotStartIndex = i;
									break;
								}
							}
						}
						
						// rewrite data with new starting index
						for (int i = 0; i < data->endPoint - 1 - pivotStartIndex; i++) {
							data->data[i].ax = data->data[i + pivotStartIndex].ax;
							data->data[i].ay = data->data[i + pivotStartIndex].ay;
							data->data[i].cx = data->data[i + pivotStartIndex].cx;
							data->data[i].cy = data->data[i + pivotStartIndex].cy;
							data->data[i].timeDiffUs = data->data[i + pivotStartIndex].timeDiffUs;
						}
						data->endPoint = data->endPoint - pivotStartIndex - 1;
						
						// normal stuff
						data->isDataReady = true;
						stickMove = false;
						display = true;
						oState = POST_INPUT_LOCK;
						stickCooldown = MEASURE_COOLDOWN_FRAMES;
					}
				// we've not recorded an input yet
				} else {
					// does the stick move outside the threshold?
					if ((x > STICK_MOVEMENT_THRESHOLD || x < -STICK_MOVEMENT_THRESHOLD) ||
					    (y > STICK_MOVEMENT_THRESHOLD) || (y < -STICK_MOVEMENT_THRESHOLD)) {
						// reset data
						for (int i = 1; i < WAVEFORM_SAMPLES; i++) {
							data->data[i].ax = 0;
							data->data[i].ay = 0;
							data->data[i].cx = 0;
							data->data[i].cy = 0;
							data->data[i].timeDiffUs = 0;
						}
						stickMove = true;
						data->data[0].ax = x;
						data->data[0].ay = y;
						data->data[0].cx = cx;
						data->data[0].cy = cy;
						data->data[0].timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
						data->endPoint = 1;
						oState = PRE_INPUT;
					}
				}
				break;
			default:
				// we're already recording an input
				if (stickMove) {
					data->data[data->endPoint].ax = x;
					data->data[data->endPoint].ay = y;
					data->data[data->endPoint].cx = cx;
					data->data[data->endPoint].cy = cy;
					data->data[data->endPoint].timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
					data->endPoint++;
					// are we close to the origin?
					if (!showCStick) {
						if ((x < STICK_MOVEMENT_THRESHOLD && x > -STICK_MOVEMENT_THRESHOLD) &&
						    (y < STICK_MOVEMENT_THRESHOLD && y > -STICK_MOVEMENT_THRESHOLD)) {
							timeStickInOrigin += (ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick));
						} else {
							timeStickInOrigin = 0;
						}
					} else {
						if ((cx < STICK_MOVEMENT_THRESHOLD && cx > -STICK_MOVEMENT_THRESHOLD) &&
						    (cy < STICK_MOVEMENT_THRESHOLD && cy > -STICK_MOVEMENT_THRESHOLD)) {
							timeStickInOrigin += (ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick));
						} else {
							timeStickInOrigin = 0;
						}
					}
					if (data->endPoint == WAVEFORM_SAMPLES || (timeStickInOrigin / 1000) >= STICK_ORIGIN_TIME_THRESHOLD_MS) {
						data->isDataReady = true;
						stickMove = false;
						display = true;
						oState = POST_INPUT_LOCK;
						stickCooldown = MEASURE_COOLDOWN_FRAMES;
					}
					// we've not recorded an input yet
				} else {
					// does the stick move outside the threshold?
					if (!showCStick) {
						if ((x > STICK_MOVEMENT_THRESHOLD || x < -STICK_MOVEMENT_THRESHOLD) ||
						    (y > STICK_MOVEMENT_THRESHOLD) || (y < -STICK_MOVEMENT_THRESHOLD)) {
							// reset data
							for (int i = 1; i < WAVEFORM_SAMPLES; i++) {
								data->data[i].ax = 0;
								data->data[i].ay = 0;
								data->data[i].cx = 0;
								data->data[i].cy = 0;
								data->data[i].timeDiffUs = 0;
							}
							stickMove = true;
							data->data[0].ax = x;
							data->data[0].ay = y;
							data->data[0].cx = cx;
							data->data[0].cy = cy;
							data->data[0].timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
							data->endPoint = 1;
							oState = PRE_INPUT;
						}
					} else {
						if ((cx > STICK_MOVEMENT_THRESHOLD || cx < -STICK_MOVEMENT_THRESHOLD) ||
						    (cy > STICK_MOVEMENT_THRESHOLD) || (cy < -STICK_MOVEMENT_THRESHOLD)) {
							// reset data
							for (int i = 1; i < WAVEFORM_SAMPLES; i++) {
								data->data[i].ax = 0;
								data->data[i].ay = 0;
								data->data[i].cx = 0;
								data->data[i].cy = 0;
								data->data[i].timeDiffUs = 0;
							}
							stickMove = true;
							data->data[0].ax = x;
							data->data[0].ay = y;
							data->data[0].cx = cx;
							data->data[0].cy = cy;
							data->data[0].timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
							data->endPoint = 1;
							oState = PRE_INPUT;
						}
					}
				}
				break;
		}
	}
}

static void printInstructions(void *currXfb) {
	setCursorPos(2, 0);
	printStr("Press X to cycle the current test, results will show above the\n"
			 "waveform. Press Y to cycle between Analog Stick and C-Stick.\n"
	         "Use DPAD left/right to scroll waveform when it is\nlarger than the "
	         "displayed area, hold R to move faster.", currXfb);
	printStr("\n\nCURRENT TEST: ", currXfb);
	switch (currentTest) {
		case SNAPBACK:
			printStr("SNAPBACK\nCheck the min/max value on a given axis depending on where\nyour "
					"stick started. If you moved the stick left, check the\nMax value on a given "
					"axis. Snapback can occur when the\nmax value is at or above 23. If right, "
					"then at or below -23.", currXfb);
			break;
		case PIVOT:
			printStr("PIVOT\nFor a successful pivot, you want the stick's position to stay "
				   "above/below +64/-64 for ~16.6ms (1 frame). Less, and you might\n"
				   "get nothing, more, and you might get a dashback. You also need\n"
				   "the stick to hit 80/-80 on both sides.\n"
				   "Check the PhobVision docs for more info.", currXfb);
			break;
		case DASHBACK:
			printStr("DASHBACK\nA (vanilla) dashback will be successful when the stick doesn't\n"
					 "get polled between 23 and 64, or -23 and -64.\n"
					 "Less time in this range is better.", currXfb);
			break;
		default:
			printStr("NO TEST SELECTED", currXfb);
			break;
	}
	if (!buttonLock) {
		if (*pressed & PAD_TRIGGER_Z) {
			state = OSC_POST_SETUP;
			buttonLock = true;
			buttonPressCooldown = 5;
		}
	}
}

// only run once
static void setup(WaveformData *d, u32 *p, u32 *h) {
	setSamplingRateHigh();
	pressed = p;
	held = h;
	cb = PAD_SetSamplingCallback(oscilloscopeCallback);
	state = OSC_POST_SETUP;
	if(data == NULL) {
		data = d;
	}
	if (data->isDataReady && oState == PRE_INPUT) {
		oState = POST_INPUT_LOCK;
	}
}

// function called from outside
void menu_oscilloscope(void *currXfb, WaveformData *data, u32 *p, u32 *h) {
	switch (state) {
		case OSC_SETUP:
			setup(data, p, h);
			break;
		case OSC_POST_SETUP:
			switch (oState) {
				case PRE_INPUT:
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
						case NO_TEST:
							printStr("None", currXfb);
							break;
						default:
							printStr("Error", currXfb);
							break;
					}
					break;
				case POST_INPUT_LOCK:
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
				case POST_INPUT:
					if (data->isDataReady) {
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
						if (data->endPoint < 500) {
							dataScrollOffset = 0;
						// move screen to end of data input if it was further from the last capture
						} else if (dataScrollOffset > (data->endPoint) - 500) {
							dataScrollOffset = data->endPoint - 501;
						}

						int prevX = data->data[dataScrollOffset].ax;
						int prevY = data->data[dataScrollOffset].ay;

						// initialize stat values to first point
						minX = prevX;
						maxX = prevX;
						minY = prevY;
						maxY = prevY;

						int waveformPrevXPos = 0;
						int waveformXPos = waveformScaleFactor;
						u64 drawnTicksUs = 0;

						// draw 500 datapoints from the scroll offset
						for (int i = dataScrollOffset + 1; i < dataScrollOffset + 500; i++) {
							// make sure we haven't gone outside our bounds
							if (i == data->endPoint || waveformXPos >= 500) {
								break;
							}
							
							int currY, currX;
							if (!showCStick) {
								currY = data->data[i].ay;
								currX = data->data[i].ax;
							} else {
								currY = data->data[i].cy;
								currX = data->data[i].cx;
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
							drawnTicksUs += data->data[i].timeDiffUs;

							// update scaling factor
							waveformPrevXPos = waveformXPos;
							waveformXPos += waveformScaleFactor;
						}

						// do we have enough data to enable scrolling?
						// TODO: enable scrolling when scaled
						if (data->endPoint >= 500 ) {
							// does the user want to scroll the waveform?
							if (*held & PAD_BUTTON_RIGHT) {
								if (*held & PAD_TRIGGER_R) {
									if (dataScrollOffset + 510 < data->endPoint) {
										dataScrollOffset += 10;
									}
								} else {
									if (dataScrollOffset + 501 < data->endPoint) {
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

						setCursorPos(3, 0);
						// total time is stored in microseconds, divide by 1000 for milliseconds
						if (!showCStick) {
							printStr("Stick ", currXfb);
						} else {
							printStr("C-Stick ", currXfb);
						}
						sprintf(strBuffer, "total: %u, %0.3f ms | Start: %d, Shown: %0.3f ms\n", data->endPoint, (data->totalTimeUs / ((float) 1000)), dataScrollOffset + 1, (drawnTicksUs / ((float) 1000)));
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
								for (int i = data->endPoint - 1; i >= 0; i--) {
									// check x coordinate for +-64 (dash threshold)
									if ( (data->data[i].ax >= 64 || data->data[i].ax <= -64) && !leftPivotRange) {
										if (pivotEndIndex == -1) {
											pivotEndIndex = i;
										}
										// pivot input must hit 80 on both sides
										if (data->data[i].ax >= 80 || data->data[i].ax <= -80) {
											pivotHit80 = true;
										}
									}

									// are we outside the pivot range and have already logged data of being in range
									if (pivotEndIndex != -1 && data->data[i].ax < 64 && data->data[i].ax > -64) {
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
									if ( (data->data[i].ax >= 64 || data->data[i].ax <= -64) && leftPivotRange) {
										// used to ensure starting input is from the opposite side
										if (pivotStartSign == 0) {
											pivotStartSign = data->data[i].ax;
										}
										prevLeftPivotRange = true;
										if (data->data[i].ax >= 80 || data->data[i].ax <= -80) {
											prevPivotHit80 = true;
											break;
										}
									}
								}

								// phobvision doc says both sides need to hit 80 to succeed
								// multiplication is to ensure signs are correct
								if (prevPivotHit80 && pivotHit80 && (data->data[pivotEndIndex].ax * pivotStartSign < 0)) {
									float noTurnPercent = 0;
									float pivotPercent = 0;
									float dashbackPercent = 0;

									u64 timeInPivotRangeUs = 0;
									for (int i = pivotStartIndex; i <= pivotEndIndex; i++) {
										timeInPivotRangeUs += data->data[i].timeDiffUs;
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
								u64 timeInRange = 0;
								for (int i = 0; i < data->endPoint; i++) {
									// is the stick in the range
									if ((data->data[i].ax >= 23 && data->data[i].ax < 64) || (data->data[i].ax <= -23 && data->data[i].ax > -64)) {
										timeInRange += data->data[i].timeDiffUs;
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
									u64 ucfTimeInRange = timeInRange;
									for (int i = dashbackStartIndex; i <= dashbackEndIndex; i++) {
										// we're gonna assume that the previous frame polled around the origin, because i cant be bothered
										// it also makes the math easier
										u64 usFromPoll = 0;
										int nextPollIndex = i;
										// we need the sample that would occur around 1f after
										while (usFromPoll < 16666) {
											nextPollIndex++;
											usFromPoll += data->data[nextPollIndex].timeDiffUs;
										}
										// the two frames need to move more than 75 units for UCF to convert it
										if (data->data[i].ax + data->data[nextPollIndex].ax > 75 ||
												data->data[i].ax + data->data[nextPollIndex].ax < -75) {
											ucfTimeInRange -= data->data[i].timeDiffUs;
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
							case NO_TEST:
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
							case NO_TEST:
								printStr("None", currXfb);
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
				if (*pressed & PAD_BUTTON_A && !buttonLock) {
					if (oState == POST_INPUT_LOCK && stickCooldown == 0) {
						oState = POST_INPUT;
					} else {
						oState = POST_INPUT_LOCK;
					}
					buttonLock = true;
					buttonPressCooldown = 5;
				}
				if (*pressed & PAD_BUTTON_X && !buttonLock && !stickMove) {
					currentTest++;
					// check if we overrun our test length
					if (currentTest == OSCILLOSCOPE_TEST_LEN) {
						currentTest = SNAPBACK;
					}
					if (showCStick && (currentTest != SNAPBACK && currentTest < NO_TEST)) {
						currentTest = NO_TEST;
					}
					buttonLock = true;
					buttonPressCooldown = 5;
				}
				if (*pressed & PAD_TRIGGER_Z && !buttonLock) {
					state = OSC_INSTRUCTIONS;
					buttonLock = true;
					buttonPressCooldown = 5;
				}
				if (*pressed & PAD_BUTTON_Y && !buttonLock && !stickMove) {
					showCStick = !showCStick;
					currentTest = SNAPBACK;
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
			printInstructions(currXfb);
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
}
