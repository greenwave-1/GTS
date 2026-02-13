//
// Created on 10/16/25.
//

#include "submenu/coordinateviewer.h"

#include <stdint.h>
#include <math.h>

#include <ogc/pad.h>

#include "util/gx.h"
#include "waveform.h"
#include "util/polling.h"
#include "util/print.h"

enum STICKMAP_LIST { NONE, FF_WD, SHIELDDROP };

// values used for individual coordinate sets
// since all of these are more than just individual values,
// these are actually static consts, rather than #defines (like in controllertest.h)

// Firefox and Wavedash min/max
static const char* STICKMAP_FF_WD_DESC = "Min/Max coordinates for Firefox and Wavedash notches around\ncardinals\n\n"
                                  "SAFE / Green:\n Ideal coordinates, aim here\n"
								  "UNSAFE / Yellow:\n Risks hitting deadzone, but works\n"
                                  "MISS:\n Self explanatory";

static const char* STICKMAP_FF_WD_RETVALS[] = {"MISS", "SAFE", "UNSAFE"};

// colors for the above responses, separated because it looks better this way
// 0 -> no color
// 1 -> black text on green background
// 2 -> black text on yellow background
static const GXColor STICKMAP_FF_WD_RETCOLORS[][2] = { {GX_COLOR_BLACK, GX_COLOR_WHITE},
                                                {GX_COLOR_GREEN, GX_COLOR_BLACK},
                                                {GX_COLOR_YELLOW, GX_COLOR_BLACK} };

enum STICKMAP_FF_WD_ENUM { FF_WD_MISS, FF_WD_SAFE, FF_WD_UNSAFE };
static const int STICKMAP_FF_WD_ENUM_LEN = 3;

static const int STICKMAP_FF_WD_COORD_SAFE[][2] = { {9375, 3125},
                                             {9375, 3250} };
static const int STICKMAP_FF_WD_COORD_SAFE_LEN = 2;

static const int STICKMAP_FF_WD_COORD_UNSAFE[][2] = { {9500, 3000},
                                               {9500, 2875} };
static const int STICKMAP_FF_WD_COORD_UNSAFE_LEN = 2;

// Shield drop coordinates
static const char* STICKMAP_SHIELDDROP_DESC = "Coorinates for Vanilla and UCF Shield drops. Different "
                                       "coordinate groups vary in requirements, check the SmashBoards "
                                       "UCF post for more info.\n\n"
                                       "VANILLA / Green:\n Coordinates that will work without UCF\n"
                                       "UCF LOWER / Blue:\n Lower coordinates for any UCF Version\n"
                                       "UCF v0.84 UPPER / Yellow:\n Upper coordinates for v0.84+ only\n";

static const char* STICKMAP_SHIELDDROP_RETVALS[] = { "MISS", "VANILLA", "UCF LOWER", "UCF v0.84 UPPER" };

// 0 -> no color
// 1 -> black text on green background
// 2 -> white text on blue background
// 3 -> black text on yellow background
static const GXColor STICKMAP_SHIELDDROP_RETCOLORS[][2] = { {GX_COLOR_BLACK, GX_COLOR_WHITE},
                                                     {GX_COLOR_GREEN, GX_COLOR_BLACK},
                                                     {GX_COLOR_BLUE, GX_COLOR_WHITE},
                                                     {GX_COLOR_YELLOW, GX_COLOR_BLACK} };

enum STICKMAP_SHIELDDROP_ENUM { SHIELDDROP_MISS, SHIELDDROP_VANILLA, SHIELDDROP_UCF_LOWER, SHIELDDROP_UCF_UPPER };
static const int STICKMAP_SHIELDDROP_ENUM_LEN = 4;

static const int STICKMAP_SHIELDDROP_COORD_VANILLA[][2] = { { 7375, 6625 },
                                                     { 7375, 6750 },
                                                     { 7250, 6875 } };
