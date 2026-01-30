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

#include <stdint.h>

#include "util/datetime.h"
#include "waveform.h"

// center of screen, 640x480
// TODO: probably should get these from rmode, just in case...
#define SCREEN_POS_CENTER_X 320
#define SCREEN_POS_CENTER_Y 240
#define COORD_CIRCLE_CENTER_X 450
#define SCREEN_TIMEPLOT_START 70


// for readability/convenience
#define TEXMAP_NONE GX_TEXMAP_NULL
#define TEXMAP_FONT GX_TEXMAP0
#define TEXMAP_FONT_BUTTON GX_TEXMAP1
#define TEXMAP_CONTROLLER GX_TEXMAP2
#define TEXMAP_STICKMAPS GX_TEXMAP3
#define TEXMAP_STICKOUTLINE GX_TEXMAP4
#define TEXMAP_P GX_TEXMAP5

// same as above
#define VTXFMT_PRIMITIVES_INT GX_VTXFMT0
#define VTXFMT_PRIMITIVES_FLOAT GX_VTXFMT1
#define VTXFMT_TEXTURES GX_VTXFMT2

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
#define GX_COLOR_ORANGE (GXColor) {0xFF, 0xA5, 0x00, 0xFF}

// window width for graphing...
#define WAVEFORM_DISPLAY_WIDTH 500.0

// returns the given color with the specified alpha
GXColor GXColorAlpha(GXColor color, uint8_t alpha);

// enum to keep track of the vertex description state
// this _should_ prevent adding unnecessary config commands? idk
enum CURRENT_VTX_MODE { VTX_NONE, VTX_PRIMITIVES, VTX_TEXTURES };

// change vertex descriptions to one of the above, specifying the tev combiner op if necessary
void updateVtxDesc(enum CURRENT_VTX_MODE mode, int tevOp);

// change the texture used for tev op
// (this changes what texture will be drawn on a primitive)
void changeLoadedTexmap(int newTexmap);

// self-explanatory
void getCurrentTexmapDims(int *width, int *height);

// update what stickmap texture is loaded into TEXMAP_STICKMAPS
void changeStickmapTexture(int image);

// basic initialization stuff
void setupGX(GXRModeObj *rmode);

// we use this to mabe a "subwindow" for a scrolling text box
// see startScrollingPrint() and endScrollingPrint() in print.c
// screen coordinates will be shifted to match the scissorbox
void setSubwindowScissorBox(int x1, int y1, int x2, int y2);
// return scissor box to normal, mainly used after setSubwindowScissorBox()
void restoreNormalScissorBox();

// debugging stuff
//uint8_t getFifoVal();

// start and end of draw
void startDraw();
void finishDraw(void *xfb);

// this will shift the screen by the specified units by calling GX_SetScissorBoxOffset() during finishDraw()
void setScreenOffset(int x, int y);

// sets the zDepth for draw() calls going forward
// will not reset until the beginning of a new frame, or when done manually with either
// restorePrevDepth() or resetDepth();
void setDepth(int z);
// sets the zDepth for a single draw() call
// TODO: this might not be respected by any printStr(), printStrColor(), or printStrBox() calls, investigate...
void setDepthForDrawCall(int z);
// sets zDepth to whatever value it was before the last call to setDepth()
// note that is setDepth() is called twice or more without running this function,
// the initial zDepth values are lost
// EX: zDepth = -5 -> setDepth(-8) -> setDepth(-10)
// restorePrevDepth() will set zDepth to -8, not -5
void restorePrevDepth();
// sets zDepth to its default value, as defined by GX_DEFAULT_Z_DEPTH in gx.c
void resetDepth();

// change alpha used for most draw functions
// follows same conventions as above
void setAlpha(uint8_t newAlpha);
void setAlphaForDrawCall(uint8_t newAlpha);
void restorePrevAlpha();
void resetAlpha();

// basic drawing functions
void drawLine(int x1, int y1, int x2, int y2, GXColor color);

void drawBox(int x1, int y1, int x2, int y2, GXColor color);
void drawSolidBox(int x1, int y1, int x2, int y2, GXColor color);

void drawTri(int x1, int y1, int x2, int y2, int x3, int y3, GXColor color);

enum GRAPH_TYPE { GRAPH_STICK, GRAPH_STICK_FULL, GRAPH_TRIGGER };
// called before drawing a new _type_ of graph
// ie: the setup() function of a given submenu
void resetDrawGraph();
// AXIS_AXY or AXIS_CXY, only applies to GRAPH_STICK and GRAPH_STICK_FULL
void setDrawGraphStickAxis(enum CONTROLLER_STICK_AXIS axis);
// sets a 'zero index offset', basically shifts the graph by the specified offset. used for GRAPH_STICK_FULL
void setDrawGraphIndexOffset(int offset);
// draw the graph
// uses 4 z layers, either n-2 -> n+1 or n-1 -> n+2, as well as z=0 (todo, needs to be fixed)
void drawGraph(ControllerRec *data, enum GRAPH_TYPE type, bool isFrozen);
// get the scrollOffset and number of samples visible
// scaling is handled in drawGraph(), so to get information about the graph, this is needed
void getGraphDisplayedInfo(int *scrollOffset, int *visibleSamples);
// get random statistics about a given drawn graph
// scaling is handled in drawGraph(), so to get information about the graph, this is needed
void getGraphStats(uint64_t *uSecs, int8_t *minX, int8_t *minY, int8_t *maxX, int8_t *maxY, bool *yMag);

// set temp rotation for a texture draw
enum TEX_ROTATE { ROTATE_0, ROTATE_90, ROTATE_180, ROTATE_270 };
void rotateTextureForDraw(enum TEX_ROTATE rotation);
// draws full texture without scaling it (texture size to screen space used is 1:1)
void drawTextureFull(int x1, int y1, GXColor color);
// draws full texture, scaled to fit the specified bounds
void drawTextureFullScaled(int x1, int y1, int x2, int y2, GXColor color);

// draw part of a given texture
void drawSubTexture(int x1, int y1, int x2, int y2, int tx1, int ty1, int tx2, int ty2, GXColor color);

#ifndef NO_DATE_CHECK
void drawDateSpecial(enum DATE_CHECK_LIST date);
#endif

#endif //GTS_GX_H
