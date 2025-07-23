//
// Created on 7/22/25.
//

#include "plotbutton.h"

#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include "../polling.h"
#include "../print.h"
#include "../draw.h"

static const float FRAME_TIME_MS = (1000/60.0);

const static int SCREEN_TIMEPLOT_START = 40;
const static int SCREEN_TIMEPLOT_Y_TOP = 150;
const static int SCREEN_TIMEPLOT_Y_BOTTOM = 380;
const static int SCREEN_TIMEPLOT_CHAR_TOP = 159;
const static int SCREEN_CHAR_SIZE = 14;

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

static WaveformDatapoint data[500];
static int dataIndex = 0;
static bool dataIsReady = false;
static int stickX = 0, stickY = 0;
static int cStickX = 0, cStickY = 0;
static u8 triggerLAnalog = 0, triggerRAnalog = 0;
static u64 totalCaptureTimeUs = 0;
static bool captureStart = false;
static bool captureAReleased = false;

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
	
	if (state == BUTTON_INPUT) {
		// A button might still be held from starting read, wait for it to be released
		if (!captureAReleased) {
			if (*held == 0) {
				captureAReleased = true;
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
				captureAReleased = false;
				state = BUTTON_DISPLAY;
			}

		// we haven't started recording yet, wait for stick/trigger to move outside user-defined range, or a button press
		} else {
			stickX = PAD_StickX(0);
			stickY = PAD_StickY(0);
			cStickX = PAD_SubStickX(0);
			cStickY = PAD_SubStickY(0);
			triggerLAnalog = PAD_TriggerL(0);
			triggerRAnalog = PAD_TriggerR(0);
			if ( stickX >= stickThreshold || stickX <= -stickThreshold ||
					stickY >= stickThreshold || stickY <= -stickThreshold ||
					cStickX >= stickThreshold || cStickX <= -stickThreshold ||
					cStickY >= stickThreshold || cStickY <= -stickThreshold ||
					triggerLAnalog >= triggerThreshold || triggerRAnalog >= triggerThreshold ||
					*held != 0 ) {
				captureStart = true;
				data[0].ax = stickX;
				data[0].ay = stickY;
				data[0].cx = cStickX;
				data[0].cy = cStickY;
				data[0].triggers.triggerLAnalog = triggerLAnalog;
				data[0].triggers.triggerRAnalog = triggerRAnalog;
				data[0].triggers.triggerLDigital = (*held & PAD_TRIGGER_L);
				data[0].triggers.triggerRDigital = (*held & PAD_TRIGGER_R);
				data[0].buttonsHeld = *held;
				data[0].timeDiffUs = 0;
				dataIndex = 1;
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
			 "Vertical gray lines show 1 frame (16.6ms) boundaries.\n"
			 "Total time in frames for the first detected input is\n"
			 "shown on the right.\n\n"
			 "Use the D-Pad to adjust the thresholds.\n"
			 "Hold R to change thresholds faster.\n", currXfb);
	
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
					printStr("Press A to start read, press Z for instructions", currXfb);
					
					setCursorPos(4,0);
					if (stickThresholdSelected) {
						sprintf(strBuffer, "> Stick Threshold: %3u |   Trigger Threshold: %3u", stickThreshold, triggerThreshold);
						printStr(strBuffer, currXfb);
						//printStr("Stick Threshold: 023 | Trigger Threshold: 255", currXfb);
					} else {
						sprintf(strBuffer, "  Stick Threshold: %3u | > Trigger Threshold: %3u", stickThreshold, triggerThreshold);
						printStr(strBuffer, currXfb);
					}
					
					if (dataIsReady) {
						setCursorPos(7, 0);
						printStr("A\nB\nX\nY\nL\nLa\nR\nRa\nZ\nAX\nAY\nCX\nCY", currXfb);
						
						u64 frameIntervalTime = 16666;
						ButtonPressedTime buttons[13] = {{ 0, false }};
						// draw data
						for (int i = 0; i < dataIndex; i++) {
							// frame intervals first
							frameIntervalTime += data[i].timeDiffUs;
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
									buttons[0].timeHeld += data[i].timeDiffUs;
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
									buttons[1].timeHeld += data[i].timeDiffUs;
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
									buttons[2].timeHeld += data[i].timeDiffUs;
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
									buttons[3].timeHeld += data[i].timeDiffUs;
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
									buttons[4].timeHeld += data[i].timeDiffUs;
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
									buttons[5].timeHeld += data[i].timeDiffUs;
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
									buttons[6].timeHeld += data[i].timeDiffUs;
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
									buttons[7].timeHeld += data[i].timeDiffUs;
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
									buttons[8].timeHeld += data[i].timeDiffUs;
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
									buttons[9].timeHeld += data[i].timeDiffUs;
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
									buttons[10].timeHeld += data[i].timeDiffUs;
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
									buttons[11].timeHeld += data[i].timeDiffUs;
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
									buttons[12].timeHeld += data[i].timeDiffUs;
								}
							} else if (buttons[12].timeHeld != 0) {
								buttons[12].pressFinished = true;
							}
						}
						
						// draw frame durations
						for (int i = 0; i < 13; i++) {
							if (buttons[i].timeHeld != 0) {
								setCursorPos(7 + i, 48);
								sprintf(strBuffer, "%2.2ff", buttons[i].timeHeld / (FRAME_TIME_MS * 1000));
								printStr(strBuffer, currXfb);
							}
						}
					}
					
					// single presses
					if (!buttonLock) {
						if (*pressed & PAD_BUTTON_A) {
							state = BUTTON_INPUT;
							buttonLock = true;
							buttonPressCooldown = 5;
						} else if (*pressed & PAD_TRIGGER_Z) {
							menuState = BUTTON_INSTRUCTIONS;
							buttonLock = true;
							buttonPressCooldown = 5;
						} else if (*pressed & PAD_BUTTON_RIGHT) {
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
					if (*held & PAD_TRIGGER_R) {
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
					
					
					break;
				case BUTTON_INPUT:
					// nothing happens here other than showing the message about waiting for an input
					// the sampling callback function will change the plotState enum when an input is done
					setCursorPos(2,0);
					printStr("Waiting for input.", currXfb);
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
	captureAReleased = false;
	dataIndex = 0;
	dataIsReady = false;
	pressed = NULL;
	held = NULL;
	menuState = BUTTON_SETUP;
}
