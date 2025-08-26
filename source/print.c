//
// Created on 2025/03/14.
//

// Code in use from the PhobGCC project is licensed under GPLv3. A copy of this license is provided in the root
// directory of this project's repository.

// Upstream URL for the PhobGCC project is: https://github.com/PhobGCC/PhobGCC-SW

// the majority of this was adapted from the Phobgcc PhobVision code:
// https://github.com/PhobGCC/PhobGCC-SW/tree/main/PhobGCC/rp2040

#include "print.h"

#include <stdio.h>
#include <stdarg.h>

#include <ogc/color.h>

#include "font.h"
#include "draw.h"

static char strBuffer[1000];

static int currX = 0;
static int currY = 0;

//Draws 8x15 character in the specified location according to the ascii codepoints
void drawChar(const unsigned int color,
              const char character) {
	if((character < 0x20 && character != '\n') || character > 0x7e) { //lower than space, larger than tilde, and not newline
		return;
	}
	// advance row if newline or if we would draw outside of bounds
	if (character == '\n' || currX + 10 > (640 - (PRINT_PADDING_HORIZONTAL * 2))) {
		currY += 15 + LINE_SPACING;
		currX = 0;
		if (character == '\n') {
			return;
		}
	}
	// stop drawing altogether if row is too far
	if ((currY + 15) >= (479 - (PRINT_PADDING_VERTICAL * 2))) {
		return;
	}
	
	// skip "drawing" if its just a space
	if (character != ' ') {
		// decode font character and draw it
		for (int row = 0; row < 15; row++) {
			uint32_t rowOffset = (row + currY + PRINT_PADDING_VERTICAL);
			for (int col = 0; col < 8; col++) {
				if ((font[(character - 0x20) * 15 + row] << col) & 0b10000000) {
					uint16_t colOffset = col + currX + PRINT_PADDING_HORIZONTAL;
					DrawDotAccurate(colOffset, rowOffset, color);
				}
			}
		}
	}
	
	currX += 10;
}

// draw a character but ignore "rows" and "columns"
void drawCharDirect(uint16_t x,
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
				DrawDotAccurate(colOffset, rowOffset, color);
			}
		}
	}
}

void drawString(const uint32_t bg_color,
				const uint32_t fg_color,
                const char string[]) {
	uint16_t i = 0;
	const char nullChar[] = "";
	while(string[i] != nullChar[0]) {
		if (bg_color != COLOR_BLACK) {
			DrawFilledBox(currX + PRINT_PADDING_HORIZONTAL - 2, currY + PRINT_PADDING_VERTICAL - 2,
			              currX + PRINT_PADDING_HORIZONTAL + 10, currY + PRINT_PADDING_VERTICAL + 15,
			              bg_color);
		}
		drawChar(fg_color, string[i]);
		i++;
	}
}

void printStr(const char* str, ...) {
	va_list list;
	va_start(list, str);
	vsnprintf(strBuffer, 1000, str, list);
	drawString(COLOR_BLACK, COLOR_WHITE, strBuffer);
	va_end(list);
}

void printStrColor(const uint32_t bg_color, const uint32_t fg_color, const char* str, ...) {
	va_list list;
	va_start(list, str);
	vsnprintf(strBuffer, 1000, str, list);
	drawString(bg_color, fg_color, strBuffer);
	va_end(list);
}

void printEllipse(const int counter, const int interval) {
	int remainder = counter / interval;
	while (remainder != 0) {
		printStr(".");
		remainder--;
	}
}

void resetCursor() {
	setCursorPos(0,0);
}

void setCursorPos(int row, int col) {
	currY = row * (15 + LINE_SPACING);
	currX = col * 10;
}