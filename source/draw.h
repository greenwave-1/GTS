//
// Created on 2/14/25.
//

#ifndef FOSSSCOPE_DRAW_H
#define FOSSSCOPE_DRAW_H

#include <gctypes.h>
#include "stickmap_coordinates.h"

// center of screen, 640x480
// TODO: replace this with a function call that takes into account other tv modes (pal)
#define SCREEN_POS_CENTER_X 320
#define SCREEN_POS_CENTER_Y 240
#define COORD_CIRCLE_CENTER_X 400

// Controller Test coordinates
// analog triggers
#define CONT_TEST_TRIGGER_LEN 255
#define CONT_TEST_TRIGGER_WIDTH 10
#define CONT_TEST_TRIGGER_L_X1 53
#define CONT_TEST_TRIGGER_R_X1 553
#define CONT_TEST_TRIGGER_Y1 69 // both l and r start on the same y line, so its a shared value

// digital triggers
//#define CONT_TEST_TRIGGER_DLR_SIZE 20
//#define CONT_TEST_TRIGGER_DL_X1 (CONT_TEST_TRIGGER_L_X1 + ((CONT_TEST_TRIGGER_DLR_SIZE * 2)))
//#define CONT_TEST_TRIGGER_DR_X1 (CONT_TEST_TRIGGER_R_X1 - (CONT_TEST_TRIGGER_DLR_SIZE * 2))

// A
#define CONT_TEST_BUTTON_A_SIZE 31 // square, so same for width
#define CONT_TEST_BUTTON_A_X1 417
#define CONT_TEST_BUTTON_A_Y1 153

// B
#define CONT_TEST_BUTTON_B_SIZE 22 // also square
#define CONT_TEST_BUTTON_B_X1 383
#define CONT_TEST_BUTTON_B_Y1 171

// X and Y
// Y uses the x1 from A
// X uses the same x1 from Z
#define CONT_TEST_BUTTON_XY_LONG 31 // x and y are same shape, but flipped
#define CONT_TEST_BUTTON_XY_SHORT 20
#define CONT_TEST_BUTTON_X_Y1 153
#define CONT_TEST_BUTTON_Y_Y1 125

// Z
// Z uses CONT_TEST_BUTTON_XY_SHORT for both sides
#define CONT_TEST_BUTTON_Z_X1 462
#define CONT_TEST_BUTTON_Z_Y1 125

// Start
#define CONT_TEST_BUTTON_START_LEN 60
#define CONT_TEST_BUTTON_START_WIDTH 23
#define CONT_TEST_BUTTON_START_X1 290
#define CONT_TEST_BUTTON_START_Y1 124

// DPad
#define CONT_TEST_DPAD_LONG 25
#define CONT_TEST_DPAD_SHORT 10
#define CONT_TEST_DPAD_UP_X1 250
#define CONT_TEST_DPAD_UP_Y1 250
#define CONT_TEST_DPAD_DOWN_Y1 (CONT_TEST_DPAD_UP_Y1 + CONT_TEST_DPAD_LONG + CONT_TEST_DPAD_SHORT) // up and down share x1
#define CONT_TEST_DPAD_LEFT_X1 (CONT_TEST_DPAD_UP_X1 - CONT_TEST_DPAD_LONG)
#define CONT_TEST_DPAD_LEFT_Y1 (CONT_TEST_DPAD_UP_Y1 + CONT_TEST_DPAD_LONG)
#define CONT_TEST_DPAD_RIGHT_X1 (CONT_TEST_DPAD_LEFT_X1 + CONT_TEST_DPAD_SHORT + CONT_TEST_DPAD_LONG) + 1 // left and right share y1

// Sticks
// Analog Stick
#define CONT_TEST_STICK_RAD 40 // analog and c stick
#define CONT_TEST_STICK_CENTER_X 180
#define CONT_TEST_STICK_CENTER_Y 150
#define CONT_TEST_CSTICK_CENTER_X 380
#define CONT_TEST_CSTICK_CENTER_Y 280


void setInterlaced(bool interlaced);

// draw functions

void drawImage(void *currXfb, const unsigned char image[], const unsigned char colorIndex[8], u16 offsetX, u16 offsetY);

// drawing functions from phobconfigtool
void DrawHLine (int x1, int x2, int y, int color, void *xfb);
void DrawVLine (int x, int y1, int y2, int color, void *xfb);
void DrawBox (int x1, int y1, int x2, int y2, int color, void *xfb);

// expanded drawing functions
void DrawFilledBox (int x1, int y1, int x2, int y2, int color, void *xfb);
void DrawLine (int x1, int y1, int x2, int y2, int color, void *xfb);
void DrawDot (int x, int y, int color, void *xfb);
void DrawDotAccurate (int x, int y, int color, void *xfb);
void DrawCircle (int cx, int cy, int r, int color, void *xfb);
void DrawFilledCircle(int cx, int cy, int interval, int color, void *xfb);
void DrawFilledBoxCenter(int x, int y, int rad, int color, void *xfb);
void DrawOctagonalGate(int x, int y, int scale, int color, void *xfb);

// draw for tests in coordinate viewer
void DrawStickmapOverlay(enum STICKMAP_LIST stickmap, int which, void *xfb);

#endif //FOSSSCOPE_DRAW_H
