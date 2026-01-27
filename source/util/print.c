//
// Created on 2025/03/14.
//

// code for printing our custom font from a texture

// previously, this housed a direct framebuffer draw implementation,
// which was heavily modified from the PhobGCC project, specifically PhobVision

#include "util/print.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <ogc/pad.h>

#include "util/gx.h"

// buffer for variable arg strings
// there's a better way to do this...
static char strBuffer[1000];

// real screen coordinates of our 'cursor'
static int cursorX = 0;
static int cursorY = 0;

static int screenWidth = 640;

// z depth
// TODO: z depth for specific elements (font, quads, lines, etc) need to be standardized
static int cursorZ = -4;
static int cursorPrevZ = -4;

static void advanceCursorLine() {
	cursorX = 0;
	cursorY += 15 + LINE_SPACING;
}

// this is almost directly adapted from the provided romfont example, but modified for our specific fontsheet,
// as well as support for background colors
// TODO: this might not respect a call to setDepthForDrawCall(), investigate...
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
		if (cursorX + 10 > screenWidth - (PRINT_PADDING_HORIZONTAL * 2) || *curr == '\n') {
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
			drawSubTexture(quadX1, quadY1, quadX2, quadY2,
						   texturePosX1, texturePosY1, texturePosX2, texturePosY2,
						   fgColor);
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

void printStr(const char* str, ...) {
	va_list list;
	va_start(list, str);
	vsnprintf(strBuffer, 999, str, list);
	changeLoadedTexmap(TEXMAP_FONT);
	setDepth(cursorZ);
	handleString(strBuffer, true, GX_COLOR_WHITE, GX_COLOR_NONE);
	va_end(list);
	restorePrevDepth();
}

// TODO: there's a better way to do this instead of calling handleString twice...
// TODO: for some reason this breaks drawing if immediately followed by drawing using VTX_TEX_NOCOLOR, why?
// specifically, coordinateviewer.c for the circle breaks in that situation if it uses VTX_TEX_NOCOLOR...
void printStrColor(const GXColor bg_color, const GXColor fg_color, const char* str, ...) {
	va_list list;
	va_start(list, str);
	vsnprintf(strBuffer, 999, str, list);
	changeLoadedTexmap(TEXMAP_FONT);
	setDepth(cursorZ);
	// only do background if it isn't transparent
	// we don't actually do transparency on bg stuff, but its useful for this check...
	if (bg_color.a != 0x00) {
		handleString(strBuffer, false, fg_color, bg_color);
	}
	handleString(strBuffer, true, fg_color, bg_color);
	va_end(list);
	restorePrevDepth();
}

/*
void printStrButton(struct INSTRUCTION_ENTRY list[]) {
	int index = 0;
	struct INSTRUCTION_ENTRY currEntry = list[0];
	
	while (currEntry.text != NULL || currEntry.texture != NULL || currEntry.button != FONT_NONE) {
		if (currEntry.text != NULL) {
			printStr(currEntry.text);
			//printStrColor(GX_COLOR_GRAY, GX_COLOR_WHITE, currEntry.text);
		} else if (currEntry.button != FONT_NONE) {
			drawFontButton(currEntry.button);
		} else {
			
			int texwidth = 0, texheight = 0;
			getCurrentTexmapDims(&texwidth, &texheight);
			
			// advance to new line if we're printing full-size
			// this _technically_ is more repeat code than necessary, but it makes it much clearer for what's happening
			if (!currEntry.printInline) {
				advanceCursorLine();
				drawTextureFull(cursorX + PRINT_PADDING_HORIZONTAL, cursorY + PRINT_PADDING_VERTICAL, GX_COLOR_WHITE);
				cursorY += texheight;
				cursorX = 0;
			}
		}
		index++;
		currEntry = list[index];
	}
}*/

// TODO: this will assume that there are no newlines in this string, figure out how to do this properly...
// TODO: this might not respect a call to setDepthForDrawCall(), investigate...
void printStrBox(const GXColor box_color, const char* str, ...) {
	va_list list;
	va_start(list, str);
	vsnprintf(strBuffer, 999, str, list);
	changeLoadedTexmap(TEXMAP_FONT);
	
	// draw box behind text
	char *subString = strtok(strBuffer, "\n");
	int length = strlen(subString);
	
	// we don't do anything if we would draw off the screen
	if (cursorX + PRINT_PADDING_HORIZONTAL + 2 + (length * 10) <= screenWidth - (PRINT_PADDING_HORIZONTAL * 2)) {
		setDepth(cursorZ);
		GX_SetLineWidth(8, GX_TO_ZERO);
		drawBox(cursorX + PRINT_PADDING_HORIZONTAL - 5, cursorY + PRINT_PADDING_VERTICAL - 4,
		             cursorX + PRINT_PADDING_HORIZONTAL + (length * 10) + 2, cursorY + PRINT_PADDING_VERTICAL + 16,
					 box_color);
		GX_SetLineWidth(12, GX_TO_ZERO);
		
		handleString(subString, true, GX_COLOR_WHITE, GX_COLOR_BLACK);
		restorePrevDepth();
	}
	
	va_end(list);
}

void drawFontButton(enum FONT_BUTTON_LIST button) {
	cursorX += 4;
	if (cursorX + 20 > screenWidth - (PRINT_PADDING_HORIZONTAL * 2)) {
		advanceCursorLine();
	}
	
	int texX1 = 18 * (button % 8);
	int texY1 = 18 * (button / 8);
	changeLoadedTexmap(TEXMAP_FONT_BUTTON);
	drawSubTexture(cursorX + PRINT_PADDING_HORIZONTAL, cursorY + PRINT_PADDING_VERTICAL - 2,
	               cursorX + PRINT_PADDING_HORIZONTAL + 18, cursorY + PRINT_PADDING_VERTICAL + 16,
	               texX1, texY1, texX1 + 18, texY1 + 18, GX_COLOR_WHITE);
	cursorX += 24;
}

static int tempX = 0, tempY = 0;
static int scrollingOffset = 0;
static int scrollModifier = 0;
static int scrollBottomBound = 0;

static int scrollTop = 0, scrollBottom = 480;
static int scrollXMid = 320;

void resetScrollingPrint() {
	scrollingOffset = 0;
	scrollBottomBound = 0;
	scrollTop = 0;
	scrollBottom = 480;
	scrollXMid = 320;
	screenWidth = 640;
}

void startScrollingPrint(int x1, int y1, int x2, int y2) {
	// set bounds
	scrollTop = y1;
	scrollBottom = y2;
	
	screenWidth = x2 - x1;
	scrollXMid = x1 + (screenWidth / 2);
	// we restore the old xy after scrolling print is done
	tempX = cursorX;
	tempY = cursorY;
	
	// get stick position as a modifier
	scrollModifier = PAD_StickY(0) / 16;

	// apply offset
	scrollingOffset += scrollModifier;
	
	// hard limit scrolling
	// 0 is the soft top bound, we allow a 20 unit shift before stopping it completely
	if (scrollingOffset > 20) {
		scrollingOffset = 20;
	}
	// bottom soft bound is determined by what text was printed, and has the same 20 unit shift allowed
	if (scrollingOffset < scrollBottomBound - 20) {
		scrollingOffset = scrollBottomBound - 20;
	}
	
	// bring the offset back to either bound if necessary
	// again, top bound is always 0
	if (scrollingOffset > 5) {
		scrollingOffset -= 2;
	} else if (scrollingOffset > 0) {
		scrollingOffset--;
	} else if (scrollingOffset < scrollBottomBound - 5) {
		scrollingOffset += 2;
	} else if (scrollingOffset < scrollBottomBound) {
		scrollingOffset++;
	}
	
	// move cursor to top left of screen, subtract vertical padding so we actually start printing at y=~0
	setCursorXY(0, scrollingOffset - PRINT_PADDING_VERTICAL + 5);
	setSubwindowScissorBox(x1, y1, x2, y2);
	setCursorDepth(2);
}

void endScrollingPrint() {
	// there are two cases we need to handle:
	// - text is short enough that scrolling isn't necessary
	//   in this case, we only allow [-20,20]
	// - text is long enough that scrolling is necessary
	//   we need to determine the bottom bound
	
	int textHeight = (cursorY + PRINT_PADDING_VERTICAL) - scrollingOffset;
	
	// text didn't make it to the bottom, first case
	if (textHeight <= (scrollBottom - scrollTop)) {
		// set bounds
		scrollBottomBound = 0;
	}
	// drawn text is bigger than the allotted space, we need to calculate bottom bound
	else {
		// scrolling up is a negative offset, so we subtract the calculated value from zero
		// subtract window height from actual text height, +20 for the text's actual height
		scrollBottomBound = 0 - (textHeight - (scrollBottom - scrollTop) + 20);
	}
	
	// set everything back to normal
	restoreNormalScissorBox();
	setCursorXY(tempX, tempY);
	screenWidth = 640;
	
	// draw indicator for if there's more text in either direction
	// TODO: generalize this in gx.c with drawTri() or something...
	//  ideally, no raw drawing should be done outside of gx.c unless necessary
	if (scrollingOffset < 0 && scrollBottomBound != 0) {
		drawTri(scrollXMid - 10, scrollTop - 4,
				scrollXMid + 10, scrollTop - 4,
				scrollXMid, scrollTop - 10,
				GX_COLOR_WHITE);
	}
	if (scrollingOffset > scrollBottomBound && scrollBottomBound != 0) {
		drawTri(scrollXMid - 10, scrollBottom + 4,
				scrollXMid + 10, scrollBottom + 4,
				scrollXMid, scrollBottom + 10,
				GX_COLOR_WHITE);
	}
	restorePrevCursorDepth();
}

void printEllipse(const int counter, const int interval) {
	int remainder = counter / interval;
	while (remainder != 0) {
		printStr(".");
		remainder--;
	}
}

static char lineSpinArr[] = { '/', '-', '\\', '|' };
static int lineSpinArrIndex = 0;
static int currInterval = 15, intervalCounter = 15;

void printSpinningLine() {
	printSpinningLineInterval(15);
}

void printSpinningLineInterval(const int waitInterval) {
	if (currInterval != waitInterval) {
		currInterval = intervalCounter = waitInterval;
		lineSpinArrIndex = 0;
	}
	printStr("%c", lineSpinArr[lineSpinArrIndex]);
	intervalCounter--;
	if (intervalCounter == 0) {
		lineSpinArrIndex++;
		lineSpinArrIndex %= 4;
		intervalCounter = currInterval;
	}
}

void resetCursor() {
	setCursorPos(0,0);
	cursorZ = -4;
	cursorPrevZ = -4;
}

void setCursorPos(int row, int col) {
	cursorY = row * (15 + LINE_SPACING);
	cursorX = col * 10;
}

void setCursorXY(int x, int y) {
	cursorX = x;
	cursorY = y;
}

void setCursorDepth(int z) {
	cursorPrevZ = cursorZ;
	cursorZ = z;
}

void restorePrevCursorDepth() {
	cursorZ = cursorPrevZ;
}
