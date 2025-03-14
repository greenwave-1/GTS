// fossScope (working title)

#include <stdio.h>
#include <gccore.h>
#include <ogc/video.h>
#include <ogc/lwp_watchdog.h>
#include "menu.h"
#include "gecko.h"
#include "polling.h"
#include "print.h"


#ifdef DEBUG
#include <ogc/lwp_watchdog.h>
#endif


// basic stuff from the template
static void *xfb1 = NULL;
static void *xfb2 = NULL;
bool xfbSwitch = false;
static GXRModeObj *rmode = NULL;

static VIRetraceCallback cb;

void retraceCallback() {
	setSamplingRate();
}

int main(int argc, char **argv) {
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

	void *currXfb = NULL;
	
	// there is a makefile target that will enable this
	#ifdef DEBUG
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
	
	/*
	#ifdef DEBUG
	int minUs = 0;
	int maxUs = 0;
	int avgUs = 0;
	int counter = 0;
	long long unsigned int time = 0;
	int us = 0;
	#endif
	 */
	
	switch (VIDEO_GetCurrentTvMode()) {
		case VI_NTSC:
		case VI_EURGB60:
			#ifdef DEBUG
			sendMessage("Video mode is NTSC/EURGB60");
			#endif
			break;
		case VI_MPAL: // no idea if this should be a supported mode...
			#ifdef DEBUG
			sendMessage("Video mode is MPAL");
			#endif
			break;
		case VI_PAL:
			#ifdef DEBUG
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
	while (true) {
		// set si polling rate
		// poll 2 times every frame, every 240 horizontal lines
		// no idea why this has to be here, it gets reset after every run of this loop,
		// probably something to do with re-initialising the framebuffers and stuff
		setSamplingRateNormal();
		//SI_SetXY(240, 2);
		
		/*
		#ifdef DEBUG
		counter++;
		time = gettime();
		#endif
		 */
		
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
		
		/*
		#ifdef DEBUG
		us = ticks_to_microsecs(gettime() - time);
		char msg[100];
		sprintf(msg, "Microseconds taken in loop: %d", us);
		sendMessage(msg, 100);
		#endif
		 */
		

		// Wait for the next frame
		VIDEO_Flush();
		VIDEO_WaitVSync();
	}

	return 0;
}
