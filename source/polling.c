//
// Created on 2025/02/21.
//

#include "polling.h"

#include <ogc/video.h>
#include <ogc/video_types.h>
#include <ogc/si.h>

#ifdef DEBUGLOG
#include "logging.h"
#endif

static bool unsupportedMode = false;
static bool firstRun = true;
static bool readHigh = false;
static int xLineCountNormal = 0;
static int xLineCountHigh = 0;
static int pollsPerFrameNormal = 2;
static int pollsPerFrameHigh = 24;

// values 9 and 18 are average 664us with low of 572
// set xLineCount based on video mode
void __setStaticXYValues() {
	switch(VIDEO_GetScanMode()) {
		case VI_INTERLACE:
			xLineCountNormal = 131;
			xLineCountHigh = 11;
#ifdef DEBUGLOG
			debugLog("Video scan mode is interlaced");
#endif
			break;
		case VI_PROGRESSIVE:
			xLineCountNormal = 263;
			xLineCountHigh = 22;
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
	
	switch (port) {
		case CONT_PORT_1:
			ret = origin[0];
			break;
		case CONT_PORT_2:
			ret = origin[1];
			break;
		case CONT_PORT_3:
			ret = origin[2];
			break;
		case CONT_PORT_4:
			ret = origin[3];
			break;
	}
	
	return ret;
}

static uint32_t padsConnected;

void attemptReadOrigin() {
	padsConnected = PAD_ScanPads();
	PAD_GetOrigin(origin);
}

bool isControllerConnected(enum CONT_PORTS_BITFLAGS port) {
	return ((padsConnected & port) == 1);
}
