//
// Created on 7/22/25.
//

#include "submenu/plotbutton.h"

#include <stdlib.h>
#include <stdint.h>

#include <ogc/pad.h>
#include <ogc/timesupp.h>
#include <ogc/color.h>

#include "polling.h"
#include "print.h"
#include "draw.h"

const static int SCREEN_TIMEPLOT_START = 95;
const static int SCREEN_TIMEPLOT_Y_TOP = 150;
const static int SCREEN_TIMEPLOT_Y_BOTTOM = 380;
const static int SCREEN_TIMEPLOT_CHAR_TOP = 159;
const static int SCREEN_CHAR_SIZE = 14;

enum PLOT_BUTTON_LIST { A, B, X, Y, L, La, R, Ra, Z, AX, AY, CX, CY, NO_BUTTON };

const static char* BUTTON_STR[13] = { "A", "B", "X", "Y",
									  "L", "La", "R", "Ra", "Z",
									  "AX", "AY", "CX", "CY"};

const static uint16_t BUTTON_MASKS[13] = { PAD_BUTTON_A, PAD_BUTTON_B, PAD_BUTTON_X, PAD_BUTTON_Y,
									  PAD_TRIGGER_L, 0, PAD_TRIGGER_R, 0, PAD_TRIGGER_Z,
									  0, 0, 0, 0 };

static uint16_t *pressed = NULL;
static uint16_t *held = NULL;
static bool buttonLock = false;
static uint8_t buttonPressCooldown = 0;

static uint8_t stickThreshold = 23;
static uint8_t triggerThreshold = 255;
// since there are two options, this can be a bool. false = trigger selected
static bool stickThresholdSelected = true;
static uint8_t thresholdFrameCounter = 0;
//static bool capture400Toggle = false;
//static bool callbackCapture400 = false;
//static bool displayCapture400 = false;

static enum PLOT_BUTTON_MENU_STATE menuState = BUTTON_SETUP;
static enum PLOT_BUTTON_STATE state = BUTTON_INPUT;

// what button/input triggered the start of the recording
static enum PLOT_BUTTON_LIST triggeringInput = NO_BUTTON;
static enum PLOT_BUTTON_LIST triggeringInputDisplay = NO_BUTTON;

// structs for storing controller data
// data: used for display once marked ready
// temp: used by the callback function while data is being collected
// structs are flipped silently by calling flipData() from waveform.h, so we don't have to change anything here
static ControllerRec **data = NULL, **temp = NULL;
static uint8_t triggerLAnalog = 0, triggerRAnalog = 0;
static bool captureStart = false;
static bool captureButtonsReleased = false;
static bool autoCapture = false;
static uint8_t autoCaptureCounter = 0;
static int totalCaptureTime = 200000;

static sampling_callback cb;
static uint64_t prevSampleCallbackTick = 0;
static uint64_t sampleCallbackTick = 0;
static uint64_t pressedTimer = 0;
static uint8_t ellipseCounter = 0;
static bool pressLocked = false;

typedef struct ButtonPressedTime {
	uint64_t timeHeld;
	bool pressFinished;
} ButtonPressedTime;

