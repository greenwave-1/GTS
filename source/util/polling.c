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

// indirectly set the XY values
// values don't actually get set until setSamplingRate() is called
// from a post-retrace callback in main.c -> retraceCallback()
void setSamplingRateHigh() {
	if (firstRun) {
		__setStaticXYValues();
	}
	readHigh = true;
	//SI_SetXY(xLineCountHigh, pollsPerFrameHigh);
}

void setSamplingRateNormal() {
	if (firstRun) {
		__setStaticXYValues();
	}
	readHigh = false;
	//SI_SetXY(xLineCountNormal, pollsPerFrameNormal);
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
// TODO: should these values be updated from here via another function?
// TODO: a function call in front of it would let us not have to check the current submenu in menu.c
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

static uint32_t padsConnected;

void attemptReadOrigin() {
	padsConnected = PAD_ScanPads();
	// TODO: we should probably only check this on padsConnected being changed from the previous run
	PAD_GetOrigin(origin);
}

bool isControllerConnected(enum CONT_PORTS_BITFLAGS port) {
	// PAD_ScanPads() returns what controllers are connected
	// the last four bits indicate if a controller is considered 'connected',
	// so we shift 0b0001 depending on the port number (0, 1, 2, 3)
	return ( padsConnected & (1 << port) );
}
