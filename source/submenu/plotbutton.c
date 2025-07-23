//
// Created on 7/22/25.
//

#include "plotbutton.h"

#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <stdlib.h>
#include "../polling.h"
#include "../print.h"
#include "../draw.h"

static const float FRAME_TIME_MS = (1000/60.0);

const static int SCREEN_TIMEPLOT_START = 80;
const static int SCREEN_TIMEPLOT_Y_TOP = 150;
const static int SCREEN_TIMEPLOT_Y_BOTTOM = 380;
const static int SCREEN_TIMEPLOT_CHAR_TOP = 159;
const static int SCREEN_CHAR_SIZE = 14;

const static char* BUTTON_STR[13] = { "A", "B", "X", "Y",
									  "L", "La", "R", "Ra", "Z",
									  "AX", "AY", "CX", "CY"};

//const static u32 DETECTABLE_BUTTONS = PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_X | PAD_BUTTON_Y |
//		PAD_TRIGGER_L | PAD_TRIGGER_R | PAD_TRIGGER_Z;

static char strBuffer[100];

static u32 *pressed = NULL;
static u32 *held = NULL;
static bool buttonLock = false;
static u8 buttonPressCooldown = 0;

static u8 stickThreshold = 23;
static u8 triggerThreshold = 255;
// since there are two options, this can be a bool. false = trigger selected
static bool stickThresholdSelected = true;
static u8 thresholdFrameCounter = 0;

static enum PLOT_BUTTON_MENU_STATE menuState = BUTTON_SETUP;
static enum PLOT_BUTTON_STATE state = BUTTON_INPUT;

// what button/input triggered the start of the recording
static enum PLOT_BUTTON_LIST triggeringInput = NO_BUTTON;
static enum PLOT_BUTTON_LIST triggeringInputDisplay = NO_BUTTON;

static WaveformDatapoint data[500];
static int dataIndex = 0;
static bool dataIsReady = false;
static u8 triggerLAnalog = 0, triggerRAnalog = 0;
static u64 totalCaptureTimeUs = 0;
static bool captureStart = false;
static bool captureButtonsReleased = false;
static bool autoCapture = false;
static u8 autoCaptureCounter = 0;

static sampling_callback cb;
static u64 prevSampleCallbackTick = 0;
static u64 sampleCallbackTick = 0;
static u64 pressedTimer = 0;
static u8 ellipseCounter = 0;
static bool pressLocked = false;

typedef struct ButtonPressedTime {
	u64 timeHeld;
	bool pressFinished;
} ButtonPressedTime;