static void plotButtonSamplingCallback() {
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
	
	if (state == BUTTON_INPUT || autoCapture) {
		// wait for buttons to be released before allowing a capture
		if (!captureButtonsReleased) {
			if (*held == 0 && abs(PAD_StickX(0)) < stickThreshold && abs(PAD_StickY(0)) < stickThreshold &&
					abs(PAD_SubStickX(0)) < stickThreshold && abs(PAD_SubStickY(0)) < stickThreshold &&
					PAD_TriggerL(0) < triggerThreshold && PAD_TriggerR(0) < triggerThreshold) {
				captureButtonsReleased = true;
			}
		// are we already capturing data?
		} else if (captureStart) {
			(*temp)->samples[(*temp)->sampleEnd].stickX = PAD_StickX(0);
			(*temp)->samples[(*temp)->sampleEnd].stickY = PAD_StickY(0);
			(*temp)->samples[(*temp)->sampleEnd].cStickX = PAD_SubStickX(0);
			(*temp)->samples[(*temp)->sampleEnd].cStickY = PAD_SubStickY(0);
			(*temp)->samples[(*temp)->sampleEnd].triggerL = PAD_TriggerL(0);
			(*temp)->samples[(*temp)->sampleEnd].triggerR = PAD_TriggerR(0);
			(*temp)->samples[(*temp)->sampleEnd].buttons = *held;
			(*temp)->samples[(*temp)->sampleEnd].timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
			(*temp)->sampleEnd++;
			
			(*temp)->totalTimeUs += ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
			
			// capture either ~200 or ~400 ms of data, depending on what the capture400Toggle was set to when recording started
			// TODO: toggling record time is disabled for now...
			if ((*temp)->totalTimeUs >= totalCaptureTime || (*temp)->sampleEnd == REC_SAMPLE_MAX) {
				(*temp)->totalTimeUs = 0;
				(*temp)->isRecordingReady = true;
				(*temp)->recordingType = REC_BUTTONTIME;
				flipData();
				captureStart = false;
				captureButtonsReleased = false;
				if (!autoCapture) {
					state = BUTTON_DISPLAY;
				}
				triggeringInputDisplay = triggeringInput;
				/*
				if (callbackCapture400) {
					displayCapture400 = true;
				} else {
					displayCapture400 = false;
				}*/
			}

		// we haven't started recording yet, wait for stick/trigger to move outside user-defined range, or a button press
		} else {
			// determine what triggered the input
			triggeringInput = NO_BUTTON;

			for (enum PLOT_BUTTON_LIST button = A; button < NO_BUTTON; button++) {
				switch (button) {
					// analog values
					case AX:
						if (abs(PAD_StickX(0)) >= stickThreshold) {
							triggeringInput = button;
						}
						break;
					case AY:
						if (abs(PAD_StickY(0)) >= stickThreshold) {
							triggeringInput = button;
						}
						break;
					case CX:
						if (abs(PAD_SubStickX(0)) >= stickThreshold) {
							triggeringInput = button;
						}
						break;
					case CY:
						if (abs(PAD_SubStickY(0)) >= stickThreshold) {
							triggeringInput = button;
						}
						break;
					case La:
						if (PAD_TriggerL(0) >= triggerThreshold) {
							triggeringInput = button;
						}
						break;
					case Ra:
						if (PAD_TriggerR(0) >= triggerThreshold) {
							triggeringInput = button;
						}
						break;
					// digital values/buttons
					default:
						if (*held & BUTTON_MASKS[button]) {
							triggeringInput = button;
							break;
						}
				}
				
				// leave loop if we've found our triggering input
				if (triggeringInput != NO_BUTTON) {
					break;
				}
			}
			
			// write our data if an input was detected
			if (triggeringInput != NO_BUTTON) {
				captureStart = true;
				clearRecordingArray(*temp);
				(*temp)->samples[0].stickX = PAD_StickX(0);
				(*temp)->samples[0].stickY = PAD_StickY(0);
				(*temp)->samples[0].cStickX = PAD_SubStickX(0);
				(*temp)->samples[0].cStickY = PAD_SubStickY(0);
				(*temp)->samples[0].triggerL = triggerLAnalog;
				(*temp)->samples[0].triggerR = triggerRAnalog;
				(*temp)->samples[0].buttons = *held;
				(*temp)->samples[0].timeDiffUs = 0;
				(*temp)->sampleEnd = 1;
				(*temp)->isRecordingReady = false;
				(*temp)->dataExported = false;
				
				/*
				callbackCapture400 = capture400Toggle;
				if (capture400Toggle) {
					totalCaptureTime = 400000;
				} else {
					totalCaptureTime = 200000;
				}*/
			}
		}
	}
}

static void setup() {
	setSamplingRateHigh();
	if (pressed == NULL) {
		pressed = getButtonsDownPtr();
		held = getButtonsHeldPtr();
	}
	cb = PAD_SetSamplingCallback(plotButtonSamplingCallback);
	if (data == NULL) {
		data = getRecordingData();
		temp = getTempData();
	}
	menuState = BUTTON_POST_SETUP;
	state = BUTTON_DISPLAY;

	// check if existing recording is valid for this menu
	if (!(RECORDING_TYPE_VALID_MENUS[(*data)->recordingType] & REC_BUTTONTIME_FLAG)) {
		clearRecordingArray(*data);
	}
	
	// prevent pressing for a short time upon entering menu
	buttonLock = true;
	buttonPressCooldown = 5;
}

