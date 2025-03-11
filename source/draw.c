//
// Created on 3/3/25.
//

#include "draw.h"
//#include "images/stickmaps.h"
#include "images/custom_colors.h"

// most of this is taken from
// https://github.com/PhobGCC/PhobGCC-SW/blob/main/PhobGCC/rp2040/src/drawImage.cpp
void drawImage(void *currXfb, const unsigned char image[], const unsigned char colorIndex[8], u16 offsetX, u16 offsetY) {
	// get information on the image to be drawn
	u32 width = image[0] << 8 | image[1];
	u32 height = image[2] << 8 | image[3];
	
	// where image drawing ends
	// calculated in advance for use in the loop
	u32 imageEndpointX = offsetX + width;
	u32 imageEndpointY = offsetY + height;
	
	// ensure image won't go out of bounds
	if (imageEndpointX > 640 || imageEndpointY > 480) {
		return;
		//printf("Image with given parameters will write incorrectly\n");
	}
	
	u32 byte = 4;
	u8 runIndex = 0;
	// first five bits are runlength
	u8 runLength = (image[byte] >> 3) + 1;
	// last three bits are color, lookup color in index
	u8 color = colorIndex[ image[byte] & 0b111];
	// begin processing data
	for (int row = offsetY; row < imageEndpointY; row++) {
		for (int column = offsetX; column < imageEndpointX; column++) {
			// is there a pixel to actually draw? (0-4 is transparency)
			if (color >= 5) {
				DrawDot(column, row, CUSTOM_COLORS[color - 5], currXfb);
			}
			
			runIndex++;
			if (runIndex >= runLength) {
				runIndex = 0;
				byte++;
				runLength = (image[byte] >> 3) + 1;
				color = colorIndex[ image[byte] & 0b111];
			}
		}
	}
}

// taken from github.com/phobgcc/phobconfigtool
// should probably replace this with something gl based at some point
/*
* takes in values to draw a horizontal line of a given color
*/
void DrawHLine (int x1, int x2, int y, int color, void *xfb) {
	int i;
	y = 320 * y;
	x1 >>= 1;
	x2 >>= 1;
	for (i = x1; i <= x2; i++) {
		u32 *tmpfb = xfb;
		tmpfb[y+i] = color;
	}
}


/*
* takes in values to draw a vertical line of a given color
*/
void DrawVLine (int x, int y1, int y2, int color, void *xfb) {
	int i;
	x >>= 1;
	for (i = y1; i <= y2; i++) {
		u32 *tmpfb = xfb;
		tmpfb[x + (640 * i) / 2] = color;
	}
}


/*
* takes in values to draw a box of a given color
*/
void DrawBox (int x1, int y1, int x2, int y2, int color, void *xfb) {
	DrawHLine (x1, x2, y1, color, xfb);
	DrawHLine (x1, x2, y2, color, xfb);
	DrawVLine (x1, y1, y2, color, xfb);
	DrawVLine (x2, y1, y2, color, xfb);
}


void DrawFilledBox (int x1, int y1, int x2, int y2, int color, void *xfb) {
	for (int i = x1; i < x2 + 1; i++) {
		DrawVLine(i, y1, y2, color, xfb);
	}
}


// draw a line given two coordinates, using Bresenham's line-drawing algorithm
void DrawLine(int x1, int y1, int x2, int y2, int color, void *xfb) {
	int distanceX = x2 - x1, distanceY = y2 - y1;
	u32 *tmpfb = xfb;
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
			tmpfb[((x1 + (i * xDir)) + (currY * 640)) / 2] = color;
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
			tmpfb[(((y1 + (i * yDir)) * 640) + currX) / 2] = color;
			//tmpfb[((x1 + i) + (currY * 640)) / 2] = color;
			if (delta > 0) {
				currX += (1 * xDir);
				delta -= (2 * distanceY);
			}
			delta += (2 * distanceX);
		}
	}
}


void DrawDot (int x, int y, int color, void *xfb) {
	x >>= 1;
	u32 *tmpfb = xfb;
	tmpfb[x + (640 * y) / 2] = color;
}


// mostly taken from https://www.geeksforgeeks.org/mid-point-circle-drawing-algorithm/
void DrawCircle (int cx, int cy, int r, int color, void *xfb) {
	int x = r, y = 0;
	
	if (r > 0) {
		DrawDot(cx + x, cy - y, color, xfb);
		DrawDot(cx - x, cy + y, color, xfb);
		DrawDot(cx + y, cy + x, color, xfb);
		DrawDot(cx - y, cy + x, color, xfb);
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
		
		DrawDot(cx + x, cy + y, color, xfb);
		DrawDot(cx - x, cy - y, color, xfb);
		DrawDot(cx + x, cy - y, color, xfb);
		DrawDot(cx - x, cy + y, color, xfb);
		
		if (x != y) {
			DrawDot(cx + y, cy + x, color, xfb);
			DrawDot(cx - y, cy + x, color, xfb);
			DrawDot(cx + y, cy - x, color, xfb);
			DrawDot(cx - y, cy - x, color, xfb);
		}
	}
}


void DrawFilledCircle(int cx, int cy, int r, int interval, int color, void *xfb) {
	for (int i = r; i > 0; i -= interval) {
		DrawCircle(cx, cy, i, color, xfb);
	}
}

