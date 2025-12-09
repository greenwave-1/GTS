//
// Created on 8/26/25.
//

#include "submenu/controllertest.h"

#include <stdint.h>
#include <stdlib.h>

#include <ogc/pad.h>

#include "util/gx.h"
#include "waveform.h"
#include "util/polling.h"
#include "util/print.h"

static uint16_t *pressed = NULL;
static uint16_t *held = NULL;

static bool rumbleSecret = false;
static int rumbleIndex = 0;
const static int rumbleOffsets[][2] = { {-2, -2}, {-2, 2}, {2, 2}, {2, -2} };

static enum CONTROLLER_TEST_MENU_STATE menuState = CONT_TEST_SETUP;

const static uint16_t codeInputSequence[] = { PAD_BUTTON_UP, PAD_BUTTON_UP, PAD_BUTTON_DOWN, PAD_BUTTON_DOWN,
                                               PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT, PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT,
                                               PAD_BUTTON_B, PAD_BUTTON_A, PAD_BUTTON_START };
static int codeInputSequenceIndex = 0;
static int codeInputRumbleCounter = 0;

static uint8_t originFlashCounter = 0;

static void controllerTestCheckInput() {
	uint16_t port4 = PAD_ButtonsDown(3);
	
	if (port4 != 0) {
		if (port4 == codeInputSequence[codeInputSequenceIndex]) {
			codeInputSequenceIndex++;
			codeInputRumbleCounter = 7;
		} else {
			codeInputSequenceIndex = 0;
		}
	}
	
	if (codeInputSequenceIndex == 11) {
		rumbleSecret = !rumbleSecret;
		codeInputSequenceIndex = 0;
		codeInputRumbleCounter = 60;
	}
	
	if (codeInputRumbleCounter > 0) {
		PAD_ControlMotor(3, PAD_MOTOR_RUMBLE);
		codeInputRumbleCounter--;
		if (codeInputRumbleCounter == 0) {
			PAD_ControlMotor(3, PAD_MOTOR_STOP_HARD);
		}
	}
}

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
			setCursorDepth(-10);
			printStr("Raw XY:    (%4d,%4d)", stickRaw.stickX, stickRaw.stickY);
			setCursorPos(19, 32);
			printStr("C-Raw XY:    (%4d,%4d)", stickRaw.cStickX, stickRaw.cStickY);
			
			// print melee coordinates
			setCursorPos(20, 0);
			printStr("Melee:  (%s)", getMeleeCoordinateString(stickMelee, AXIS_AXY));
			
			setCursorPos(20, 32);
			printStr("C-Melee:  (%s)", getMeleeCoordinateString(stickMelee, AXIS_CXY));
			
			// show origin info if controller is connected
			if (isControllerConnected(CONT_PORT_1)) {
				PADStatus origin = getOriginStatus(CONT_PORT_1);
				setCursorPos(21, 0);
				printStr("Origin XY: ");
				
				// change text color depending on the origin's value
				// mainly useful for phobgcc calibration stuffs
				{
					int8_t max = abs(origin.stickX) > abs(origin.stickY) ? abs(origin.stickX) : abs(origin.stickY);
					if (max >= 100) {
						if (originFlashCounter >= 30) {
							printStrColor(GX_COLOR_RED, GX_COLOR_WHITE, "(%4d,%4d)", origin.stickX, origin.stickY);
						} else {
							printStrColor(GX_COLOR_RED, GX_COLOR_BLACK, "(%4d,%4d)", origin.stickX, origin.stickY);
						}
					} else if (max >= 40) {
						printStrColor(GX_COLOR_ORANGE, GX_COLOR_BLACK, "(%4d,%4d)", origin.stickX, origin.stickY);
					} else if (max >= 20) {
						printStrColor(GX_COLOR_YELLOW, GX_COLOR_BLACK, "(%4d,%4d)", origin.stickX, origin.stickY);
					} else {
						printStr("(%4d,%4d)", origin.stickX, origin.stickY);
					}
				}
				setCursorPos(21, 32);
				printStr("C-Origin XY: ");
				
				// change text color depending on the origin's value
				// mainly useful for phobgcc calibration stuffs
				{
					int8_t max = abs(origin.substickX) > abs(origin.substickY) ? abs(origin.substickX) : abs(origin.substickY);
					if (max >= 100) {
						if (originFlashCounter >= 30) {
							printStrColor(GX_COLOR_RED, GX_COLOR_WHITE, "(%4d,%4d)", origin.substickX, origin.substickY);
						} else {
							printStrColor(GX_COLOR_RED, GX_COLOR_BLACK, "(%4d,%4d)", origin.substickX, origin.substickY);
						}
					} else if (max >= 40) {
						printStrColor(GX_COLOR_ORANGE, GX_COLOR_BLACK, "(%4d,%4d)", origin.substickX, origin.substickY);
					} else if (max >= 20) {
						printStrColor(GX_COLOR_YELLOW, GX_COLOR_BLACK, "(%4d,%4d)", origin.substickX, origin.substickY);
					} else {
						printStr("(%4d,%4d)", origin.substickX, origin.substickY);
					}
				}
				
				if (!(*held & PAD_TRIGGER_L)) {
					setCursorPos(18, 2);
					printStr("L Origin: %3d", origin.triggerL);
				}
				if (!(*held & PAD_TRIGGER_R)) {
					setCursorPos(18, 44);
					printStr("R Origin: %3d", origin.triggerR);
				}
				if (rumbleSecret) {
					setCursorPos(0, 37);
					printStr("Press Z to test 'Rumble'");
				} else {
					// 'shake' the rumble text if z is held
					if (*held & PAD_TRIGGER_Z) {
						setCursorXY(390 + rumbleOffsets[rumbleIndex][0], rumbleOffsets[rumbleIndex][1]);
					} else {
						setCursorPos(0, 39);
					}
					printStr("Press Z to test Rumble");
				}
			}
			
			setCursorPos(17,2);
			printStr("Analog L: %3d", PAD_TriggerL(0));
			if (*held & PAD_TRIGGER_L) {
				setCursorPos(18, 2);
				printStr("Digital L Pressed");
			}
			
			setCursorPos(17,44);
			printStr("Analog R: %3d", PAD_TriggerR(0));
			if (*held & PAD_TRIGGER_R) {
				setCursorPos(18, 40);
				printStr("Digital R Pressed");
			}
			
			restorePrevCursorDepth();
			
			// gui
			
			// triggers
			// L
			int sliderBottomY = LAYOUT_ANALOG_SLIDER_POS_Y + 255;
			int sliderTopY = sliderBottomY - PAD_TriggerL(0);
			GX_SetLineWidth(24, GX_TO_ZERO);
			
			GXColor sliderColor = GX_COLOR_RED;
			if (*held & PAD_TRIGGER_L) {
				sliderColor = GX_COLOR_BLUE;
				GX_SetLineWidth(32, GX_TO_ZERO);
			}
			
			updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
			
			drawSolidBox(LAYOUT_ANALOG_SLIDER_POS_X, LAYOUT_ANALOG_SLIDER_POS_Y,
					LAYOUT_ANALOG_SLIDER_POS_X + 16, sliderBottomY, GX_COLOR_BLACK);
			
			drawSolidBox(LAYOUT_ANALOG_SLIDER_POS_X, sliderTopY,
			             LAYOUT_ANALOG_SLIDER_POS_X + 16, sliderBottomY, sliderColor);
			
			drawBox(LAYOUT_ANALOG_SLIDER_POS_X, LAYOUT_ANALOG_SLIDER_POS_Y,
			        LAYOUT_ANALOG_SLIDER_POS_X + 16, sliderBottomY, GX_COLOR_WHITE);
		
			GX_SetLineWidth(24, GX_TO_ZERO);
			
			// R
			sliderTopY = sliderBottomY - PAD_TriggerR(0);
			
			sliderColor = GX_COLOR_RED;
			if (*held & PAD_TRIGGER_R) {
				sliderColor = GX_COLOR_BLUE;
				GX_SetLineWidth(32, GX_TO_ZERO);
			}
			
			drawSolidBox(640 - 16 - LAYOUT_ANALOG_SLIDER_POS_X, LAYOUT_ANALOG_SLIDER_POS_Y,
			             640 - LAYOUT_ANALOG_SLIDER_POS_X, sliderBottomY, GX_COLOR_BLACK);
			
			drawSolidBox(640 - 16 - LAYOUT_ANALOG_SLIDER_POS_X, sliderTopY,
			             640 - LAYOUT_ANALOG_SLIDER_POS_X, sliderBottomY, sliderColor);
			
			drawBox(640 - 16 - LAYOUT_ANALOG_SLIDER_POS_X, LAYOUT_ANALOG_SLIDER_POS_Y,
			        640 - LAYOUT_ANALOG_SLIDER_POS_X, sliderBottomY, GX_COLOR_WHITE);
			
			GX_SetLineWidth(12, GX_TO_ZERO);
			
			updateVtxDesc(VTX_TEX_COLOR, GX_MODULATE);
			
			changeLoadedTexmap(TEXMAP_CONTROLLER);
			
			// buttons
			// A
			int texOffsetX = TEX_A_OFFSET_X;
			if (*held & PAD_BUTTON_A) {
				texOffsetX += TEX_NORMAL_DIMENSIONS;
			}
			
			GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
			
			GX_Position3s16(LAYOUT_A_POS_X, LAYOUT_A_POS_Y, -4);
			GX_Color4u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b, GX_COLOR_GREEN.a);
			GX_TexCoord2s16(texOffsetX, TEX_A_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_A_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_A_POS_Y, -4);
			GX_Color4u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b, GX_COLOR_GREEN.a);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS, TEX_A_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_A_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_A_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_Color4u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b, GX_COLOR_GREEN.a);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS,
			                TEX_A_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_A_POS_X,
			                LAYOUT_A_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_Color4u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b, GX_COLOR_GREEN.a);
			GX_TexCoord2s16(texOffsetX,
			                TEX_A_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			// B
			texOffsetX = TEX_B_OFFSET_X;
			if (*held & PAD_BUTTON_B) {
				texOffsetX += TEX_NORMAL_DIMENSIONS;
			}
			
			GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
			
			GX_Position3s16(LAYOUT_B_POS_X, LAYOUT_B_POS_Y, -4);
			GX_Color4u8(GX_COLOR_RED.r, GX_COLOR_RED.g, GX_COLOR_RED.b, GX_COLOR_RED.a);
			GX_TexCoord2s16(texOffsetX, TEX_B_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_B_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_B_POS_Y, -4);
			GX_Color4u8(GX_COLOR_RED.r, GX_COLOR_RED.g, GX_COLOR_RED.b, GX_COLOR_RED.a);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS, TEX_B_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_B_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_B_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_Color4u8(GX_COLOR_RED.r, GX_COLOR_RED.g, GX_COLOR_RED.b, GX_COLOR_RED.a);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS,
			                TEX_B_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_B_POS_X,
			                LAYOUT_B_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_Color4u8(GX_COLOR_RED.r, GX_COLOR_RED.g, GX_COLOR_RED.b, GX_COLOR_RED.a);
			GX_TexCoord2s16(texOffsetX,
			                TEX_B_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			// X
			texOffsetX = TEX_X_OFFSET_X;
			if (*held & PAD_BUTTON_X) {
				texOffsetX += TEX_NORMAL_DIMENSIONS;
			}
			
			GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
			
			GX_Position3s16(LAYOUT_X_POS_X, LAYOUT_X_POS_Y, -4);
			GX_Color4u8(GX_COLOR_SILVER.r, GX_COLOR_SILVER.g, GX_COLOR_SILVER.b, GX_COLOR_SILVER.a);
			GX_TexCoord2s16(texOffsetX, TEX_X_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_X_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_X_POS_Y, -4);
			GX_Color4u8(GX_COLOR_SILVER.r, GX_COLOR_SILVER.g, GX_COLOR_SILVER.b, GX_COLOR_SILVER.a);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS, TEX_X_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_X_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_X_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_Color4u8(GX_COLOR_SILVER.r, GX_COLOR_SILVER.g, GX_COLOR_SILVER.b, GX_COLOR_SILVER.a);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS,
			                TEX_X_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_X_POS_X,
			                LAYOUT_X_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_Color4u8(GX_COLOR_SILVER.r, GX_COLOR_SILVER.g, GX_COLOR_SILVER.b, GX_COLOR_SILVER.a);
			GX_TexCoord2s16(texOffsetX,
			                TEX_X_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			// Y
			texOffsetX = TEX_Y_OFFSET_X;
			if (*held & PAD_BUTTON_Y) {
				texOffsetX += TEX_NORMAL_DIMENSIONS;
			}
			
			GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
			
			GX_Position3s16(LAYOUT_Y_POS_X, LAYOUT_Y_POS_Y, -4);
			GX_Color4u8(GX_COLOR_SILVER.r, GX_COLOR_SILVER.g, GX_COLOR_SILVER.b, GX_COLOR_SILVER.a);
			GX_TexCoord2s16(texOffsetX, TEX_Y_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_Y_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_Y_POS_Y, -4);
			GX_Color4u8(GX_COLOR_SILVER.r, GX_COLOR_SILVER.g, GX_COLOR_SILVER.b, GX_COLOR_SILVER.a);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS, TEX_Y_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_Y_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_Y_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_Color4u8(GX_COLOR_SILVER.r, GX_COLOR_SILVER.g, GX_COLOR_SILVER.b, GX_COLOR_SILVER.a);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS,
			                TEX_Y_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_Y_POS_X,
			                LAYOUT_Y_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_Color4u8(GX_COLOR_SILVER.r, GX_COLOR_SILVER.g, GX_COLOR_SILVER.b, GX_COLOR_SILVER.a);
			GX_TexCoord2s16(texOffsetX,
			                TEX_Y_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			// Z
			texOffsetX = TEX_Z_OFFSET_X;
			if (*held & PAD_TRIGGER_Z) {
				texOffsetX += TEX_NORMAL_DIMENSIONS;
			}
			
			GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
			
			GX_Position3s16(LAYOUT_Z_POS_X, LAYOUT_Z_POS_Y, -4);
			GX_Color4u8(0x93, 0x70, 0xDB, 0xFF);
			GX_TexCoord2s16(texOffsetX, TEX_Z_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_Z_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_Z_POS_Y, -4);
			GX_Color4u8(0x93, 0x70, 0xDB, 0xFF);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS, TEX_Z_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_Z_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_Z_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_Color4u8(0x93, 0x70, 0xDB, 0xFF);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS,
			                TEX_Z_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_Z_POS_X,
			                LAYOUT_Z_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_Color4u8(0x93, 0x70, 0xDB, 0xFF);
			GX_TexCoord2s16(texOffsetX,
			                TEX_Z_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
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
				GXColor color = GX_COLOR_SILVER;
				if (i != 0) {
					color = GX_COLOR_WHITE;
				}
				texOffsetX = dpadPressedOffsets[i];
				
				GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
				
				GX_Position3s16(LAYOUT_DPAD_POS_X, LAYOUT_DPAD_POS_Y, -4);
				GX_Color4u8(color.r, color.g, color.b, color.a);
				GX_TexCoord2s16(texOffsetX, TEX_DPAD_OFFSET_Y);
				
				GX_Position3s16(LAYOUT_DPAD_POS_X + TEX_NORMAL_DIMENSIONS,
				                LAYOUT_DPAD_POS_Y, -4);
				GX_Color4u8(color.r, color.g, color.b, color.a);
				GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS, TEX_DPAD_OFFSET_Y);
				
				GX_Position3s16(LAYOUT_DPAD_POS_X + TEX_NORMAL_DIMENSIONS,
				                LAYOUT_DPAD_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
				GX_Color4u8(color.r, color.g, color.b, color.a);
				GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS,
				                TEX_DPAD_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
				
				GX_Position3s16(LAYOUT_DPAD_POS_X,
				                LAYOUT_DPAD_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
				GX_Color4u8(color.r, color.g, color.b, color.a);
				GX_TexCoord2s16(texOffsetX,
				                TEX_DPAD_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
				
				GX_End();
			}
			
			// Start
			texOffsetX = TEX_START_OFFSET_X;
			if (*held & PAD_BUTTON_START) {
				texOffsetX += TEX_NORMAL_DIMENSIONS;
			}
			
			GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
			
			GX_Position3s16(LAYOUT_START_POS_X, LAYOUT_START_POS_Y, -4);
			GX_Color4u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b, GX_COLOR_WHITE.a);
			GX_TexCoord2s16(texOffsetX, TEX_START_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_START_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_START_POS_Y, -4);
			GX_Color4u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b, GX_COLOR_WHITE.a);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS, TEX_START_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_START_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_START_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_Color4u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b, GX_COLOR_WHITE.a);
			GX_TexCoord2s16(texOffsetX + TEX_NORMAL_DIMENSIONS,
			                TEX_START_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_START_POS_X,
			                LAYOUT_START_POS_Y + TEX_NORMAL_DIMENSIONS, -4);
			GX_Color4u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b, GX_COLOR_WHITE.a);
			GX_TexCoord2s16(texOffsetX,
			                TEX_START_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			// stick gate
			GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
			
			GX_Position3s16(LAYOUT_ASTICK_GATE_POS_X, LAYOUT_ASTICK_GATE_POS_Y, -2);
			GX_Color4u8(GX_COLOR_GRAY.r, GX_COLOR_GRAY.g, GX_COLOR_GRAY.b, GX_COLOR_GRAY.a);
			GX_TexCoord2s16(TEX_ASTICK_GATE_OFFSET_X, TEX_ASTICK_GATE_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_ASTICK_GATE_POS_X + TEX_ASTICK_GATE_DIMENSIONS,
			                LAYOUT_ASTICK_GATE_POS_Y, -2);
			GX_Color4u8(GX_COLOR_GRAY.r, GX_COLOR_GRAY.g, GX_COLOR_GRAY.b, GX_COLOR_GRAY.a);
			GX_TexCoord2s16(TEX_ASTICK_GATE_OFFSET_X + TEX_ASTICK_GATE_DIMENSIONS, TEX_ASTICK_GATE_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_ASTICK_GATE_POS_X + TEX_ASTICK_GATE_DIMENSIONS,
			                LAYOUT_ASTICK_GATE_POS_Y + TEX_ASTICK_GATE_DIMENSIONS, -2);
			GX_Color4u8(GX_COLOR_GRAY.r, GX_COLOR_GRAY.g, GX_COLOR_GRAY.b, GX_COLOR_GRAY.a);
			GX_TexCoord2s16(TEX_ASTICK_GATE_OFFSET_X + TEX_ASTICK_GATE_DIMENSIONS,
			                TEX_ASTICK_GATE_OFFSET_Y + TEX_ASTICK_GATE_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_ASTICK_GATE_POS_X,
			                LAYOUT_ASTICK_GATE_POS_Y + TEX_ASTICK_GATE_DIMENSIONS, -2);
			GX_Color4u8(GX_COLOR_GRAY.r, GX_COLOR_GRAY.g, GX_COLOR_GRAY.b, GX_COLOR_GRAY.a);
			GX_TexCoord2s16(TEX_ASTICK_GATE_OFFSET_X,
			                TEX_ASTICK_GATE_OFFSET_Y + TEX_ASTICK_GATE_DIMENSIONS);
			
			GX_End();
			
			// line connecting gate to stick cap
			updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
			
			// figure out where stick should go to
			int stickModX = PAD_StickX(0) / 2;
			int stickModY = PAD_StickY(0) / 2;
			
			GX_SetPointSize(32, GX_TO_ZERO);
			
			// dot at the center, since GX_LINES doesn't look great on its own
			GX_Begin(GX_POINTS, GX_VTXFMT0, 1);
			
			GX_Position3s16(LAYOUT_ASTICK_GATE_POS_X + (TEX_ASTICK_GATE_DIMENSIONS / 2),
			                LAYOUT_ASTICK_GATE_POS_Y + (TEX_ASTICK_GATE_DIMENSIONS / 2), -2);
			GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
			
			GX_End();
			
			GX_SetLineWidth(40, GX_TO_ZERO);
			
			// actually draw the line
			GX_Begin(GX_LINES, GX_VTXFMT0, 2);
			
			GX_Position3s16(LAYOUT_ASTICK_GATE_POS_X + (TEX_ASTICK_GATE_DIMENSIONS / 2),
							LAYOUT_ASTICK_GATE_POS_Y + (TEX_ASTICK_GATE_DIMENSIONS / 2), -2);
			GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
			
			GX_Position3s16(LAYOUT_ASTICK_GATE_POS_X + (TEX_ASTICK_GATE_DIMENSIONS / 2) + stickModX,
			                LAYOUT_ASTICK_GATE_POS_Y + (TEX_ASTICK_GATE_DIMENSIONS / 2) - stickModY, -2);
			GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
			
			GX_End();
			
			GX_SetLineWidth(12, GX_TO_ZERO);
			
			// stick cap
			updateVtxDesc(VTX_TEX_COLOR, GX_MODULATE);
			GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
			
			GX_Position3s16(LAYOUT_ASTICK_CAP_POS_X + stickModX, LAYOUT_ASTICK_CAP_POS_Y - stickModY, -2);
			GX_Color4u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b, GX_COLOR_WHITE.a);
			GX_TexCoord2s16(TEX_ASTICK_CAP_OFFSET_X, TEX_ASTICK_CAP_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_ASTICK_CAP_POS_X + TEX_NORMAL_DIMENSIONS + stickModX,
			                LAYOUT_ASTICK_CAP_POS_Y - stickModY, -2);
			GX_Color4u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b, GX_COLOR_WHITE.a);
			GX_TexCoord2s16(TEX_ASTICK_CAP_OFFSET_X + TEX_NORMAL_DIMENSIONS, TEX_ASTICK_CAP_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_ASTICK_CAP_POS_X + TEX_NORMAL_DIMENSIONS + stickModX,
			                LAYOUT_ASTICK_CAP_POS_Y + TEX_NORMAL_DIMENSIONS - stickModY, -2);
			GX_Color4u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b, GX_COLOR_WHITE.a);
			GX_TexCoord2s16(TEX_ASTICK_CAP_OFFSET_X + TEX_NORMAL_DIMENSIONS,
			                TEX_ASTICK_CAP_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_ASTICK_CAP_POS_X + stickModX,
			                LAYOUT_ASTICK_CAP_POS_Y + TEX_NORMAL_DIMENSIONS - stickModY, -2);
			GX_Color4u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b, GX_COLOR_WHITE.a);
			GX_TexCoord2s16(TEX_ASTICK_CAP_OFFSET_X,
			                TEX_ASTICK_CAP_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			// c-stick gate
			GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X, LAYOUT_CSTICK_POS_Y, -2);
			GX_Color4u8(GX_COLOR_GRAY.r, GX_COLOR_GRAY.g, GX_COLOR_GRAY.b, GX_COLOR_GRAY.a);
			GX_TexCoord2s16(TEX_CSTICK_GATE_OFFSET_X, TEX_CSTICK_GATE_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_CSTICK_POS_Y, -2);
			GX_Color4u8(GX_COLOR_GRAY.r, GX_COLOR_GRAY.g, GX_COLOR_GRAY.b, GX_COLOR_GRAY.a);
			GX_TexCoord2s16(TEX_CSTICK_GATE_OFFSET_X + TEX_NORMAL_DIMENSIONS, TEX_CSTICK_GATE_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X + TEX_NORMAL_DIMENSIONS,
			                LAYOUT_CSTICK_POS_Y + TEX_NORMAL_DIMENSIONS, -2);
			GX_Color4u8(GX_COLOR_GRAY.r, GX_COLOR_GRAY.g, GX_COLOR_GRAY.b, GX_COLOR_GRAY.a);
			GX_TexCoord2s16(TEX_CSTICK_GATE_OFFSET_X + TEX_NORMAL_DIMENSIONS,
			                TEX_CSTICK_GATE_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X,
			                LAYOUT_CSTICK_POS_Y + TEX_NORMAL_DIMENSIONS, -2);
			GX_Color4u8(GX_COLOR_GRAY.r, GX_COLOR_GRAY.g, GX_COLOR_GRAY.b, GX_COLOR_GRAY.a);
			GX_TexCoord2s16(TEX_CSTICK_GATE_OFFSET_X,
			                TEX_CSTICK_GATE_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			// line connecting gate to cstick cap
			updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
			
			// figure out where stick should go to
			int cStickModX = PAD_SubStickX(0) / 2;
			int cStickModY = PAD_SubStickY(0) / 2;
			
			GX_SetPointSize(32, GX_TO_ZERO);
			
			// dot at the center, since GX_LINES doesn't look great on its own
			GX_Begin(GX_POINTS, GX_VTXFMT0, 1);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X + (TEX_NORMAL_DIMENSIONS / 2),
			                LAYOUT_CSTICK_POS_Y + (TEX_NORMAL_DIMENSIONS / 2), -2);
			GX_Color3u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b);
			
			GX_End();
			
			GX_SetLineWidth(40, GX_TO_ZERO);
			
			// actually draw the line
			GX_Begin(GX_LINES, GX_VTXFMT0, 2);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X + (TEX_NORMAL_DIMENSIONS / 2),
			                LAYOUT_CSTICK_POS_Y + (TEX_NORMAL_DIMENSIONS / 2), -2);
			GX_Color3u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X + (TEX_NORMAL_DIMENSIONS / 2) + cStickModX,
			                LAYOUT_CSTICK_POS_Y + (TEX_NORMAL_DIMENSIONS / 2) - cStickModY, -2);
			GX_Color3u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b);
			
			GX_End();
			
			GX_SetLineWidth(12, GX_TO_ZERO);
			
			// c-stick cap
			updateVtxDesc(VTX_TEX_COLOR, GX_MODULATE);
			GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X + cStickModX, LAYOUT_CSTICK_POS_Y - cStickModY - 1, -2);
			GX_Color4u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b, GX_COLOR_YELLOW.a);
			GX_TexCoord2s16(TEX_CSTICK_CAP_OFFSET_X, TEX_CSTICK_CAP_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X + TEX_NORMAL_DIMENSIONS + cStickModX,
			                LAYOUT_CSTICK_POS_Y - cStickModY - 1, -2);
			GX_Color4u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b, GX_COLOR_YELLOW.a);
			GX_TexCoord2s16(TEX_CSTICK_CAP_OFFSET_X + TEX_NORMAL_DIMENSIONS, TEX_CSTICK_CAP_OFFSET_Y);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X + TEX_NORMAL_DIMENSIONS + cStickModX,
			                LAYOUT_CSTICK_POS_Y + TEX_NORMAL_DIMENSIONS - cStickModY - 1, -2);
			GX_Color4u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b, GX_COLOR_YELLOW.a);
			GX_TexCoord2s16(TEX_CSTICK_CAP_OFFSET_X + TEX_NORMAL_DIMENSIONS,
			                TEX_CSTICK_CAP_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_Position3s16(LAYOUT_CSTICK_POS_X + cStickModX,
			                LAYOUT_CSTICK_POS_Y + TEX_NORMAL_DIMENSIONS - cStickModY - 1, -2);
			GX_Color4u8(GX_COLOR_YELLOW.r, GX_COLOR_YELLOW.g, GX_COLOR_YELLOW.b, GX_COLOR_YELLOW.a);
			GX_TexCoord2s16(TEX_CSTICK_CAP_OFFSET_X,
			                TEX_CSTICK_CAP_OFFSET_Y + TEX_NORMAL_DIMENSIONS);
			
			GX_End();
			
			controllerTestCheckInput();
			
			// enable rumble if z is held
			if (*held & PAD_TRIGGER_Z) {
				PAD_ControlMotor(0, PAD_MOTOR_RUMBLE);
				if (rumbleSecret) {
					// called indirectly so that there isn't anything that is 'lagging behind'
					setScreenOffset(rumbleOffsets[rumbleIndex][0], rumbleOffsets[rumbleIndex][1]);
				}
				rumbleIndex++;
				rumbleIndex %= 4;
			} else {
				PAD_ControlMotor(0, PAD_MOTOR_STOP);
				if (rumbleSecret) {
					setScreenOffset(0, 0);
				}
				rumbleIndex = 0;
			}

			break;
	}
	
	originFlashCounter++;
	originFlashCounter %= 60;
}

void menu_controllerTestEnd() {
	// not sure if this is actually useful, maybe get rid of it...
	//menuState = CONT_TEST_SETUP;
}
