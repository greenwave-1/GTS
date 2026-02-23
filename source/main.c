#include <stdint.h>
#include <stdlib.h>

#include <gccore.h>

#include "menu.h"
#include "waveform.h"
#include "util/gx.h"
#include "util/polling.h"
#include "util/print.h"
#include "util/args.h"
#include "util/file.h"

#ifdef DEBUGLOG
#include "util/logging.h"
#endif

#ifdef DEBUGGDB
#include <debug.h>
#endif

#ifdef BENCH
#include <string.h>
#include <ogc/timesupp.h>
#endif

// basic stuff from the template
static void *xfb[2] = { NULL, NULL };
static int xfbSwitch = 0;
static GXRModeObj *rmode = NULL;

static VIRetraceCallback cb;

static char* resetMessage = "Exiting...";

void retraceCallback(uint32_t retraceCnt) {
	setSamplingRate();
	
	// reading fifo and gp status stuff
	// debugging
	//debugLog("Wrap: %u", getFifoVal());
	//uint8_t overhi, underlow, readidle, cmdidle, brkpt;
	//GX_GetGPStatus(&overhi, &underlow, &readidle, &cmdidle, &brkpt);
	//debugLog("GPStatus: %u %u %u %u %u\n", overhi, underlow, readidle, cmdidle, brkpt);
	
	// can be used to make the reset button be a breakpoint? idk
	//#ifdef DEBUGGDB
	//if (SYS_ResetButtonDown()) {
		//_break();
	//}
	//#endif
}

#if defined(HW_RVL)
// this is stupid, but makes the #ifdef in logic look a bit nicer
static char* powerButtonMessage = "Shutting down...";

static bool powerButtonPressed = false;
void powerButtonCallback() {
	powerButtonPressed = true;
}
#endif

void cleanupBeforeExit() {
	// avoid distorted graphics on GC (I have no idea if this actually does what I think it does...)
	// https://github.com/emukidid/swiss-gc: cube/swiss/source/video.c -> unsetVideo()
	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitForFlush();
	
	// free framebuffers
	free(xfb[0]);
	free(xfb[1]);
}