// rad should be number of pixels from the center, _not_ including the center
void DrawFilledBoxCenter(int x, int y, int rad, int color, void *xfb) {
	int topLeftX = x - rad;
	int topLeftY = y - rad;
	DrawFilledBox(topLeftX, topLeftY, x + rad, y + rad, color, xfb);
}

void DrawOctagonalGate(int x, int y, int scale, int color, void *xfb) {
	const int CARDINAL_MAX = 100 / scale;
	const int DIAGONAL_MAX = 74 / scale;
	// analog stick
	DrawLine(x - CARDINAL_MAX, y,
	         x - DIAGONAL_MAX, y - DIAGONAL_MAX,
	         color, xfb);
	DrawLine(x - DIAGONAL_MAX, y - DIAGONAL_MAX,
	         x, y - CARDINAL_MAX,
	         color, xfb);
	DrawLine(x, y - CARDINAL_MAX,
	         x + DIAGONAL_MAX, y - DIAGONAL_MAX,
	         color, xfb);
	DrawLine(x + DIAGONAL_MAX, y - DIAGONAL_MAX,
	         x + CARDINAL_MAX, y,
	         color, xfb);
	DrawLine(x - CARDINAL_MAX, y,
	         x - DIAGONAL_MAX, y + DIAGONAL_MAX,
	         color, xfb);
	DrawLine(x - DIAGONAL_MAX, y + DIAGONAL_MAX,
	         x, y + CARDINAL_MAX,
	         color, xfb);
	DrawLine(x, y + CARDINAL_MAX,
	         x + DIAGONAL_MAX, y + DIAGONAL_MAX,
	         color, xfb);
	DrawLine(x + DIAGONAL_MAX, y + DIAGONAL_MAX,
	         x + CARDINAL_MAX, y,
	         color, xfb);
}

void DrawStickmapOverlay(enum STICKMAP_LIST stickmap, int which, void *currXfb) {
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
					                    2, COLOR_LIME, currXfb);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]),
					                    toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_LIME, currXfb);
					DrawFilledBoxCenter(toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]) + COORD_CIRCLE_CENTER_X,
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]),
					                    2, COLOR_LIME, currXfb);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]),
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]),
					                    2, COLOR_LIME, currXfb);
					DrawFilledBoxCenter(toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]) + COORD_CIRCLE_CENTER_X,
					                    toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_LIME, currXfb);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]),
					                    toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_LIME, currXfb);
					DrawFilledBoxCenter(toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]) + COORD_CIRCLE_CENTER_X,
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]),
					                    2, COLOR_LIME, currXfb);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]),
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]),
					                    2, COLOR_LIME, currXfb);
				}
			}
			if (drawUnsafe) {
				for (int i = 0; i < STICKMAP_FF_WD_COORD_UNSAFE_LEN; i++) {
					DrawFilledBoxCenter(toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]) + COORD_CIRCLE_CENTER_X,
					                    toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_YELLOW, currXfb);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]),
					                    toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_YELLOW, currXfb);
					DrawFilledBoxCenter(toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]) + COORD_CIRCLE_CENTER_X,
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]),
					                    2, COLOR_YELLOW, currXfb);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]),
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][1]),
					                    2, COLOR_YELLOW, currXfb);
					DrawFilledBoxCenter(toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]) + COORD_CIRCLE_CENTER_X,
					                    toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_YELLOW, currXfb);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]),
					                    toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][0]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_YELLOW, currXfb);
					DrawFilledBoxCenter(toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]) + COORD_CIRCLE_CENTER_X,
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]),
					                    2, COLOR_YELLOW, currXfb);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_FF_WD_COORD_UNSAFE[i][1]),
					                    SCREEN_POS_CENTER_Y - toStickmap(STICKMAP_FF_WD_COORD_SAFE[i][0]),
					                    2, COLOR_YELLOW, currXfb);
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
					                    2, COLOR_YELLOW, currXfb);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][0]),
					                    toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][1]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_YELLOW, currXfb);
				}
			}
			if (drawUCFLower) {
				for (int i = 0; i < STICKMAP_SHIELDDROP_COORD_UCF_LOWER_LEN; i++) {
					DrawFilledBoxCenter(toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][0]) + COORD_CIRCLE_CENTER_X,
					                    toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][1]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_BLUE, currXfb);
					DrawFilledBoxCenter(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][0]),
					                    toStickmap(STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][1]) + SCREEN_POS_CENTER_Y,
					                    2, COLOR_BLUE, currXfb);
				}
			}
			if (drawVanilla) {
				for (int i = 0; i < STICKMAP_SHIELDDROP_COORD_VANILLA_LEN; i++) {
					DrawHLine(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][0]) - 1,
					          toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][0]) + COORD_CIRCLE_CENTER_X + 1,
					          toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][1]) + SCREEN_POS_CENTER_Y, COLOR_LIME, currXfb);
					DrawHLine(COORD_CIRCLE_CENTER_X - toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][0]) - 1,
					          toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][0]) + COORD_CIRCLE_CENTER_X + 1,
					          toStickmap(STICKMAP_SHIELDDROP_COORD_VANILLA[i][1]) + SCREEN_POS_CENTER_Y + 1, COLOR_LIME, currXfb);
				}
			}
			break;
		default:
			break;
	}
}