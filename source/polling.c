//
// Created on 2/21/25.
//

#include "polling.h"
#include <ogc/video.h>
#include <ogc/video_types.h>
#include <ogc/si.h>

#ifdef DEBUGLOG
#include "gecko.h"
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
			xLineCountNormal = 120;
			xLineCountHigh = 11;
#ifdef DEBUGLOG
			sendMessage("Video scan mode is interlaced");
#endif
			break;
		case VI_PROGRESSIVE:
			xLineCountNormal = 240;
			xLineCountHigh = 22;
#ifdef DEBUGLOG
			sendMessage("Video scan mode is progressive");
#endif
			break;
		case VI_NON_INTERLACE:
		default:
			xLineCountNormal = 240;
			xLineCountHigh = 240;
			unsupportedMode = true;
#ifdef DEBUGLOG
			sendMessage("Video scan mode is unsupported");
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
