//
// Created on 10/14/25.
//

#ifndef GTS_CONTROLLERTEST_CONSTANTS_H
#define GTS_CONTROLLERTEST_CONSTANTS_H

// texture constants

#define TEX_NORMAL_DIMENSIONS 128

// normal button
// offsets are for non-pressed versions, pressed versions are X + 128
#define TEX_A_OFFSET_X 0
#define TEX_A_OFFSET_Y 0

#define TEX_B_OFFSET_X 256
#define TEX_B_OFFSET_Y 0

#define TEX_X_OFFSET_X 512
#define TEX_X_OFFSET_Y 0

#define TEX_Y_OFFSET_X 0
#define TEX_Y_OFFSET_Y 128

#define TEX_Z_OFFSET_X 256
#define TEX_Z_OFFSET_Y 128

#define TEX_START_OFFSET_X 512
#define TEX_START_OFFSET_Y 128

// dpad
#define TEX_DPAD_OFFSET_X 0
#define TEX_DPAD_OFFSET_Y 256

#define TEX_DPAD_UP_OFFSET_X 128
#define TEX_DPAD_UP_OFFSET_Y 256

#define TEX_DPAD_RIGHT_OFFSET_X 256
#define TEX_DPAD_RIGHT_OFFSET_Y 256

#define TEX_DPAD_LEFT_OFFSET_X 384
#define TEX_DPAD_LEFT_OFFSET_Y 256

#define TEX_DPAD_DOWN_OFFSET_X 512
#define TEX_DPAD_DOWN_OFFSET_Y 256

// analog sliders
// analog sliders are 116x24
#define TEX_ANALOG_SLIDER_DIMENSIONS_X 116
#define TEX_ANALOG_SLIDER_DIMENSIONS_Y 24

#define TEX_ANALOG_SLIDER_OFFSET_X 640
#define TEX_ANALOG_SLIDER_OFFSET_Y 256

// sticks
// analog stick gate is 132x132, everything else is 128x128
#define TEX_ASTICK_GATE_DIMENSIONS 132
#define TEX_ASTICK_GATE_OFFSET_X 0
#define TEX_ASTICK_GATE_OFFSET_Y 384

#define TEX_ASTICK_CAP_OFFSET_X 256
#define TEX_ASTICK_CAP_OFFSET_Y 384

#define TEX_CSTICK_GATE_OFFSET_X 384
#define TEX_CSTICK_GATE_OFFSET_Y 384

#define TEX_CSTICK_CAP_OFFSET_X 512
#define TEX_CSTICK_CAP_OFFSET_Y 384

#define TEX_CSTICK_CAP_ALT_OFFSET_X 640
#define TEX_CSTICK_CAP_ALT_OFFSET_Y 384


// layout constants
// top-left coordinate for a given button, when drawn on the screen

#define LAYOUT_A_POS_X 383 // 320 + 32 (padding) + 31
#define LAYOUT_A_POS_Y 80

#define LAYOUT_B_POS_X (LAYOUT_A_POS_X - 50)
#define LAYOUT_B_POS_Y (LAYOUT_A_POS_Y + 45)

#define LAYOUT_X_POS_X (LAYOUT_A_POS_X + 60)
#define LAYOUT_X_POS_Y (LAYOUT_A_POS_Y - 15)

#define LAYOUT_Y_POS_X (LAYOUT_A_POS_X - 15)
#define LAYOUT_Y_POS_Y (LAYOUT_A_POS_Y - 60)

#define LAYOUT_Z_POS_X (LAYOUT_A_POS_X + 42)
#define LAYOUT_Z_POS_Y (LAYOUT_A_POS_Y - 72)

#define LAYOUT_START_POS_X 256 // x center of screen - 64
#define LAYOUT_START_POS_Y 82

#define LAYOUT_DPAD_POS_X 197
#define LAYOUT_DPAD_POS_Y 215

#define LAYOUT_ANALOG_SLIDER_POS_X 66
#define LAYOUT_ANALOG_SLIDER_POS_Y 64

#define LAYOUT_ASTICK_GATE_POS_X 124 // 120
#define LAYOUT_ASTICK_GATE_POS_Y 79

// stick cap texture is 128x128, gate is 132x132, so + 2 to center on gate
#define LAYOUT_ASTICK_CAP_POS_X (LAYOUT_ASTICK_GATE_POS_X + 2)
#define LAYOUT_ASTICK_CAP_POS_Y (LAYOUT_ASTICK_GATE_POS_Y + 2)

#define LAYOUT_CSTICK_POS_X 315
#define LAYOUT_CSTICK_POS_Y 215



#endif // GTS_CONTROLLERTEST_CONSTANTS_H
