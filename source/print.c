//
// Created on 3/14/25.
//

// the majority of this was adapted from the Phobgcc PhobVision code:
// https://github.com/PhobGCC/PhobGCC-SW/tree/main/PhobGCC/rp2040

#include "print.h"
#include "font.h"
#include "draw.h"
#include <ogc/color.h>

static int currX = 0;
static int currY = 0;

//Draws 8x15 character in the specified location according to the ascii codepoints
void drawChar(unsigned char bitmap[],
              const unsigned int color,
              const char character) {
	if((character < 0x20 && character != '\n') || character > 0x7e) { //lower than space, larger than tilde, and not newline
		return;
	}
	// advance row if newline or if we would draw outside of bounds
	if (character == '\n' || currX + 10 > (640 - (PRINT_PADDING_HORIZONTAL * 2))) {
		currY += 15 + LINE_SPACING;
		currX = 0;
		if (character != '\n') {
			drawChar(bitmap, color, character);
		}
		return;
	}
	// stop drawing altogether if row is too far
	if ((currY + 15) >= (479 - (PRINT_PADDING_VERTICAL * 2))) {
		return;
	}

	if (character == ' ' && currX == 0) {
		return;
	}
	
	for(int row = 0; row < 15; row++) {
		uint32_t rowOffset = (row+currY + PRINT_PADDING_VERTICAL);
		for(int col = 0; col < 8; col++) {
			if((font[(character-0x20)*15+row] << col) & 0b10000000) {
				uint16_t colOffset = col+currX + PRINT_PADDING_HORIZONTAL;
				DrawDot(colOffset, rowOffset, color, bitmap);
			}
		}
	}
	currX += 10;
}

// draw a character but ignore "rows" and "columns"
void drawCharDirect(unsigned char bitmap[],
                    uint16_t x,
                    uint16_t y,
                    const unsigned int color,
                    const char character) {
	if(character < 0x20 || character > 0x7e) { //lower than space, larger than tilde
		return;
	}
	for(int row = 0; row < 15; row++) {
		uint32_t rowOffset = (row+y);
		for(int col = 0; col < 8; col++) {
			if((font[(character-0x20)*15+row] << col) & 0b10000000) {
				uint16_t colOffset = col+x;
				DrawDot(colOffset, rowOffset, color, bitmap);
			}
		}
	}
}

void drawString(unsigned char bitmap[],
                const uint32_t bg_color,
				const uint32_t fg_color,
                const char string[]) {
	uint16_t i = 0;
	const char nullChar[] = "";
	while(string[i] != nullChar[0]) {
		if (bg_color != COLOR_BLACK) {
			DrawFilledBox(currX + PRINT_PADDING_HORIZONTAL - 2, currY + PRINT_PADDING_VERTICAL - 2,
			              currX + PRINT_PADDING_HORIZONTAL + 10, currY + PRINT_PADDING_VERTICAL + 15,
			              bg_color, bitmap);
		}
		drawChar(bitmap, fg_color, string[i]);
		i++;
	}
}

void printStr(const char* str, void *xfb) {
	printStrColor(str, xfb, COLOR_BLACK, COLOR_WHITE);
}

void printStrColor(const char* str, void *xfb, const uint32_t bg_color, const uint32_t fg_color) {
	drawString(xfb, bg_color, fg_color, str);
}

void resetCursor() {
	setCursorPos(0,0);
}

void setCursorPos(int row, int col) {
	currY = row * (15 + LINE_SPACING);
	currX = col * 10;
}