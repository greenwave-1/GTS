//
// Created on 2025/02/21.
//

#include "util/polling.h"

#include <ogc/video.h>
#include <ogc/video_types.h>
#include <ogc/si.h>

#ifdef DEBUGLOG
#include "util/logging.h"
#endif

// rough time in milliseconds where a vsync would occur
// calculated by multiplying 16.666 by x amount, and truncating the decimal
// this is specifically for menus that have 1 column = one millisecond
const int FRAME_INTERVAL_MS[] = {16, 33, 49, 66, 83, 100,
                                        116, 133, 149, 166, 183, 199,
                                        216, 233, 249, 266, 283, 299,
                                        316, 333, 349, 366, 383, 399,
										416, 433, 449, 466, 483, 499, 516};

static bool unsupportedMode = false;
static bool firstRun = true;
static bool readHigh = false;

// polling for gamecube controllers is based on two values (referred to as XY in si.h)
// X -> how many lines should be rastered before a hardware interrupt occurs to poll the controller
// Y -> how many total times should the hardware poll the controller (per frame)
// for interlaced, since the video signal is 15khz, there are ~262.5 lines per frame
// for progressive, since the video signal is 31khz, there are ~525 lines per frame
// this is why there are different values depending on the video mode
// basically, the value of X for progressive should be roughly double the value for interlaced
// I think this was the cause of SmashScope's 480p polling bug
static int xLineCountNormal = 0;
static int xLineCountHigh = 0;
static int pollsPerFrameNormal = 2;
static int pollsPerFrameHigh = 35;

// values 9 and 18 are average 664us with low of 572
// set xLineCount based on video mode
void __setStaticXYValues() {
	switch(VIDEO_GetScanMode()) {
		case VI_INTERLACE:
			xLineCountNormal = 131;
			xLineCountHigh = 8;
#ifdef DEBUGLOG
			debugLog("Video scan mode is interlaced");
#endif
			break;
		case VI_PROGRESSIVE:
			xLineCountNormal = 263;
			xLineCountHigh = 15;
#ifdef DEBUGLOG
			debugLog("Video scan mode is progressive");
#endif
			break;
		case VI_NON_INTERLACE:
		default:
			//xLineCountNormal = 240;
			//xLineCountHigh = 240;
			unsupportedMode = true;
#ifdef DEBUGLOG
			debugLog("Video scan mode is unsupported");
#endif
	}
	firstRun = false;
}

// external polling functions pass us any button presses during that frame
static uint16_t buttonsDownDuringFrame = 0;

// we need to store the previous state so that we can debounce
static uint16_t previousFramePressed = 0;

// indirectly set the XY values
// values don't actually get set until setSamplingRate() is called
// from a post-retrace callback in main.c -> retraceCallback()
void setSamplingRateHigh() {
	if (firstRun) {
		__setStaticXYValues();
	}
	readHigh = true;
	// consider all buttons pressed when first switching
	// this prevents a button from being 'pressed' for two consecutive frames
	previousFramePressed = 0xffff;
}

void setSamplingRateNormal() {
	if (firstRun) {
		__setStaticXYValues();
	}
	readHigh = false;
}

// actually set the XY values
// this is called per-frame after a retrace occurs, since VIDEO_Flush() clears our custom XY values
// (thanks again extrems)
void setSamplingRate() {
	if (readHigh) {
		SI_SetXY(xLineCountHigh, pollsPerFrameHigh);
	} else {
		SI_SetXY(xLineCountNormal, pollsPerFrameNormal);
	}
}

bool isUnsupportedMode() {
	return unsupportedMode;
}

// button fields
// defined here so that we don't have to worry about passing pointers between menu.c and other submenus
static uint16_t buttonsDown;
static uint16_t buttonsHeld;

uint16_t* getButtonsDownPtr() {
	return &buttonsDown;
}

uint16_t* getButtonsHeldPtr() {
	return &buttonsHeld;
}

static PADStatus origin[PAD_CHANMAX];

PADStatus getOriginStatus(enum CONT_PORTS_BITFLAGS port) {
	PADStatus ret;
	// simple bounds check
	if (port >= CONT_PORT_1 && port <= CONT_PORT_4) {
		ret = origin[port];
	}
	
	return ret;
}

static uint32_t padsConnected = 0;

// read controller
// updates buttonsPressed, buttonsHeld, which controllers are connected, and origin information
// the bool determines if we should attempt to update buttonsPressed
// the only circumstance where false should be passed is when calling this function multiple times per frame
// tl;dr each call of ScanPads() will update what buttons are considered pressed/down,
// in some menus, we call ScanPads() multiple times per-frame, meaning ButtonsDown() is basically useless.
// in this case, we handle setting 'pressed' buttons manually
void readController(bool updatePressed) {
	// update controller state and get which controllers are connected
	padsConnected = PAD_ScanPads();
	// get origin info
	PAD_GetOrigin(origin);
	
	// always update held buttons
	buttonsHeld = PAD_ButtonsHeld(0);
	
	// handle 'pressed' buttons
	if (updatePressed) {
		if (!readHigh) {
			// update normally
			buttonsDown = PAD_ButtonsDown(0);
		}
		// function is being called multiple times per frame, so we need to do this logic ourselves
		else {
			// libogc2/libogc/pad.c -> PAD_ScanPads()
			// we specifically want 'presses' that occurred on this frame but not on the last frame,
			// ~previousFramePressed gives us buttons that weren't 'pressed' last frame
			// buttonsDownDuringFrame is any button that was pressed at _any_ point between calls of readController()
			buttonsDown = buttonsDownDuringFrame & (~previousFramePressed);
			previousFramePressed = buttonsDownDuringFrame;
		}
		// clear it for next run
		buttonsDownDuringFrame = 0;
	} else {
		// store held buttons for use in determining 'pressed'
		// this does nothing if readHigh is false
		buttonsDownDuringFrame |= buttonsHeld;
	}
	
}

bool isControllerConnected(enum CONT_PORTS_BITFLAGS port) {
	// PAD_ScanPads() returns what controllers are connected
	// the last four bits indicate if a controller is considered 'connected',
	// so we shift 0b0001 depending on the port number (0, 1, 2, 3)
	return ( padsConnected & (1 << port) );
}
