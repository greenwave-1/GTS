// fossScope (working title)

#include <stdio.h>
#include <gccore.h>
#include "menu.h"
#include "gecko.h"


// basic stuff from the template
static void *xfb1 = NULL;
static void *xfb2 = NULL;
bool xfbSwitch = false;
static GXRModeObj *rmode = NULL;

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

	CON_Init(xfb1,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	CON_Init(xfb2,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	VIDEO_Configure(rmode);

	VIDEO_SetNextFramebuffer(xfb1);

	VIDEO_SetBlack(FALSE);

	VIDEO_Flush();

	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	bool shouldExit = false;

	void *currXfb = NULL;
	
	// there is a makefile target that will enable this
	#ifdef DEBUG
	sendMessage("USB Gecko Debug output enabled", 100);
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

	// main loop of the program
	while (true) {
		if (shouldExit) {
			break;
		}

		// check which framebuffer is next
		if (xfbSwitch) {
			VIDEO_ClearFrameBuffer(rmode, xfb1, COLOR_BLACK);
			CON_Init(xfb1,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
			currXfb = xfb1;
		} else {
			VIDEO_ClearFrameBuffer(rmode, xfb2, COLOR_BLACK);
			CON_Init(xfb2,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
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

		// Wait for the next frame
		VIDEO_Flush();
		VIDEO_WaitVSync();
	}

	return 0;
}
