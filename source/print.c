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

#include "gx.h"
#include "font.h"
#include "draw.h"

// buffer for variable arg strings
static char strBuffer[1000];

// real screen coordinates of our 'cursor'
static int cursorX = 0;
static int cursorY = 0;
/*
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
*/

void drawCharDirect(uint16_t x,
                    uint16_t y,
                    const unsigned int color,
                    const char character) {
	return;
}

static void handleString(const char* str, bool draw, GXColor fgColor, GXColor bgColor) {
	// pointer for iterating over string
	const char* curr = str;
	
	// stores size of a given character from the font
	int texturePosX1, texturePosY1;
	
	// starting cursor values, stored so that we can restore them if we didn't actually print anything
	int startingX = cursorX, startingY = cursorY;
	// "working" values, used to determine if we need to draw a background color
	int workingX = startingX, workingY = startingY;
	
	// loop until we hit a null terminator
	for ( ; *curr != '\0'; curr++) {
		// lower than space, larger than tilde, and not newline
		if((*curr < 0x20 && *curr != '\n') || *curr > 0x7e) {
			continue;
		}
		
		// get texture coordinates
		// font sheet starts at 0x20 ascii, 10 chars per line
		int charIndex = (*curr) - 0x20;
		texturePosX1 = (charIndex % 10) * 8;
		texturePosY1 = (charIndex / 10) * 16;
		
		// go to a "new line" if drawing would put us outside the safe area, or if newline
		if (cursorX + 10 > 640 - (PRINT_PADDING_HORIZONTAL * 2) || *curr == '\n') {
			// draw our background color, if applicable
			if (!draw) {
				drawSolidBox(workingX + PRINT_PADDING_HORIZONTAL - 2, workingY + PRINT_PADDING_VERTICAL - 2,
			       cursorX + PRINT_PADDING_HORIZONTAL, cursorY + PRINT_PADDING_VERTICAL + 16, false, bgColor);
			}
			cursorY += 15 + LINE_SPACING;
			cursorX = 0;
			workingX = cursorX;
			workingY = cursorY;
			if (*curr == '\n') {
				continue;
			}
		}
		
		// determine real coordinates for drawing
		if (draw) {
			int quadX1 = cursorX + PRINT_PADDING_HORIZONTAL;
			int quadY1 = cursorY + PRINT_PADDING_VERTICAL;
			int quadX2 = quadX1 + 8;
			int quadY2 = quadY1 + 15;
			
			// get secondary coordinates for texture
			int texturePosX2 = texturePosX1 + 8;
			int texturePosY2 = texturePosY1 + 15;
			
			// draw the char
			updateVtxDesc(VTX_TEX_COLOR, GX_MODULATE);
			
			GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
			
			GX_Position2s16(quadX1, quadY1);
			GX_Color4u8(fgColor.r, fgColor.g, fgColor.b, fgColor.a);
			GX_TexCoord2s16(texturePosX1, texturePosY1);
			
			GX_Position2s16(quadX2, quadY1);
			GX_Color4u8(fgColor.r, fgColor.g, fgColor.b, fgColor.a);
			GX_TexCoord2s16(texturePosX2, texturePosY1);
			
			GX_Position2s16(quadX2, quadY2);
			GX_Color4u8(fgColor.r, fgColor.g, fgColor.b, fgColor.a);
			GX_TexCoord2s16(texturePosX2, texturePosY2);
			
			GX_Position2s16(quadX1, quadY2);
			GX_Color4u8(fgColor.r, fgColor.g, fgColor.b, fgColor.a);
			GX_TexCoord2s16(texturePosX1, texturePosY2);
			
			/*
			GX_Position2s16(quadX1, quadY2);
			GX_Color4u8(fgColor.r, fgColor.g, fgColor.b, fgColor.a);
			GX_TexCoord2s16(texturePosX1, texturePosY1);
			
			GX_Position2s16(quadX2, quadY2);
			GX_Color4u8(fgColor.r, fgColor.g, fgColor.b, fgColor.a);
			GX_TexCoord2s16(texturePosX2, texturePosY1);
			
			GX_Position2s16(quadX2, quadY1);
			GX_Color4u8(fgColor.r, fgColor.g, fgColor.b, fgColor.a);
			GX_TexCoord2s16(texturePosX2, texturePosY2);
			
			GX_Position2s16(quadX1, quadY1);
			GX_Color4u8(fgColor.r, fgColor.g, fgColor.b, fgColor.a);
			GX_TexCoord2s16(texturePosX1, texturePosY2);
			*/
			GX_End();
		}
		
		// advance cursor
		cursorX += 10;
	}
	if (workingX != cursorX && !draw) {
		drawSolidBox(workingX + PRINT_PADDING_HORIZONTAL - 2, workingY + PRINT_PADDING_VERTICAL - 2,
		       cursorX + PRINT_PADDING_HORIZONTAL, cursorY + PRINT_PADDING_VERTICAL + 16, false, bgColor);
	}
	if (!draw) {
		cursorX = startingX;
		cursorY = startingY;
	}
}

void printStr(const char* str, ...) {
	va_list list;
	va_start(list, str);
	vsnprintf(strBuffer, 999, str, list);
	changeLoadedTexmap(TEXMAP_FONT);
	handleString(strBuffer, false, GX_COLOR_WHITE, GX_COLOR_BLACK);
	handleString(strBuffer, true, GX_COLOR_WHITE, GX_COLOR_BLACK);
	//drawString(COLOR_BLACK, COLOR_WHITE, strBuffer);
	va_end(list);
}

void printStrColor(const GXColor bg_color, const GXColor fg_color, const char* str, ...) {
	va_list list;
	va_start(list, str);
	vsnprintf(strBuffer, 999, str, list);
	handleString(strBuffer, false, fg_color, bg_color);
	handleString(strBuffer, true, fg_color, bg_color);
	//drawString(bg_color, fg_color, strBuffer);
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
	cursorY = row * (15 + LINE_SPACING);
	cursorX = col * 10;
}
