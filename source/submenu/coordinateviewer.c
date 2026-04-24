//
// Created on 10/16/25.
//

#include "submenu/coordinateviewer.h"

#include <stdint.h>
#include <math.h>
#include <string.h>

#include <jansson.h>

#include <ogc/pad.h>

#include "util/gx.h"
#include "waveform.h"
#include "util/polling.h"
#include "util/print.h"
#include "util/stickmap.h"

static Stickmap **builtinStickmaps = NULL;
static int builtinStickmapsLen = 0;
static int selectedStickmap = 0;

static int selectedStickmapSub = 0;

static void drawStickmapOverlay(Stickmap *selection) {
	updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
	changeLoadedTexmap(TEXMAP_NONE);
	// 16 seems to be pixel-accurate, if needed
	GX_SetPointSize(20, GX_TO_ZERO);
	
	// are we drawing everything?
	if (selectedStickmapSub == 0) {
		// draw the stickmap
		for (int i = 0; i < selection->subcategoryListLen; i++) {
			StickmapSubcategory *iter = &selection->subcategoryList[i];
			// iterate over each subcategory
			GX_Begin(GX_POINTS, VTXFMT_PRIMITIVES_INT, iter->numOfCoords);
			for (int j = 0; j < iter->numOfCoords; j++) {
				GX_Position3s16((iter->coordList[j][0] * 2) + COORD_CIRCLE_CENTER_X,
				                SCREEN_POS_CENTER_Y - (iter->coordList[j][1] * 2), -9);
				GX_Color4u8(iter->color.r, iter->color.g, iter->color.b, iter->color.a);
			}
			GX_End();
		}
	} else {
		
		int start = selection->subcategoryDescList[selectedStickmapSub - 1].listIndexStart;
		int end = start;
		if (selectedStickmapSub == selection->subcategoryDescListLen) {
			end = selection->subcategoryListLen;
		} else {
			end = selection->subcategoryDescList[selectedStickmapSub].listIndexStart;
		}
		
		// iterate over the specified categories
		for (int i = start; i < end; i++) {
			StickmapSubcategory *iter = &selection->subcategoryList[i];
			
			// draw each list of points
			GX_Begin(GX_POINTS, VTXFMT_PRIMITIVES_INT, iter->numOfCoords);
			for (int j = 0; j < iter->numOfCoords; j++) {
				GX_Position3s16((iter->coordList[j][0] * 2) + COORD_CIRCLE_CENTER_X,
				                SCREEN_POS_CENTER_Y - (iter->coordList[j][1] * 2), -9);
				GX_Color4u8(iter->color.r, iter->color.g, iter->color.b, iter->color.a);
			}
			GX_End();
			
		}
	}
}

static uint16_t *pressed = NULL;
static uint16_t *held = NULL;

static enum COORD_VIEW_MENU_STATE menuState = COORD_VIEW_SETUP;

static bool menuLockEnabled = false;

static int externalJsonIndex = -1;
static ExternalStickmap *externalJsonList = NULL;
static int externalJsonListLen = 0;

static bool showDesc = false;

static void setup() {
	if (pressed == NULL) {
		pressed = getButtonsDownPtr();
		held = getButtonsHeldPtr();
	}
	
	if (builtinStickmaps == NULL) {
		builtinStickmaps = getBuiltinStickmap(STICKMAP_COORDVIEW, &builtinStickmapsLen);
	}
	
	if (externalJsonList == NULL) {
		externalJsonList = getExternalJsonList(&externalJsonListLen);
	}
	
	showDesc = false;
	
	menuState = COORD_VIEW_POST_SETUP;
	resetScrollingPrint();
}

