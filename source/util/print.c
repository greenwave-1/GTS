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

static int changeButtons = 0;

static int workingHorizontalPadding = PRINT_PADDING_HORIZONTAL;

static bool allowWordWrap = false;
static int lastSpaceListIndex = 0;
static const char* lastSpaceList[50] = { NULL };

// this is almost directly adapted from the provided romfont example, but modified for our specific fontsheet,
// as well as support for background colors
// TODO: this might not respect a call to setDepthForDrawCall(), investigate...
static void handleString(bool draw, GXColor bgColor, GXColor fgColor) {
	// pointer for iterating over string
	const char* curr = strBuffer;
	
	// stores size of a given character from the font
	int texturePosX1, texturePosY1;
	
	// starting cursor values, stored so that we can restore them if we didn't actually print anything
	int startingX = cursorX, startingY = cursorY;
	// "working" values, used to determine if we need to draw a background color
	int workingX = startingX, workingY = startingY;
	
	int workingSpaceIndex = 0;
	
	// loop until we hit a null terminator
	for ( ; *curr != '\0'; curr++) {
		// lower than space, larger than tilde, and not newline
		if((*curr < 0x20 && *curr != '\n') || *curr > 0x7e) {
			continue;
		}
		
		// move to new line if needed
		if (draw && curr == lastSpaceList[workingSpaceIndex] && workingSpaceIndex != lastSpaceListIndex && allowWordWrap) {
			cursorY += 15 + LINE_SPACING;
			cursorX = 0;
			workingSpaceIndex++;
			continue;
		}
		
		// get texture coordinates
		// font sheet starts at 0x20 ascii, 10 chars per line
		int charIndex = (*curr) - 0x20;
		texturePosX1 = (charIndex % 10) * 8;
		texturePosY1 = (charIndex / 10) * 16;
		
		// go to a "new line" if drawing would put us outside the safe area, or if newline
		if (cursorX + 10 > screenWidth - (workingHorizontalPadding * 2) || *curr == '\n') {
			// move cursor back to the last 'space' char we encountered
			if (*curr != ' ' && *curr != '\n' && !draw && allowWordWrap) {
				// cursorX gets set to zero later, but this is for bg drawing if applicable
				cursorX -= (10 * (curr - lastSpaceList[lastSpaceListIndex]));
				// move current char back to last non-space char
				curr = lastSpaceList[lastSpaceListIndex];
				// we only increment here because _this_ space is the important one
				lastSpaceListIndex++;
			}
			
			// draw our background color, if applicable
			if (!draw && !allowWordWrap) {
				drawSolidBox(workingX + workingHorizontalPadding - 2, workingY + PRINT_PADDING_VERTICAL - 2,
			       cursorX + workingHorizontalPadding, cursorY + PRINT_PADDING_VERTICAL + 15, bgColor);
			}
			cursorY += 15 + LINE_SPACING;
			cursorX = 0;
			workingX = cursorX;
			workingY = cursorY;
			// advance cursor by one if newline or space
			if (*curr == '\n' || (*curr == ' ' && allowWordWrap)) {
				continue;
			}
		}
		
		// store last encountered space char
		if (*curr == ' ' && !draw && allowWordWrap) {
			lastSpaceList[lastSpaceListIndex] = curr;
		}
		
		// determine real coordinates for drawing
		if (draw) {
			int quadX1 = cursorX + workingHorizontalPadding;
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
	if (workingX != cursorX && !draw && !allowWordWrap) {
		drawSolidBox(workingX + workingHorizontalPadding - 2, workingY + PRINT_PADDING_VERTICAL - 2,
		       cursorX + workingHorizontalPadding, cursorY + PRINT_PADDING_VERTICAL + 15, bgColor);
	}
	if (!draw) {
		cursorX = startingX;
		cursorY = startingY;
	}
}

// TODO: there's a better way to do this instead of calling handleString twice...
static void handleStringPre(const GXColor bg_color, const GXColor fg_color) {
	// reset word wrap index if needed
	if (allowWordWrap) {
		lastSpaceListIndex = 0;
	}
	
	// setup
	changeLoadedTexmap(TEXMAP_FONT);
	setDepth(cursorZ);
	
	// do a first loop to draw background color if needed
	if (bg_color.a != 0x00 || allowWordWrap) {
		handleString(false, bg_color, fg_color);
	}
	handleString(true, bg_color, fg_color);
	restorePrevDepth();
}

void printStr(const char* str, ...) {
	va_list list;
	va_start(list, str);
	vsnprintf(strBuffer, 999, str, list);
	va_end(list);
	
	handleStringPre(GX_COLOR_NONE, GX_COLOR_WHITE);
}

void printStrColor(const GXColor bg_color, const GXColor fg_color, const char* str, ...) {
	va_list list;
	va_start(list, str);
	vsnprintf(strBuffer, 999, str, list);
	va_end(list);
	
	handleStringPre(bg_color, fg_color);
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
				drawTextureFull(cursorX + workingHorizontalPadding, cursorY + PRINT_PADDING_VERTICAL, GX_COLOR_WHITE);
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
	va_end(list);
	
	// get text before a newline, if encountered
	char *subString = strtok(strBuffer, "\n");
	int length = strlen(subString);
	
	// we don't do anything if we would draw off the screen
	if (cursorX + 2 + (length * 10) <= screenWidth - (workingHorizontalPadding * 2)) {
		GX_SetLineWidth(8, GX_TO_ZERO);
		
		setDepthForDrawCall(cursorZ);
		drawBox(cursorX + workingHorizontalPadding - 5, cursorY + PRINT_PADDING_VERTICAL - 4,
		             cursorX + workingHorizontalPadding + (length * 10) + 2, cursorY + PRINT_PADDING_VERTICAL + 16,
					 box_color);
		
		GX_SetLineWidth(12, GX_TO_ZERO);
		
		handleStringPre(GX_COLOR_BLACK, GX_COLOR_WHITE);
	}
}

static bool dpadFlash = false;
void fontButtonFlashIncrement(int *counter, int increment) {
	(*counter)++;
	if (*counter >= increment) {
		dpadFlash = !dpadFlash;
		*counter = 0;
	}
}

static int dpadDirections = FONT_DPAD_NONE;
void fontButtonSetDpadDirections(int selection) {
	dpadDirections = selection;
}

void drawFontButton(enum FONT_BUTTON_LIST button) {
	if (button == FONT_NONE) {
		return;
	}
	
	cursorX += 4;
	if (cursorX + 20 > screenWidth - (workingHorizontalPadding * 2)) {
		advanceCursorLine();
	}
	
	int texX1 = 20 * (button % 6);
	int texY1 = 20 * (button / 6);
	
	changeLoadedTexmap(TEXMAP_FONT_BUTTON);
	drawSubTexture(cursorX + workingHorizontalPadding - 1, cursorY + PRINT_PADDING_VERTICAL - 2,
	               cursorX + workingHorizontalPadding + 19, cursorY + PRINT_PADDING_VERTICAL + 18,
	               texX1, texY1, texX1 + 20, texY1 + 20, GX_COLOR_WHITE);
	
	// draw dpad overlay if applicable
	if (dpadDirections != FONT_DPAD_NONE && button == FONT_DPAD) {
		// move texture to dpad overlay
		texX1 += 20;
		
		// xy offsets for screenspace coordinates,
		const int dpadScreenspaceOffset[][4] = {
				{ 0, 0, 0, 0 },
				{ 12, 0, 0, 12 },
				{ 0, 12, 0, 12 },
				{ 0, 0, -12, 12 }
		};
		
		// four possible directions
		// up -> right -> down -> left
		for (int i = 0; i < 4; i++) {
			// dpadDirections has directions or'd together, so continue to shift 1 to check each
			if (dpadDirections & (1 << i)) {
				// i matches the rotation directions, just use it:
				// 0 (dont rotate) -> 90 -> 180 -> 270
				rotateTextureForDraw((enum TEX_ROTATE) i);
				
				if (dpadFlash) {
					drawSubTexture(cursorX + workingHorizontalPadding - 1 + dpadScreenspaceOffset[i][0],
					               cursorY + PRINT_PADDING_VERTICAL - 2 + dpadScreenspaceOffset[i][1],
					               cursorX + workingHorizontalPadding + 19 + dpadScreenspaceOffset[i][2],
					               cursorY + PRINT_PADDING_VERTICAL + 6 + dpadScreenspaceOffset[i][3],
					               texX1, texY1 + 20, texX1 + 20, texY1 + 12, GX_COLOR_WHITE);
				} else {
					drawSubTexture(cursorX + workingHorizontalPadding - 1 + dpadScreenspaceOffset[i][0],
					               cursorY + PRINT_PADDING_VERTICAL - 2 + dpadScreenspaceOffset[i][1],
					               cursorX + workingHorizontalPadding + 19 + dpadScreenspaceOffset[i][2],
					               cursorY + PRINT_PADDING_VERTICAL + 6 + dpadScreenspaceOffset[i][3],
					               texX1, texY1, texX1 + 20, texY1 + 8, GX_COLOR_WHITE);
				}
			}
		}
		
		// unset after we're done
		dpadDirections = FONT_DPAD_NONE;
	}

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
	workingHorizontalPadding = 10;
	setSubwindowScissorBox(x1, y1, x2, y2);
	setCursorDepth(2);
}

void endScrollingPrint() {
	// there are two cases we need to handle:
	// - text is short enough that scrolling isn't necessary
	//   in this case, we only allow [-20,20]
	// - text is long enough that scrolling is necessary
	//   we need to determine the bottom bound
	
	int textHeight = (cursorY + PRINT_PADDING_VERTICAL + 15) - scrollingOffset;
	
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
	workingHorizontalPadding = PRINT_PADDING_HORIZONTAL;
	
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
	lastSpaceListIndex = 0;
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
	lastSpaceListIndex = 0;
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

void setWordWrap(bool enable) {
	allowWordWrap = enable;
}


int swapButtonTex() {
	changeButtons++;
	changeButtons %= 3;
	
	return changeButtons;
}