static void plotButtonSamplingCallback() {
	// time from last call of this function calculation
	prevSampleCallbackTick = sampleCallbackTick;
	sampleCallbackTick = gettime();
	
	//static s8 x, y, cx, cy;
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
			data[dataIndex].ax = PAD_StickX(0);
			data[dataIndex].ay = PAD_StickY(0);
			data[dataIndex].cx = PAD_SubStickX(0);
			data[dataIndex].cy = PAD_SubStickY(0);
			data[dataIndex].triggers.triggerLAnalog = PAD_TriggerL(0);
			data[dataIndex].triggers.triggerRAnalog = PAD_TriggerR(0);
			data[dataIndex].triggers.triggerLDigital = (*held & PAD_TRIGGER_L);
			data[dataIndex].triggers.triggerRDigital = (*held & PAD_TRIGGER_R);
			data[dataIndex].buttonsHeld = *held;
			data[dataIndex].timeDiffUs = ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
			dataIndex++;
			
			totalCaptureTimeUs += ticks_to_microsecs(sampleCallbackTick - prevSampleCallbackTick);
			
			// not moving for 300 ms
			if (totalCaptureTimeUs >= 300000 || dataIndex == 500) {
				totalCaptureTimeUs = 0;
				dataIsReady = true;
				captureStart = false;
				captureButtonsReleased = false;
				state = BUTTON_DISPLAY;
			}

		// we haven't started recording yet, wait for stick/trigger to move outside user-defined range, or a button press
		} else {
			// determine what triggered the input
			// TODO: there has to be a better way to do this...
			if (*held & PAD_BUTTON_A) {
				triggeringInput = A;
			} else if (*held & PAD_BUTTON_B) {
				triggeringInput = B;
			} else if (*held & PAD_BUTTON_X) {
				triggeringInput = X;
			} else if (*held & PAD_BUTTON_Y) {
				triggeringInput = Y;
			} else if (*held & PAD_TRIGGER_L) {
				triggeringInput = L;
			} else if (PAD_TriggerL(0) >= triggerThreshold) {
				triggeringInput = La;
			} else if (*held & PAD_TRIGGER_R) {
				triggeringInput = R;
			} else if (PAD_TriggerR(0) >= triggerThreshold) {
				triggeringInput = Ra;
			} else if (*held & PAD_TRIGGER_Z) {
				triggeringInput = Z;
			} else if (abs(PAD_StickX(0)) >= stickThreshold) {
				triggeringInput = AX;
			} else if (abs(PAD_StickY(0)) >= stickThreshold) {
				triggeringInput = AY;
			} else if (abs(PAD_SubStickX(0)) >= stickThreshold) {
				triggeringInput = CX;
			} else if (abs(PAD_SubStickY(0)) >= stickThreshold) {
				triggeringInput = CY;
			} else {
				triggeringInput = NO_BUTTON;
			}
			
			// write our data if an input was detected
			if (triggeringInput != NO_BUTTON) {
				triggeringInputDisplay = triggeringInput;
				captureStart = true;
				data[0].ax = PAD_StickX(0);
				data[0].ay = PAD_StickY(0);
				data[0].cx = PAD_SubStickX(0);
				data[0].cy = PAD_SubStickY(0);
				data[0].triggers.triggerLAnalog = triggerLAnalog;
				data[0].triggers.triggerRAnalog = triggerRAnalog;
				data[0].triggers.triggerLDigital = (*held & PAD_TRIGGER_L);
				data[0].triggers.triggerRDigital = (*held & PAD_TRIGGER_R);
				data[0].buttonsHeld = *held;
				data[0].timeDiffUs = 0;
				dataIndex = 1;
				dataIsReady = false;
			}
		}
	}
}

static void setup(u32 *p, u32 *h) {
	setSamplingRateHigh();
	pressed = p;
	held = h;
	cb = PAD_SetSamplingCallback(plotButtonSamplingCallback);
	menuState = BUTTON_POST_SETUP;
	state = BUTTON_DISPLAY;
}

static void displayInstructions(void *currXfb) {
	setCursorPos(2, 0);
	printStr("Press A to prepare a 300ms recording.\n"
			 "Recording will start when a digital button is pressed,\n"
			 "or when an analog value moves beyond its threshold.\n\n"
			 "Vertical gray lines show ~1 frame (~16.6ms) boundaries.\n"
			 "Frame times are shown on the right. The highlighted number\n"
			 "shows what button started the recording, and how long its\n"
			 "initial input was held. The other numbers show how much time\n"
			 "passed between the recording starting, and when the first\n"
			 "input was detected for a given button.\n\n"
			 "Use the D-Pad to adjust the thresholds.\n"
			 "Hold R to change thresholds faster.\n"
			 "Hold Start to toggle Auto-Trigger. Enabling this removes\n"
			 "the need to press A, but disables the instruction menu and\n"
			 "the R modifier.\n", currXfb);
	
	setCursorPos(21, 0);
	printStr("Press Z to close instructions.", currXfb);
	
	if (!buttonLock) {
		if (*pressed & PAD_TRIGGER_Z) {
			menuState = BUTTON_POST_SETUP;
			buttonLock = true;
			buttonPressCooldown = 5;
		}
	}
}

