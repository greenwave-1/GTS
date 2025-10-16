#include <stdint.h>

#include <gccore.h>

#include "menu.h"
#include "polling.h"
#include "print.h"
#include "draw.h"
#include "waveform.h"
#include "gx.h"

#ifdef DEBUGLOG
#include "logging.h"
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

static char* resetMessage = "Reset button pressed, exiting...";

void retraceCallback(uint32_t retraceCnt) {
	setSamplingRate();
	
	//debugLog("Wrap: %u", getFifoVal());
	//uint8_t overhi, underlow, readidle, cmdidle, brkpt;
	//GX_GetGPStatus(&overhi, &underlow, &readidle, &cmdidle, &brkpt);
	//debugLog("GPStatus: %u %u %u %u %u\n", overhi, underlow, readidle, cmdidle, brkpt);
	//#ifdef DEBUGGDB
	//if (SYS_ResetButtonDown()) {
		//_break();
	//}
	//#endif
}

#if defined(HW_RVL)
// this is stupid, but makes the #ifdef in logic look a bit nicer
static char* powerButtonMessage = "Power button pressed, shutting down...";

static bool powerButtonPressed = false;
void powerButtonCallback() {
	powerButtonPressed = true;
}
#endif

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
	// the only thing we're doing differently is a double buffer (because terminal weirdness on actual hardware)
	VIDEO_Init();
	PAD_Init();
	PAD_ScanPads();

	rmode = VIDEO_GetPreferredMode(NULL);

	xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	
	//CON_Init(xfb2,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	//CON_Init(xfb1,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_ClearFrameBuffer(rmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(rmode, xfb[1], COLOR_BLACK);

	VIDEO_Configure(rmode);

	VIDEO_SetNextFramebuffer(xfb[xfbSwitch]);

	VIDEO_SetBlack(false);

	VIDEO_Flush();

	VIDEO_WaitVSync();
	
	cb = VIDEO_SetPostRetraceCallback(retraceCallback);
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
	
	setupGX(rmode);

	bool normalExit = false;
	
	#ifdef DEBUGGDB
	_break();
	#endif
	
	// there is a makefile target that will enable this
	#ifdef DEBUGLOG
	
	// options are defined in logging.h
	setupLogging(USBGECKO_B);
	
	if (getLoggingType() == NETWORKSOCK) {
		setFramebuffer(xfb[xfbSwitch]);
		printStr("Setting up network...\n");
		while (!isNetworkConfigured()) {
			VIDEO_WaitVSync();
		}
		
		printStr("Waiting for connection...\n");
		printStr(getConfiguredIP());
		printStr(":43256");
		
		while (!isConnectionMade()) {
			VIDEO_WaitVSync();
		}
	}
	debugLog("Debug output enabled");
	
	#ifdef HW_RVL
	debugLog("Running on Wii");
	#elifdef HW_DOL
	debugLog("Running on GC");
	#endif
	
	#endif
	
	
	// TODO: WIP Logic for forcing interlaced
	// this needs much more logic than what's present
	// mainly, using the correct tv mode instead of just doing ntsc
	// probably will need testing from PAL users
	/*
	// wait a couple frames, inputs don't register initially???
	PAD_ScanPads();
	VIDEO_WaitVSync();
	PAD_ScanPads();
	VIDEO_WaitVSync();
	PAD_ScanPads();
	
	uint32_t buttons = PAD_ButtonsHeld(0);
	//uint32_t useless = 42069;
	
	// check if b is held and we are in progressive scan
	if (buttons & PAD_BUTTON_B) {
		char msg[500];
		memset(msg, 0, sizeof(msg));
		uint32_t scanMode = VIDEO_GetScanMode();
		uint32_t tvMode = VIDEO_GetCurrentTvMode();
		snprintf(msg, 500, "B held, Progressive scan: %d | TV Mode: %d\r\n\0", scanMode, tvMode);
		usb_sendbuffer(1, msg, 500);
		usb_flush(EXI_CHANNEL_1);
		if (scanMode == 2) {
			memset(msg, 0, sizeof(msg));
			snprintf(msg, 500, "Progressive Scan detected, asking if user wants to force interlaced\n\0");
			usb_sendbuffer(1, msg, 500);
			usb_flush(EXI_CHANNEL_1);
			
			// force interlaced
			VIDEO_Configure(&TVNtsc480IntDf);
			
			VIDEO_ClearFrameBuffer(rmode, xfb2, COLOR_BLACK);
			CON_Init(xfb2,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
			currXfb = xfb2;
			VIDEO_SetNextFramebuffer(xfb2);
			
			xfbSwitch = true;
			
			printf("\x1b[3;0H");
			printf("Press A to enable progressive scan, press B to disable\n");
			while (true) {
				VIDEO_Flush();
				VIDEO_WaitVSync();
				PAD_ScanPads();
				buttons = PAD_ButtonsDown(0);
				if (buttons & PAD_BUTTON_A) {
					VIDEO_Configure(&TVNtsc480Prog);
					break;
				}
				if (buttons & PAD_BUTTON_B) {
					break;
				}
			}
		}
	}
	*/
	
	
	#ifdef BENCH
	long long unsigned int time = 0;
	int us = 0;
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
			setFramebuffer(xfb[xfbSwitch]);
			printStr("\n\nUnsupported Video Mode\nEnsure your system is using NTSC or EURGB60\n"
					 "Program will exit in 5 seconds...");
			for (int i = 0; i < 300; i++) {
				VIDEO_WaitVSync();
			}
			return 0;
			break; // unnecessary
	}
	
	setSamplingRateNormal();
	if (isUnsupportedMode()) { // unsupported mode is probably 240p? no idea
		resetCursor();
		setFramebuffer(xfb[xfbSwitch]);
		printStr("\n\nUnsupported Video Scan Mode\nEnsure your system will use 480i or 480p\n"
				 "Program will exit in 5 seconds...");
		for (int i = 0; i < 300; i++) {
			VIDEO_WaitVSync();
		}
		return 0;
	}
	
	// allocate memory for recording structs
	initControllerRecStructs();
	
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
		
		setFramebuffer(xfb[xfbSwitch]);
		
		startDraw(rmode);
		resetCursor();
		//printStr("Hello there");
		// run menu
		normalExit = menu_runMenu();
		
		#ifdef BENCH
		us = ticks_to_microsecs(gettime() - time);
		setCursorPos(22, 56);
		printStrColor(COLOR_WHITE, COLOR_BLACK, "%d", us);
		#endif
		
		xfbSwitch ^= 1;
		
		finishDraw(xfb[xfbSwitch]);
		
		// change framebuffer for next frame
		VIDEO_SetNextFramebuffer(xfb[xfbSwitch]);

		// Wait for the next frame
		VIDEO_Flush();
		VIDEO_WaitVSync();
	}
	
	// return some of our stuff to normal before exit
	// not sure if its needed but eh
	setSamplingRateNormal();
	PAD_SetSamplingCallback(NULL);
	VIDEO_SetPostRetraceCallback(NULL);
	
	// close log
	#ifdef DEBUGLOG
	stopLogging();
	#endif
	
	// clear screen and show message if not exiting "normally" (pressing start on main menu)
	if (!normalExit) {
		startDraw(rmode);
		setCursorPos(10, 15);
		
		// dumb way to have a different message show, while also avoiding two #if defined().
		// if using hard-coded strings, then either there would be repeat code, or you'd need two #if defined()
		// to create a wii-only if-else
		char* strPointer = resetMessage;
		
		#if defined(HW_RVL)
		if (powerButtonPressed) {
			setCursorPos(10, 12);
			strPointer = powerButtonMessage;
		}
		#endif
		
		printStr(strPointer);
		xfbSwitch ^= 1;
		
		finishDraw(xfb[xfbSwitch]);
		VIDEO_SetNextFramebuffer(xfb[xfbSwitch]);
	}
	
	// draw one more frame, since buffer is 1 frame behind
	startDraw(rmode);
	xfbSwitch ^= 1;
	finishDraw(xfb[xfbSwitch]);
	
	VIDEO_Flush();
	// show final frame for at least one second
	for (int i = 0; i < 60; i++) {
		VIDEO_WaitVSync();
	}
	
	// free memory (probably don't need to do this but eh)
	freeControllerRecStructs();
	
	// avoid distorted graphics on GC (I have no idea if this actually does what I think it does...)
	// https://github.com/emukidid/swiss-gc: cube/swiss/source/video.c -> unsetVideo()
	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	
	// issue poweroff if the power button was pressed
	// done here so that any logging stuff can finish cleanly
	#if defined(HW_RVL)
	if (powerButtonPressed) {
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	}
	#endif

	return 0;
}
