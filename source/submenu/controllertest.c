//
// Created on 8/26/25.
//

#include "submenu/controllertest.h"

#include <stdint.h>

#include <ogc/pad.h>
#include <ogc/color.h>

#include "polling.h"
#include "print.h"
#include "draw.h"

static uint16_t *pressed = NULL;
static uint16_t *held = NULL;

static enum CONTROLLER_TEST_MENU_STATE menuState = CONT_TEST_SETUP;

static void setup() {
	if (pressed == NULL) {
		pressed = getButtonsDownPtr();
		held = getButtonsHeldPtr();
	}
	
	menuState = CONT_TEST_POST_SETUP;
}

// controller test submenu
// basic visual button and stick test
// also shows coordinates (raw and melee converted), and origin values
void menu_controllerTest() {
	switch(menuState) {
		case CONT_TEST_SETUP:
			setup();
			break;
		case CONT_TEST_POST_SETUP:
			// melee stick coordinates stuff
			// a lot of this comes from github.com/phobgcc/phobconfigtool
			
			static ControllerSample stickRaw;
			static MeleeCoordinates stickMelee;
			
			// get raw stick values
			stickRaw.stickX = PAD_StickX(0), stickRaw.stickY = PAD_StickY(0);
			stickRaw.cStickX = PAD_SubStickX(0), stickRaw.cStickY = PAD_SubStickY(0);
			
			// get converted stick values
			stickMelee = convertStickRawToMelee(stickRaw);
			
			// print raw stick coordinates
			setCursorPos(19, 0);
			printStr("Raw XY: (%04d,%04d)", stickRaw.stickX, stickRaw.stickY);
			setCursorPos(19, 38);
			printStr("C-Raw XY: (%04d,%04d)", stickRaw.cStickX, stickRaw.cStickY);
			
			// print melee coordinates
			setCursorPos(20, 0);
			printStr("Melee: (");
			// is the value negative?
			if (stickRaw.stickX < 0) {
				printStr("-");
			} else {
				printStr("0");
			}
			// is this a 1.0 value?
			if (stickMelee.stickXUnit == 10000) {
				printStr("1.0000");
			} else {
				printStr("0.%04d", stickMelee.stickXUnit);
			}
			printStr(",");
			
			// is the value negative?
			if (stickRaw.stickY < 0) {
				printStr("-");
			} else {
				printStr("0");
			}
			// is this a 1.0 value?
			if (stickMelee.stickYUnit == 10000) {
				printStr("1.0000");
			} else {
				printStr("0.%04d", stickMelee.stickYUnit);
			}
			printStr(")");
			
			setCursorPos(20, 33);
			printStr("C-Melee: (");
			// is the value negative?
			if (stickRaw.cStickX < 0) {
				printStr("-");
			} else {
				printStr("0");
			}
			// is this a 1.0 value?
			if (stickMelee.cStickXUnit == 10000) {
				printStr("1.0000");
			} else {
				printStr("0.%04d", stickMelee.cStickXUnit);
			}
			printStr(",");
			
			// is the value negative?
			if (stickRaw.cStickY < 0) {
				printStr("-");
			} else {
				printStr("0");
			}
			// is this a 1.0 value?
			if (stickMelee.cStickYUnit == 10000) {
				printStr("1.0000");
			} else {
				printStr("0.%04d", stickMelee.cStickYUnit);
			}
			printStr(")");
			
			// show origin info if controller is connected
			if (isControllerConnected(CONT_PORT_1)) {
				PADStatus origin = getOriginStatus(CONT_PORT_1);
				setCursorPos(21, 0);
				printStr("Origin XY: (%04d,%04d)", origin.stickX, origin.stickY);
				setCursorPos(21, 35);
				printStr("C-Origin XY: (%04d,%04d)", origin.substickX, origin.substickY);
				
				if (!(*held & PAD_TRIGGER_L)) {
					setCursorPos(18, 2);
					printStr("L Origin: %d", origin.triggerL);
				}
				if (!(*held & PAD_TRIGGER_R)) {
					setCursorPos(18, 44);
					printStr("R Origin: %d", origin.triggerR);
				}
			}
			
			
			// visual stuff
			// Buttons
			
			// A
			if (*held & PAD_BUTTON_A) {
				DrawFilledBox(CONT_TEST_BUTTON_A_X1, CONT_TEST_BUTTON_A_Y1,
				              CONT_TEST_BUTTON_A_X1 + CONT_TEST_BUTTON_A_SIZE, CONT_TEST_BUTTON_A_Y1 + CONT_TEST_BUTTON_A_SIZE,
				              COLOR_WHITE);
				drawCharDirect(CONT_TEST_BUTTON_A_X1 + 12, CONT_TEST_BUTTON_A_Y1 + 8, COLOR_BLACK, 'A');
			} else {
				DrawBox(CONT_TEST_BUTTON_A_X1, CONT_TEST_BUTTON_A_Y1,
				        CONT_TEST_BUTTON_A_X1 + CONT_TEST_BUTTON_A_SIZE, CONT_TEST_BUTTON_A_Y1 + CONT_TEST_BUTTON_A_SIZE,
				        COLOR_WHITE);
				drawCharDirect(CONT_TEST_BUTTON_A_X1 + 12, CONT_TEST_BUTTON_A_Y1 + 8, COLOR_WHITE, 'A');
			}
			
			// B
			if (*held & PAD_BUTTON_B) {
				DrawFilledBox(CONT_TEST_BUTTON_B_X1, CONT_TEST_BUTTON_B_Y1,
				              CONT_TEST_BUTTON_B_X1 + CONT_TEST_BUTTON_B_SIZE, CONT_TEST_BUTTON_B_Y1 + CONT_TEST_BUTTON_B_SIZE,
				              COLOR_WHITE);
				drawCharDirect(CONT_TEST_BUTTON_B_X1 + 8, CONT_TEST_BUTTON_B_Y1 + 5, COLOR_BLACK, 'B');
			} else {
				DrawBox(CONT_TEST_BUTTON_B_X1, CONT_TEST_BUTTON_B_Y1,
				        CONT_TEST_BUTTON_B_X1 + CONT_TEST_BUTTON_B_SIZE, CONT_TEST_BUTTON_B_Y1 + CONT_TEST_BUTTON_B_SIZE,
				        COLOR_WHITE);
				drawCharDirect(CONT_TEST_BUTTON_B_X1 + 8, CONT_TEST_BUTTON_B_Y1 + 5, COLOR_WHITE, 'B');
			}
			
			// X
			if (*held & PAD_BUTTON_X) {
				DrawFilledBox(CONT_TEST_BUTTON_Z_X1, CONT_TEST_BUTTON_X_Y1,
				              CONT_TEST_BUTTON_Z_X1 + CONT_TEST_BUTTON_XY_SHORT, CONT_TEST_BUTTON_X_Y1 + CONT_TEST_BUTTON_XY_LONG,
				              COLOR_WHITE);
				drawCharDirect(CONT_TEST_BUTTON_Z_X1 + 7, CONT_TEST_BUTTON_X_Y1 + 8, COLOR_BLACK, 'X');
			} else {
				DrawBox(CONT_TEST_BUTTON_Z_X1, CONT_TEST_BUTTON_X_Y1,
				        CONT_TEST_BUTTON_Z_X1 + CONT_TEST_BUTTON_XY_SHORT, CONT_TEST_BUTTON_X_Y1 + CONT_TEST_BUTTON_XY_LONG,
				        COLOR_WHITE);
				drawCharDirect(CONT_TEST_BUTTON_Z_X1 + 7, CONT_TEST_BUTTON_X_Y1 + 8, COLOR_WHITE, 'X');
			}
			
			// Y
			if (*held & PAD_BUTTON_Y) {
				DrawFilledBox(CONT_TEST_BUTTON_A_X1, CONT_TEST_BUTTON_Y_Y1,
				              CONT_TEST_BUTTON_A_X1 + CONT_TEST_BUTTON_XY_LONG, CONT_TEST_BUTTON_Y_Y1 + CONT_TEST_BUTTON_XY_SHORT,
				              COLOR_WHITE);
				drawCharDirect(CONT_TEST_BUTTON_A_X1 + 12, CONT_TEST_BUTTON_Y_Y1 + 4, COLOR_BLACK, 'Y');
			} else {
				DrawBox(CONT_TEST_BUTTON_A_X1, CONT_TEST_BUTTON_Y_Y1,
				        CONT_TEST_BUTTON_A_X1 + CONT_TEST_BUTTON_XY_LONG, CONT_TEST_BUTTON_Y_Y1 + CONT_TEST_BUTTON_XY_SHORT,
				        COLOR_WHITE);
				drawCharDirect(CONT_TEST_BUTTON_A_X1 + 12, CONT_TEST_BUTTON_Y_Y1 + 4, COLOR_WHITE, 'Y');
			}
			
			// Z
			if (*held & PAD_TRIGGER_Z) {
				DrawFilledBox(CONT_TEST_BUTTON_Z_X1, CONT_TEST_BUTTON_Z_Y1,
				              CONT_TEST_BUTTON_Z_X1 + CONT_TEST_BUTTON_XY_SHORT, CONT_TEST_BUTTON_Z_Y1 + CONT_TEST_BUTTON_XY_SHORT,
				              COLOR_WHITE);
				drawCharDirect(CONT_TEST_BUTTON_Z_X1 + 7, CONT_TEST_BUTTON_Z_Y1 + 4, COLOR_BLACK, 'Z');
				// also enable rumble if z is held
				PAD_ControlMotor(0, PAD_MOTOR_RUMBLE);
			} else {
				DrawBox(CONT_TEST_BUTTON_Z_X1, CONT_TEST_BUTTON_Z_Y1,
				        CONT_TEST_BUTTON_Z_X1 + CONT_TEST_BUTTON_XY_SHORT, CONT_TEST_BUTTON_Z_Y1 + CONT_TEST_BUTTON_XY_SHORT,
				        COLOR_WHITE);
				drawCharDirect(CONT_TEST_BUTTON_Z_X1 + 7, CONT_TEST_BUTTON_Z_Y1 + 4, COLOR_WHITE, 'Z');
				// stop rumble if z is not *held
				PAD_ControlMotor(0, PAD_MOTOR_STOP);
			}
			
			// Start
			if (*held & PAD_BUTTON_START) {
				DrawFilledBox(CONT_TEST_BUTTON_START_X1, CONT_TEST_BUTTON_START_Y1,
				              CONT_TEST_BUTTON_START_X1 + CONT_TEST_BUTTON_START_LEN, CONT_TEST_BUTTON_START_Y1 + CONT_TEST_BUTTON_START_WIDTH,
				              COLOR_WHITE);
				// TODO: this is ugly
				drawCharDirect(CONT_TEST_BUTTON_START_X1 + 7, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_BLACK, 'S');
				drawCharDirect(CONT_TEST_BUTTON_START_X1 + 17, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_BLACK, 'T');
				drawCharDirect(CONT_TEST_BUTTON_START_X1 + 27, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_BLACK, 'A');
				drawCharDirect(CONT_TEST_BUTTON_START_X1 + 37, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_BLACK, 'R');
				drawCharDirect(CONT_TEST_BUTTON_START_X1 + 47, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_BLACK, 'T');
			} else {
				DrawBox(CONT_TEST_BUTTON_START_X1, CONT_TEST_BUTTON_START_Y1,
				        CONT_TEST_BUTTON_START_X1 + CONT_TEST_BUTTON_START_LEN, CONT_TEST_BUTTON_START_Y1 + CONT_TEST_BUTTON_START_WIDTH,
				        COLOR_WHITE);
				drawCharDirect(CONT_TEST_BUTTON_START_X1 + 7, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_WHITE, 'S');
				drawCharDirect(CONT_TEST_BUTTON_START_X1 + 17, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_WHITE, 'T');
				drawCharDirect(CONT_TEST_BUTTON_START_X1 + 27, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_WHITE, 'A');
				drawCharDirect(CONT_TEST_BUTTON_START_X1 + 37, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_WHITE, 'R');
				drawCharDirect(CONT_TEST_BUTTON_START_X1 + 47, CONT_TEST_BUTTON_START_Y1 + 5, COLOR_WHITE, 'T');
			}
			
			// DPad
			// up
			if (*held & PAD_BUTTON_UP) {
				DrawFilledBox(CONT_TEST_DPAD_UP_X1, CONT_TEST_DPAD_UP_Y1,
				              CONT_TEST_DPAD_UP_X1 + CONT_TEST_DPAD_SHORT, CONT_TEST_DPAD_UP_Y1 + CONT_TEST_DPAD_LONG,
				              COLOR_WHITE);
			} else {
				DrawBox(CONT_TEST_DPAD_UP_X1, CONT_TEST_DPAD_UP_Y1,
				        CONT_TEST_DPAD_UP_X1 + CONT_TEST_DPAD_SHORT, CONT_TEST_DPAD_UP_Y1 + CONT_TEST_DPAD_LONG,
				        COLOR_WHITE);
			}
			
			// down
			if (*held & PAD_BUTTON_DOWN) {
				DrawFilledBox(CONT_TEST_DPAD_UP_X1, CONT_TEST_DPAD_DOWN_Y1,
				              CONT_TEST_DPAD_UP_X1 + CONT_TEST_DPAD_SHORT, CONT_TEST_DPAD_DOWN_Y1 + CONT_TEST_DPAD_LONG,
				              COLOR_WHITE);
			} else {
				DrawBox(CONT_TEST_DPAD_UP_X1, CONT_TEST_DPAD_DOWN_Y1,
				        CONT_TEST_DPAD_UP_X1 + CONT_TEST_DPAD_SHORT, CONT_TEST_DPAD_DOWN_Y1 + CONT_TEST_DPAD_LONG,
				        COLOR_WHITE);
			}
			
			
			//left
			if (*held & PAD_BUTTON_LEFT) {
				DrawFilledBox(CONT_TEST_DPAD_LEFT_X1, CONT_TEST_DPAD_LEFT_Y1,
				              CONT_TEST_DPAD_LEFT_X1 + CONT_TEST_DPAD_LONG, CONT_TEST_DPAD_LEFT_Y1 + CONT_TEST_DPAD_SHORT,
				              COLOR_WHITE);
			} else {
				DrawBox(CONT_TEST_DPAD_LEFT_X1, CONT_TEST_DPAD_LEFT_Y1,
				        CONT_TEST_DPAD_LEFT_X1 + CONT_TEST_DPAD_LONG, CONT_TEST_DPAD_LEFT_Y1 + CONT_TEST_DPAD_SHORT,
				        COLOR_WHITE);
			}
			
			// right
			if (*held & PAD_BUTTON_RIGHT) {
				DrawFilledBox(CONT_TEST_DPAD_RIGHT_X1, CONT_TEST_DPAD_LEFT_Y1,
				              CONT_TEST_DPAD_RIGHT_X1 + CONT_TEST_DPAD_LONG, CONT_TEST_DPAD_LEFT_Y1 + CONT_TEST_DPAD_SHORT,
				              COLOR_WHITE);
			} else {
				DrawBox(CONT_TEST_DPAD_RIGHT_X1, CONT_TEST_DPAD_LEFT_Y1,
				        CONT_TEST_DPAD_RIGHT_X1 + CONT_TEST_DPAD_LONG, CONT_TEST_DPAD_LEFT_Y1 + CONT_TEST_DPAD_SHORT,
				        COLOR_WHITE);
			}
			
			
			// Analog L Slider
			//DrawBox(53, 69, 66, 326, COLOR_WHITE);;
			DrawBox(CONT_TEST_TRIGGER_L_X1, CONT_TEST_TRIGGER_Y1,
			        CONT_TEST_TRIGGER_L_X1 + CONT_TEST_TRIGGER_WIDTH + 1, CONT_TEST_TRIGGER_Y1 + CONT_TEST_TRIGGER_LEN + 1,
			        COLOR_WHITE);
			if (*held & PAD_TRIGGER_L) {
				DrawFilledBox(CONT_TEST_TRIGGER_L_X1 + 2, CONT_TEST_TRIGGER_Y1 + 1 + (255 - PAD_TriggerL(0)),
				              CONT_TEST_TRIGGER_L_X1 + CONT_TEST_TRIGGER_WIDTH, CONT_TEST_TRIGGER_Y1 + CONT_TEST_TRIGGER_LEN,
				              COLOR_BLUE);
			} else {
				DrawFilledBox(CONT_TEST_TRIGGER_L_X1 + 2, CONT_TEST_TRIGGER_Y1 + 1 + (255 - PAD_TriggerL(0)),
				              CONT_TEST_TRIGGER_L_X1 + CONT_TEST_TRIGGER_WIDTH, CONT_TEST_TRIGGER_Y1 + CONT_TEST_TRIGGER_LEN,
				              COLOR_RED);
			}
			
			setCursorPos(17,2);
			printStr("Analog L: %d", PAD_TriggerL(0));
			if (*held & PAD_TRIGGER_L) {
				setCursorPos(18, 2);
				printStr("Digital L Pressed");
			}
			
			// Analog R Slider
			DrawBox(CONT_TEST_TRIGGER_R_X1, CONT_TEST_TRIGGER_Y1,
			        CONT_TEST_TRIGGER_R_X1 + CONT_TEST_TRIGGER_WIDTH + 1, CONT_TEST_TRIGGER_Y1 + CONT_TEST_TRIGGER_LEN + 1,
			        COLOR_WHITE);
			if (*held & PAD_TRIGGER_R) {
				DrawFilledBox(CONT_TEST_TRIGGER_R_X1 + 2, CONT_TEST_TRIGGER_Y1 + 1 + (255 - PAD_TriggerR(0)),
				              CONT_TEST_TRIGGER_R_X1 + CONT_TEST_TRIGGER_WIDTH, CONT_TEST_TRIGGER_Y1 + CONT_TEST_TRIGGER_LEN,
				              COLOR_BLUE);
			} else {
				DrawFilledBox(CONT_TEST_TRIGGER_R_X1 + 2, CONT_TEST_TRIGGER_Y1 + 1 + (255 - PAD_TriggerR(0)),
				              CONT_TEST_TRIGGER_R_X1 + CONT_TEST_TRIGGER_WIDTH, CONT_TEST_TRIGGER_Y1 + CONT_TEST_TRIGGER_LEN,
				              COLOR_RED);
			}
			
			setCursorPos(17,44);
			printStr("Analog R: %d", PAD_TriggerR(0));
			if (*held & PAD_TRIGGER_R) {
				setCursorPos(18, 40);
				printStr("Digital R Pressed");
			}
			
			// Analog Stick
			// calculate screen coordinates for stick position drawing
			int xfbCoordX = (stickMelee.stickXUnit / 250);
			if (stickRaw.stickX < 0) {
				xfbCoordX *= -1;
			}
			xfbCoordX += CONT_TEST_STICK_CENTER_X;
			
			int xfbCoordY = (stickMelee.stickYUnit / 250);
			if (stickRaw.stickY > 0) {
				xfbCoordY *= -1;
			}
			xfbCoordY += CONT_TEST_STICK_CENTER_Y;
			
			int xfbCoordCX = (stickMelee.cStickXUnit / 250);
			if (stickRaw.cStickX < 0) {
				xfbCoordCX *= -1;
			}
			xfbCoordCX += CONT_TEST_CSTICK_CENTER_X;
			
			int xfbCoordCY = (stickMelee.cStickYUnit / 250);
			if (stickRaw.cStickY > 0) {
				xfbCoordCY *= -1;
			}
			xfbCoordCY += CONT_TEST_CSTICK_CENTER_Y;
			
			// analog stick
			// line from center
			DrawOctagonalGate(CONT_TEST_STICK_CENTER_X, CONT_TEST_STICK_CENTER_Y, 2, COLOR_GRAY);; // perimeter
			DrawLine(CONT_TEST_STICK_CENTER_X, CONT_TEST_STICK_CENTER_Y,
			         CONT_TEST_STICK_CENTER_X + (stickRaw.stickX / 2), CONT_TEST_STICK_CENTER_Y - (stickRaw.stickY / 2),
			         COLOR_SILVER);
			// smaller circles
			for (int i = CONT_TEST_STICK_RAD / 2; i > 0; i -= 5) {
				DrawCircle(CONT_TEST_STICK_CENTER_X + (stickRaw.stickX / 2), CONT_TEST_STICK_CENTER_Y - (stickRaw.stickY / 2), i, COLOR_WHITE);;
			}
			
			// c-stick
			// perimeter
			DrawOctagonalGate(CONT_TEST_CSTICK_CENTER_X, CONT_TEST_CSTICK_CENTER_Y, 2, COLOR_GRAY);;
			// line from center
			DrawLine(CONT_TEST_CSTICK_CENTER_X, CONT_TEST_CSTICK_CENTER_Y,
			         CONT_TEST_CSTICK_CENTER_X + (stickRaw.cStickX / 2), CONT_TEST_CSTICK_CENTER_Y - (stickRaw.cStickY / 2),
			         COLOR_MEDGRAY);
			// smaller circle
			DrawFilledCircle(CONT_TEST_CSTICK_CENTER_X + (stickRaw.cStickX / 2), CONT_TEST_CSTICK_CENTER_Y - (stickRaw.cStickY / 2),
			                 CONT_TEST_STICK_RAD / 2, COLOR_YELLOW);
							 
			break;
	}
}

void menu_controllerTestEnd() {
	// not sure if this is actually useful, maybe get rid of it...
	//menuState = CONT_TEST_SETUP;
}
