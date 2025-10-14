//
// Created on 2025/03/03.
//

#include "draw.h"

#include <stdint.h>
#include <stddef.h>

#include <ogc/color.h>

#include "images/stickmaps.h"

#define BUFFER_SIZE_X 128
#define BUFFER_SIZE_Y 255

static bool do2xHorizontalDraw = false;

uint32_t *currXfb = NULL;

void setFramebuffer(void *xfb) {
	currXfb = xfb;
}

void setInterlaced(bool interlaced) {
	do2xHorizontalDraw = interlaced;
}

enum IMAGE currImage = NO_IMAGE;

uint32_t imageBuffer[(BUFFER_SIZE_X)][BUFFER_SIZE_Y];

// Decodes a given image into imageBuffer
// decoding the image is costly, so we decode it once, then just copy from the decoded buffer on subsequent frames
static void copyImageToBuffer(const unsigned char image[], const unsigned char colorIndex[8]) {
	
	return;
	
	// get information on the image to be drawn
	// first four bytes are dimensions
	int newX = image[0] << 8 | image[1];
	int newY = image[2] << 8 | image[3];
	
	// don't do anything with an image that doesn't fit our buffer
	if (newY != BUFFER_SIZE_Y || newX != (BUFFER_SIZE_X * 2) - 1) {
		return;
	}
	
	//memset(imageBuffer, COLOR_BLACK, sizeof(uint32_t) * BUFFER_SIZE_X * BUFFER_SIZE_Y);
	for (int i = 0; i < BUFFER_SIZE_X; i++) {
		for (int j = 0; j < BUFFER_SIZE_Y; j++) {
			imageBuffer[i][j] = COLOR_BLACK;
		}
	}
	
	// start index for actual draw data
	uint32_t byte = 4;
	
	// counts how many pixels we've been through for a given byte
	uint8_t runIndex = 0;
	
	// how many pixels does a given byte draw?
	// first five bits are runlength
	uint8_t runLength = (image[byte] >> 3) + 1;
	
	// last three bits are color, lookup color in index
	uint8_t color = colorIndex[ image[byte] & 0b111];
	
	// used for skipping sections of no drawing that extend beyond a single row
	int carryover = 0;
	
	// begin processing data
	for (int row = 0; row < newY; row++) {
		// carryover will be added to column and then be reset immediately
		for (int column = 0 + carryover; column < newX; column++) {
			// reset carryover if its been set
			carryover = 0;
			
			// is there a pixel to actually draw? (0-4 is transparency)
			if (color > 5) {
				
				// copied from DrawDotAccurate
				uint32_t newColor = CUSTOM_COLORS[color - 5];
				uint32_t data = imageBuffer[column/2][row];
				if (column % 2 == 1) {
					if (data >> 24 == 0x10) {
						// no data in left pixel, leave it as-is
						imageBuffer[column/2][row] = newColor & 0x00FFFFFF;
					} else {
						// preserve the left pixel luminance
						uint32_t leftLuminance = data & 0xFF000000;
						uint32_t rightLuminance = newColor & 0x0000FF00;
						uint32_t cb, cr;
						// mix cb and cr
						cb = ( ((data & 0x00FF0000) >> 16) + ((newColor & 0x00FF0000) >> 16) ) / 2;
						cr = ( (data & 0x000000FF) + (newColor & 0x000000FF) ) / 2;
						imageBuffer[column/2][row] = leftLuminance | (cb << 16) | rightLuminance | cr;
					}
				} else {
					if ((data & 0xFFFF00FF) >> 8 == 0x10) {
						// no data in right pixel, leave it as-is
						imageBuffer[column/2][row] = newColor & 0xFFFF00FF;
					} else {
						// preserve the right pixel luminance
						uint32_t leftLuminance = newColor & 0xFF000000;
						uint32_t rightLuminance = data & 0x0000FF00;
						uint32_t cb, cr;
						// mix cb and cr
						cb = ( ((data & 0x00FF0000) >> 16) + ((newColor & 0x00FF0000) >> 16) ) / 2;
						cr = ( (data & 0x000000FF) + (newColor & 0x000000FF) ) / 2;
						imageBuffer[column/2][row] = leftLuminance | (cb << 16) | rightLuminance | cr;
					}
				}
				
				runIndex++;
			} else {
				// skip idle loop and move to next position
				column += runLength - 1;
				runIndex = runLength;
				// did we go outside the bounds of this row?
				if (column >= newX) {
					// set carryover
					carryover = column - newX + 1;
				}
			}
			
			// set up for the next byte of information/drawing
			if (runIndex >= runLength) {
				runIndex = 0;
				byte++;
				runLength = (image[byte] >> 3) + 1;
				color = colorIndex[ image[byte] & 0b111];
			}
		}
	}
}

