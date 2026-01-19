//
// Created on 5/16/25.
//

// Code in use from the PhobGCC project is licensed under GPLv3. A copy of this license is provided in the root
// directory of this project's repository.

// Upstream URL for the PhobGCC project is: https://github.com/PhobGCC/PhobGCC-SW

// Other than adt ps math, this mostly clones the visual side of phobvision

#include "submenu/trigger.h"

#include <stdint.h>

#include <ogc/pad.h>
#include <ogc/timesupp.h>

#include "util/gx.h"
#include "waveform.h"
#include "util/polling.h"
#include "util/print.h"

const static uint8_t SCREEN_TIMEPLOT_START = 70;
const static int TRIGGER_SAMPLES = 500;

static uint16_t *pressed = NULL;
static uint16_t *held = NULL;
static uint8_t captureStartFrameCooldown = 0;

static enum TRIG_MENU_STATE menuState = TRIG_SETUP;
static enum TRIG_STATE trigState = TRIG_INPUT;
static enum TRIG_CAPTURE_SELECTION captureSelection = TRIGGER_NONE;
static enum TRIG_CAPTURE_SELECTION displaySelection = TRIGGER_NONE;

// structs for storing controller data
// data: used for display once marked ready
// temp: used by the callback function while data is being collected
// structs are flipped silently by calling flipData() from waveform.h, so we don't have to change anything here
static ControllerRec **data = NULL, **temp = NULL;
// stores current captured data
static ControllerSample curr;
// acts as a prepend loop
// once conditions are met, ~25ms of previous data is added to the start of the capture from here
static ControllerSample startingLoop[200];
static int startingLoopIndex = 0;
static bool startedCapture = false;

static sampling_callback cb;
static uint64_t prevSampleCallbackTick = 0;
static uint64_t sampleCallbackTick = 0;
static uint8_t ellipseCounter = 0;

void triggerSamplingCallback() {
	// time from last call of this function calculation
	prevSampleCallbackTick = sampleCallbackTick;
	sampleCallbackTick = gettime();
	
	readController(false);
	
	// detection logic
	if (trigState != TRIG_DISPLAY_LOCK && captureStartFrameCooldown == 0) {
		// record current data
		curr.triggerL = PAD_TriggerL(0);
		curr.triggerR = PAD_TriggerR(0);
		curr.buttons = *held;
		curr.timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
		
		if (startedCapture) {
			// capture data
			(*temp)->samples[(*temp)->sampleEnd] = curr;
			(*temp)->sampleEnd++;
			
			// fill buffer
			if ((*temp)->sampleEnd == TRIGGER_SAMPLES) {
				trigState = TRIG_DISPLAY;
				startedCapture = false;
				(*temp)->isRecordingReady = true;
				if (captureSelection == TRIGGER_L) {
					(*temp)->recordingType = REC_TRIGGER_L;
				} else {
					(*temp)->recordingType = REC_TRIGGER_R;
				}
				
				displaySelection = captureSelection;
				captureSelection = TRIGGER_NONE;
				flipData();
				captureStartFrameCooldown = 5;
			}
		} else {
			// continuously capture data
			startingLoop[startingLoopIndex] = curr;
			startingLoopIndex++;
			if (startingLoopIndex == 200) {
				startingLoopIndex = 0;
			}
			// check for analog value above 42, or for any digital trigger
			if (curr.triggerL >= 43 || curr.buttons & PAD_TRIGGER_L) {
				captureSelection = TRIGGER_L;
			} else if (curr.triggerR >= 43 || curr.buttons & PAD_TRIGGER_R) {
				captureSelection = TRIGGER_R;
			}
			if (captureSelection != TRIGGER_NONE) {
				clearRecordingArray(*temp);
				(*temp)->sampleEnd = 0;
				// prepend ~25 ms of data to the recording
				int loopStartIndex = startingLoopIndex - 1;
				if (loopStartIndex == -1) {
					loopStartIndex = 200;
				}
				int loopPrependCount = 0;
				uint64_t prependedTimeUs = 0;
				// go back 25 ms
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
					}
					(*temp)->sampleEnd++;
				}
				// clear startingLoop
				for (int i = 0; i < 200; i++) {
					startingLoop[i].triggerL = 0;
					startingLoop[i].triggerR = 0;
					startingLoop[i].buttons = 0;
					startingLoop[i].timeDiffUs = 0;
				}
				(*temp)->isRecordingReady = false;
				(*temp)->dataExported = false;
				trigState = TRIG_INPUT;
				startedCapture = true;
			}
		}
	}
}

