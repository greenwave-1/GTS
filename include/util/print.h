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

void drawString(const uint32_t bg_color,
				const uint32_t fg_color,
                const char string[]);

void printStr(const char* str, ...);
void printStrColor(const GXColor bg_color, const GXColor fg_color, const char* str, ...);

void printEllipse(const int counter, const int interval);

void resetCursor();
void setCursorPos(int row, int col);
void setCursorXY(int x, int y);
void setCursorDepth(int z);
void restorePrevCursorDepth();

#endif //GTS_PRINT_H