void displayImage(enum IMAGE newImage, int offsetX, int offsetY) {
	
	return;
	// do we need to draw a different image?
	if (currImage != newImage) {
		switch (newImage) {
			case DEADZONE:
				copyImageToBuffer(deadzone_image, deadzone_indexes);
				break;
			case A_WAIT:
				copyImageToBuffer(await_image, await_indexes);
				break;
			case MOVE_WAIT:
				copyImageToBuffer(deadzone_image, deadzone_indexes);
				break;
			case CROUCH:
				copyImageToBuffer(crouch_image, crouch_indexes);
				break;
			case LEDGE_L:
				copyImageToBuffer(ledgeL_image, ledgeL_indexes);
				break;
			case LEDGE_R:
				copyImageToBuffer(ledgeR_image, ledgeR_indexes);
				break;
			case NO_IMAGE:
			default:
				break;
		}
		currImage = newImage;
	}
	
	// draw our image if its not "NO_IMAGE"
	if (currImage != NO_IMAGE) {
		for (int x = 0; x < BUFFER_SIZE_X; x++) {
			for (int y = 0; y < BUFFER_SIZE_Y; y++) {
				// every uint32_t in an xfb is two bytes
				// x * 2 in currxfb gives us the actual image's x value
				// which is then divided by 2 to give the corresponding position in currxfb
				// x is then added to y, to get the correct (vertical) line
				currXfb[((offsetX + (x * 2)) >> 1) + (640 * (y + offsetY)) / 2] = imageBuffer[x][y];
			}
		}
	}
}

// draws a "runlength encoded" image, with top-left at the provided coordinates
// most of this is taken from
// https://github.com/PhobGCC/PhobGCC-SW/blob/main/PhobGCC/rp2040/src/drawImage.cpp

// Old implementation that calls DrawDotAccurate()
/*
void drawImage(const unsigned char image[], const unsigned char colorIndex[8], uint16_t offsetX, uint16_t offsetY) {
	// get information on the image to be drawn
	// first four bytes are dimensions
	uint32_t width = image[0] << 8 | image[1];
	uint32_t height = image[2] << 8 | image[3];
	
	// where image drawing ends
	// calculated in advance for use in the loop
	uint32_t imageEndpointX = offsetX + width;
	uint32_t imageEndpointY = offsetY + height;
	
	// ensure image won't go out of bounds
	if (imageEndpointX > 640 || imageEndpointY > 480) {
		return;
		//printf("Image with given parameters will write incorrectly\n");
	}
	
	// start index for actual draw data
	uint32_t byte = 4;
	
	// counts how many pixels we've been through for a given byte
	uint8_t runIndex = 0;
	
	// how many pixels does a given byte draw?
	// first five bits are runlength
	uint8_t runLength = (image[byte] >> 3) + 1;
	
	// last three bits are color, lookup color in index
	uint8_t color = colorIndex[ image[byte] & 0b111];
	
	// used for skipping sections of no drawing that extend beyond a single row
	int carryover = 0;
	
	// begin processing data
	for (int row = offsetY; row < imageEndpointY; row++) {
		// carryover will be added to column and then be reset immediately
		for (int column = offsetX + carryover; column < imageEndpointX; column++) {
			// reset carryover if its been set
			carryover = 0;
			
			// is there a pixel to actually draw? (0-4 is transparency)
			if (color > 5) {
				DrawDotAccurate(column, row, CUSTOM_COLORS[color - 5]);
				runIndex++;
			} else {
				// skip idle loop and move to next position
				column += runLength - 1;
				runIndex = runLength;
				// did we go outside the bounds of this row?
				if (column >= imageEndpointX) {
					// set carryover
					carryover = column - imageEndpointX + 1;
				}
			}
			
			// set up for the next byte of information/drawing
			if (runIndex >= runLength) {
				runIndex = 0;
				byte++;
				runLength = (image[byte] >> 3) + 1;
				color = colorIndex[ image[byte] & 0b111];
			}
		}
	}
}
 */