static const int STICKMAP_SHIELDDROP_COORD_VANILLA_LEN = 3;

static const int STICKMAP_SHIELDDROP_COORD_UCF_LOWER[][2] = { { 7000, 7000 },
                                                       { 7125, 7000 },
                                                       
                                                       { 6875, 7125 },
                                                       { 7000, 7125 },
                                                       
                                                       { 6750, 7250 },
                                                       { 6875, 7250 },
                                                       
                                                       { 6500, 7375 },
                                                       { 6625, 7375 },
                                                       { 6750, 7375 },
                                                       
                                                       { 6375, 7500 },
                                                       { 6500, 7500 },
                                                       
                                                       { 6250, 7625 },
                                                       { 6375, 7625 },
                                                       
                                                       { 6125, 7785 },
                                                       { 6250, 7785 },
                                                       { 6000, 7875 },
                                                       { 6125, 7875 } };
static const int STICKMAP_SHIELDDROP_COORD_UCF_LOWER_LEN = 17;

static const int STICKMAP_SHIELDDROP_COORD_UCF_UPPER[][2] = { { 7875, 6125 },
                                                       { 7750, 6125 },
                                                       
                                                       { 7625, 6250 },
                                                       { 7750, 6250 },
                                                       
                                                       { 7500, 6375 },
                                                       { 7625, 6375 },
                                                       
                                                       { 7375, 6500 },
                                                       { 7500, 6500 } };
static const int STICKMAP_SHIELDDROP_COORD_UCF_UPPER_LEN = 8;

static int isCoordValid(enum STICKMAP_LIST test, MeleeCoordinates coords) {
	// 0 index is always "show all"
	int ret = 0;
	switch (test) {
		case FF_WD:
			// safe coords
			for (int i = 0; i < STICKMAP_FF_WD_COORD_SAFE_LEN; i++) {
				if ((coords.stickXUnit == STICKMAP_FF_WD_COORD_SAFE[i][0] && coords.stickYUnit == STICKMAP_FF_WD_COORD_SAFE[i][1]) ||
				    (coords.stickYUnit == STICKMAP_FF_WD_COORD_SAFE[i][0] && coords.stickXUnit == STICKMAP_FF_WD_COORD_SAFE[i][1])) {
					ret = 1;
					break;
				}
			}
			// unsafe coords
			for (int i = 0; i < STICKMAP_FF_WD_COORD_UNSAFE_LEN; i++) {
				if ((coords.stickXUnit == STICKMAP_FF_WD_COORD_UNSAFE[i][0] && coords.stickYUnit == STICKMAP_FF_WD_COORD_UNSAFE[i][1]) ||
				    (coords.stickYUnit == STICKMAP_FF_WD_COORD_UNSAFE[i][0] && coords.stickXUnit == STICKMAP_FF_WD_COORD_UNSAFE[i][1]) ) {
					ret = 2;
					break;
				}
			}
			break;
		case SHIELDDROP:
			if (coords.stickYNegative) {
				// vanilla
				for (int i = 0; i < STICKMAP_SHIELDDROP_COORD_VANILLA_LEN; i++) {
					if (coords.stickYUnit == STICKMAP_SHIELDDROP_COORD_VANILLA[i][1] ||
					    (coords.stickYUnit * -1) == STICKMAP_SHIELDDROP_COORD_VANILLA[i][1]) {
						ret = 1;
						break;
					}
				}
				// ucf lower
				for (int i = 0; i < STICKMAP_SHIELDDROP_COORD_UCF_LOWER_LEN; i++) {
					if ((coords.stickXUnit == STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][0] &&
					     coords.stickYUnit == STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][1]) ||
					    ((coords.stickXUnit * -1) == STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][0] &&
					     coords.stickYUnit == STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][1])) {
						ret = 2;
						break;
					}
				}
				// ucf v0.84 upper
				for (int i = 0; i < STICKMAP_SHIELDDROP_COORD_UCF_UPPER_LEN; i++) {
					if ((coords.stickXUnit == STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][0] &&
					     coords.stickYUnit == STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][1]) ||
					    ((coords.stickXUnit * -1) == STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][0] &&
					     coords.stickYUnit == STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][1])) {
						ret = 3;
						break;
					}
				}
			}
		case (NONE):
		default:
			break;
	}
	return ret;
}