static void displayInstructions() {
	startScrollingPrint(40, 70, 600, 400, GX_COLOR_WHITE);
	setWordWrap(true);
	printStr("Move either the Analog Stick");
	drawFontButton(FONT_STICK_A);
	printStr("or the C-Stick");
	drawFontButton(FONT_STICK_C);
	printStr("to show its corresponding position on the Melee coordinate stickmap.\n\n");
	
	printStr("Press");
	fontButtonSetDpadDirections(FONT_DPAD_LEFT | FONT_DPAD_RIGHT);
	drawFontButton(FONT_DPAD);
	printStr("to change the overall coordinate category, and");
	fontButtonSetDpadDirections(FONT_DPAD_UP | FONT_DPAD_DOWN);
	drawFontButton(FONT_DPAD);
	printStr("to change what subset of coordinates are shown. Melee "
	         "Coordinates are shown on the left.\n\n");
	
	printStr("The white line shows the analog stick's position, and the "
	         "yellow line shows the c-stick's position.\n\n");
			 
	printStr("Press A");
	drawFontButton(FONT_A);
	printStr("to \'hold' the stick\'s current position. This allows "
			 "the \'Result\' window to scroll with the\nAnalog Stick");
	drawFontButton(FONT_STICK_A);
	printStr(".\n\n");
	
	printStr("Press L");
	drawFontButton(FONT_L);
	printStr("and Z");
	drawFontButton(FONT_Z);
	printStr("together to view information on the current stickmap.\n\n");
	
	printStr("Press L");
	drawFontButton(FONT_L);
	printStr("and A");
	drawFontButton(FONT_A);
	printStr("together to load a new stickmap list from a file. "
			 "These should be placed in /gts/stickmaps/ and be in JSON format.\n\n"
			 "Stickmaps exported from \"Altimor\'s Stickmap\" are supported, "
			 "as well as an extended format, check the GitHub for an example.\n\n");
	
	printStr("Hold Start");
	drawFontButton(FONT_START);
	printStr("'lock' the menu. This disables the "
			 "instructions page (Z");
	drawFontButton(FONT_Z);
	printStr(") and exiting (B");
	drawFontButton(FONT_B);
	printStr(")");
	setWordWrap(false);
	endScrollingPrint();
	
	if (isControllerConnected(CONT_PORT_1)) {
		setCursorPos(0, 31);
		printStr("Close Instructions (Z");
		drawFontButton(FONT_Z);
		printStr(")");
	}
	
	if ((*pressed == PAD_TRIGGER_Z && *held == PAD_TRIGGER_Z) || menuLockEnabled) {
		menuState = COORD_VIEW_POST_SETUP;
	}
}

static bool holdCoordinate = false;
static int dpadFlashCounter = 0;
static ControllerSample stickRaw;
static MeleeCoordinates stickMelee;

static int8_t xOffset = 0;
static int8_t yOffset = 0;

