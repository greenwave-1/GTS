//
// Created on 8/26/25.
//

// Controller Test submenu
// basic visual button and stick test
// also shows coordinates (raw and melee converted), and origin values

#ifndef GTS_CONTROLLERTEST_H
#define GTS_CONTROLLERTEST_H

enum CONTROLLER_TEST_MENU_STATE { CONT_TEST_SETUP, CONT_TEST_POST_SETUP };

void menu_controllerTest();
void menu_controllerTestEnd();

// specific screen coordinates for texture layout in controller test

// normal button

// A
// pressed is x + 90
#define TEX_A_DIMS 90
#define TEX_A_OFFSET_X 0
#define TEX_A_OFFSET_Y 0

// B
// pressed is y + 50
#define TEX_B_DIMS 50
#define TEX_B_OFFSET_X 180
#define TEX_B_OFFSET_Y 0

// X and Y
// pressed is y + 50
// tex needs to be rotated for use as X
#define TEX_XY_DIMS_LONG 80
#define TEX_XY_DIMS_SHORT 50
#define TEX_XY_OFFSET_X 230
#define TEX_XY_OFFSET_Y 0

// Z
// pressed is Y + 45
#define TEX_Z_DIMS_LONG 52
#define TEX_Z_DIMS_SHORT 45
#define TEX_Z_OFFSET_X 310
#define TEX_Z_OFFSET_Y 0

// Start
// pressed is X + 45 - 2
#define TEX_START_DIMS 45
#define TEX_START_OFFSET_X 257
#define TEX_START_OFFSET_Y 270

// dpad
// pressed is y + 85
// tex needs to be rotated to match direction
#define TEX_DPAD_DIMS 85
#define TEX_DPAD_OFFSET_X 260
#define TEX_DPAD_OFFSET_Y 100

// allows iteration over dpad directions in for loop
// see controllertest.c
#define TEX_DPAD_DIRECTIONS_LIST (uint16_t[]) { PAD_BUTTON_UP, PAD_BUTTON_RIGHT, PAD_BUTTON_DOWN, PAD_BUTTON_LEFT }
// rotation is slightly off for some reason, offsets to fix placement...
#define TEX_DPAD_DIRECTIONS_FIX_X (int[]) { 0, 1, 1, 0 }
#define TEX_DPAD_DIRECTIONS_FIX_Y (int[]) { 0, 0, 1, 1 }

// sticks
// analog stick gate is 132x132, everything else from here on is 128x128
#define TEX_ASTICK_GATE_DIMS 132
#define TEX_ASTICK_GATE_OFFSET_X 0
#define TEX_ASTICK_GATE_OFFSET_Y 100

#define TEX_STICK_DIMS 128
#define TEX_ASTICK_CAP_OFFSET_X 0
#define TEX_ASTICK_CAP_OFFSET_Y 232

#define TEX_CSTICK_GATE_OFFSET_X 132
#define TEX_CSTICK_GATE_OFFSET_Y 100

#define TEX_CSTICK_CAP_OFFSET_X 132
#define TEX_CSTICK_CAP_OFFSET_Y 228


// layout constants
// top-left coordinate for a given button, when drawn on the screen

#define LAYOUT_A_POS_X 402 // 320 + 32 (padding) + 31
#define LAYOUT_A_POS_Y 99

#define LAYOUT_B_POS_X (LAYOUT_A_POS_X - 30)
#define LAYOUT_B_POS_Y (LAYOUT_A_POS_Y + 65)

#define LAYOUT_X_POS_X (LAYOUT_A_POS_X + 80)
#define LAYOUT_X_POS_Y (LAYOUT_A_POS_Y - 10)

#define LAYOUT_Y_POS_X (LAYOUT_A_POS_X - 12)
#define LAYOUT_Y_POS_Y (LAYOUT_A_POS_Y - 40)

#define LAYOUT_Z_POS_X (LAYOUT_A_POS_X + 61)
#define LAYOUT_Z_POS_Y (LAYOUT_A_POS_Y - 48)

#define LAYOUT_START_POS_X 297
#define LAYOUT_START_POS_Y 123

#define LAYOUT_DPAD_POS_X 218
#define LAYOUT_DPAD_POS_Y 236

#define LAYOUT_ANALOG_SLIDER_POS_X 66
#define LAYOUT_ANALOG_SLIDER_POS_Y 64

#define LAYOUT_ASTICK_GATE_POS_X 124
#define LAYOUT_ASTICK_GATE_POS_Y 79

// stick cap texture is 128x128, gate is 132x132, so + 2 to center on gate
#define LAYOUT_ASTICK_CAP_POS_X (LAYOUT_ASTICK_GATE_POS_X + 2)
#define LAYOUT_ASTICK_CAP_POS_Y (LAYOUT_ASTICK_GATE_POS_Y + 2)

#define LAYOUT_CSTICK_POS_X 315
#define LAYOUT_CSTICK_POS_Y 215

#endif //GTS_CONTROLLERTEST_H