void menu_plotButton(void *currXfb, u32 *p, u32 *h) {
	switch (menuState) {
		case BUTTON_SETUP:
			setup(p, h);
			break;
		case BUTTON_INSTRUCTIONS:
			displayInstructions(currXfb);
			break;
		case BUTTON_POST_SETUP:
			switch (state) {
				case BUTTON_DISPLAY:
					if (!autoCapture) {
						printStr("Press A to start read, press Z for instructions", currXfb);
					}
					
					setCursorPos(4,7);
					if (stickThresholdSelected) {
						printStrColor("Stick Threshold:", currXfb, COLOR_WHITE, COLOR_BLACK);
						sprintf(strBuffer, " %3u", stickThreshold);
						printStr(strBuffer, currXfb);
						setCursorPos(4, 32);
						printStr("Trigger Threshold:", currXfb);
						sprintf(strBuffer, " %3u", triggerThreshold);
						printStr(strBuffer, currXfb);
					} else {
						printStr("Stick Threshold:", currXfb);
						sprintf(strBuffer, " %3u", stickThreshold);
						printStr(strBuffer, currXfb);
						setCursorPos(4, 32);
						printStrColor("Trigger Threshold:", currXfb, COLOR_WHITE, COLOR_BLACK);
						sprintf(strBuffer, " %3u", triggerThreshold);
						printStr(strBuffer, currXfb);
					}
					
					if (dataIsReady) {
						DrawBox(SCREEN_TIMEPLOT_START - 40, SCREEN_TIMEPLOT_Y_TOP - 1, 600, SCREEN_TIMEPLOT_Y_BOTTOM + 1, COLOR_WHITE, currXfb);
						
						for (enum PLOT_BUTTON_LIST button = A; button < NO_BUTTON; button++) {
							setCursorPos(7 + button, 4);
							if (button == triggeringInputDisplay) {
								printStrColor(BUTTON_STR[button], currXfb, COLOR_WHITE, COLOR_BLACK);
							} else {
								printStr(BUTTON_STR[button], currXfb);
							}
						}
						//printStr("   A\n   B\n   X\n   Y\n   L\n   La\n   R\n   Ra\n   Z\n   AX\n   AY\n   CX\n   CY", currXfb);
						
						u64 frameIntervalTime = 16666;
						u64 totalTimeUs = 0;
						ButtonPressedTime buttons[13] = {{ 0, false }};
						// draw data
						for (int i = 0; i < dataIndex; i++) {
							// frame intervals first
							frameIntervalTime += data[i].timeDiffUs;
							totalTimeUs += data[i].timeDiffUs;
							if (frameIntervalTime >= 16666) {
								DrawVLine(SCREEN_TIMEPLOT_START + i, SCREEN_TIMEPLOT_Y_TOP, SCREEN_TIMEPLOT_Y_BOTTOM,
										  COLOR_GRAY, currXfb);
								frameIntervalTime = 0;
							}
							
							// button press lines
							// each is separated by a multiple of 17, since that's the spacing between drawn chars
							// we also get frame count here
							
							// A
							if (data[i].buttonsHeld & PAD_BUTTON_A) {
								DrawVLine(SCREEN_TIMEPLOT_START + i, SCREEN_TIMEPLOT_CHAR_TOP,
										  SCREEN_TIMEPLOT_CHAR_TOP + SCREEN_CHAR_SIZE,
										  COLOR_WHITE, currXfb);
								if (!buttons[0].pressFinished) {
									if (A == triggeringInputDisplay) {
										buttons[0].timeHeld += data[i].timeDiffUs;
									} else {
										buttons[0].timeHeld = totalTimeUs;
										buttons[0].pressFinished = true;
									}
								}
							} else if (buttons[0].timeHeld != 0) {
								buttons[0].pressFinished = true;
							}
							
							// B
							if (data[i].buttonsHeld & PAD_BUTTON_B) {
								DrawVLine(SCREEN_TIMEPLOT_START + i, SCREEN_TIMEPLOT_CHAR_TOP + 17,
								          SCREEN_TIMEPLOT_CHAR_TOP + 17 + SCREEN_CHAR_SIZE,
								          COLOR_WHITE, currXfb);
								if (!buttons[1].pressFinished) {
									if (B == triggeringInputDisplay) {
										buttons[1].timeHeld += data[i].timeDiffUs;
									} else {
										buttons[1].timeHeld = totalTimeUs;
										buttons[1].pressFinished = true;
									}
								}
							} else if (buttons[1].timeHeld != 0) {
								buttons[1].pressFinished = true;
							}
							
							// X
							if (data[i].buttonsHeld & PAD_BUTTON_X) {
								DrawVLine(SCREEN_TIMEPLOT_START + i, SCREEN_TIMEPLOT_CHAR_TOP + 34,
								          SCREEN_TIMEPLOT_CHAR_TOP + 34 + SCREEN_CHAR_SIZE,
								          COLOR_WHITE, currXfb);
								if (!buttons[2].pressFinished) {
									if (X == triggeringInputDisplay) {
										buttons[2].timeHeld += data[i].timeDiffUs;
									} else {
										buttons[2].timeHeld = totalTimeUs;
										buttons[2].pressFinished = true;
									}
								}
							} else if (buttons[2].timeHeld != 0) {
								buttons[2].pressFinished = true;
							}
							
							// Y
							if (data[i].buttonsHeld & PAD_BUTTON_Y) {
								DrawVLine(SCREEN_TIMEPLOT_START + i, SCREEN_TIMEPLOT_CHAR_TOP + 51,
								          SCREEN_TIMEPLOT_CHAR_TOP + 51 + SCREEN_CHAR_SIZE,
								          COLOR_WHITE, currXfb);
								if (!buttons[3].pressFinished) {
									if (Y == triggeringInputDisplay) {
										buttons[3].timeHeld += data[i].timeDiffUs;
									} else {
										buttons[3].timeHeld = totalTimeUs;
										buttons[3].pressFinished = true;
									}
								}
							} else if (buttons[3].timeHeld != 0) {
								buttons[3].pressFinished = true;
							}
							
							// Digital L
							if (data[i].buttonsHeld & PAD_TRIGGER_L) {
								DrawVLine(SCREEN_TIMEPLOT_START + i, SCREEN_TIMEPLOT_CHAR_TOP + 68,
								          SCREEN_TIMEPLOT_CHAR_TOP + 68 + SCREEN_CHAR_SIZE,
								          COLOR_WHITE, currXfb);
								if (!buttons[4].pressFinished) {
									if (L == triggeringInputDisplay) {
										buttons[4].timeHeld += data[i].timeDiffUs;
									} else {
										buttons[4].timeHeld = totalTimeUs;
										buttons[4].pressFinished = true;
									}
								}
							} else if (buttons[4].timeHeld != 0) {
								buttons[4].pressFinished = true;
							}
							
							// Analog L
							if (data[i].triggers.triggerLAnalog >= triggerThreshold) {
								DrawVLine(SCREEN_TIMEPLOT_START + i, SCREEN_TIMEPLOT_CHAR_TOP + 85,
								          SCREEN_TIMEPLOT_CHAR_TOP + 85 + SCREEN_CHAR_SIZE,
								          COLOR_WHITE, currXfb);
								if (!buttons[5].pressFinished) {
									if (La == triggeringInputDisplay) {
										buttons[5].timeHeld += data[i].timeDiffUs;
									} else {
										buttons[5].timeHeld = totalTimeUs;
										buttons[5].pressFinished = true;
									}
								}
							} else if (buttons[5].timeHeld != 0) {
								buttons[5].pressFinished = true;
							}
							
							// Digital R
							if (data[i].buttonsHeld & PAD_TRIGGER_R) {
								DrawVLine(SCREEN_TIMEPLOT_START + i, SCREEN_TIMEPLOT_CHAR_TOP + 102,
								          SCREEN_TIMEPLOT_CHAR_TOP + 102 + SCREEN_CHAR_SIZE,
								          COLOR_WHITE, currXfb);
								if (!buttons[6].pressFinished) {
									if (R == triggeringInputDisplay) {
										buttons[6].timeHeld += data[i].timeDiffUs;
									} else {
										buttons[6].timeHeld = totalTimeUs;
										buttons[6].pressFinished = true;
									}
								}
							} else if (buttons[6].timeHeld != 0) {
								buttons[6].pressFinished = true;
							}
							
							// Analog R
							if (data[i].triggers.triggerRAnalog >= triggerThreshold) {
								DrawVLine(SCREEN_TIMEPLOT_START + i, SCREEN_TIMEPLOT_CHAR_TOP + 119,
								          SCREEN_TIMEPLOT_CHAR_TOP + 119 + SCREEN_CHAR_SIZE,
								          COLOR_WHITE, currXfb);
								if (!buttons[7].pressFinished) {
									if (Ra == triggeringInputDisplay) {
										buttons[7].timeHeld += data[i].timeDiffUs;
									} else {
										buttons[7].timeHeld = totalTimeUs;
										buttons[7].pressFinished = true;
									}
								}
							} else if (buttons[7].timeHeld != 0) {
								buttons[7].pressFinished = true;
							}
							
							// Z
							if (data[i].buttonsHeld & PAD_TRIGGER_Z) {
								DrawVLine(SCREEN_TIMEPLOT_START + i, SCREEN_TIMEPLOT_CHAR_TOP + 136,
								          SCREEN_TIMEPLOT_CHAR_TOP + 136 + SCREEN_CHAR_SIZE,
								          COLOR_WHITE, currXfb);
								if (!buttons[8].pressFinished) {
									if (Z == triggeringInputDisplay) {
										buttons[8].timeHeld += data[i].timeDiffUs;
									} else {
										buttons[8].timeHeld = totalTimeUs;
										buttons[8].pressFinished = true;
									}
								}
							} else if (buttons[8].timeHeld != 0) {
								buttons[8].pressFinished = true;
							}
							
							// Stick X Axis
							if (data[i].ax >= stickThreshold || data[i].ax <= -stickThreshold) {
								DrawVLine(SCREEN_TIMEPLOT_START + i, SCREEN_TIMEPLOT_CHAR_TOP + 153,
								          SCREEN_TIMEPLOT_CHAR_TOP + 153 + SCREEN_CHAR_SIZE,
								          COLOR_WHITE, currXfb);
								if (!buttons[9].pressFinished) {
									if (AX == triggeringInputDisplay) {
										buttons[9].timeHeld += data[i].timeDiffUs;
									} else {
										buttons[9].timeHeld = totalTimeUs;
										buttons[9].pressFinished = true;
									}
								}
							} else if (buttons[9].timeHeld != 0) {
								buttons[9].pressFinished = true;
							}
							
							// Stick Y Axis
							if (data[i].ay >= stickThreshold || data[i].ay <= -stickThreshold) {
								DrawVLine(SCREEN_TIMEPLOT_START + i, SCREEN_TIMEPLOT_CHAR_TOP + 170,
								          SCREEN_TIMEPLOT_CHAR_TOP + 170 + SCREEN_CHAR_SIZE,
								          COLOR_WHITE, currXfb);
								if (!buttons[10].pressFinished) {
									if (AY == triggeringInputDisplay) {
										buttons[10].timeHeld += data[i].timeDiffUs;
									} else {
										buttons[10].timeHeld = totalTimeUs;
										buttons[10].pressFinished = true;
									}
								}
							} else if (buttons[10].timeHeld != 0) {
								buttons[10].pressFinished = true;
							}
							
							// C-Stick X Axis
							if (data[i].cx >= stickThreshold || data[i].cx <= -stickThreshold) {
								DrawVLine(SCREEN_TIMEPLOT_START + i, SCREEN_TIMEPLOT_CHAR_TOP + 187,
								          SCREEN_TIMEPLOT_CHAR_TOP + 187 + SCREEN_CHAR_SIZE,
								          COLOR_WHITE, currXfb);
								if (!buttons[11].pressFinished) {
									if (CX == triggeringInputDisplay) {
										buttons[11].timeHeld += data[i].timeDiffUs;
									} else {
										buttons[11].timeHeld = totalTimeUs;
										buttons[11].pressFinished = true;
									}
								}
							} else if (buttons[11].timeHeld != 0) {
								buttons[11].pressFinished = true;
							}
							
							// C-Stick Y Axis
							if (data[i].cy >= stickThreshold || data[i].cy <= -stickThreshold) {
								DrawVLine(SCREEN_TIMEPLOT_START + i, SCREEN_TIMEPLOT_CHAR_TOP + 204,
								          SCREEN_TIMEPLOT_CHAR_TOP + 204 + SCREEN_CHAR_SIZE,
								          COLOR_WHITE, currXfb);
								if (!buttons[12].pressFinished) {
									if (CY == triggeringInputDisplay) {
										buttons[12].timeHeld += data[i].timeDiffUs;
									} else {
										buttons[12].timeHeld = totalTimeUs;
										buttons[12].pressFinished = true;
									}
								}
							} else if (buttons[12].timeHeld != 0) {
								buttons[12].pressFinished = true;
							}
						}
						
						// draw frame durations
						
						for (enum PLOT_BUTTON_LIST button = A; button < NO_BUTTON; button++) {
							if (buttons[button].timeHeld != 0) {
								setCursorPos(7 + button, 52);
								sprintf(strBuffer, "%2.2ff", buttons[button].timeHeld / (FRAME_TIME_MS * 1000));
								// indicate the initial input with black on white text
								if (button == triggeringInputDisplay) {
									printStrColor(strBuffer, currXfb, COLOR_WHITE, COLOR_BLACK);
								} else {
									printStr(strBuffer, currXfb);
								}
							}
						}
					}
					
					// single presses
					if (!buttonLock) {
						// disable certain buttons if auto-capture is enabled
						if (!autoCapture) {
							if (*pressed & PAD_BUTTON_A) {
								state = BUTTON_INPUT;
								buttonLock = true;
								buttonPressCooldown = 5;
							} else if (*pressed & PAD_TRIGGER_Z) {
								menuState = BUTTON_INSTRUCTIONS;
								buttonLock = true;
								buttonPressCooldown = 5;
							}
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
							printStr("Disabling ", currXfb);
						} else {
							printStr("Enabling ", currXfb);
						}
						printStr("Auto-Trigger", currXfb);
						printEllipse(autoCaptureCounter, 40, currXfb);
						autoCaptureCounter++;
						if (autoCaptureCounter == 120) {
							autoCapture = !autoCapture;
							captureButtonsReleased = false;
							buttonLock = true;
							buttonPressCooldown = 5;
							autoCaptureCounter = 0;
						}
					} else {
						printStr("Hold start to toggle Auto-Trigger.", currXfb);
						autoCaptureCounter = 0;
					}
					
					if (!autoCapture) {
						break;
					}
				case BUTTON_INPUT:
					// nothing happens here other than showing the message about waiting for an input
					// the sampling callback function will change the plotState enum when an input is done
					setCursorPos(2,0);
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
					break;
				default:
					printStr("button plot default case?", currXfb);
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
	dataIndex = 0;
	dataIsReady = false;
	autoCaptureCounter = 0;
	autoCapture = false;
	pressed = NULL;
	held = NULL;
	menuState = BUTTON_SETUP;
}

bool menu_plotButtonHasCaptureStarted() {
	return captureStart;
}