// taken from github.com/phobgcc/phobconfigtool
// should probably replace this with something gl based at some point
/*
* takes in values to draw a horizontal line of a given color
*/
void DrawHLine (int x1, int x2, int y, int color) {
	
	return;
	for (int i = x1; i <= x2; i++) {
		DrawDotAccurate(i, y, color);
	}
}


/*
* takes in values to draw a vertical line of a given color
*/
void DrawVLine (int x, int y1, int y2, int color) {
	
	return;
	for (int i = y1; i <= y2; i++) {
		DrawDot(x, i, color);
	}
}


/*
* takes in values to draw a box of a given color
*/
void DrawBox (int x1, int y1, int x2, int y2, int color) {
	
	return;
	DrawHLine (x1, x2, y1, color);
	DrawHLine (x1, x2, y2, color);
	DrawVLine (x1, y1, y2, color);
	DrawVLine (x2, y1, y2, color);
}


// since every pixel between should be filled, we don't worry about "accurate" drawing
void DrawFilledBox (int x1, int y1, int x2, int y2, int color) {
	
	return;
	for (int i = x1; i < x2 + 1; i++) {
		for (int j = y1; j < y2; j++) {
			currXfb[(i >> 1) + (640 * j) / 2] = color;
		}
	}
}


// draw a line given two coordinates, using Bresenham's line-drawing algorithm
void DrawLine(int x1, int y1, int x2, int y2, int color) {
	
	return;
	// use simpler algorithm if line is horizontal or vertical
	if (x1 == x2) {
		if (y1 < y2) {
			DrawVLine(x1, y1, y2, color);
		} else {
			DrawVLine(x1, y2, y1, color);
		}
		return;
	}
	if (y1 == y2) {
		if (x1 < x2) {
			DrawHLine(x1, x2, y1, color);
		} else {
			DrawHLine(x2, x1, y1, color);
		}
		return;
	}
	int distanceX = x2 - x1, distanceY = y2 - y1;
	// used for when one coordinate goes negative
	int xDir = 1, yDir = 1;
	
	// make sure our magnitude is positive, and note if the drawing direction should be negative
	if (distanceX < 0) {
		xDir = -1;
		distanceX *= -1;
	}
	if (distanceY < 0) {
		yDir = -1;
		distanceY *= -1;
	}
	
	// x will be our primary axis
	if (distanceX > distanceY) {
		int delta = 2 * distanceY - distanceX;
		int currY = y1;
		
		for (int i = 0; i < distanceX; i++) {
			DrawDot(x1 + (i * xDir), currY, color);
			if (delta > 0) {
				currY += (1 * yDir);
				delta -= (2 * distanceX);
			}
			delta += (2 * distanceY);
		}
		// y will be our primary axis
	} else {
		int delta = 2 * distanceX - distanceY;
		int currX = x1;
		
		for (int i = 0; i < distanceY; i++) {
			DrawDot(currX, (y1 + (i * yDir)), color);
			//tmpfb[(((y1 + (i * yDir)) * 640) + currX) / 2] = color;
			//tmpfb[((x1 + i) + (currY * 640)) / 2] = color;
			if (delta > 0) {
				currX += (1 * xDir);
				delta -= (2 * distanceY);
			}
			delta += (2 * distanceX);
		}
	}
}


void DrawDot (int x, int y, int color) {
	
	return;
	if (do2xHorizontalDraw) {
		x >>= 1;
		currXfb[x + (640 * y) / 2] = color;
	} else {
		DrawDotAccurate(x, y, color);
	}
}


void DrawDotAccurate (int x, int y, int color) {
	
	return;
	int index = (x >> 1) + (640 * y) / 2;
	uint32_t data = currXfb[index];
	
	if (x % 2 == 1) {
		if (data >> 24 == 0) {
			// no data in left pixel, leave it as-is
			currXfb[index] = color & 0x00FFFFFF;
		} else {
			// preserve the left pixel luminance
			uint32_t leftLuminance = data & 0xFF000000;
			uint32_t rightLuminance = color & 0x0000FF00;
			uint32_t cb, cr;
			// mix cb and cr
			cb = ( ((data & 0x00FF0000) >> 16) + ((color & 0x00FF0000) >> 16) ) / 2;
			cr = ( (data & 0x000000FF) + (color & 0x000000FF) ) / 2;
			currXfb[index] = leftLuminance | (cb << 16) | rightLuminance | cr;
		}
	} else {
		if ((data & 0xFFFF00FF) >> 8 == 0) {
			// no data in right pixel, leave it as-is
			currXfb[index] = color & 0xFFFF00FF;
		} else {
			// preserve the right pixel luminance
			uint32_t leftLuminance = color & 0xFF000000;
			uint32_t rightLuminance = data & 0x0000FF00;
			uint32_t cb, cr;
			// mix cb and cr
			cb = ( ((data & 0x00FF0000) >> 16) + ((color & 0x00FF0000) >> 16) ) / 2;
			cr = ( (data & 0x000000FF) + (color & 0x000000FF) ) / 2;
			currXfb[index] = leftLuminance | (cb << 16) | rightLuminance | cr;
		}
	}
}