int main(int argc, char **argv) {
	// register power button handler if we're on wii
	#if defined(HW_RVL)
	SYS_SetPowerCallback(powerButtonCallback);
	#endif
	
	#ifdef DEBUGGDB
	DEBUG_Init(GDBSTUB_DEVICE_USB, 1);
	#endif
	
	// do basic initialization
	// see the devkitpro wii template if you want an explanation of the following
	// the only thing we're doing differently is a double buffer
	VIDEO_Init();
	PAD_Init();
	PAD_ScanPads();

	rmode = VIDEO_GetPreferredMode(NULL);
	
	// https://github.com/extremscorner/GRRLIB -> GRRLib_core.c
	// correct aspect ratio
	rmode->viWidth = 704;
	rmode->viXOrigin = 8; // (720 - 704) / 2
	
	VIDEO_Configure(rmode);
	
	xfb[0] = SYS_AllocateFramebuffer(rmode);
	xfb[1] = SYS_AllocateFramebuffer(rmode);
	
	VIDEO_ClearFrameBuffer(rmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(rmode, xfb[1], COLOR_BLACK);

	VIDEO_SetNextFramebuffer(xfb[xfbSwitch]);

	VIDEO_SetBlack(false);

	VIDEO_Flush();

	VIDEO_WaitForFlush();
	
	setupGX(rmode);

	bool normalExit = false;
	
	#ifdef DEBUGGDB
	_break();
	#endif
	
	// there is a makefile target that will enable this
	#ifdef DEBUGLOG
	
	// options are defined in logging.h
	setupLogging(USBGECKO_B);
	//setupLogging(NETWORKSOCK);
	
	// if we're using a socket for logging, we need to prepare it
	// TODO: should this just be in logging.c outright? if so it needs to have return values for different outcomes
	if (getLoggingType() == NETWORKSOCK) {
		int netSetupEllipseCounter = 0;
		while (true) {
			// leave once connection is made
			if (isConnectionMade()) {
				break;
			}
			
			// exit completely if reset is pressed
			if (SYS_ResetButtonDown()) {
				cleanupBeforeExit();
				return 0;
			}
			
			resetCursor();
			startDraw();
			
			switch (getNetworkSetupState()) {
				case NETLOG_INIT:
					printStr("Setting up network.");
					printEllipse(netSetupEllipseCounter, 20);
					break;
				case NETLOG_CONF_SUCCESS:
					printStr("Waiting for connection.");
					printEllipse(netSetupEllipseCounter, 20);
					printStr("\n%s:43256", getConfiguredIP());
					break;
				case NETLOG_CONF_FAIL:
					printStr("Network config failed! Press reset to exit.");
					break;
				default:
					printStr("Default case in network setup?");
					break;
			}
			netSetupEllipseCounter++;
			if (netSetupEllipseCounter == 60) {
				netSetupEllipseCounter = 0;
			}
			
			finishDraw(xfb[xfbSwitch]);
			VIDEO_WaitVSync();
		}
	}
	debugLog("Debug output enabled");
	
	#ifdef HW_RVL
	debugLog("Running on Wii");
	#elifdef HW_DOL
	debugLog("Running on GC");
	#endif
	
	debugLog("argc: %d", argc);
	
	setAllowDuplicateMessages(true);
	for (int i = 0; i < argc; i++) {
		debugLog("argv[%d] @ 0x%x: %s", i, argv[i], argv[i]);
	}
	setAllowDuplicateMessages(false);
	
	#endif

	switch (VIDEO_GetCurrentTvMode()) {
		case VI_NTSC:
		case VI_EURGB60:
			#ifdef DEBUGLOG
			debugLog("Video mode is NTSC/EURGB60");
			#endif
			break;
		case VI_MPAL: // no idea if this should be a supported mode...
			#ifdef DEBUGLOG
			debugLog("Video mode is MPAL");
			#endif
			break;
		case VI_PAL:
			#ifdef DEBUGLOG
			debugLog("Video mode is PAL");
			#endif
		default:
			resetCursor();
			setCursorPos(2, 0);
			
			startDraw();
			printStr("Unsupported Video Mode\nEnsure your system is using NTSC or EURGB60\n"
						  "Program will exit in 5 seconds...");
			finishDraw(xfb[xfbSwitch]);
			
			VIDEO_WaitForRetrace(VIDEO_GetRetraceCount() + 300);
			
			cleanupBeforeExit();
			return 0;
			break; // unnecessary
	}
	
	// handle args passed to main
	handleArgs(argc, argv);
	
	setSamplingRateNormal();
	if (isUnsupportedMode()) { // unsupported mode is probably 240p? no idea
		resetCursor();
		setCursorPos(2, 0);
		
		startDraw();
		printStr("Unsupported Video Scan Mode\nEnsure your system will use 480i or 480p\n"
					  "Program will exit in 5 seconds...");
		finishDraw(xfb[xfbSwitch]);
		
		VIDEO_WaitForRetrace(VIDEO_GetRetraceCount() + 300);
		
		cleanupBeforeExit();
		return 0;
	}
	
	// register retrace callback function
	// VIDEO_Flush() clears our custom xy values, so they're set again here
	cb = VIDEO_SetPostRetraceCallback(retraceCallback);
	
	// allocate memory for recording structs
	initControllerRecStructs();
	
	
	#ifdef BENCH
	long long unsigned int time = 0;
	int gxtime = 0;
	int us = 0;
	#endif
	
	// main loop of the program
	// exits when menu_runMenu() returns true, or when either power or reset are pressed
	while (true) {
		#if defined(HW_RVL)
		if (powerButtonPressed) {
			break;
		}
		#endif
		
		if (SYS_ResetButtonDown()) {
			break;
		}
		
		#ifdef BENCH
		time = gettime();
		#endif
		
		
		if (normalExit) {
			break;
		}
		
		startDraw();
		
		// run menu
		normalExit = menu_runMenu();
		
		#ifdef BENCH
		us = ticks_to_microsecs(gettime() - time);
		setCursorPos(23, 32);
		printStrColor(GX_COLOR_WHITE, GX_COLOR_BLACK, "LOGIC: %5d | GX: %5d", us, gxtime);
		#endif
		
		finishDraw(xfb[xfbSwitch]);
		
		#ifdef BENCH
		gxtime = ticks_to_microsecs(gettime() - time);
		#endif
		
		// change framebuffer for next frame
		xfbSwitch ^= 1;
		VIDEO_SetNextFramebuffer(xfb[xfbSwitch]);

		// Wait for the next frame
		VIDEO_Flush();
		VIDEO_WaitForFlush();
	}
	
	// return some of our stuff to normal before exit
	// not sure if its needed but eh
	PAD_SetSamplingCallback(NULL);
	setSamplingRateNormal();
	setSamplingRate();
	VIDEO_SetPostRetraceCallback(NULL);
	
	// close log
	#ifdef DEBUGLOG
	stopLogging();
	#endif
	
	// close filesystem if necessary
	deinitFilesystem();
	
	// dumb way to have a different message show, while also avoiding two #if defined().
	// if using hard-coded strings, then either there would be repeat code, or you'd need two #if defined()
	// to create a wii-only if-else
	char* strPointer = resetMessage;
	int exitMessageX = 23;
	
	#if defined(HW_RVL)
	if (!normalExit) {
		if (powerButtonPressed) {
			exitMessageX = 20;
			strPointer = powerButtonMessage;
		}
	}
	#endif
	
	// hold for ~1 second, then fade over 15 frames
	for (int i = 0; i < 75; i++) {
		startDraw();
		runMenuVisual();
		
		// display exit message
		setCursorPos(10, exitMessageX);
		printStr(strPointer);
		
		// ~1 second has passed
		if (i >= 60) {
			setDepthForDrawCall(0);
			// draw a black quad in front of everything, with an increasing alpha value
			// we don't need to actually give this 255, since we blank out the frame at the end anyways
			setAlphaForDrawCall(15 * (i - 60));
			drawSolidBox(-10, -10, 700, 700, GX_COLOR_BLACK);
		}
		
		xfbSwitch ^= 1;
		
		finishDraw(xfb[xfbSwitch]);
		VIDEO_SetNextFramebuffer(xfb[xfbSwitch]);
		VIDEO_Flush();
		VIDEO_WaitForFlush();
	}
	
	// free memory (probably don't need to do this but eh)
	freeControllerRecStructs();
	
	cleanupBeforeExit();

	
	// issue poweroff if the power button was pressed
	// done here so that any logging stuff can finish cleanly
	#if defined(HW_RVL)
	if (powerButtonPressed) {
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	}
	#endif

	return 0;
}
