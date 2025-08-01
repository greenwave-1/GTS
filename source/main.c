#include <stdio.h>
#include <string.h>
#include <gccore.h>
#include <ogc/video.h>
#include <ogc/lwp_watchdog.h>
#include "menu.h"
#include "gecko.h"
#include "polling.h"
#include "print.h"


#ifdef DEBUGLOG
#include <ogc/lwp_watchdog.h>
#endif

#ifdef DEBUGGDB
#include <debug.h>
#endif

// basic stuff from the template
static void *xfb1 = NULL;
static void *xfb2 = NULL;
bool xfbSwitch = false;
static GXRModeObj *rmode = NULL;

static VIRetraceCallback cb;

void retraceCallback(u32 retraceCnt) {
	setSamplingRate();
}

#if defined(HW_RVL)
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

	xfb1 = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	xfb2 = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	
	//CON_Init(xfb2,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	//CON_Init(xfb1,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_ClearFrameBuffer(rmode, xfb1, COLOR_BLACK);
	VIDEO_ClearFrameBuffer(rmode, xfb2, COLOR_BLACK);

	VIDEO_Configure(rmode);

	VIDEO_SetNextFramebuffer(xfb1);

	VIDEO_SetBlack(FALSE);

	VIDEO_Flush();

	VIDEO_WaitVSync();
	
	cb = VIDEO_SetPostRetraceCallback(retraceCallback);
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	bool shouldExit = false;
	
	#ifdef DEBUGGDB
	_break();
	#endif

	void *currXfb = NULL;
	
	// there is a makefile target that will enable this
	#ifdef DEBUGLOG
	sendMessage("USB Gecko Debug output enabled");
	#ifdef HW_RVL
	sendMessage("Running on Wii");
	#elifdef HW_DOL
	sendMessage("Running on GC");
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
	
	u32 buttons = PAD_ButtonsHeld(0);
	//u32 useless = 42069;
	
	// check if b is held and we are in progressive scan
	if (buttons & PAD_BUTTON_B) {
		char msg[500];
		memset(msg, 0, sizeof(msg));
		u32 scanMode = VIDEO_GetScanMode();
		u32 tvMode = VIDEO_GetCurrentTvMode();
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
			sendMessage("Video mode is NTSC/EURGB60");
			#endif
			break;
		case VI_MPAL: // no idea if this should be a supported mode...
			#ifdef DEBUGLOG
			sendMessage("Video mode is MPAL");
			#endif
			break;
		case VI_PAL:
			#ifdef DEBUGLOG
			sendMessage("Video mode is PAL");
			#endif
		default:
			VIDEO_SetNextFramebuffer(xfb2);
			VIDEO_Flush();
			resetCursor();
			printStr("\n\nUnsupported Video Mode\nEnsure your system is using NTSC or EURGB60\n"
					 "Program will exit in 5 seconds...", xfb2);
			VIDEO_WaitVSync();
			u64 timer = gettime();
			while (ticks_to_secs(gettime() - timer) < 5) ;
			return 0;
			break; // unnecessary
	}
	
	setSamplingRateNormal();
	if (isUnsupportedMode()) { // unsupported mode is probably 240p? no idea
		VIDEO_SetNextFramebuffer(xfb2);
		VIDEO_Flush();
		resetCursor();
		printStr("\n\nUnsupported Video Scan Mode\nEnsure your system will use 480i or 480p\n"
				 "Program will exit in 5 seconds...", xfb2);
		VIDEO_WaitVSync();
		u64 timer = gettime();
		while (ticks_to_secs(gettime() - timer) < 5) ;
		return 0;
	}
	
	// main loop of the program
	// exits when menu_runMenu() returns true, or when either power or reset are pressed
	while (true) {
		#if defined(HW_RVL)
		if (powerButtonPressed) {
			SYS_ResetSystem(SYS_POWEROFF, 0, 0);
			break;
		}
		#endif
		
		#ifdef BENCH
		time = gettime();
		#endif
		
		
		if (shouldExit) {
			break;
		}

		// check which framebuffer is next
		if (xfbSwitch) {
			VIDEO_ClearFrameBuffer(rmode, xfb1, COLOR_BLACK);
			//CON_Init(xfb1,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
			currXfb = xfb1;
		} else {
			VIDEO_ClearFrameBuffer(rmode, xfb2, COLOR_BLACK);
			//CON_Init(xfb2,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
			currXfb = xfb2;
		}

		// run menu
		shouldExit = menu_runMenu(currXfb);

		// change framebuffer for next frame
		if (xfbSwitch) {
			VIDEO_SetNextFramebuffer(xfb1);
		} else {
			VIDEO_SetNextFramebuffer(xfb2);
		}
		xfbSwitch = !xfbSwitch;
		
		
		#ifdef BENCH
		us = ticks_to_microsecs(gettime() - time);
		char msg[100];
		sprintf(msg, "%d", us);
		setCursorPos(22, 62 - strlen(msg));
		printStrColor(msg, currXfb, COLOR_WHITE, COLOR_BLACK);
		#endif
		
		
		if (SYS_ResetButtonDown()) {
			VIDEO_ClearFrameBuffer(rmode, currXfb, COLOR_BLACK);
			setCursorPos(10, 15);
			printStr("Reset button pressed, exiting...", currXfb);
			VIDEO_Flush();
			VIDEO_WaitVSync();
			break;
		}

		// Wait for the next frame
		VIDEO_Flush();
		VIDEO_WaitVSync();
	}

	return 0;
}
