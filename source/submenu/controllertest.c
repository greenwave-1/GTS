//
// Created on 8/26/25.
//

#include "submenu/controllertest.h"

#include <stdint.h>

#include <ogc/pad.h>
#include <ogc/color.h>

#include "gx.h"
#include "submenu/controllertest_constants.h"
#include "waveform.h"
#include "polling.h"
#include "print.h"

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
			
			setCursorPos(17,2);
			printStr("Analog L: %d", PAD_TriggerL(0));
			if (*held & PAD_TRIGGER_L) {
				setCursorPos(18, 2);
				printStr("Digital L Pressed");
			}
			
			setCursorPos(17,44);
			printStr("Analog R: %d", PAD_TriggerR(0));
			if (*held & PAD_TRIGGER_R) {
				setCursorPos(18, 40);
				printStr("Digital R Pressed");
			}
			
			updateVtxDesc(VTX_TEX_NOCOLOR, GX_REPLACE);
			
			changeLoadedTexmap(TEXMAP_CONTROLLER);
			
			// buttons
			// A
			int texOffsetX = TEX_A_OFFSET_X;
			if (*held & PAD_BUTTON_A) {
				texOffsetX += TEX_NORMAL_DIMENSIONS;
			}
			
			GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
			
			GX_Position3s16(LAYOUT_A_POS_X, LAYOUT_A_POS_Y, -4);
			GX_TexCoord2s16(texOffsetX, TEX_A_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_A_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_A_POS_Y, -4);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS, TEX_A_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_A_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_A_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS,
			                TEX_A_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_A_POS_X,
			                LAYOUT_A_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_TexCoord2s16(texOffsetX,
			                TEX_A_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			// B
			texOffsetX = TEX_B_OFFSET_X;
			if (*held & PAD_BUTTON_B) {
				texOffsetX += TEX_NORMAL_DIMENSIONS;
			}
			
			GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
			
			GX_Position3s16(LAYOUT_B_POS_X, LAYOUT_B_POS_Y, -4);
			GX_TexCoord2s16(texOffsetX, TEX_B_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_B_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_B_POS_Y, -4);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS, TEX_B_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_B_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_B_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS,
			                TEX_B_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_B_POS_X,
			                LAYOUT_B_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_TexCoord2s16(texOffsetX,
			                TEX_B_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			// X
			texOffsetX = TEX_X_OFFSET_X;
			if (*held & PAD_BUTTON_X) {
				texOffsetX += TEX_NORMAL_DIMENSIONS;
			}
			
			GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
			
			GX_Position3s16(LAYOUT_X_POS_X, LAYOUT_X_POS_Y, -4);
			GX_TexCoord2s16(texOffsetX, TEX_X_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_X_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_X_POS_Y, -4);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS, TEX_X_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_X_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_X_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS,
			                TEX_X_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_X_POS_X,
			                LAYOUT_X_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_TexCoord2s16(texOffsetX,
			                TEX_X_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			// Y
			texOffsetX = TEX_Y_OFFSET_X;
			if (*held & PAD_BUTTON_Y) {
				texOffsetX += TEX_NORMAL_DIMENSIONS;
			}
			
			GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
			
			GX_Position3s16(LAYOUT_Y_POS_X, LAYOUT_Y_POS_Y, -4);
			GX_TexCoord2s16(texOffsetX, TEX_Y_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_Y_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_Y_POS_Y, -4);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS, TEX_Y_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_Y_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_Y_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS,
			                TEX_Y_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_Y_POS_X,
			                LAYOUT_Y_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_TexCoord2s16(texOffsetX,
			                TEX_Y_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			// Z
			texOffsetX = TEX_Z_OFFSET_X;
			if (*held & PAD_TRIGGER_Z) {
				texOffsetX += TEX_NORMAL_DIMENSIONS;
				PAD_ControlMotor(0, PAD_MOTOR_RUMBLE);
			} else {
				PAD_ControlMotor(0, PAD_MOTOR_STOP);
			}
			
			GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
			
			GX_Position3s16(LAYOUT_Z_POS_X, LAYOUT_Z_POS_Y, -4);
			GX_TexCoord2s16(texOffsetX, TEX_Z_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_Z_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_Z_POS_Y, -4);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS, TEX_Z_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_Z_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_Z_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS,
			                TEX_Z_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_Z_POS_X,
			                LAYOUT_Z_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_TexCoord2s16(texOffsetX,
			                TEX_Z_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			// Start
			texOffsetX = TEX_START_OFFSET_X;
			if (*held & PAD_BUTTON_START) {
				texOffsetX += TEX_NORMAL_DIMENSIONS;
			}
			
			GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
			
			GX_Position3s16(LAYOUT_START_POS_X, LAYOUT_START_POS_Y, -4);
			GX_TexCoord2s16(texOffsetX, TEX_START_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_START_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_START_POS_Y, -4);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS, TEX_START_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_START_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_START_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS,
			                TEX_START_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_START_POS_X,
			                LAYOUT_START_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_TexCoord2s16(texOffsetX,
			                TEX_START_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			// d-pad
			int dpadPressedIndex = 1;
			int dpadPressedOffsets[5];
			dpadPressedOffsets[0] = TEX_DPAD_OFFSET_X;
			
			if (*held & PAD_BUTTON_UP) {
				dpadPressedOffsets[dpadPressedIndex] = TEX_DPAD_UP_OFFSET_X;
				dpadPressedIndex++;
			}
			if (*held & PAD_BUTTON_RIGHT) {
				dpadPressedOffsets[dpadPressedIndex] = TEX_DPAD_RIGHT_OFFSET_X;
				dpadPressedIndex++;
			}
			if (*held & PAD_BUTTON_LEFT) {
				dpadPressedOffsets[dpadPressedIndex] = TEX_DPAD_LEFT_OFFSET_X;
				dpadPressedIndex++;
			}
			if (*held & PAD_BUTTON_DOWN) {
				dpadPressedOffsets[dpadPressedIndex] = TEX_DPAD_DOWN_OFFSET_X;
				dpadPressedIndex++;
			}
			
			for (int i = 0; i < dpadPressedIndex; i++) {
				texOffsetX = dpadPressedOffsets[i];
				
				GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
				
				GX_Position3s16(LAYOUT_DPAD_POS_X, LAYOUT_DPAD_POS_Y, -4);
				GX_TexCoord2s16(texOffsetX, TEX_DPAD_OFFSET_Y);
				
				GX_Position3s16(LAYOUT_DPAD_POS_X + TEX_NORMAL_DIMENSIONS,
				                LAYOUT_DPAD_POS_Y, -4);
				GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS, TEX_DPAD_OFFSET_Y);
				
				GX_Position3s16(LAYOUT_DPAD_POS_X + TEX_NORMAL_DIMENSIONS,
				                LAYOUT_DPAD_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
				GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS,
				                TEX_DPAD_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
				
				GX_Position3s16(LAYOUT_DPAD_POS_X,
				                LAYOUT_DPAD_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
				GX_TexCoord2s16(texOffsetX,
				                TEX_DPAD_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
				
				GX_End();
			}
			
			// triggers
			// L
			int sliderBottomY = LAYOUT_ANALOG_SLIDER_POS_Y + 255;
			int sliderTopY = sliderBottomY - PAD_TriggerL(0);
			GX_SetLineWidth(24, GX_TO_ZERO);
			
			GXColor sliderColor = GX_COLOR_BLUE;
			if (*held & PAD_TRIGGER_L) {
				sliderColor = GX_COLOR_RED;
				GX_SetLineWidth(32, GX_TO_ZERO);
			}
			
			updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
			
			GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
			
			GX_Position3s16(LAYOUT_ANALOG_SLIDER_POS_X, sliderTopY, -3);
			GX_Color3u8(sliderColor.r, sliderColor.g, sliderColor.b);
			
			GX_Position3s16(LAYOUT_ANALOG_SLIDER_POS_X + 16, sliderTopY, -3);
			GX_Color3u8(sliderColor.r, sliderColor.g, sliderColor.b);
			
			GX_Position3s16(LAYOUT_ANALOG_SLIDER_POS_X + 16, sliderBottomY, -3);
			GX_Color3u8(sliderColor.r, sliderColor.g, sliderColor.b);
			
			GX_Position3s16(LAYOUT_ANALOG_SLIDER_POS_X, sliderBottomY, -3);
			GX_Color3u8(sliderColor.r, sliderColor.g, sliderColor.b);
			
			GX_End();
			
			GX_Begin(GX_LINESTRIP, GX_VTXFMT0, 5);
			
			GX_Position3s16(LAYOUT_ANALOG_SLIDER_POS_X, LAYOUT_ANALOG_SLIDER_POS_Y, -2);
			GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
			
			GX_Position3s16(LAYOUT_ANALOG_SLIDER_POS_X + 16, LAYOUT_ANALOG_SLIDER_POS_Y, -2);
			GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
			
			GX_Position3s16(LAYOUT_ANALOG_SLIDER_POS_X + 16, sliderBottomY, -2);
			GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
			
			GX_Position3s16(LAYOUT_ANALOG_SLIDER_POS_X, sliderBottomY, -2);
			GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
			
			GX_Position3s16(LAYOUT_ANALOG_SLIDER_POS_X, LAYOUT_ANALOG_SLIDER_POS_Y, -2);
			GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
			
			GX_End();
			
			GX_SetLineWidth(24, GX_TO_ZERO);
			
			// R
			sliderTopY = sliderBottomY - PAD_TriggerR(0);
			
			sliderColor = GX_COLOR_BLUE;
			if (*held & PAD_TRIGGER_R) {
				sliderColor = GX_COLOR_RED;
				GX_SetLineWidth(32, GX_TO_ZERO);
			}
			
			GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
			
			GX_Position3s16(640 - 16 - LAYOUT_ANALOG_SLIDER_POS_X, sliderTopY, -3);
			GX_Color3u8(sliderColor.r, sliderColor.g, sliderColor.b);
			
			GX_Position3s16(640 - LAYOUT_ANALOG_SLIDER_POS_X, sliderTopY, -3);
			GX_Color3u8(sliderColor.r, sliderColor.g, sliderColor.b);
			
			GX_Position3s16(640 - LAYOUT_ANALOG_SLIDER_POS_X, sliderBottomY, -3);
			GX_Color3u8(sliderColor.r, sliderColor.g, sliderColor.b);
			
			GX_Position3s16(640 - 16 - LAYOUT_ANALOG_SLIDER_POS_X, sliderBottomY, -3);
			GX_Color3u8(sliderColor.r, sliderColor.g, sliderColor.b);
			
			GX_End();
			
			GX_Begin(GX_LINESTRIP, GX_VTXFMT0, 5);
			
			GX_Position3s16(640 - 16 - LAYOUT_ANALOG_SLIDER_POS_X, LAYOUT_ANALOG_SLIDER_POS_Y, -2);
			GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
			
			GX_Position3s16(640 - LAYOUT_ANALOG_SLIDER_POS_X, LAYOUT_ANALOG_SLIDER_POS_Y, -2);
			GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
			
			GX_Position3s16(640 - LAYOUT_ANALOG_SLIDER_POS_X, sliderBottomY, -2);
			GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
			
			GX_Position3s16(640 - 16 - LAYOUT_ANALOG_SLIDER_POS_X, sliderBottomY, -2);
			GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
			
			GX_Position3s16(640 - 16 - LAYOUT_ANALOG_SLIDER_POS_X, LAYOUT_ANALOG_SLIDER_POS_Y, -2);
			GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
			
			GX_End();
			
			GX_SetLineWidth(12, GX_TO_ZERO);
			
			updateVtxDesc(VTX_TEX_NOCOLOR, GX_REPLACE);
			
			// stick gate
			GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
			
			GX_Position3s16(LAYOUT_ASTICK_GATE_POS_X, LAYOUT_ASTICK_GATE_POS_Y, -2);
			GX_TexCoord2s16(TEX_ASTICK_GATE_OFFSET_X, TEX_ASTICK_GATE_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_ASTICK_GATE_POS_X + TEX_ASTICK_GATE_DIMENSIONS,
			                LAYOUT_ASTICK_GATE_POS_Y, -2);
			GX_TexCoord2s16(TEX_ASTICK_GATE_OFFSET_X + TEX_ASTICK_GATE_DIMENSIONS, TEX_ASTICK_GATE_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_ASTICK_GATE_POS_X + TEX_ASTICK_GATE_DIMENSIONS,
			                LAYOUT_ASTICK_GATE_POS_Y + TEX_ASTICK_GATE_DIMENSIONS, -2);
			GX_TexCoord2s16(TEX_ASTICK_GATE_OFFSET_X + TEX_ASTICK_GATE_DIMENSIONS,
			                TEX_ASTICK_GATE_OFFSET_Y + TEX_ASTICK_GATE_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_ASTICK_GATE_POS_X,
			                LAYOUT_ASTICK_GATE_POS_Y + TEX_ASTICK_GATE_DIMENSIONS, -2);
			GX_TexCoord2s16(TEX_ASTICK_GATE_OFFSET_X,
			                TEX_ASTICK_GATE_OFFSET_Y + TEX_ASTICK_GATE_DIMENSIONS);
			
			GX_End();
			
			// stick cap
			int stickModX = PAD_StickX(0) / 2;
			int stickModY = PAD_StickY(0) / 2;
			
			GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
			
			GX_Position3s16(LAYOUT_ASTICK_CAP_POS_X + stickModX, LAYOUT_ASTICK_CAP_POS_Y - stickModY, -2);
			GX_TexCoord2s16(TEX_ASTICK_CAP_OFFSET_X, TEX_ASTICK_CAP_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_ASTICK_CAP_POS_X + TEX_NORMAL_DIMENSIONS + stickModX,
			                LAYOUT_ASTICK_CAP_POS_Y - stickModY, -2);
			GX_TexCoord2s16(TEX_ASTICK_CAP_OFFSET_X + TEX_NORMAL_DIMENSIONS, TEX_ASTICK_CAP_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_ASTICK_CAP_POS_X + TEX_NORMAL_DIMENSIONS + stickModX,
			                LAYOUT_ASTICK_CAP_POS_Y + TEX_NORMAL_DIMENSIONS - stickModY, -2);
			GX_TexCoord2s16(TEX_ASTICK_CAP_OFFSET_X + TEX_NORMAL_DIMENSIONS,
			                TEX_ASTICK_CAP_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_ASTICK_CAP_POS_X + stickModX,
			                LAYOUT_ASTICK_CAP_POS_Y + TEX_NORMAL_DIMENSIONS - stickModY, -2);
			GX_TexCoord2s16(TEX_ASTICK_CAP_OFFSET_X,
			                TEX_ASTICK_CAP_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			// c-stick gate
			GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X, LAYOUT_CSTICK_POS_Y, -2);
			GX_TexCoord2s16(TEX_CSTICK_GATE_OFFSET_X, TEX_CSTICK_GATE_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_CSTICK_POS_Y, -2);
			GX_TexCoord2s16(TEX_CSTICK_GATE_OFFSET_X + TEX_NORMAL_DIMENSIONS, TEX_CSTICK_GATE_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_CSTICK_POS_Y + TEX_NORMAL_DIMENSIONS, -2);
			GX_TexCoord2s16(TEX_CSTICK_GATE_OFFSET_X + TEX_NORMAL_DIMENSIONS,
			                TEX_CSTICK_GATE_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X,
			                LAYOUT_CSTICK_POS_Y + TEX_NORMAL_DIMENSIONS, -2);
			GX_TexCoord2s16(TEX_CSTICK_GATE_OFFSET_X,
			                TEX_CSTICK_GATE_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			
			// c-stick cap
			int cStickModX = PAD_SubStickX(0) / 2;
			int cStickModY = PAD_SubStickY(0) / 2;
			
			GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X + cStickModX, LAYOUT_CSTICK_POS_Y - cStickModY - 1, -2);
			GX_TexCoord2s16(TEX_CSTICK_CAP_OFFSET_X, TEX_CSTICK_CAP_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X + TEX_NORMAL_DIMENSIONS + cStickModX,
			                LAYOUT_CSTICK_POS_Y - cStickModY - 1, -2);
			GX_TexCoord2s16(TEX_CSTICK_CAP_OFFSET_X + TEX_NORMAL_DIMENSIONS, TEX_CSTICK_CAP_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X + TEX_NORMAL_DIMENSIONS + cStickModX,
			                LAYOUT_CSTICK_POS_Y + TEX_NORMAL_DIMENSIONS - cStickModY - 1, -2);
			GX_TexCoord2s16(TEX_CSTICK_CAP_OFFSET_X + TEX_NORMAL_DIMENSIONS,
			                TEX_CSTICK_CAP_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X + cStickModX,
			                LAYOUT_CSTICK_POS_Y + TEX_NORMAL_DIMENSIONS - cStickModY - 1, -2);
			GX_TexCoord2s16(TEX_CSTICK_CAP_OFFSET_X,
			                TEX_CSTICK_CAP_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();

			break;
	}
}

void menu_controllerTestEnd() {
	// not sure if this is actually useful, maybe get rid of it...
	//menuState = CONT_TEST_SETUP;
}
