//
// Created on 2025/03/14.
//

// the majority of this was adapted from the Phobgcc PhobVision code:
// https://github.com/PhobGCC/PhobGCC-SW/tree/main/PhobGCC/rp2040

#ifndef GTS_PRINT_H
#define GTS_PRINT_H

#include <stdint.h>

// padding on both sides that print funtion won't touch
#define PRINT_PADDING_HORIZONTAL 10
#define PRINT_PADDING_VERTICAL 40

// number of pixels between given columns
#define LINE_SPACING 2

void drawChar(unsigned char bitmap[],
              const unsigned int color,
              const char character);

void drawCharDirect(unsigned char bitmap[],
			   uint16_t x,
			   uint16_t y,
               const unsigned int color,
               const char character);

void drawString(unsigned char bitmap[],
                const uint32_t bg_color,
				const uint32_t fg_color,
                const char string[]);

void printStr(const char* str, void *xfb);
void printStrColor(const char* str, void *xfb, const uint32_t bg_color, const uint32_t fg_color);

void resetCursor();
void setCursorPos(int row, int col);

#endif //GTS_PRINT_H
