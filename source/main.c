// fossScope (working title)

#include <stdio.h>
#include <gccore.h>
#include "menu.h"


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

	rmode = VIDEO_GetPreferredMode(NULL);

	xfb1 = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    xfb2 = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    console_init(xfb1,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
    console_init(xfb2,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	VIDEO_Configure(rmode);

	VIDEO_SetNextFramebuffer(xfb1);

	VIDEO_SetBlack(FALSE);

	VIDEO_Flush();

	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	bool shouldExit = false;

	void *currXfb = NULL;

    // main loop of the program
	while (true) {
		if (shouldExit) {
			break;
		}

        // check which framebuffer is next
        if (xfbSwitch) {
            VIDEO_ClearFrameBuffer(rmode, xfb1, COLOR_BLACK);
            console_init(xfb1,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
			currXfb = xfb1;
        } else {
            VIDEO_ClearFrameBuffer(rmode, xfb2, COLOR_BLACK);
            console_init(xfb2,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
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