// mostly taken from https://www.geeksforgeeks.org/mid-point-circle-drawing-algorithm/
void DrawCircle (int cx, int cy, int r, int color) {
	
	return;
	int x = r, y = 0;
	
	if (r > 0) {
		DrawDotAccurate(cx + x, cy - y, color);
		DrawDotAccurate(cx - x, cy + y, color);
		DrawDotAccurate(cx + y, cy - x, color);
		DrawDotAccurate(cx - y, cy + x, color);
	}
	
	int delta = 1 - r;
	
	while (x > y) {
		y++;
		
		if (delta <= 0) {
			delta = delta + 2*y + 1;
		} else {
			x--;
			delta = delta + 2 * y - 2 * x + 1;
		}
		
		if (x < y) {
			break;
		}
		
		DrawDotAccurate(cx + x, cy + y, color);
		DrawDotAccurate(cx - x, cy - y, color);
		DrawDotAccurate(cx + x, cy - y, color);
		DrawDotAccurate(cx - x, cy + y, color);
		
		if (x != y) {
			DrawDotAccurate(cx + y, cy + x, color);
			DrawDotAccurate(cx - y, cy + x, color);
			DrawDotAccurate(cx + y, cy - x, color);
			DrawDotAccurate(cx - y, cy - x, color);
		}
	}
}


// taken from
// https://stackoverflow.com/questions/1201200/fast-algorithm-for-drawing-filled-circles
// originally this just called DrawCircle for a smaller radius, but it broke when I fixed the DrawDot function.
void DrawFilledCircle(int cx, int cy, int r, int color) {
	
	return;
	for (int ty = (r * -1); ty <= r; ty++) {
		for (int tx = (r * -1); tx <= r; tx++) {
			if ( (tx * tx) + (ty * ty) <= (r * r)) {
				DrawDotAccurate(cx + tx, cy + ty, color);
			}
		}
	}
}


// rad should be number of pixels from the center, _not_ including the center
void DrawFilledBoxCenter(int x, int y, int rad, int color) {
	
	return;
	int topLeftX = x - rad;
	int topLeftY = y - rad;
	DrawFilledBox(topLeftX, topLeftY, x + rad, y + rad, color);
}

void DrawOctagonalGate(int x, int y, int scale, int color) {
	
	return;
	const int CARDINAL_MAX = 100 / scale;
	const int DIAGONAL_MAX = 74 / scale;
	// analog stick
	DrawLine(x - CARDINAL_MAX, y,
	         x - DIAGONAL_MAX, y - DIAGONAL_MAX,
	         color);
	DrawLine(x - DIAGONAL_MAX, y - DIAGONAL_MAX,
	         x, y - CARDINAL_MAX,
	         color);
	DrawLine(x, y - CARDINAL_MAX,
	         x + DIAGONAL_MAX, y - DIAGONAL_MAX,
	         color);
	DrawLine(x + DIAGONAL_MAX, y - DIAGONAL_MAX,
	         x + CARDINAL_MAX, y,
	         color);
	DrawLine(x - CARDINAL_MAX, y,
	         x - DIAGONAL_MAX, y + DIAGONAL_MAX,
	         color);
	DrawLine(x - DIAGONAL_MAX, y + DIAGONAL_MAX,
	         x, y + CARDINAL_MAX,
	         color);
	DrawLine(x, y + CARDINAL_MAX,
	         x + DIAGONAL_MAX, y + DIAGONAL_MAX,
	         color);
	DrawLine(x + DIAGONAL_MAX, y + DIAGONAL_MAX,
	         x + CARDINAL_MAX, y,
	         color);
}