static void setup() {
	if (pressed == NULL) {
		pressed = getButtonsDownPtr();
		held = getButtonsHeldPtr();
	}
	setSamplingRateHigh();
	cb = PAD_SetSamplingCallback(triggerSamplingCallback);
	if (data == NULL) {
		data = getRecordingData();
		temp = getTempData();
	}
	
	// check if existing recording is valid for this menu
	if (!(RECORDING_TYPE_VALID_MENUS[(*data)->recordingType] & (REC_TRIGGER_L_FLAG | REC_TRIGGER_R_FLAG))) {
		clearRecordingArray(*data);
		trigState = TRIG_INPUT;
	}
	menuState = TRIG_POST_SETUP;
}

static void displayInstructions() {
	setCursorPos(2, 0);
	printStr("Press and release either trigger to capture. A capture will\n"
			 "start if a digital press is detected, or if the analog value\n"
			 "is above 42. Capture will be displayed once the buffer fills\n"
			 "(500 samples).\n\n"
			 "A Green line indicates when a digital press is detected.\n"
			 "A Gray line shows the minimum value Melee uses for Analog\n"
			 "shield (43 or above).\n\n"
			 "Percents for projectile powershields are shown at the bottom.\n");
	
	setCursorPos(21, 0);
	printStr("Press Z to close instructions.");
	
	if (*pressed & PAD_TRIGGER_Z) {
		menuState = TRIG_POST_SETUP;
	}
}