static void displayInstructions() {
	setCursorPos(2, 0);
	printStr("Press A to prepare a recording. Recording will start when\n"
			 "a digital button is pressed, or when an analog value moves\n"
			 "beyond its threshold. Recording will last ~200 ms.\n\n"
			 "Vertical gray lines show ~1 frame (~16.6ms) boundaries.\n"
			 "Frame times are shown on the right. The highlighted number\n"
			 "shows what button started the recording, and how long its\n"
			 "initial input was held. The other numbers show how much time\n"
			 "passed between the recording starting, and when the first\n"
			 "input was detected for a given button.\n\n"
			 "Use the D-Pad to adjust the thresholds.\n"
			 "Hold R to change thresholds faster.\n"
			 "Hold Start to toggle Auto-Trigger. Enabling this removes\n"
			 "the need to press A, but disables the instruction menu (Z),\n"
			 "and the R modifier.\n");
	
	setCursorPos(21, 0);
	printStr("Press Z to close instructions.");
	
	if (!buttonLock) {
		if (*pressed & PAD_TRIGGER_Z) {
			menuState = BUTTON_POST_SETUP;
			buttonLock = true;
			buttonPressCooldown = 5;
		}
	}
}

void menu_plotButton() {
	switch (menuState) {
		case BUTTON_SETUP:
			setup();
			break;
		case BUTTON_INSTRUCTIONS:
			displayInstructions();
			break;
		case BUTTON_POST_SETUP:
			// we're getting the address of the object itself here, not the address of the pointer,
			// which means we will always point to the same object, regardless of a flip
			ControllerRec *dispData = *data;
			
			// set here for the same reason as the above var
			// since this is separate from recording logic, this could change during draw
			//bool menuDisplay400 = displayCapture400;
			
			switch (state) {
				case BUTTON_INPUT:
					// nothing happens here other than showing the message about waiting for an input
					// the sampling callback function will change the plotState enum when an input is done
					setCursorPos(2,0);
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
					
				case BUTTON_DISPLAY:
					if (!autoCapture && state != BUTTON_INPUT) {
						printStr("Press A to start read, press Z for instructions");
					}
					
					setCursorPos(4,7);
					if (stickThresholdSelected) {
						printStrColor(COLOR_WHITE, COLOR_BLACK, "Stick Threshold:");
						printStr(" %3u", stickThreshold);
						setCursorPos(4, 32);
						printStr("Trigger Threshold: %3u", triggerThreshold);
					} else {
						printStr("Stick Threshold: %3u", stickThreshold);
						setCursorPos(4, 32);
						printStrColor(COLOR_WHITE, COLOR_BLACK, "Trigger Threshold:");
						printStr(" %3u", triggerThreshold);
					}
					
					//setCursorPos(21,38);
					//printStr("Capture length: %3dms", (capture400Toggle ? 400 : 200));
					
					if (dispData->isRecordingReady) {
						DrawBox(SCREEN_TIMEPLOT_START - 55, SCREEN_TIMEPLOT_Y_TOP - 1, 600, SCREEN_TIMEPLOT_Y_BOTTOM + 1, COLOR_WHITE);;
						
						for (enum PLOT_BUTTON_LIST button = A; button < NO_BUTTON; button++) {
							setCursorPos(7 + button, 4);
							if (button == triggeringInputDisplay) {
								printStrColor(COLOR_WHITE, COLOR_BLACK, BUTTON_STR[button]);
							} else {
								printStr(BUTTON_STR[button]);
							}
						}
						
						uint64_t totalTimeUs = 0;
						ButtonPressedTime buttons[13] = { {0, false} };
						
						// initial "frame" line
						DrawVLine(SCREEN_TIMEPLOT_START, SCREEN_TIMEPLOT_Y_TOP,
						          SCREEN_TIMEPLOT_Y_BOTTOM,
						          COLOR_SILVER);
						DrawVLine(SCREEN_TIMEPLOT_START + 1, SCREEN_TIMEPLOT_Y_TOP,
						          SCREEN_TIMEPLOT_Y_BOTTOM,
						          COLOR_SILVER);
						
						int currMs = 0;
						int frameIntervalIndex = 0;
						
						// draw data
						for (int i = 0; i < dispData->sampleEnd; i++) {
							// frame intervals first
							totalTimeUs += dispData->samples[i].timeDiffUs;
							
							if (totalTimeUs >= (1000 * currMs)) {
								currMs++;
								if (totalTimeUs / 1000 >= FRAME_INTERVAL_MS[frameIntervalIndex]) {
									/*
									if (menuDisplay400) {
										DrawVLine(SCREEN_TIMEPLOT_START + currMs, SCREEN_TIMEPLOT_Y_TOP,
										          SCREEN_TIMEPLOT_Y_BOTTOM,
										          COLOR_GRAY);
									} else {*/
										DrawVLine(SCREEN_TIMEPLOT_START + (currMs * 2), SCREEN_TIMEPLOT_Y_TOP,
										          SCREEN_TIMEPLOT_Y_BOTTOM,
										          COLOR_GRAY);
										DrawVLine(SCREEN_TIMEPLOT_START + (currMs * 2) + 1, SCREEN_TIMEPLOT_Y_TOP,
										          SCREEN_TIMEPLOT_Y_BOTTOM,
										          COLOR_GRAY);
									//}
									
									frameIntervalIndex++;
									/*
									if (menuDisplay400) {
										if (currMs >= 400000) {
											break;
										}
									} else {*/
										if (currMs >= 200000) {
											break;
										}
									//}
								}
							}
							
							// button press lines
							// we also get frame count here
							for (enum PLOT_BUTTON_LIST currButton = A; currButton < NO_BUTTON; currButton++) {
								bool result = false;
								switch (currButton) {
									// handle all specific cases
									case AX:
										result = abs(dispData->samples[i].stickX) >= stickThreshold;
										break;
									case AY:
										result = abs(dispData->samples[i].stickY) >= stickThreshold;
										break;
									case CX:
										result = abs(dispData->samples[i].cStickX) >= stickThreshold;
										break;
									case CY:
										result = abs(dispData->samples[i].cStickY) >= stickThreshold;
										break;
									case La:
										result = dispData->samples[i].triggerL >= triggerThreshold;
										break;
									case Ra:
										result = dispData->samples[i].triggerR >= triggerThreshold;
										break;
									// "normal" cases
									default:
										result = dispData->samples[i].buttons & BUTTON_MASKS[currButton];
										break;
								}
								
								// draw bar if button state was triggered
								if (result) {
									/*
									if (menuDisplay400) {
										DrawVLine(SCREEN_TIMEPLOT_START + currMs, SCREEN_TIMEPLOT_CHAR_TOP + (17 * currButton),
										          SCREEN_TIMEPLOT_CHAR_TOP + (17 * currButton) + SCREEN_CHAR_SIZE,
										          COLOR_WHITE);
									} else {*/
										DrawVLine(SCREEN_TIMEPLOT_START + (currMs * 2), SCREEN_TIMEPLOT_CHAR_TOP + (17 * currButton),
										          SCREEN_TIMEPLOT_CHAR_TOP + (17 * currButton) + SCREEN_CHAR_SIZE,
										          COLOR_WHITE);
										DrawVLine(SCREEN_TIMEPLOT_START + (currMs * 2) + 1, SCREEN_TIMEPLOT_CHAR_TOP + (17 * currButton),
										          SCREEN_TIMEPLOT_CHAR_TOP + (17 * currButton) + SCREEN_CHAR_SIZE,
										          COLOR_WHITE);
									//}
								}
								
								// calculate what timing to show if not marked done
								if (!buttons[currButton].pressFinished) {
									// triggering button
									if (currButton == triggeringInputDisplay) {
										if (result) {
											// triggering input should have length of time that the first input was held
											if (currButton == triggeringInputDisplay) {
												buttons[currButton].timeHeld += dispData->samples[i].timeDiffUs;
											}
										} else if (buttons[currButton].timeHeld != 0) {
											buttons[currButton].pressFinished = true;
										}
									}
									// literally everything else
									else {
										if (result) {
											buttons[currButton].timeHeld = totalTimeUs;
											buttons[currButton].pressFinished = true;
										}
									}
								}
							}
						}
						
						// draw end line
						/*if (menuDisplay400) {
							DrawVLine(SCREEN_TIMEPLOT_START + currMs, SCREEN_TIMEPLOT_Y_TOP,
							          SCREEN_TIMEPLOT_Y_BOTTOM,
							          COLOR_SILVER);
						} else {*/
							DrawVLine(SCREEN_TIMEPLOT_START + (currMs * 2), SCREEN_TIMEPLOT_Y_TOP,
							          SCREEN_TIMEPLOT_Y_BOTTOM,
							          COLOR_SILVER);
							DrawVLine(SCREEN_TIMEPLOT_START + (currMs * 2) + 1, SCREEN_TIMEPLOT_Y_TOP,
							          SCREEN_TIMEPLOT_Y_BOTTOM,
							          COLOR_SILVER);
						//}
						
						// draw frame durations
						for (enum PLOT_BUTTON_LIST button = A; button < NO_BUTTON; button++) {
							if (buttons[button].timeHeld != 0) {
								setCursorPos(7 + button, 52);
								float frames = buttons[button].timeHeld / FRAME_TIME_US_F;
								if (frames >= 10) {
									setCursorPos(7 + button, 51);
								}
								// indicate the initial input with black on white text
								if (button == triggeringInputDisplay) {
									printStrColor(COLOR_WHITE, COLOR_BLACK, "%2.2ff", frames);
								} else {
									printStr("%2.2ff", frames);
								}
							}
						}
					}
					
					// single presses
					if (!buttonLock) {
						// disable certain buttons a capture is occurring
						if (!autoCapture && state != BUTTON_INPUT) {
							if (*pressed & PAD_BUTTON_A) {
								state = BUTTON_INPUT;
								buttonLock = true;
								buttonPressCooldown = 5;
							} else if (*pressed & PAD_TRIGGER_Z) {
								menuState = BUTTON_INSTRUCTIONS;
								buttonLock = true;
								buttonPressCooldown = 5;
							} /* else if (*pressed & PAD_BUTTON_Y) {
								capture400Toggle = !capture400Toggle;
								buttonLock = true;
								buttonPressCooldown = 5;
							}*/
						}
						if (*pressed & PAD_BUTTON_RIGHT) {
							stickThresholdSelected = false;
							buttonLock = true;
							buttonPressCooldown = 5;
						} else if (*pressed & PAD_BUTTON_LEFT) {
							stickThresholdSelected = true;
							buttonLock = true;
							buttonPressCooldown = 5;
						}
					}
					
					// changing the thresholds
					if (*held & PAD_TRIGGER_R && !autoCapture) {
						if (*held & PAD_BUTTON_UP) {
							if (stickThresholdSelected) {
								if (stickThreshold < 100) {
									stickThreshold++;
								}
							} else {
								if (triggerThreshold < 255) {
									triggerThreshold++;
								}
							}
						} else if (*held & PAD_BUTTON_DOWN) {
							if (stickThresholdSelected) {
								if (stickThreshold > 0) {
									stickThreshold--;
								}
							} else {
								if (triggerThreshold > 0) {
									triggerThreshold--;
								}
							}
						}
					} else {
						if (thresholdFrameCounter == 0) {
							if (*held & PAD_BUTTON_UP) {
								if (stickThresholdSelected) {
									if (stickThreshold < 100) {
										stickThreshold++;
										thresholdFrameCounter = 10;
									}
								} else {
									if (triggerThreshold < 255) {
										triggerThreshold++;
										thresholdFrameCounter = 10;
									}
								}
							} else if (*held & PAD_BUTTON_DOWN) {
								if (stickThresholdSelected) {
									if (stickThreshold > 0) {
										stickThreshold--;
										thresholdFrameCounter = 10;
									}
								} else {
									if (triggerThreshold > 0) {
										triggerThreshold--;
										thresholdFrameCounter = 10;
									}
								}
							}
						} else {
							thresholdFrameCounter--;
						}
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
							if (autoCapture) {
								state = BUTTON_INPUT;
							} else {
								state = BUTTON_DISPLAY;
							}
							captureButtonsReleased = false;
							buttonLock = true;
							buttonPressCooldown = 5;
							autoCaptureCounter = 0;
						}
					} else {
						printStr("Hold start to toggle Auto-Trigger.");
						autoCaptureCounter = 0;
					}
					break;

				default:
					printStr("button plot default case?");
					break;
			}
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

void menu_plotButtonEnd() {
	setSamplingRateNormal();
	PAD_SetSamplingCallback(cb);
	captureStart = false;
	captureButtonsReleased = false;
	if (!(*temp)->isRecordingReady) {
		(*temp)->sampleEnd = 0;
	}
	autoCaptureCounter = 0;
	autoCapture = false;
	pressed = NULL;
	held = NULL;
	menuState = BUTTON_SETUP;
}

bool menu_plotButtonHasCaptureStarted() {
	return captureStart;
}
