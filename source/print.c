//
// Created on 2025/03/14.
//

// code for printing our custom font from a texture

// previously, this housed a direct framebuffer draw implementation,
// which was heavily modified from the PhobGCC project, specifically PhobVision

#include "print.h"

#include <stdio.h>
#include <stdarg.h>

#include "gx.h"

// buffer for variable arg strings
static char strBuffer[1000];

// real screen coordinates of our 'cursor'
static int cursorX = 0;
static int cursorY = 0;

// this is almost directly adapted from the provided romfont example, but modified for our specific fontsheet,
// as well as support for background colors
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
			       cursorX + PRINT_PADDING_HORIZONTAL, cursorY + PRINT_PADDING_VERTICAL + 15, bgColor);
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
			
			GX_Position3s16(quadX1, quadY1, 0);
			GX_Color4u8(fgColor.r, fgColor.g, fgColor.b, fgColor.a);
			GX_TexCoord2s16(texturePosX1, texturePosY1);
			
			GX_Position3s16(quadX2, quadY1, 0);
			GX_Color4u8(fgColor.r, fgColor.g, fgColor.b, fgColor.a);
			GX_TexCoord2s16(texturePosX2, texturePosY1);
			
			GX_Position3s16(quadX2, quadY2, 0);
			GX_Color4u8(fgColor.r, fgColor.g, fgColor.b, fgColor.a);
			GX_TexCoord2s16(texturePosX2, texturePosY2);
			
			GX_Position3s16(quadX1, quadY2, 0);
			GX_Color4u8(fgColor.r, fgColor.g, fgColor.b, fgColor.a);
			GX_TexCoord2s16(texturePosX1, texturePosY2);
			
			GX_End();
		}
		
		// advance cursor
		cursorX += 10;
	}
	if (workingX != cursorX && !draw) {
		drawSolidBox(workingX + PRINT_PADDING_HORIZONTAL - 2, workingY + PRINT_PADDING_VERTICAL - 2,
		       cursorX + PRINT_PADDING_HORIZONTAL, cursorY + PRINT_PADDING_VERTICAL + 15, bgColor);
	}
	if (!draw) {
		cursorX = startingX;
		cursorY = startingY;
	}
}

// TODO: there's a better way to do this instead of calling handleString twice...
void printStr(const char* str, ...) {
	va_list list;
	va_start(list, str);
	vsnprintf(strBuffer, 999, str, list);
	changeLoadedTexmap(TEXMAP_FONT);
	handleString(strBuffer, false, GX_COLOR_WHITE, GX_COLOR_BLACK);
	handleString(strBuffer, true, GX_COLOR_WHITE, GX_COLOR_BLACK);
	va_end(list);
}

void printStrColor(const GXColor bg_color, const GXColor fg_color, const char* str, ...) {
	va_list list;
	va_start(list, str);
	vsnprintf(strBuffer, 999, str, list);
	handleString(strBuffer, false, fg_color, bg_color);
	handleString(strBuffer, true, fg_color, bg_color);
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

void setCursorXY(int x, int y) {
	cursorX = x;
	cursorY = y;
}