// coordinate viewer submenu
// draws melee coordinates for both sticks on a circle
// "overlays" can be toggled to show specific coordinate groups (shield drop, for example)
void menu_coordView() {
	
	// which stickmap array are we dealing with?
	Stickmap **displayList;
	if (externalJsonIndex == -1) {
		displayList = builtinStickmaps;
		if (selectedStickmap > builtinStickmapsLen) {
			selectedStickmap = 0;
		}
	} else {
		displayList = externalJsonList[externalJsonIndex].stickmapArr;
		if (selectedStickmap > externalJsonListLen) {
			selectedStickmap = 0;
		}
	}
	
	// sanity check, just to be sure...
	if (selectedStickmap != 0) {
		if (selectedStickmapSub > displayList[selectedStickmap - 1]->subcategoryDescListLen) {
			selectedStickmapSub = 0;
		}
	}
	
	int displayListLen = 0;
	if (externalJsonIndex != -1) {
		displayListLen = externalJsonList[externalJsonIndex].stickmapArrLen;
	} else {
		displayListLen = builtinStickmapsLen;
	}
	
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
				setCursorPos(1, 35);
				printStr("Load JSON (L");
				drawFontButton(FONT_L);
				printStr("+A");
				drawFontButton(FONT_A);
				printStr(")");
			}
			setCursorPos(2, 0);
			printStr("List (L");
			drawFontButton(FONT_L);
			printStr("+Z");
			drawFontButton(FONT_Z);
			
			printStr("):");
			setCursorPos(3, 2);
			if (externalJsonIndex == -1) {
				printStr("Built-in");
			} else {
				if (strlen(externalJsonList[externalJsonIndex].fileName) >= 22) {
					printStr("%.*s...", 22, externalJsonList[externalJsonIndex].fileName);
				} else {
					printStr("%s", externalJsonList[externalJsonIndex].fileName);
				}
			}
			
			if (!holdCoordinate) {
				// get raw stick values
				stickRaw.stickX = PAD_StickX(0), stickRaw.stickY = PAD_StickY(0);
				stickRaw.cStickX = PAD_SubStickX(0), stickRaw.cStickY = PAD_SubStickY(0);

				// override values if port 1 is disconnected and 4 is connected
				if (isControllerConnected(CONT_PORT_4) && !isControllerConnected(CONT_PORT_1)) {
					stickRaw.stickX = xOffset;
					stickRaw.stickY = yOffset;
				}
				
				// get converted stick values
				stickMelee = convertStickRawToMelee(stickRaw);
			} else {
				setCursorPos(2, 25);
				printStrColor(GX_COLOR_WHITE, GX_COLOR_BLACK, "HOLDING");
			}
			
			int index = -1;
			if (selectedStickmap != 0) {
				index = getCoordSubcategory(stickMelee, displayList[selectedStickmap - 1]);
			}

			// coordinate conversion test
			if (!isControllerConnected(CONT_PORT_1) && isControllerConnected(CONT_PORT_4)) {
				setCursorPos(9, 0);
				printStr("DEBUG Raw->Melee (");
				fontButtonSetDpadDirections(FONT_DPAD_UP | FONT_DPAD_DOWN | FONT_DPAD_LEFT | FONT_DPAD_RIGHT);
				drawFontButton(FONT_DPAD);

				printStr("):");
				setCursorPos(10, 2);
				printStr("(%4d,%4d)->\n  (%4d,%4d)", stickRaw.stickX, stickRaw.stickY, stickMelee.stickX, stickMelee.stickY);
				setCursorPos(12, 2);
				printStr("M: %4.1f A: %4.1f", stickMelee.stickMagnitude, stickMelee.stickAngle);
			} else {
				// print melee coordinates
				setCursorPos(9, 0);
				printStr("Analog Stick:");
				setCursorPos(10, 2);
				printStr("(%s)", getMeleeCoordinateString(stickMelee, AXIS_AXY));

				setCursorPos(11, 0);
				printStr("C-Stick:");
				setCursorPos(12, 2);
				printStr("(%s)", getMeleeCoordinateString(stickMelee, AXIS_CXY));
			}
			
			setCursorPos(4, 0);
			printStr("Stickmap %2d/%2d (", selectedStickmap, displayListLen);
			fontButtonSetDpadDirections(FONT_DPAD_LEFT | FONT_DPAD_RIGHT);
			drawFontButton(FONT_DPAD);
			printStr("):");
			setPrintOffset(4);
			setCursorPos(5, 2);
			if (selectedStickmap != 0) {
				int stringLen = strlen(displayList[selectedStickmap - 1]->name);
				if (stringLen < 20) {
					printStr(displayList[selectedStickmap - 1]->name);
				} else {
					printStr("%.*s...", 17, displayList[selectedStickmap - 1]->name);
				}
			} else {
				printStr("None");
			}

			setPrintOffset(8);
			setCursorPos(6, 0);
			
			if (selectedStickmap == 0) {
				printStr("Shown     0/ 0 (");
			} else {
				printStr("Shown    %2d/%2d (", selectedStickmapSub,
				         displayList[selectedStickmap - 1]->subcategoryDescListLen);
			}
			fontButtonSetDpadDirections(FONT_DPAD_UP | FONT_DPAD_DOWN);
			drawFontButton(FONT_DPAD);
			printStr("):");
			setPrintOffset(12);
			setCursorPos(7, 2);
			if (selectedStickmap == 0) {
				printStr("N/A");
			} else if (selectedStickmapSub == 0) {
				printStr("All");
			} else {
				int stringLen = strlen(displayList[selectedStickmap - 1]->subcategoryDescList[selectedStickmapSub - 1].name);
				if (stringLen < 20) {
					printStr(displayList[selectedStickmap - 1]->subcategoryDescList[selectedStickmapSub - 1].name);
				} else {
					printStr("%.*s...", 17, displayList[selectedStickmap - 1]->subcategoryDescList[selectedStickmapSub - 1].name);
				}
			}
			setPrintOffset(0);
			
			setPrintOffset(0);
			setCursorPos(14, 0);
			printStr("Result (A");
			drawFontButton(FONT_A);
			printStr("to Hold):");
			
			setPrintOffset(4);
			
			setCursorPos(15, 2);
			
			if (!showDesc) {
				if (index == -1 || selectedStickmap == 0) {
					printStr("Miss");
				} else {
					if (!holdCoordinate) {
						scrollingPrintFreeze(true);
					}
					startScrollingPrint(30, 300, 270, 400,
					                    displayList[selectedStickmap - 1]->subcategoryList[index].color);
					setWordWrap(true);
					printStr(displayList[selectedStickmap - 1]->subcategoryList[index].name);
					endScrollingPrint();
					setWordWrap(false);
					scrollingPrintFreeze(false);
				}
			}
			
			setPrintOffset(0);
			
			// calculate screen coordinates for stick position drawing
			int screenCoordX = (stickMelee.stickX * 2) + COORD_CIRCLE_CENTER_X;
			int screenCoordY = (stickMelee.stickY * -2) + SCREEN_POS_CENTER_Y;
			
			int screenCoordCX = (stickMelee.cStickX * 2) + COORD_CIRCLE_CENTER_X;
			int screenCoordCY = (stickMelee.cStickY * -2) + SCREEN_POS_CENTER_Y;
			
			changeLoadedTexmap(TEXMAP_STICKOUTLINE);
			setDepthForDrawCall(-15);
			drawTextureFullScaled(COORD_CIRCLE_CENTER_X - 164, SCREEN_POS_CENTER_Y - 164,
			                      COORD_CIRCLE_CENTER_X + 163, SCREEN_POS_CENTER_Y + 163,
			                      GX_COLOR_GRAY);
			
			if (selectedStickmap != 0) {
				drawStickmapOverlay(displayList[selectedStickmap - 1]);
			}
			
			// draw analog stick line
			drawLine(COORD_CIRCLE_CENTER_X, SCREEN_POS_CENTER_Y, screenCoordX, screenCoordY, GX_COLOR_WHITE);
			drawBox(screenCoordX - 4, screenCoordY - 4, screenCoordX + 4, screenCoordY + 4, GX_COLOR_WHITE);
			
			// draw c-stick line
			drawLine(COORD_CIRCLE_CENTER_X, SCREEN_POS_CENTER_Y, screenCoordCX, screenCoordCY, GX_COLOR_YELLOW);
			drawSolidBox(screenCoordCX - 2, screenCoordCY - 2, screenCoordCX + 2, screenCoordCY + 2, GX_COLOR_YELLOW);
			
			if (showDesc) {
				startScrollingPrint(100, 100, 540, 350, GX_COLOR_WHITE);
				// clear screen in new bounds
				setDepthForDrawCall(0);
				drawSolidBox(0, 0, 640, 4800, GX_COLOR_BLACK);
				setWordWrap(true);
				printStr("Current List:\n - ");
				if (externalJsonIndex == -1) {
					printStr("Built-in\n\n");
				} else {
					printStr("%s\n\n", externalJsonList[externalJsonIndex].fileName);
				}
				printStr("Current Stickmap (");
				fontButtonSetDpadDirections(FONT_DPAD_LEFT | FONT_DPAD_RIGHT);
				drawFontButton(FONT_DPAD);
				printStr("):\n - ");
				if (selectedStickmap == 0) {
					printStr("None\n");
				} else {
					printStr("%s\n", displayList[selectedStickmap - 1]->name);
					printStr("\nDescription:\n - ");
					printStr("%s\n\n", displayList[selectedStickmap - 1]->desc);
					printStr("Zones:\n");
					for (int i = 0; i < displayList[selectedStickmap - 1]->subcategoryDescListLen; i++) {
						printStr(" - %s",
						         displayList[selectedStickmap - 1]->subcategoryDescList[i].name);
						
						if (displayList[selectedStickmap - 1]->subcategoryDescList[i].desc != NULL) {
							printStr(": %s", displayList[selectedStickmap - 1]->subcategoryDescList[i].desc);
						}
						printStr("\n\n");
					}
				}
				setWordWrap(false);
				endScrollingPrint();
			}
			
			if (!menuLockEnabled) {
				if (*pressed == PAD_TRIGGER_Z && *held == PAD_TRIGGER_Z) {
					menuState = COORD_VIEW_INSTRUCTIONS;
					showDesc = false;
				}
			}
			
			break;
		case COORD_VIEW_FILE_PICKER:
			if (externalJsonIndex == -1) {
				if (isControllerConnected(CONT_PORT_1)) {
					setCursorPos(1, 27);
					printStr("Close File Picker (L");
					drawFontButton(FONT_L);
					printStr("+A");
					drawFontButton(FONT_A);
					printStr(")");
				}
				externalJsonIndex = drawJsonFilePicker(externalJsonList);
			} else {
				menuState = COORD_VIEW_POST_SETUP;
				selectedStickmap = 1;
				selectedStickmapSub = 0;
			}
			break;
		case COORD_VIEW_INSTRUCTIONS:
			displayInstructions();
			break;
	}
	
	if (menuState != COORD_VIEW_FILE_PICKER) {
		// cycle stickmap
		if (*pressed == PAD_BUTTON_LEFT) {
			selectedStickmapSub = 0;
			
			if (selectedStickmap == 0) {
				selectedStickmap = displayListLen;
			} else {
				selectedStickmap--;
			}
		} else if (*pressed == PAD_BUTTON_RIGHT) {
			selectedStickmapSub = 0;
			
			selectedStickmap++;
			if (selectedStickmap == displayListLen + 1) {
				selectedStickmap = 0;
			}
		}
		
		// cycle stickmap categories
		else if (*pressed == PAD_BUTTON_UP) {
			if (selectedStickmap != 0) {
				selectedStickmapSub++;
				if (selectedStickmapSub == displayList[selectedStickmap - 1]->subcategoryDescListLen + 1) {
					selectedStickmapSub = 0;
				}
			}
		} else if (*pressed == PAD_BUTTON_DOWN) {
			if (selectedStickmap != 0) {
				selectedStickmapSub--;
				if (selectedStickmapSub == -1) {
					selectedStickmapSub = displayList[selectedStickmap - 1]->subcategoryDescListLen;
				}
			}
		}
		
		// "freeze" currently held coordinate
		if (*pressed == PAD_BUTTON_A && *held == PAD_BUTTON_A && menuState == COORD_VIEW_POST_SETUP) {
			holdCoordinate = !holdCoordinate;
		}

		// coordinate conversion test
		if (isControllerConnected(CONT_PORT_4) && !isControllerConnected(CONT_PORT_1)) {
			uint16_t p4Buttons = PAD_ButtonsDown(3), p4Held = PAD_ButtonsHeld(3);

			bool moveSingleUnit = (p4Held & PAD_TRIGGER_L);

			// modifying X
			if (p4Held & PAD_BUTTON_X) {
				if ((p4Buttons & PAD_BUTTON_RIGHT && moveSingleUnit) || (p4Held & PAD_BUTTON_RIGHT && !moveSingleUnit)) {
					xOffset++;
				}
				else if ((p4Buttons & PAD_BUTTON_LEFT && moveSingleUnit) || (p4Held & PAD_BUTTON_LEFT && !moveSingleUnit)) {
					xOffset--;
				}
			}
			// modifying Y
			else if (p4Held & PAD_BUTTON_Y) {
				if ((p4Buttons & PAD_BUTTON_UP && moveSingleUnit) || (p4Held & PAD_BUTTON_UP && !moveSingleUnit)) {
					yOffset++;
				}
				else if ((p4Buttons & PAD_BUTTON_DOWN && moveSingleUnit) || (p4Held & PAD_BUTTON_DOWN && !moveSingleUnit)) {
					yOffset--;
				}
			} else {
				if ((p4Buttons & PAD_BUTTON_RIGHT && moveSingleUnit) || (p4Held & PAD_BUTTON_RIGHT && !moveSingleUnit)) {
					xOffset++;
				}
				else if ((p4Buttons & PAD_BUTTON_LEFT && moveSingleUnit) || (p4Held & PAD_BUTTON_LEFT && !moveSingleUnit)) {
					xOffset--;
				}
				if ((p4Buttons & PAD_BUTTON_UP && moveSingleUnit) || (p4Held & PAD_BUTTON_UP && !moveSingleUnit)) {
					yOffset++;
				}
				else if ((p4Buttons & PAD_BUTTON_DOWN && moveSingleUnit) || (p4Held & PAD_BUTTON_DOWN && !moveSingleUnit)) {
					yOffset--;
				}
			}
		}

		fontButtonFlashIncrement(&dpadFlashCounter, 30);
	}
	
	if (!menuLockEnabled) {
		// L + A -> toggle file selection
		if (*held == (PAD_BUTTON_A | PAD_TRIGGER_L) && (*pressed & (PAD_BUTTON_A | PAD_TRIGGER_L))) {
			if (menuState == COORD_VIEW_POST_SETUP) {
				menuState = COORD_VIEW_FILE_PICKER;
				selectedStickmap = 0;
				selectedStickmapSub = 0;
				externalJsonIndex = -1;
			} else if (menuState == COORD_VIEW_FILE_PICKER) {
				menuState = COORD_VIEW_POST_SETUP;
				externalJsonIndex = -1;
			}
		}
		
		// L + Z -> toggle stickmap info display
		else if (*held == (PAD_TRIGGER_L | PAD_TRIGGER_Z) && (*pressed & (PAD_TRIGGER_L | PAD_TRIGGER_Z))) {
			if (menuState == COORD_VIEW_POST_SETUP) {
				showDesc = !showDesc;
			}
		}
	}
}

void menu_coordViewEnd() {
	// not sure if this is actually useful, maybe get rid of it...
	menuState = COORD_VIEW_SETUP;
}

void menu_coordViewSetLockState(bool state) {
	menuLockEnabled = state;
}