void DrawStickmapOverlay(enum STICKMAP_LIST stickmap, int which) {
	
	return;
	switch (stickmap) {
		case (FF_WD):
			// bools for which parts to draw
			// this is dumb, but avoids duplicate code
			bool drawSafe = true, drawUnsafe = true;
			
			switch ((enum STICKMAP_FF_WD_ENUM) which) {
				case (FF_WD_SAFE):
					drawUnsafe = false;
					break;
				case (FF_WD_UNSAFE):
					drawSafe = false;
					break;
					// miss is interpreted as "all" in this case
				case (FF_WD_MISS):
				default:
					break;
			}
			
			if (drawSafe) {
				for (int i = 0; i < STICKMAP_FF_WD_COORD_SAFE_LEN; i++) {
					DrawFilledBoxCenter(toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]) + COORD_CIRCLE_CENTER_X,
					                    toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_LIME);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]),
					                    toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_LIME);
					DrawFilledBoxCenter(toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]) + COORD_CIRCLE_CENTER_X,
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]),
					                    2, COLOR_LIME);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]),
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]),
					                    2, COLOR_LIME);
					DrawFilledBoxCenter(toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]) + COORD_CIRCLE_CENTER_X,
					                    toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_LIME);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]),
					                    toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_LIME);
					DrawFilledBoxCenter(toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]) + COORD_CIRCLE_CENTER_X,
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]),
					                    2, COLOR_LIME);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]),
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]),
					                    2, COLOR_LIME);
				}
			}
			if (drawUnsafe) {
				for (int i = 0; i < STICKMAP_FF_WD_COORD_UNSAFE_LEN; i++) {
					DrawFilledBoxCenter(toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]) + COORD_CIRCLE_CENTER_X,
					                    toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_YELLOW);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]),
					                    toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_YELLOW);
					DrawFilledBoxCenter(toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]) + COORD_CIRCLE_CENTER_X,
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]),
					                    2, COLOR_YELLOW);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]),
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]),
					                    2, COLOR_YELLOW);
					DrawFilledBoxCenter(toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]) + COORD_CIRCLE_CENTER_X,
					                    toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_YELLOW);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]),
					                    toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_YELLOW);
					DrawFilledBoxCenter(toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]) + COORD_CIRCLE_CENTER_X,
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]),
					                    2, COLOR_YELLOW);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]),
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]),
					                    2, COLOR_YELLOW);
				}
			}
			break;
		case (SHIELDDROP):
			// bools for which parts to draw
			// this is dumb, but avoids duplicate code
			bool drawVanilla = true, drawUCFUpper = true, drawUCFLower = true;
			switch ((enum STICKMAP_SHIELDDROP_ENUM) which) {
				case (SHIELDDROP_VANILLA):
					drawUCFUpper = false;
					drawUCFLower = false;
					break;
				case (SHIELDDROP_UCF_LOWER):
					drawVanilla = false;
					drawUCFUpper = false;
					break;
				case (SHIELDDROP_UCF_UPPER):
					drawVanilla = false;
					drawUCFLower = false;
					break;
				case (SHIELDDROP_MISS):
				default:
					break;
			}
			
			if (drawUCFUpper) {
				for (int i = 0; i < STICKMAP_SHIELDDROP_COORD_UCF_UPPER_LEN; i++) {
					DrawFilledBoxCenter(toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][0]) + COORD_CIRCLE_CENTER_X,
					                    toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][1]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_YELLOW);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][0]),
					                    toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][1]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_YELLOW);
				}
			}
			if (drawUCFLower) {
				for (int i = 0; i < STICKMAP_SHIELDDROP_COORD_UCF_LOWER_LEN; i++) {
					DrawFilledBoxCenter(toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][0]) + COORD_CIRCLE_CENTER_X,
					                    toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][1]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_BLUE);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][0]),
					                    toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][1]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_BLUE);
				}
			}
			if (drawVanilla) {
				for (int i = 0; i < STICKMAP_SHIELDDROP_COORD_VANILLA_LEN; i++) {
					DrawHLine(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][0]) - 1,
					          toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][0]) + COORD_CIRCLE_CENTER_X + 1,
					          toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][1]) + SCREEN_POS_CENTER_Y, COLOR_LIME);
					DrawHLine(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][0]) - 1,
					          toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][0]) + COORD_CIRCLE_CENTER_X + 1,
					          toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][1]) + SCREEN_POS_CENTER_Y + 1, COLOR_LIME);
				}
			}
			break;
		default:
			break;
	}
}