void menu_triggerOscilloscope() {
	switch (menuState) {
		case TRIG_SETUP:
			setup();
			break;
		case TRIG_POST_SETUP:
			// we're getting the address of the object itself here, not the address of the pointer,
			// which means we will always point to the same object, regardless of a flip
			ControllerRec *dispData = *data;
			
			switch (trigState) {
				case TRIG_INPUT:
					// nothing happens here other than showing the message about waiting for an input
					// the sampling callback function will change the trigState enum when an input is done
					setCursorPos(2,0);
					printStr("Waiting for input.");
					printEllipse(ellipseCounter, 20);
					ellipseCounter++;
					if (ellipseCounter == 60) {
						ellipseCounter = 0;
					}
				case TRIG_DISPLAY_LOCK:
					// TODO: this is dumb, do this a better way to fit better
					if (trigState == TRIG_DISPLAY_LOCK) {
						setCursorPos(2, 28);
						printStrColor(GX_COLOR_WHITE, GX_COLOR_BLACK, "LOCKED");
					}
				case TRIG_DISPLAY:
					if (dispData->isRecordingReady) {
						// bounding box
						drawSolidBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128,
						             SCREEN_TIMEPLOT_START + 501, SCREEN_POS_CENTER_Y + 128, GX_COLOR_BLACK);
						drawBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128,
								SCREEN_TIMEPLOT_START + 501, SCREEN_POS_CENTER_Y + 128, GX_COLOR_WHITE);
						
						// line at 43, start of melee analog shield range
						drawLine(SCREEN_TIMEPLOT_START, (SCREEN_POS_CENTER_Y + 85),
								  SCREEN_TIMEPLOT_START + 500, (SCREEN_POS_CENTER_Y + 85), GX_COLOR_GRAY);
						
						uint8_t curr = 0;
						
						uint16_t selectionMask;
						
						if (displaySelection == TRIGGER_L) {
							selectionMask = PAD_TRIGGER_L;
						} else if (displaySelection == TRIGGER_R) {
							selectionMask = PAD_TRIGGER_R;
						} else {
							printStr("Data ready but selection is not valid.");
							break;
						}
						
						uint64_t totalTime = 0;
						
						// we need to calculate the total number of vertices ahead of time,
						// frame interval and digital press will be an unknown amount
						int frameIntervalIndex = 0;
						int frameIntervalList[500];
						int digitalPressInterval = 0;
						int digitalPressList[500];
						
						// cycle through data and determine frame intervals and digital presses
						for (int i = 0; i < 500; i++) {
							// frame intervals
							totalTime += dispData->samples[i].timeDiffUs;
							if (totalTime >= FRAME_TIME_US) {
								frameIntervalList[frameIntervalIndex] = i;
								frameIntervalIndex++;
								totalTime = 0;
							}
							
							// digital presses
							if (dispData->samples[i].buttons & selectionMask) {
								digitalPressList[digitalPressInterval] = i;
								digitalPressInterval++;
							}
						}
						
						// draw frame intervals
						updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
						
						GX_Begin(GX_LINES, VTXFMT_PRIMITIVES_RGB, (frameIntervalIndex * 2));
						for (int i = 0; i < frameIntervalIndex; i++) {
							GX_Position3s16(SCREEN_TIMEPLOT_START + frameIntervalList[i], (SCREEN_POS_CENTER_Y - 127), -5);
							GX_Color3u8(GX_COLOR_GRAY.r, GX_COLOR_GRAY.g, GX_COLOR_GRAY.b);
							
							GX_Position3s16(SCREEN_TIMEPLOT_START + frameIntervalList[i], (SCREEN_POS_CENTER_Y - 112), -5);
							GX_Color3u8(GX_COLOR_GRAY.r, GX_COLOR_GRAY.g, GX_COLOR_GRAY.b);
						}
						GX_End();
						
						// draw digital presses
						GX_Begin(GX_POINTS, VTXFMT_PRIMITIVES_RGB, digitalPressInterval);
						for (int i = 0; i < digitalPressInterval; i++) {
							GX_Position3s16(SCREEN_TIMEPLOT_START + digitalPressList[i], (SCREEN_POS_CENTER_Y + 28), -4);
							GX_Color3u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b);
						}
						GX_End();
						
						// draw analog data
						GX_Begin(GX_LINESTRIP, VTXFMT_PRIMITIVES_RGB, 500);
						for (int i = 0; i < 500; i++) {
							switch (displaySelection) {
								case TRIGGER_L:
									curr = dispData->samples[i].triggerL;
									break;
								case TRIGGER_R:
									curr = dispData->samples[i].triggerR;
									break;
								default:
									curr = 0;
									break;
							}
							
							GX_Position3s16(SCREEN_TIMEPLOT_START + i, (SCREEN_POS_CENTER_Y + 128) - curr, -3);
							GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
						}
						GX_End();
						
						setCursorPos(3, 27);
						switch (displaySelection) {
							case TRIGGER_L:
								printStr("L Trigger");
								break;
							case TRIGGER_R:
								printStr("R Trigger");
								break;
							default:
								printStr("Capture selection invalid");
								break;
						}
						
						// calculate projectile powershield percentages
						// mostly based on phobvision code:
						// https://github.com/PhobGCC/PhobGCC-SW/blob/main/PhobGCC/rp2040/src/main.cpp#L581
						float psDigital = 0.0, psADT = 0.0, psNone = 0.0;
						uint64_t timeInAnalogRangeUs = 0;
						int sampleDigitalBegin = -1;
						switch (displaySelection) {
							case TRIGGER_L:
								for (int i = 0; i < dispData->sampleEnd; i++) {
									if (dispData->samples[i].buttons & PAD_TRIGGER_L) {
										sampleDigitalBegin = i;
										break;
									}
									if (dispData->samples[i].triggerL > 42) {
										timeInAnalogRangeUs += dispData->samples[i].timeDiffUs;
									}
								}
								break;
							case TRIGGER_R:
								for (int i = 0; i < dispData->sampleEnd; i++) {
									if (dispData->samples[i].buttons & PAD_TRIGGER_R) {
										sampleDigitalBegin = i;
										break;
									}
									if (dispData->samples[i].triggerR > 42) {
										timeInAnalogRangeUs += dispData->samples[i].timeDiffUs;
									}
								}
								break;
							default:
								printStr("Capture selection invalid");
								break;
						}
						
						// float representing percent of a frame
						float analogRangeFrame = (timeInAnalogRangeUs / 1000.0) / FRAME_TIME_MS_F;
						
						// digital press never occurred
						if (sampleDigitalBegin == -1) {
							// do nothing
						// time before digital press is more than a frame
						} else if (analogRangeFrame > 1) {
							psADT = 100 * (2 - analogRangeFrame);
							if (psADT < 0) {
								psADT = 0;
							}
							psNone = 100 - psADT;
						// time before digital press is less than/equal to a frame
						} else {
							psDigital = 100 * (1 - analogRangeFrame);
							psADT = 100 - psDigital;
						}
						
						setCursorPos(20, 0);
						printStr("Digital PS: %5.1f%% | ADT PS: %5.1f%% | No PS: %5.1f%%", psDigital, psADT, psNone);

						if (*pressed & PAD_BUTTON_A) {
							if (trigState == TRIG_DISPLAY) {
								trigState = TRIG_DISPLAY_LOCK;
							} else {
								trigState = TRIG_DISPLAY;
							}
						}
					} else {
						trigState = TRIG_INPUT;
					}
					if (*pressed & PAD_TRIGGER_Z) {
						menuState = TRIG_INSTRUCTIONS;
					}
					if (captureStartFrameCooldown != 0 && PAD_TriggerL(0) < 43 && PAD_TriggerR(0) < 43) {
						captureStartFrameCooldown--;
					}
					break;
				default:
					printStr("trigState default case! how did we get here?");
					break;
			}
			break;
		case TRIG_INSTRUCTIONS:
			displayInstructions();
			break;
		default:
			printStr("menuState default case! how did we get here?");
			break;
	}
}

void menu_triggerOscilloscopeEnd() {
	setSamplingRateNormal();
	PAD_SetSamplingCallback(cb);
	pressed = NULL;
	held = NULL;
	menuState = TRIG_SETUP;
	if (!(*temp)->isRecordingReady) {
		(*temp)->sampleEnd = 0;
		startedCapture = false;
	}
}
