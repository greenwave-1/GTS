//
// Created on 10/14/25.
//

// custom wrapper to try and make dealing with gx easier

// most of this is pulled from the provided examples, other projects that use gx (swiss-gc),
// and this https://devkitpro.org/wiki/libogc/GX (that last one is very old tho).
// most of this is likely incorrect in multiple way...

#ifndef GTS_GX_H
#define GTS_GX_H

#include <ogc/gx.h>
#include <ogc/tpl.h>

#include "util/datetime.h"

// center of screen, 640x480
#define SCREEN_POS_CENTER_X 320
#define SCREEN_POS_CENTER_Y 240
#define COORD_CIRCLE_CENTER_X 450

#define TEXMAP_NONE GX_TEXMAP_NULL
#define TEXMAP_FONT GX_TEXMAP0
#define TEXMAP_CONTROLLER GX_TEXMAP1
#define TEXMAP_STICKMAPS GX_TEXMAP2
#define TEXMAP_P GX_TEXMAP3

// normal colors
// mostly based on ogc/color.h
#define GX_COLOR_WHITE (GXColor) {0xFF, 0xFF, 0xFF, 0xFF}
#define GX_COLOR_BLACK (GXColor) {0x00, 0x00, 0x00, 0xFF}
#define GX_COLOR_GRAY (GXColor) {0x80, 0x80, 0x80, 0xFF}
#define GX_COLOR_SILVER (GXColor) {0xC0, 0xC0, 0xC0, 0xFF}
#define GX_COLOR_RED (GXColor) {0xFF, 0x00, 0x00, 0xFF}
#define GX_COLOR_GREEN (GXColor) {0x00, 0xFF, 0x00, 0xFF}
#define GX_COLOR_BLUE (GXColor) {0x00, 0x00, 0xFF, 0xFF}
#define GX_COLOR_DARKGREEN (GXColor) {0x00, 0x80, 0x00, 0xFF}
#define GX_COLOR_YELLOW (GXColor) {0xFF, 0xFF, 0x00, 0xFF}

// hacky way to tell print function to not draw a background, if desired
#define GX_COLOR_NONE (GXColor) {0x00, 0x00, 0x00, 0x00}

// 'custom' colors
#define GX_COLOR_RED_X (GXColor) {0xFF, 0x20, 0x00, 0xFF}
#define GX_COLOR_BLUE_Y (GXColor) {0x00, 0x6A, 0xFF, 0xFF}

// enum to keep track of the vertex description state
// this _should_ prevent adding unnecessary config commands? idk
enum CURRENT_VTX_MODE { VTX_NONE, VTX_PRIMITIVES, VTX_TEX_NOCOLOR, VTX_TEX_COLOR };

// change vertex descriptions to one of the above, specifying the tev combiner op if necessary
void updateVtxDesc(enum CURRENT_VTX_MODE mode, int tevOp);

// change the texture used for tev op
// (this changes what texture will be drawn on a primitive)
void changeLoadedTexmap(int newTexmap);

void getCurrentTexmapDims(int *width, int *height);

// update what stickmap texture is loaded into TEXMAP_STICKMAPS
void changeStickmapTexture(int image);

// basic initialization stuff
void setupGX(GXRModeObj *rmode);

// debugging stuff
//uint8_t getFifoVal();

// start and end of draw
void startDraw(GXRModeObj *rmode);
void finishDraw(void *xfb);

void setScreenOffset(int x, int y);

// basic drawing functions
void setDepth(int z);
void restorePrevDepth();
void drawLine(int x1, int y1, int x2, int y2, GXColor color);

void drawBox(int x1, int y1, int x2, int y2, GXColor color);
void drawSolidBox(int x1, int y1, int x2, int y2, GXColor color);

void drawDateSpecial(enum DATE_CHECK_LIST date);

#endif //GTS_GX_H