// we're storing coordinates as whole integers of the decimal part
// each visible unit is 0.0125, so we divide by 125 to get the number of units moved
// then scaled by 2x for visibility
static int toStickmap(int meleeCoord) {
	return ((meleeCoord / 125) * 2);
}

static uint16_t *pressed = NULL;
static uint16_t *held = NULL;

static enum COORD_VIEW_MENU_STATE menuState = COORD_VIEW_SETUP;

static enum STICKMAP_LIST selectedStickmap = NONE;
// will be casted to whichever stickmap is selected
static int selectedStickmapSub = 0;

static bool menuLockEnabled = false;

static void setup() {
	if (pressed == NULL) {
		pressed = getButtonsDownPtr();
		held = getButtonsHeldPtr();
	}
	
	menuState = COORD_VIEW_POST_SETUP;
	resetScrollingPrint();
}


// TODO: this is awful. find a better way to do this...
static void drawStickmapOverlay(enum STICKMAP_LIST stickmap, int which) {
	updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
	changeLoadedTexmap(TEXMAP_NONE);
	GX_SetPointSize(20, GX_TO_ZERO);
	
	switch (stickmap) {
		case (FF_WD):
			// bools for which parts to draw
			// this is dumb, but avoids duplicate code
			bool drawSafe = true, drawUnsafe = true;
			
			switch ((enum STICKMAP_FF_WD_ENUM) which) {
				case (FF_WD_SAFE):
					drawUnsafe = false;
					break;
				case (FF_WD_UNSAFE):
					drawSafe = false;
					break;
				// miss is interpreted as "all" in this case
				case (FF_WD_MISS):
				default:
					break;
			}
			
			if (drawSafe) {
				GX_Begin(GX_POINTS, VTXFMT_PRIMITIVES_INT, STICKMAP_FF_WD_COORD_SAFE_LEN * 8);
				for (int i = 0; i < STICKMAP_FF_WD_COORD_SAFE_LEN; i++) {
					GX_Position3s16(toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]) + COORD_CIRCLE_CENTER_X,
					                toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]) + SCREEN_POS_CENTER_Y, -9);
					GX_Color4u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b, GX_COLOR_GREEN.a);
					
					GX_Position3s16(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]),
					                toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]) + SCREEN_POS_CENTER_Y, -9);
					GX_Color4u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b, GX_COLOR_GREEN.a);
					
					GX_Position3s16(toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]) + COORD_CIRCLE_CENTER_X,
					                SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]), -9);
					GX_Color4u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b, GX_COLOR_GREEN.a);
					
					GX_Position3s16(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]),
					                SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]), -9);
					GX_Color4u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b, GX_COLOR_GREEN.a);
					
					GX_Position3s16(toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]) + COORD_CIRCLE_CENTER_X,
					                toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]) + SCREEN_POS_CENTER_Y, -9);
					GX_Color4u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b, GX_COLOR_GREEN.a);
					
					GX_Position3s16(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]),
					                toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]) + SCREEN_POS_CENTER_Y, -9);
					GX_Color4u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b, GX_COLOR_GREEN.a);
					
					GX_Position3s16(toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]) + COORD_CIRCLE_CENTER_X,
					                SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]), -9);
					GX_Color4u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b, GX_COLOR_GREEN.a);
					
					GX_Position3s16(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]),
					                SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]), -9);
					GX_Color4u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b, GX_COLOR_GREEN.a);
				}
				GX_End();
			}
			if (drawUnsafe) {
				GX_Begin(GX_POINTS, VTXFMT_PRIMITIVES_INT, STICKMAP_FF_WD_COORD_UNSAFE_LEN * 8);
				for (int i = 0; i < STICKMAP_FF_WD_COORD_UNSAFE_LEN; i++) {
					GX_Position3s16(toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]) + COORD_CIRCLE_CENTER_X,
					                    toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]) + SCREEN_POS_CENTER_Y, -8);
					GX_Color4u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b, GX_COLOR_YELLOW.a);
					
					GX_Position3s16(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]),
					                    toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]) + SCREEN_POS_CENTER_Y, -8);
					GX_Color4u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b, GX_COLOR_YELLOW.a);
					
					GX_Position3s16(toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]) + COORD_CIRCLE_CENTER_X,
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]), -8);
					GX_Color4u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b, GX_COLOR_YELLOW.a);
					
					GX_Position3s16(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]),
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]), -8);
					GX_Color4u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b, GX_COLOR_YELLOW.a);
					
					GX_Position3s16(toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]) + COORD_CIRCLE_CENTER_X,
					                    toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]) + SCREEN_POS_CENTER_Y, -8);
					GX_Color4u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b, GX_COLOR_YELLOW.a);
					
					GX_Position3s16(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]),
					                    toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]) + SCREEN_POS_CENTER_Y, -8);
					GX_Color4u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b, GX_COLOR_YELLOW.a);
					
					GX_Position3s16(toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]) + COORD_CIRCLE_CENTER_X,
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]), -8);
					GX_Color4u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b, GX_COLOR_YELLOW.a);
					
					GX_Position3s16(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]),
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]), -8);
					GX_Color4u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b, GX_COLOR_YELLOW.a);
				}
				GX_End();
			}
			break;
		case (SHIELDDROP):
			// bools for which parts to draw
			// this is dumb, but avoids duplicate code
			bool drawVanilla = true, drawUCFUpper = true, drawUCFLower = true;
			switch ((enum STICKMAP_SHIELDDROP_ENUM) which) {
				case (SHIELDDROP_VANILLA):
					drawUCFUpper = false;
					drawUCFLower = false;
					break;
				case (SHIELDDROP_UCF_LOWER):
					drawVanilla = false;
					drawUCFUpper = false;
					break;
				case (SHIELDDROP_UCF_UPPER):
					drawVanilla = false;
					drawUCFLower = false;
					break;
				case (SHIELDDROP_MISS):
				default:
					break;
			}
			
			if (drawUCFUpper) {
				GX_Begin(GX_POINTS, VTXFMT_PRIMITIVES_INT, STICKMAP_SHIELDDROP_COORD_UCF_UPPER_LEN * 2);
				for (int i = 0; i < STICKMAP_SHIELDDROP_COORD_UCF_UPPER_LEN; i++) {
					GX_Position3s16(toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][0]) + COORD_CIRCLE_CENTER_X,
					                    toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][1]) + SCREEN_POS_CENTER_Y, -10);
					GX_Color4u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b, GX_COLOR_YELLOW.a);
					
					GX_Position3s16(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][0]),
					                    toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][1]) + SCREEN_POS_CENTER_Y, -10);
					GX_Color4u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b, GX_COLOR_YELLOW.a);
				}
				GX_End();
			}
			if (drawUCFLower) {
				GX_Begin(GX_POINTS, VTXFMT_PRIMITIVES_INT, STICKMAP_SHIELDDROP_COORD_UCF_LOWER_LEN * 2);
				for (int i = 0; i < STICKMAP_SHIELDDROP_COORD_UCF_LOWER_LEN; i++) {
					GX_Position3s16(toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][0]) + COORD_CIRCLE_CENTER_X,
					                    toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][1]) + SCREEN_POS_CENTER_Y, -9);
					GX_Color4u8(GX_COLOR_BLUE.r, GX_COLOR_BLUE.g, GX_COLOR_BLUE.b, GX_COLOR_BLUE.a);
					
					GX_Position3s16(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][0]),
					                    toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][1]) + SCREEN_POS_CENTER_Y, -9);
					GX_Color4u8(GX_COLOR_BLUE.r, GX_COLOR_BLUE.g, GX_COLOR_BLUE.b, GX_COLOR_BLUE.a);
				}
				GX_End();
			}
			if (drawVanilla) {
				GX_Begin(GX_LINES, VTXFMT_PRIMITIVES_INT, STICKMAP_SHIELDDROP_COORD_VANILLA_LEN * 2);
				for (int i = 0; i < STICKMAP_SHIELDDROP_COORD_VANILLA_LEN; i++) {
					
					GX_Position3s16(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][0]) - 1,
					          toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][1]) + SCREEN_POS_CENTER_Y, -8);
					GX_Color4u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b, GX_COLOR_GREEN.a);
					
					GX_Position3s16(toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][0]) + COORD_CIRCLE_CENTER_X + 1,
					          toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][1]) + SCREEN_POS_CENTER_Y, -8);
					GX_Color4u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b, GX_COLOR_GREEN.a);
				}
				GX_End();
			}
			break;
		default:
			break;
	}
}

