//
// Created on 2025/03/14.
//

// utilities for drawing a custom font from a texture, and handing a virtual cursor.
// currently using the PhobVision font.

// previously, this housed a direct framebuffer draw implementation,
// which was heavily modified from the PhobGCC project, specifically PhobVision:
// https://github.com/PhobGCC/PhobGCC-SW/tree/main/PhobGCC/rp2040

#ifndef GTS_PRINT_H
#define GTS_PRINT_H

#include <stdint.h>

#include "util/gx.h"

// padding on both sides that print funtion won't touch
#define PRINT_PADDING_HORIZONTAL 10
#define PRINT_PADDING_VERTICAL 40

// number of pixels between given columns
#define LINE_SPACING 2

// enum for a given button picture when printing
enum FONT_BUTTON_LIST {
	FONT_A, FONT_B, FONT_X, FONT_Y, FONT_START, FONT_L, FONT_R, FONT_Z,
	FONT_DPAD, FONT_DPAD_UP, FONT_DPAD_LEFT, FONT_DPAD_DOWN, FONT_DPAD_RIGHT, FONT_STICK_A, FONT_STICK_C, FONT_NONE = -1 };

/*
// struct for custom printing, used in instruction pages
typedef struct INSTRUCTION_ENTRY {
	char *text;
	GXTexObj *texture;
	enum FONT_BUTTON_LIST button; // should get truncated to an enum
} INSTRUCTION_ENTRY;*/

void drawString(const uint32_t bg_color,
				const uint32_t fg_color,
                const char string[]);

void printStr(const char* str, ...);
void printStrColor(const GXColor bg_color, const GXColor fg_color, const char* str, ...);
void printStrBox(const GXColor box_color, const char* str, ...);

void drawFontButton(enum FONT_BUTTON_LIST button);

//void printStrButton(struct INSTRUCTION_ENTRY list[]);

void resetScrollingPrint();
void startScrollingPrint(int x1, int y1, int x2, int y2);
void endScrollingPrint();

void printEllipse(const int counter, const int interval);
void printSpinningLine();
void printSpinningLineInterval(const int waitInterval);

void resetCursor();
void setCursorPos(int row, int col);
void setCursorXY(int x, int y);
void setCursorDepth(int z);
void restorePrevCursorDepth();

#endif //GTS_PRINT_H
