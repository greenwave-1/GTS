//
// Created on 5/16/25.
//

// Code in use from the PhobGCC project is licensed under GPLv3. A copy of this license is provided in the root
// directory of this project's repository.

// Upstream URL for the PhobGCC project is: https://github.com/PhobGCC/PhobGCC-SW

// Other than adt ps math, this mostly clones the visual side of phobvision

#include "submenu/trigger.h"

#include <stdio.h>
#include <stdint.h>

#include <ogc/pad.h>
#include <ogc/timesupp.h>
#include <ogc/color.h>

#include "draw.h"
#include "polling.h"
#include "print.h"

static const float FRAME_TIME_MS = (1000/60.0);
const static uint8_t SCREEN_TIMEPLOT_START = 70;

static uint32_t *pressed = NULL;
static uint32_t *held = NULL;
static bool buttonLock = false;
static uint8_t buttonPressCooldown = 0;
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
// once conditions are met, ~50ms of previous data is added to the start of the capture from here
static ControllerSample startingLoop[100];
static int startingLoopIndex = 0;
static bool startedCapture = false;

static sampling_callback cb;
static uint64_t prevSampleCallbackTick = 0;
static uint64_t sampleCallbackTick = 0;
static uint64_t pressedTimer = 0;
static uint8_t ellipseCounter = 0;
static bool pressLocked = false;

static char strBuffer[100];

void triggerSamplingCallback() {
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
				(*temp)->recordingType = REC_TRIGGER;
				displaySelection = captureSelection;
				captureSelection = TRIGGER_NONE;
				flipData();
				captureStartFrameCooldown = 5;
			}
		} else {
			// continuously capture data
			startingLoop[startingLoopIndex] = curr;
			startingLoopIndex++;
			if (startingLoopIndex == 100) {
				startingLoopIndex = 0;
			}
			// check for analog value above 42, or for any digital trigger
			if (curr.triggerL >= 43 || curr.buttons & PAD_TRIGGER_L) {
				captureSelection = TRIGGER_L;
			} else if (curr.triggerR >= 43 || curr.buttons & PAD_TRIGGER_R) {
				captureSelection = TRIGGER_R;
			}
			if (captureSelection != TRIGGER_NONE) {
				(*temp)->sampleEnd = 0;
				// prepend ~50 ms of data to the recording
				int loopStartIndex = startingLoopIndex - 1;
				if (loopStartIndex == -1) {
					loopStartIndex = 100;
				}
				int loopPrependCount = 0;
				uint64_t prependedTimeUs = 0;
				// go back 50 ms
				while (prependedTimeUs < 50000) {
					// break out of loop if data doesn't exist
					if (startingLoop[loopStartIndex].timeDiffUs == 0) {
						break;
					}
					prependedTimeUs += startingLoop[loopStartIndex].timeDiffUs;
					loopStartIndex--;
					if (loopStartIndex == -1) {
						loopStartIndex = 100;
					}
					loopPrependCount++;
				}
				for (int i = 0; i < loopPrependCount; i++ ) {
					(*temp)->samples[(*temp)->sampleEnd] = startingLoop[(loopStartIndex + i) % 100];
					if ((*temp)->sampleEnd == 0) {
						(*temp)->samples[0].timeDiffUs = 0;
					}
					(*temp)->sampleEnd++;
				}
				// clear startingLoop
				for (int i = 0; i < 100; i++) {
					startingLoop[i].triggerL = 0;
					startingLoop[i].triggerR = 0;
					startingLoop[i].buttons = 0;
					startingLoop[i].timeDiffUs = 0;
				}
				(*temp)->isRecordingReady = false;
				trigState = TRIG_INPUT;
				startedCapture = true;
			}
		}
	}
}

static void setup(uint32_t *p, uint32_t *h) {
	pressed = p;
	held = h;
	setSamplingRateHigh();
	cb = PAD_SetSamplingCallback(triggerSamplingCallback);
	if (data == NULL) {
		data = getRecordingData();
		temp = getTempData();
	}
	
	// don't use data not recorded for triggers
	// _technically_ this doesn't need to happen, but other recording types are basically useless here
	if ((*data)->recordingType != REC_TRIGGER) {
		clearRecordingArray(*data);
		trigState = TRIG_INPUT;
	}
	menuState = TRIG_POST_SETUP;
}

static void displayInstructions(void *currXfb) {
	setCursorPos(2, 0);
	printStr("Press and release either trigger to capture. A capture will\n"
			 "start if a digital press is detected, or if the analog value\n"
			 "is above 42. Capture will once the buffer fills\n"
			 "(500 samples).\n\n"
			 "A Green line indicates when a digital press is detected.\n"
			 "A Gray line shows the minimum value Melee uses for Analog\n"
			 "shield (43 or above).\n\n"
			 "Percents for projectile powershields are shown at the bottom.\n", currXfb);
	
	setCursorPos(21, 0);
	printStr("Press Z to close instructions.", currXfb);
	
	if (!buttonLock) {
		if (*pressed & PAD_TRIGGER_Z) {
			menuState = TRIG_POST_SETUP;
			buttonLock = true;
			buttonPressCooldown = 5;
		}
	}
}