static void displayInstructions() {
	//setCursorPos(2, 0);
	startScrollingPrint(40, 70, 600, 400);
	setWordWrap(true);
	printStr("Press");
	fontButtonSetDpadDirections(FONT_DPAD_RIGHT);
	drawFontButton(FONT_DPAD);
	printStr("to change the overall coordinate category, and");
	fontButtonSetDpadDirections(FONT_DPAD_UP);
	drawFontButton(FONT_DPAD);
	printStr("to change what subset of coordinates are shown. Melee "
	         "Coordinates are shown on the left.\n\n"
	         "The white line shows the analog stick's position, and the "
	         "yellow line shows the c-stick's position.\n\n"
	         "Hold Start to 'lock' the menu. This disables the "
			 "instructions page (Z");
	drawFontButton(FONT_Z);
	printStr(") and exiting (B");
	drawFontButton(FONT_B);
	printStr(").\n\n"
			 "Current Stickmap");
	fontButtonSetDpadDirections(FONT_DPAD_RIGHT);
	drawFontButton(FONT_DPAD);
	printStr(": ");
	/*
	printStr("Press X to change the overall coordinate category, and Y to\n"
			 "change what subset of coordinates are shown. Melee\n"
			 "Coordinates are shown on the left.\n\n"
	         "The white line shows the analog stick's position, and the\n"
			 "yellow line shows the c-stick's position.\n\n"
			 "Hold Start to 'lock' the menu. This disables the\n"
			 "instructions page (Z) and exiting (B), and swaps X/Y with\n"
			 "DPad Up and Right.\n\n"
	         "Current Stickmap: ");
	*/
	switch (selectedStickmap) {
		case FF_WD:
			printStr("Firefox / Wavedash\n");
			printStr(STICKMAP_FF_WD_DESC);
			break;
		case SHIELDDROP:
			printStr("Shield Drop\n");
			printStr(STICKMAP_SHIELDDROP_DESC);
			break;
		case NONE:
		default:
			printStr("None\n");
			break;
	}
	setWordWrap(false);
	endScrollingPrint();
	
	if (isControllerConnected(CONT_PORT_1)) {
		setCursorPos(0, 31);
		printStr("Close Instructions (Z");
		drawFontButton(FONT_Z);
		printStr(")");
	}
	
	if (*pressed & PAD_TRIGGER_Z || menuLockEnabled) {
		menuState = COORD_VIEW_POST_SETUP;
	}
}