void menu_triggerOscilloscope(void *currXfb, uint32_t *p, uint32_t *h) {
	// we're getting the address of the object itself here, not the address of the pointer,
	// which means we will always point to the same object, regardless of a flip
	ControllerRec *dispData = *data;
	
	switch (menuState) {
		case TRIG_SETUP:
			setup(p, h);
			break;
		case TRIG_POST_SETUP:
			switch (trigState) {
				case TRIG_INPUT:
					// nothing happens here other than showing the message about waiting for an input
					// the sampling callback function will change the trigState enum when an input is done
					setCursorPos(2,0);
					printStr("Waiting for input.", currXfb);
					printEllipse(ellipseCounter, 20, currXfb);
					ellipseCounter++;
					if (ellipseCounter == 60) {
						ellipseCounter = 0;
					}
				case TRIG_DISPLAY_LOCK:
					// TODO: this is dumb, do this a better way to fit better
					if (trigState == TRIG_DISPLAY_LOCK) {
						setCursorPos(2, 28);
						printStrColor("LOCKED", currXfb, COLOR_WHITE, COLOR_BLACK);
					}
				case TRIG_DISPLAY:
					if (dispData->isRecordingReady) {
						// bounding box
						DrawBox(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128, SCREEN_TIMEPLOT_START + 500, SCREEN_POS_CENTER_Y + 128, COLOR_WHITE, currXfb);
						// line at 43, start of melee analog shield range
						DrawHLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_START + 499, (SCREEN_POS_CENTER_Y + 85), COLOR_GRAY, currXfb);
						
						uint8_t curr = 0, prev = 0;
						bool currDigital = false;
						int waveformXPos = 1, waveformPrevXPos = 0;
						
						uint64_t totalTime = 0;
						
						// draw 500 datapoints
						for (int i = 0; i < 500; i++) {
							// make sure we haven't gone outside our bounds
							if (i == dispData->sampleEnd || waveformXPos >= 500) {
								break;
							}
							
							switch (displaySelection) {
								case TRIGGER_L:
									curr = dispData->samples[i].triggerL;
									currDigital = dispData->samples[i].buttons & PAD_TRIGGER_L;
									break;
								case TRIGGER_R:
									curr = dispData->samples[i].triggerR;
									currDigital = dispData->samples[i].buttons & PAD_TRIGGER_R;
									break;
								default:
									//dispData->isRecordingReady = false;
									printStr("Data ready but selection is not valid.", currXfb);
									break;
							}
							
							// frame intervals
							totalTime += dispData->samples[i].timeDiffUs;
							if (totalTime >= 16666) {
								DrawLine(SCREEN_TIMEPLOT_START + waveformXPos, (SCREEN_POS_CENTER_Y - 127),
								         SCREEN_TIMEPLOT_START + waveformXPos, (SCREEN_POS_CENTER_Y - 112),
								         COLOR_GRAY, currXfb);
								totalTime = 0;
							}
							
							// analog
							DrawLine(SCREEN_TIMEPLOT_START + waveformPrevXPos, (SCREEN_POS_CENTER_Y + 128) - prev,
							         SCREEN_TIMEPLOT_START + waveformXPos, (SCREEN_POS_CENTER_Y + 128) - curr,
							         COLOR_WHITE, currXfb);
							prev = curr;
							
							// digital
							if (currDigital) {
								DrawDot(SCREEN_TIMEPLOT_START + waveformXPos, (SCREEN_POS_CENTER_Y + 28),
										COLOR_LIME, currXfb);
							}
							
							// update scaling factor
							waveformPrevXPos = waveformXPos;
							waveformXPos++;
						}
						
						setCursorPos(3, 27);
						switch (displaySelection) {
							case TRIGGER_L:
								printStr("L Trigger", currXfb);
								break;
							case TRIGGER_R:
								printStr("R Trigger", currXfb);
								break;
							default:
								printStr("Capture selection invalid", currXfb);
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
								printStr("Capture selection invalid", currXfb);
								break;
						}
						
						// float representing percent of a frame
						float analogRangeFrame = (timeInAnalogRangeUs / 1000.0) / FRAME_TIME_MS;
						
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
						//sprintf(strBuffer, "%llu %2.2f %d %d\n", timeInAnalogRangeUs, analogRangeFrame, sampleDigitalBegin, data.endPoint);
						//printStr(strBuffer, currXfb);
						sprintf(strBuffer, "Digital PS: %3.1f%% | ADT PS: %3.1f%% | No PS: %3.1f%%", psDigital, psADT, psNone);
						printStr(strBuffer, currXfb);
						
						if (!buttonLock) {
							if (*pressed & PAD_BUTTON_A) {
								if (trigState == TRIG_DISPLAY) {
									trigState = TRIG_DISPLAY_LOCK;
								} else {
									trigState = TRIG_DISPLAY;
								}
								buttonLock = true;
								buttonPressCooldown = 5;
							}
						}
					}
					if (!buttonLock) {
						if (*pressed & PAD_TRIGGER_Z) {
							menuState = TRIG_INSTRUCTIONS;
							buttonLock = true;
							buttonPressCooldown = 5;
						}
					}
					if (captureStartFrameCooldown != 0 && PAD_TriggerL(0) < 43 && PAD_TriggerR(0) < 43) {
						captureStartFrameCooldown--;
					}
					break;
				default:
					printStr("trigState default case! how did we get here?", currXfb);
					break;
			}
			break;
		case TRIG_INSTRUCTIONS:
			displayInstructions(currXfb);
			break;
		default:
			printStr("menuState default case! how did we get here?", currXfb);
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