static int dpadFlashCounter = 0;
// coordinate viewer submenu
// draws melee coordinates for both sticks on a circle
// "overlays" can be toggled to show specific coordinate groups (shield drop, for example)
void menu_coordView() {
	switch(menuState) {
		case COORD_VIEW_SETUP:
			setup();
			break;
		case COORD_VIEW_POST_SETUP:
			// melee stick coordinates stuff
			// a lot of this comes from github.com/phobgcc/phobconfigtool
			
			if (!menuLockEnabled && isControllerConnected(CONT_PORT_1)) {
				setCursorPos(0, 32);
				printStr("View Instructions (Z");
				drawFontButton(FONT_Z);
				printStr(")");
			}
			
			static ControllerSample stickRaw;
			static MeleeCoordinates stickMelee;
			
			// get raw stick values
			stickRaw.stickX = PAD_StickX(0), stickRaw.stickY = PAD_StickY(0);
			stickRaw.cStickX = PAD_SubStickX(0), stickRaw.cStickY = PAD_SubStickY(0);
			
			// get converted stick values
			stickMelee = convertStickRawToMelee(stickRaw);
			
			// print melee coordinates
			setCursorPos(9, 0);
			printStr("Analog Stick:");
			setCursorPos(10, 2);
			printStr("(%s)", getMeleeCoordinateString(stickMelee, AXIS_AXY));
			
			setCursorPos(11, 0);
			printStr("C-Stick:");
			setCursorPos(12, 2);
			printStr("(%s)", getMeleeCoordinateString(stickMelee, AXIS_CXY));
			
			setCursorPos(4, 0);
			printStr("Stickmap ");
			fontButtonSetDpadDirections(FONT_DPAD_RIGHT);
			drawFontButton(FONT_DPAD);
			printStr(":");
			
			//setCursorPos(6, 15);
			int stickmapRetVal = isCoordValid(selectedStickmap, stickMelee);
			setCursorPos(6, 0);
			
			printStr("Shown ");
			fontButtonSetDpadDirections(FONT_DPAD_UP);
			drawFontButton(FONT_DPAD);
			printStr(":");
			
			setCursorPos(14, 0);
			printStr("Result:");
			switch (selectedStickmap) {
				case FF_WD:
					setCursorPos(5, 2);
					printStr("Firefox/Wavedash");
					setCursorPos(7, 2);
					if (selectedStickmapSub == 0) {
						printStr("ALL");
					} else {
						printStrColor(STICKMAP_FF_WD_RETCOLORS[selectedStickmapSub][0], STICKMAP_FF_WD_RETCOLORS[selectedStickmapSub][1],
						              STICKMAP_FF_WD_RETVALS[selectedStickmapSub]);
					}
					setCursorPos(15, 2);
					printStrColor(STICKMAP_FF_WD_RETCOLORS[stickmapRetVal][0], STICKMAP_FF_WD_RETCOLORS[stickmapRetVal][1],
					              STICKMAP_FF_WD_RETVALS[stickmapRetVal]);
					break;
				case SHIELDDROP:
					setCursorPos(5, 2);
					printStr("Shield Drop");
					setCursorPos(7, 2);
					if (selectedStickmapSub == 0) {
						printStr("ALL");
					} else {
						printStrColor(STICKMAP_SHIELDDROP_RETCOLORS[selectedStickmapSub][0], STICKMAP_SHIELDDROP_RETCOLORS[selectedStickmapSub][1],
						              STICKMAP_SHIELDDROP_RETVALS[selectedStickmapSub]);
					}
					setCursorPos(15, 2);
					printStrColor(STICKMAP_SHIELDDROP_RETCOLORS[stickmapRetVal][0], STICKMAP_SHIELDDROP_RETCOLORS[stickmapRetVal][1],
					              STICKMAP_SHIELDDROP_RETVALS[stickmapRetVal]);
					break;
				case NONE:
				default:
					setCursorPos(5, 2);
					printStr("NONE");
					setCursorPos(7, 2);
					printStr("N/A");
					setCursorPos(15, 2);
					printStr("N/A");
					break;
			}
			
			// calculate screen coordinates for stick position drawing
			int xfbCoordX = (stickMelee.stickXUnit / 125) * 2;
			if (stickRaw.stickX < 0) {
				xfbCoordX *= -1;
			}
			xfbCoordX += COORD_CIRCLE_CENTER_X;
			
			int xfbCoordY = (stickMelee.stickYUnit / 125) * 2;
			if (stickRaw.stickY > 0) {
				xfbCoordY *= -1;
			}
			xfbCoordY += SCREEN_POS_CENTER_Y;
			
			int xfbCoordCX = (stickMelee.cStickXUnit / 125) * 2;
			if (stickRaw.cStickX < 0) {
				xfbCoordCX *= -1;
			}
			xfbCoordCX += COORD_CIRCLE_CENTER_X;
			
			int xfbCoordCY = (stickMelee.cStickYUnit / 125) * 2;
			if (stickRaw.cStickY > 0) {
				xfbCoordCY *= -1;
			}
			xfbCoordCY += SCREEN_POS_CENTER_Y;
			
			changeLoadedTexmap(TEXMAP_STICKOUTLINE);
			setDepthForDrawCall(-15);
			drawTextureFullScaled(COORD_CIRCLE_CENTER_X - 164, SCREEN_POS_CENTER_Y - 164,
			                      COORD_CIRCLE_CENTER_X + 163, SCREEN_POS_CENTER_Y + 163,
			                      GX_COLOR_WHITE);
			
			drawStickmapOverlay(selectedStickmap, selectedStickmapSub);
			
			// draw analog stick line
			drawLine(COORD_CIRCLE_CENTER_X, SCREEN_POS_CENTER_Y, xfbCoordX, xfbCoordY, GX_COLOR_WHITE);
			drawBox(xfbCoordX - 4, xfbCoordY - 4, xfbCoordX + 4, xfbCoordY + 4, GX_COLOR_WHITE);
			
			// draw c-stick line
			drawLine(COORD_CIRCLE_CENTER_X, SCREEN_POS_CENTER_Y, xfbCoordCX, xfbCoordCY, GX_COLOR_YELLOW);
			drawSolidBox(xfbCoordCX - 2, xfbCoordCY - 2, xfbCoordCX + 2, xfbCoordCY + 2, GX_COLOR_YELLOW);
			
			if (!menuLockEnabled) {
				if (*pressed & PAD_TRIGGER_Z && menuState) {
					menuState = COORD_VIEW_INSTRUCTIONS;
				}
			}
			
			break;
		case COORD_VIEW_INSTRUCTIONS:
			displayInstructions();
			break;
	}
	
	if (*pressed == PAD_BUTTON_RIGHT) {
		selectedStickmap++;
		selectedStickmapSub = 0;
		if (selectedStickmap == 3) {
			selectedStickmap = 0;
		}
	} else if (*pressed == PAD_BUTTON_UP) {
		selectedStickmapSub++;
		switch (selectedStickmap) {
			case (FF_WD):
				if (selectedStickmapSub == STICKMAP_FF_WD_ENUM_LEN) {
					selectedStickmapSub = 0;
				}
				break;
			case (SHIELDDROP):
				if (selectedStickmapSub == STICKMAP_SHIELDDROP_ENUM_LEN) {
					selectedStickmapSub = 0;
				}
				break;
			case (NONE):
			default:
				selectedStickmapSub = 0;
				break;
		}
	}
	
	fontButtonFlashIncrement(&dpadFlashCounter, 30);
}

void menu_coordViewEnd() {
	// not sure if this is actually useful, maybe get rid of it...
	menuState = COORD_VIEW_SETUP;
}

void menu_coordViewSetLockState(bool state) {
	menuLockEnabled = state;
}
