//
// Created on 10/14/25.
//

// custom wrapper to try and make dealing with gx easier

// most of this is pulled from the provided examples, other projects that use gx (swiss-gc),
// and this https://devkitpro.org/wiki/libogc/GX (that last one is very old tho).
// most of this is likely incorrect in multiple way...

#include "util/gx.h"

#include <malloc.h>
#include <memory.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <ogc/tpl.h>
#include <ogc/pad.h>

#include "util/polling.h"

#include "textures.h"
#include "textures_tpl.h"

GXColor GXColorAlpha(GXColor color, uint8_t alpha) {
	return (GXColor) { color.r, color.g, color.b, alpha };
}

// default fifo size, specific number from provided gx examples
#define DEFAULT_FIFO_SIZE (256 * 1024)

static enum CURRENT_VTX_MODE currentVtxMode = VTX_NONE;
static int currentTexmap = TEXMAP_NONE;

// address of memory allocated for fifo
static void *gp_fifo = nullptr;

// fifo object itself
// only used if we need to actually interface with the fifo later
// (basically for debugging)
//static GXFifoObj *gxFifoObj = nullptr;

// rmode pointer
// passed in setupGX()
static GXRModeObj *rmodePtr = NULL;

// matrix math stuff
static Mtx view, model, modelview;
static Mtx44 projectionMatrix;

// tpl object and texture objects
static TPLFile tpl;
static GXTexObj fontTex;
static GXTexObj fontButtonTex;
static GXTexObj controllerTex;
static GXTexObj stickmapTexArr[6];
static GXTexObj stickOutlineTex;
static GXTexObj pTex;

// keeps track of our current z depth, for drawing helper functions
// TODO: z depth for specific elements (font, quads, lines, etc) need to be standardized
static int zDepth = -5;
static int zPrevDepth = -5;

// change vertex descriptions to one of the above, specifying the tev combiner op if necessary
void updateVtxDesc(enum CURRENT_VTX_MODE mode, int tevOp) {
	// only run if we are drawing something different
	if (mode != currentVtxMode) {
		// we always provide direct data for position
		// probably not needed...
		GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
		
		switch (mode) {
			case VTX_PRIMITIVES:
				GX_SetVtxDesc(GX_VA_TEX0, GX_DISABLE);
				GX_SetTevOp(GX_TEVSTAGE0, tevOp);
				break;
			case VTX_TEXTURES:
				GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
				GX_SetTevOp(GX_TEVSTAGE0, tevOp);
				break;
			case VTX_NONE:
			default:
				// error case?
				break;
		}
		
		GX_SetTevDirect(GX_TEVSTAGE0);
		currentVtxMode = mode;
	}
}

// change the texture used for tev op
// (this changes what texture will be drawn on a primitive)
void changeLoadedTexmap(int newTexmap) {
	if (currentTexmap != newTexmap) {
		if (newTexmap == TEXMAP_NONE) {
			GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
		} else {
			GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, newTexmap, GX_COLOR0A0);
		}
		currentTexmap = newTexmap;
	}
}

void getCurrentTexmapDims(int *width, int *height) {
	GXTexObj *selection = NULL;
	switch (currentTexmap) {
		case TEXMAP_CONTROLLER:
			selection = &controllerTex;
			break;
		case TEXMAP_FONT:
			selection = &fontTex;
			break;
		case TEXMAP_P:
			selection = &pTex;
			break;
		case TEXMAP_STICKOUTLINE:
			selection = &stickOutlineTex;
			break;
		case TEXMAP_FONT_BUTTON:
			selection = &fontButtonTex;
			break;
		case TEXMAP_STICKMAPS:
			// all of these are the same, so any will do
			selection = &(stickmapTexArr[0]);
			break;
		case TEXMAP_NONE:
		default:
			break;
	}
	
	if (selection != NULL) {
		*width = GX_GetTexObjWidth(selection);
		*height = GX_GetTexObjHeight(selection);
	}
}

// expects enum IMAGE as defined in plot2d.h
void changeStickmapTexture(int image) {
	if (image > 0 && image < 7) {
		// texmap array doesn't hold a "no image" texture, so we have to shift index by one from the provided enum
		GX_LoadTexObj(&(stickmapTexArr[image - 1]), TEXMAP_STICKMAPS);
	}
}

// basic initialization stuff
void setupGX(GXRModeObj *rmode) {
	// allocate fifo space, 32 byte aligned
	gp_fifo = memalign(32, DEFAULT_FIFO_SIZE);
	// clear newly allocated space
	memset(gp_fifo, 0, DEFAULT_FIFO_SIZE);
	
	// start gx stuff
	GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);
	//gxFifoObj = GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);
	//GX_InitFifoLimits(gxFifoObj, DEFAULT_FIFO_SIZE - GX_FIFO_HIWATERMARK, DEFAULT_FIFO_SIZE / 2);
	
	// set this for future use, so we don't have to continue to pass it
	rmodePtr = rmode;
	
	// setup gx to clear background on each new frame
	GXColor background = {0,0,0,0xff};
	//GXColor background = {0xff, 0xff, 0xff, 0xff};
	GX_SetCopyClear(background, GX_MAX_Z24);
	
	// other gx setup stuff
	GX_SetViewport(0, 0, rmodePtr->fbWidth, rmodePtr->efbHeight,0,1);
	float yscale = GX_GetYScaleFactor(rmodePtr->efbHeight, rmodePtr->xfbHeight);
	uint32_t xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0, 0, rmodePtr->fbWidth, rmodePtr->efbHeight);
	GX_SetDispCopySrc(0,0,rmodePtr->fbWidth,rmodePtr->efbHeight);
	GX_SetDispCopyDst(rmodePtr->fbWidth, xfbHeight);
	GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	
	GX_SetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL);
	//GX_SetCopyFilter(rmodePtr->aa, rmodePtr->sample_pattern, GX_TRUE, rmodePtr->vfilter);
	GX_SetFieldMode(rmodePtr->field_rendering,((rmodePtr->viHeight==2*rmodePtr->xfbHeight)?GX_ENABLE:GX_DISABLE));
	
	GX_SetCullMode(GX_CULL_NONE);
	
	// clear vertex stuff
	GX_ClearVtxDesc();
	
	// setup vertex stuff
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	
	// VTXFMT0, configured for primitive drawing
	GX_SetVtxAttrFmt(VTXFMT_PRIMITIVES_RGB, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt(VTXFMT_PRIMITIVES_RGB, GX_VA_CLR0, GX_CLR_RGB, GX_RGB8, 0);
	
	// VTXFMT1, configured for primitive drawing, but with alpha available
	GX_SetVtxAttrFmt(VTXFMT_PRIMITIVES_RGBA, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt(VTXFMT_PRIMITIVES_RGBA, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	
	// VTXFMT2, configured for primitive drawing, float screenspace coordinates
	GX_SetVtxAttrFmt(VTXFMT_PRIMITIVES_FLOAT, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(VTXFMT_PRIMITIVES_FLOAT, GX_VA_CLR0, GX_CLR_RGB, GX_RGB8, 0);
	
	// VTXFMT3, configured for textures
	GX_SetVtxAttrFmt(VTXFMT_TEXTURES, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt(VTXFMT_TEXTURES, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(VTXFMT_TEXTURES, GX_VA_TEX0, GX_TEX_ST, GX_S16, 0);

	
	currentVtxMode = VTX_NONE;
	
	// TODO: this is where my understanding of the code goes completely out the window,
	// TODO: there's probably multiple things wrong here, even more than above...
	
	GX_SetNumChans(1);
	//GX_SetChanCtrl(GX_COLOR0A0, GX_ENABLE, GX_SRC_VTX, GX_SRC_VTX, 0, GX_DF_NONE, GX_AF_NONE);
	
	GX_SetNumTexGens(1);
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 1, 1);
	
	//GX_SetNumTevStages(2);
	GX_SetNumTevStages(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetTevDirect(GX_TEVSTAGE0);
	
	// TODO: is there a way to have this apply only to certain vertex descriptions?
	// i'm probably supposed to change stuff with tevcolorin, tevcolorop, tevalphain...
	//GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	//GX_SetTevOp(GX_TEVSTAGE1, GX_MODULATE);
	
	GX_InvVtxCache();
	GX_InvalidateTexAll();
	
	// get textures
	TPL_OpenTPLFromMemory(&tpl, (void*)textures_tpl, textures_tpl_size);
	
	// font
	TPL_GetTexture(&tpl, font, &fontTex);
	GX_LoadTexObj(&fontTex, TEXMAP_FONT);
	
	// controller sheet
	TPL_GetTexture(&tpl, controller, &controllerTex);
	GX_LoadTexObj(&controllerTex, TEXMAP_CONTROLLER);
	
	// all the stickmaps
	// we don't actually load all the stickmaps into the texmap slots, there aren't enough
	// we'll switch what stickmap is in the texmap slot on when its needed
	TPL_GetTexture(&tpl, deadzone, &stickmapTexArr[0]);
	TPL_GetTexture(&tpl, await, &stickmapTexArr[1]);
	TPL_GetTexture(&tpl, movewait, &stickmapTexArr[2]);
	TPL_GetTexture(&tpl, crouch, &stickmapTexArr[3]);
	TPL_GetTexture(&tpl, ledgel, &stickmapTexArr[4]);
	TPL_GetTexture(&tpl, ledger, &stickmapTexArr[5]);
	// load first one by default, because why not?
	GX_LoadTexObj(&(stickmapTexArr[0]), TEXMAP_STICKMAPS);
	
	// stick outline for coordinate viewer
	TPL_GetTexture(&tpl, outline, &stickOutlineTex);
	GX_LoadTexObj(&stickOutlineTex, TEXMAP_STICKOUTLINE);
	
	TPL_GetTexture(&tpl, font_button, &fontButtonTex);
	GX_LoadTexObj(&fontButtonTex, TEXMAP_FONT_BUTTON);
	
	TPL_GetTexture(&tpl, p, &pTex);
	GX_LoadTexObj(&pTex, TEXMAP_P);
	
	TPL_CloseTPLFile(&tpl);
	
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	
	// setup our camera at the origin
	// looking down the -z axis with y up
	guVector cam = {0.0F, 0.0F, 0.0F},
			up = {0.0F, 1.0F, 0.0F},
			look = {0.0F, 0.0F, -1.0F};
	guLookAt(view, &cam, &up, &look);
	
	// setup projection matrix
	// orthographic projection, z distance doesn't affect apparent size
	guOrtho(projectionMatrix, 0, rmodePtr->viHeight, 0, rmodePtr->viWidth, 0.1f, 300.0f);
	
	// perspecive matrix, z distance affects apparent size
	//guPerspective(projectionMatrix, 45, (float) rmodePtr->viWidth / rmodePtr->viHeight, 0.1f, 300.0f);
	
	GX_LoadProjectionMtx(projectionMatrix, GX_ORTHOGRAPHIC);
}

// we use this to mabe a "subwindow" for a scrolling text box
// see startScrollingPrint() and endScrollingPrint() in print.c
void setTextScrollingScissorBox(int top, int bottom) {
	drawBox(5, top, 635, bottom, GX_COLOR_WHITE);
	GX_SetScissor(0, 0, 640, bottom - top);
	GX_SetScissorBoxOffset(0, -1 * top);
}

// return scissor box to normal, mainly used after setTextScrollingScissorBox()
void restoreNormalScissorBox() {
	GX_SetScissor(0, 0, rmodePtr->fbWidth, rmodePtr->efbHeight);
	GX_SetScissorBoxOffset(0, 0);
}

/*
uint8_t getFifoVal() {
	return GX_GetFifoWrap(gxFifoObj);
}
*/

// start and end of draw
void startDraw() {
	GX_SetViewport(0, 0, rmodePtr->fbWidth, rmodePtr->efbHeight, 0, 1);
	
	guMtxIdentity(model);
	guMtxTransApply(model, model, 0.0f, 0.0f, -1.0f);
	guMtxConcat(view,model,modelview);
	
	GX_LoadPosMtxImm(modelview, GX_PNMTX0);
	
	GX_SetLineWidth(12, GX_TO_ZERO);
	GX_SetPointSize(12, GX_TO_ZERO);
	zDepth = -5;
}

static int offsetX = 0, offsetY = 0;
void finishDraw(void *xfb) {
	GX_SetScissorBoxOffset(offsetX, offsetY);
	//GX_Flush();
	GX_DrawDone();
	
	//GX_SetAlphaUpdate(GX_TRUE);
	//GX_SetColorUpdate(GX_TRUE);
	
	GX_CopyDisp(xfb, GX_TRUE);
}

void setScreenOffset(int x, int y) {
	offsetX = x;
	offsetY = y;
}

void setDepth(int z) {
	zPrevDepth = zDepth;
	zDepth = z;
}

void restorePrevDepth() {
	zDepth = zPrevDepth;
}

void drawLine(int x1, int y1, int x2, int y2, GXColor color) {
	updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
	
	GX_Begin(GX_LINES, VTXFMT_PRIMITIVES_RGB, 2);
	
	GX_Position3s16(x1, y1, zDepth);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x2, y2, zDepth);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_End();
}

void drawBox(int x1, int y1, int x2, int y2, GXColor color) {
	updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
	
	GX_Begin(GX_LINESTRIP, VTXFMT_PRIMITIVES_RGB, 5);
	
	GX_Position3s16(x1, y1, zDepth);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x2, y1, zDepth);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x2, y2, zDepth);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x1, y2, zDepth);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x1, y1, zDepth);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_End();
}

void drawSolidBox(int x1, int y1, int x2, int y2, GXColor color) {
	updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
	
	GX_Begin(GX_QUADS, VTXFMT_PRIMITIVES_RGB, 4);
	
	GX_Position3s16(x1, y1, zDepth);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x2, y1, zDepth);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x2, y2, zDepth);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x1, y2, zDepth);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_End();
}

void drawSolidBoxAlpha(int x1, int y1, int x2, int y2, GXColor color) {
	updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
	
	GX_Begin(GX_QUADS, VTXFMT_PRIMITIVES_RGBA, 4);
	
	GX_Position3s16(x1, y1, zDepth);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	
	GX_Position3s16(x2, y1, zDepth);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	
	GX_Position3s16(x2, y2, zDepth);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	
	GX_Position3s16(x1, y2, zDepth);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	
	GX_End();
}

void drawTri(int x1, int y1, int x2, int y2, int x3, int y3, GXColor color) {
	updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
	
	GX_Begin(GX_TRIANGLES, VTXFMT_PRIMITIVES_RGB, 3);
	
	GX_Position3s16(x1, y1, zDepth);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x2, y2, zDepth);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x3, y3, zDepth);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_End();
}

// controls the start point of what we draw (offset from index 0)
static int graphScrollOffset = 0;
// how many points to draw
// also controls scale
static int graphVisibleDatapoints = -1;
// our maximum number of allowed datapoints
static int graphMaxVisibleDatapoints = -1;
// what number we consider our zero index
// mainly used for continuous oscilloscope
static int graphZeroIndexOffset = 0;

// should be called in a setup() function
void resetDrawGraph() {
	graphScrollOffset = 0;
	graphVisibleDatapoints = -1;
	graphMaxVisibleDatapoints = -1;
	graphZeroIndexOffset = 0;
}

// for everything other than GRAPH_TRIGGER
// what axis do we draw?
static enum CONTROLLER_STICK_AXIS drawnAxis = AXIS_AXY;
void setDrawGraphStickAxis(enum CONTROLLER_STICK_AXIS axis) {
	drawnAxis = axis;
}

void setDrawGraphOffset(int offset) {
	graphZeroIndexOffset = offset;
}

// stat values that menus retrieve
static uint64_t graphTimeUsecs = 0;
static bool yMagnitudeIsGreater = false;
static int graphXMin = 0, graphYMin = 0;
static int graphXMax = 0, graphYMax = 0;

void getGraphStats(uint64_t *uSecs, int8_t *minX, int8_t *minY, int8_t *maxX, int8_t *maxY, bool *yMag) {
	*uSecs = graphTimeUsecs;
	*minX = graphXMin;
	*minY = graphYMin;
	*maxX = graphXMax;
	*maxY = graphYMax;
	*yMag = yMagnitudeIsGreater;
}

void getGraphDisplayedInfo(int *scrollOffset, int *visibleSamples) {
	*scrollOffset = graphScrollOffset;
	*visibleSamples = graphVisibleDatapoints;
}

// actually draw the graph
void drawGraph(ControllerRec *data, enum GRAPH_TYPE type, bool isFrozen) {
	// initialize data if needed
	if (graphVisibleDatapoints == -1) {
		switch (type) {
			case GRAPH_STICK_FULL:
				graphVisibleDatapoints = graphMaxVisibleDatapoints = REC_SAMPLE_MAX;
				break;
			case GRAPH_STICK:
			case GRAPH_TRIGGER:
				graphVisibleDatapoints = graphMaxVisibleDatapoints = WAVEFORM_DISPLAY_WIDTH;
				graphZeroIndexOffset = 0;
				break;
		}
	}
	
	// allow pan/zoom if the image is considered "frozen"
	if (isFrozen) {
		uint16_t *held = getButtonsHeldPtr();
		int datapointModifier = 0;
		if (abs(PAD_SubStickY(0)) >= 23) {
			datapointModifier = PAD_SubStickY(0) / 12;
			// datapointModifier needs to be even to make zooming work properly...
			if (datapointModifier < 0 && abs(datapointModifier) % 2 == 1) {
				datapointModifier--;
			}
			if (datapointModifier > 0 && datapointModifier % 2 == 1) {
				datapointModifier++;
			}
		}
		
		int scrollModifier = 0;
		// scrolling needs to change rate based on visible number of samples
		if (abs(PAD_SubStickX(0)) >= 23) {
			scrollModifier = ((graphVisibleDatapoints / 250) + 1) * (PAD_SubStickX(0) / 25);
		}
		
		if (*held & PAD_TRIGGER_L) {
			datapointModifier /= 8;
			// datapointModifier needs to be even to make zooming work properly...
			if (datapointModifier < 0 && abs(datapointModifier) % 2 == 1) {
				datapointModifier--;
			}
			if (datapointModifier > 0 && datapointModifier % 2 == 1) {
				datapointModifier++;
			}
			
			if (PAD_SubStickX(0) >= 23) {
				scrollModifier = 1;
			} else if (PAD_SubStickX(0) <= -23) {
				scrollModifier = -1;
			}
			
			if (abs(scrollModifier) > 0) {
				graphScrollOffset += scrollModifier;
			}
			if (abs(datapointModifier) > 0) {
				graphVisibleDatapoints -= datapointModifier;
				if (graphVisibleDatapoints > 25) {
					graphScrollOffset += (datapointModifier / 2);
				}
			}
		} else {
			if (*held & PAD_TRIGGER_R) {
				datapointModifier *= 2;
				scrollModifier *= 2;
			}
			if (datapointModifier) {
				graphVisibleDatapoints -= datapointModifier;
				if (graphVisibleDatapoints > 25) {
					graphScrollOffset += (datapointModifier / 2);
				}
			}
			if (scrollModifier) {
				graphScrollOffset += scrollModifier;
			}
		}
	} else {
		graphScrollOffset = 0;
		graphVisibleDatapoints = graphMaxVisibleDatapoints;
	}
	
	// bounds check
	if (graphVisibleDatapoints < 25) {
		graphVisibleDatapoints = 25;
	}
	if (graphVisibleDatapoints > graphMaxVisibleDatapoints) {
		graphVisibleDatapoints = graphMaxVisibleDatapoints;
	}
	
	if (type == GRAPH_STICK_FULL) {
		// cap continuous to max visible, always
		if (graphScrollOffset > graphMaxVisibleDatapoints - graphVisibleDatapoints) {
			graphScrollOffset = graphMaxVisibleDatapoints - graphVisibleDatapoints;
		}
	} else {
		// cap everything else to the bounds of the recording
		if (graphScrollOffset > data->sampleEnd - graphVisibleDatapoints) {
			graphScrollOffset = data->sampleEnd - graphVisibleDatapoints;
		}
	}
	
	if (graphScrollOffset < 0) {
		graphScrollOffset = 0;
	}
	
	// for any stick graphing, we start at the center of the screen
	int yPosModifier = SCREEN_POS_CENTER_Y;
	
	// for trigger graphing, we start at the bottom of the screen
	// window is 256 pixels high, so +128 to move to bottom
	if (type == GRAPH_TRIGGER) {
		yPosModifier += 128;
	}
	
	// calculate how many points we're going to draw
	// we also get miscellaneous information here, such as min/max, digital press for trigger, and frame intervals
	int frameIntervalIndex = 0;
	float frameIntervalList[500];
	uint64_t timeFromLastInterval = 0;
	int digitalPressInterval = 0;
	float digitalPressList[500];
	bool digitalPressOccurring = false;
	graphXMin = graphYMin = 0;
	graphXMax = graphYMax = 0;
	yMagnitudeIsGreater = false;
	graphTimeUsecs = 0;
	
	// for GRAPH_TRIGGER, we need to update timeFromLastInterval to match what it would be at the first point to draw
	if (type == GRAPH_TRIGGER && graphScrollOffset != 0) {
		for (int i = 0; i < graphScrollOffset; i++) {
			timeFromLastInterval += data->samples[i].timeDiffUs;
			if (timeFromLastInterval >= FRAME_TIME_US) {
				// just reset it, we don't actually need to know what the previous frame intervals were...
				timeFromLastInterval = 0;
			}
		}
	}
	
	// calculate units per shown sample
	float waveformScreenUnitPer = WAVEFORM_DISPLAY_WIDTH / (graphVisibleDatapoints - 1);
	
	updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
	
	int linesToDraw = 1;
	
	// both stick graphs draw two lines
	if (data->recordingType != REC_TRIGGER_L && data->recordingType != REC_TRIGGER_R) {
		linesToDraw = 2;
	}
	
	// either 1 or -1, depending on where the next axis needs to be drawn
	int lineModifier = 0;
	
	// line = 0 -> draw X axis OR draw trigger
	// calculate stat values while drawing (frame intervals, min/max, digital presses)
	// use those values to determine if Y should be drawn above or below
	// line = 1 -> draw Y axis at zDepth + lineModifier OR leave loop because trigger
	for (int line = 0; line < linesToDraw; line++) {
		// set color of line based on what we're drawing
		GXColor lineColor = (line == 0) ? GX_COLOR_RED_X : GX_COLOR_BLUE_Y;
		if (type == GRAPH_TRIGGER) {
			lineColor = GX_COLOR_WHITE;
		}
		
		GX_Begin(GX_LINESTRIP, VTXFMT_PRIMITIVES_FLOAT, graphVisibleDatapoints);
		
		for (int i = 0; i < graphVisibleDatapoints; i++) {
			int dataIndex = i + graphScrollOffset;
			
			// window x position based on how many points we've drawn
			float windowXPos = SCREEN_TIMEPLOT_START + (waveformScreenUnitPer * (i));
		
			int currSampleValue = 0;
			// for trigger digital press detection
			uint16_t currSampleTriggerMask = 0;
			// get our value for graphing
			switch (type) {
				case GRAPH_STICK_FULL:
					// offset index by the base offset provided, mod to ensure we're in range
					dataIndex = (dataIndex + graphZeroIndexOffset) % graphMaxVisibleDatapoints;
					
					// gray out line that isn't recorded yet
					if (i > graphZeroIndexOffset && data->sampleEnd != graphMaxVisibleDatapoints) {
						lineColor = GX_COLOR_GRAY;
					} else
					// fall through to the next if
				case GRAPH_STICK:
					// are we looking at data that exists?
					if (dataIndex < data->sampleEnd) {
						// get x or y
						currSampleValue = line == 0 ?
						                  getControllerSampleXValue(data->samples[dataIndex], drawnAxis) :
						                  getControllerSampleYValue(data->samples[dataIndex], drawnAxis);
						lineColor = (line == 0) ? GX_COLOR_RED_X : GX_COLOR_BLUE_Y;
					} else {
						// we're beyond the list, draw at 0 with gray
						lineColor = GX_COLOR_GRAY;
					}
					break;
				case GRAPH_TRIGGER:
					// either trigger L or R
					if (data->recordingType == REC_TRIGGER_L) {
						currSampleValue = data->samples[dataIndex].triggerL;
						currSampleTriggerMask = PAD_TRIGGER_L;
					} else {
						currSampleValue = data->samples[dataIndex].triggerR;
						currSampleTriggerMask = PAD_TRIGGER_R;
					}
					break;
			}
			
			// calculate stat values
			// this only needs to happen during the first run of the outside for loop
			if (line == 0) {
				switch (type) {
					// stick wants total time, and min/max
					case GRAPH_STICK:
						// get both x and y at once
						int8_t currX, currY;
						getControllerSampleAxisPair(data->samples[dataIndex], drawnAxis, &currX, &currY);
						
						// update min/max
						if (graphXMin > currX) {
							graphXMin = currX;
						}
						if (graphXMax < currX) {
							graphXMax = currX;
						}
						if (graphYMin > currY) {
							graphYMin = currY;
						}
						if (graphYMax < currY) {
							graphYMax = currY;
						}
						
						// set initial stat values if this is the first datapoint
						if (i == 0) {
							graphXMin = graphXMax = currX;
							graphYMin = graphYMax = currY;
						}
						// we don't want to add the time from the first datapoint, we start counting _from_ there
						else {
							// adding time from drawn points, to show how long the current view is
							graphTimeUsecs += data->samples[dataIndex].timeDiffUs;
						}
						break;
					// stick_full just needs frame intervals
					// this is different from every other frame interval calculation
					case GRAPH_STICK_FULL:
						// timeDiffUs is abused here to indicate frame intervals, 0 is not a frame interval, 1 is
						if (data->samples[dataIndex].timeDiffUs == 1) {
							// we only populate the list if we are partially zoomed in
							// also make sure we don't overrun our array...
							if (graphVisibleDatapoints <= 1500 && frameIntervalIndex < 500) {
								// store where the current value is being drawn
								// easier to do this than having to recalculate...
								frameIntervalList[frameIntervalIndex] = windowXPos;
								frameIntervalIndex++;
							}
						}
						break;
					// triggers want frame intervals and digital presses
					case GRAPH_TRIGGER:
						// more traditional way of calculating frame intervals
						// timeFromLastInterval is updated with data from before drawing,
						// to ensure that frame intervals are consistent across a single recording
						timeFromLastInterval += data->samples[dataIndex].timeDiffUs;
						if (timeFromLastInterval >= FRAME_TIME_US) {
							frameIntervalList[frameIntervalIndex] = windowXPos;
							frameIntervalIndex++;
							timeFromLastInterval = 0;
						}
						
						// digital presses
						// this is done a bit differently than you'd expect
						// we store the start and end windowXPos of a continuous segment of digital press
						// digitalPressInterval is updated when the end point is found, or when we're out of data
						if (data->samples[dataIndex].buttons & currSampleTriggerMask) {
							// we mark start point
							if (!digitalPressOccurring) {
								digitalPressList[digitalPressInterval] = windowXPos;
								if (i == 0) {
									// if this is the first datapoint, start line from start of plot...
									//digitalPressList[digitalPressInterval] = SCREEN_TIMEPLOT_START;
								}
								digitalPressOccurring = true;
							}
							// we're at the end of the list, mark this as the end point
							else if (i == graphVisibleDatapoints - 1) {
								digitalPressList[digitalPressInterval + 1] = windowXPos;
								digitalPressInterval += 2;
								digitalPressOccurring = false;
							}
						}
						// we encountered no digital press while looking for the endpoint...
						else if (digitalPressOccurring) {
							// mark this as the end point
							digitalPressList[digitalPressInterval + 1] = windowXPos;
							digitalPressInterval += 2;
							digitalPressOccurring = false;
						}
						break;
				}
			}
			
			// actually draw the vertex
			GX_Position3f32(windowXPos, yPosModifier - currSampleValue, zDepth + lineModifier);
			GX_Color3u8(lineColor.r, lineColor.g, lineColor.b);
		}
		
		GX_End();
		
		// finalize stats based on results
		if (line == 0) {
			switch (type) {
				// determine if Y should be drawn above or below
				case GRAPH_STICK:
					int magnitudeX = abs(graphXMax) >= abs(graphXMin) ? abs(graphXMax) : abs(graphXMin);
					int magnitudeY = abs(graphYMax) >= abs(graphYMin) ? abs(graphYMax) : abs(graphYMin);
					
					// this is slightly unintuitive
					// this will be false because we draw the axis with the larger magnitude _second_
					// which lets it show over the other axis
					// Also, we don't use triggeringAxis here since the magnitude is screen-local
					if (magnitudeY > magnitudeX) {
						lineModifier = 1;
						yMagnitudeIsGreater = true;
						break;
					}
					// fail case matches with what STICK_FULL does, so don't break if above check fails
				// stick_full just needs frame intervals
				// this is different from every other frame interval calculation
				case GRAPH_STICK_FULL:
					// we always draw Y axis below in continuous oscilloscope
					lineModifier = -1;
					break;
				case GRAPH_TRIGGER:
					// nothing to really do here...
				default:
					break;
			}
		}
	}
	
	// draw frame intervals
	switch (type) {
		case GRAPH_TRIGGER:
			// draw digital presses
			GX_Begin(GX_LINES, VTXFMT_PRIMITIVES_RGB, digitalPressInterval);
			for (int i = 0; i < digitalPressInterval; i++) {
				GX_Position3s16(digitalPressList[i], (SCREEN_POS_CENTER_Y + 28), zDepth - 2);
				GX_Color3u8(GX_COLOR_GREEN.r, GX_COLOR_GREEN.g, GX_COLOR_GREEN.b);
			}
			GX_End();
		case GRAPH_STICK_FULL:
			// draw frame intervals
			// *2 since each line has two vertices
			GX_Begin(GX_LINES, VTXFMT_PRIMITIVES_RGB, frameIntervalIndex * 2);
			for (int i = 0; i < frameIntervalIndex; i++) {
				GX_Position3s16(frameIntervalList[i], (SCREEN_POS_CENTER_Y - 127), zDepth - 2);
				GX_Color3u8(GX_COLOR_GRAY.r, GX_COLOR_GRAY.g, GX_COLOR_GRAY.b);
				
				GX_Position3s16(frameIntervalList[i], (SCREEN_POS_CENTER_Y - 112), zDepth - 2);
				GX_Color3u8(GX_COLOR_GRAY.r, GX_COLOR_GRAY.g, GX_COLOR_GRAY.b);
			}
			GX_End();
			break;
		case GRAPH_STICK:
			break;
	}
	
	// scrollbar if we are zoomed in
	// continuous is a special case where we want the scrollbar to _always_ scale based on the whole image,
	// in every other case, we just use sampleEnd, since thats where the data ends...
	// WIP
	float divisor = (type == GRAPH_STICK_FULL) ? graphMaxVisibleDatapoints : data->sampleEnd;
	
	int sliderStart = ((graphScrollOffset / divisor) * WAVEFORM_DISPLAY_WIDTH);

	if (sliderStart < 0) {
		sliderStart = 0;
	}
	int sliderEnd = (((graphScrollOffset + graphVisibleDatapoints) / divisor) * WAVEFORM_DISPLAY_WIDTH);

	if (sliderEnd > WAVEFORM_DISPLAY_WIDTH) {
		sliderEnd = WAVEFORM_DISPLAY_WIDTH;
	}
	
	if (sliderStart > 0 || sliderEnd < WAVEFORM_DISPLAY_WIDTH) {
		setDepth(0);
		// scroll bar at the top
		drawLine(SCREEN_TIMEPLOT_START, SCREEN_POS_CENTER_Y - 128,
		         SCREEN_TIMEPLOT_START + WAVEFORM_DISPLAY_WIDTH, SCREEN_POS_CENTER_Y - 128,
		         GX_COLOR_GRAY);
		// slider on scroll bar
		//GX_SetLineWidth(24, GX_TO_ZERO);
		
		drawLine(SCREEN_TIMEPLOT_START + sliderStart, SCREEN_POS_CENTER_Y - 128,
		         SCREEN_TIMEPLOT_START + sliderEnd, SCREEN_POS_CENTER_Y - 128,
		         GX_COLOR_WHITE);
		GX_SetLineWidth(12, GX_TO_ZERO);
		restorePrevDepth();
	}
	
	// scroll indicators
	// zeroIndexOffset will be 0 for every menu other than continuous,
	// so the logic is basically if (graphScrollOffset > 0) {}
	if (graphScrollOffset + graphZeroIndexOffset > graphZeroIndexOffset) {
		drawTri(SCREEN_TIMEPLOT_START - 20, SCREEN_POS_CENTER_Y,
		        SCREEN_TIMEPLOT_START - 10, SCREEN_POS_CENTER_Y + 15,
		        SCREEN_TIMEPLOT_START - 10, SCREEN_POS_CENTER_Y - 15,
		        GX_COLOR_WHITE);
		/*
		setDepth(0);
		drawLine(SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y - 128,
		         SCREEN_TIMEPLOT_START - 1, SCREEN_POS_CENTER_Y + 128,
				 GX_COLOR_GRAY);
		restorePrevDepth();
		 */
	}
	// TODO: there's a better way to do this than two distinct checks...
	// first check: is our last drawn point not the end point?
	// second check: is our last drawn point below 3000? (only for continuous)
	if (graphScrollOffset + graphVisibleDatapoints < data->sampleEnd ||
			(type == GRAPH_STICK_FULL && graphScrollOffset + graphVisibleDatapoints < graphMaxVisibleDatapoints)) {
		drawTri(SCREEN_TIMEPLOT_START + 520, SCREEN_POS_CENTER_Y,
		        SCREEN_TIMEPLOT_START + 510, SCREEN_POS_CENTER_Y + 15,
		        SCREEN_TIMEPLOT_START + 510, SCREEN_POS_CENTER_Y - 15,
		        GX_COLOR_WHITE);
		/*
		setDepth(0);
		drawLine(SCREEN_TIMEPLOT_START + WAVEFORM_DISPLAY_WIDTH + 1, SCREEN_POS_CENTER_Y - 128,
		         SCREEN_TIMEPLOT_START + WAVEFORM_DISPLAY_WIDTH + 1, SCREEN_POS_CENTER_Y + 128,
		         GX_COLOR_GRAY);
		restorePrevDepth();
		 */
	}
}

void drawTextureFull(int x1, int y1, GXColor color) {
	int width = 0, height = 0;
	getCurrentTexmapDims(&width, &height);
	
	drawTextureFullScaled(x1, y1, x1 + width, y1 + height, color);
}

void drawTextureFullScaled(int x1, int y1, int x2, int y2, GXColor color) {
	updateVtxDesc(VTX_TEXTURES, GX_MODULATE);

	// we could pass the width and height as parameters, but imo that makes the function call look worse
	// some calls want to keep aspect ratio tho, and they would need to calculate x2 and y2 from that anyways...
	int width = 0, height = 0;
	getCurrentTexmapDims(&width, &height);
	
	GX_Begin(GX_QUADS, VTXFMT_TEXTURES, 4);
	
	GX_Position3s16(x1, y1, zDepth);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2s16(0, 0);
	
	GX_Position3s16(x2, y1, zDepth);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2s16(width, 0);
	
	GX_Position3s16(x2, y2, zDepth);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2s16(width, height);
	
	GX_Position3s16(x1, y2, zDepth);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2s16(0, height);
	
	GX_End();
}

// a bit ugly, probably a better way to do this...
void drawSubTexture(int x1, int y1, int x2, int y2, int tx1, int ty1, int tx2, int ty2, GXColor color) {
	updateVtxDesc(VTX_TEXTURES, GX_MODULATE);

	GX_Begin(GX_QUADS, VTXFMT_TEXTURES, 4);
	
	GX_Position3s16(x1, y1, zDepth);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2s16(tx1, ty1);
	
	GX_Position3s16(x2, y1, zDepth);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2s16(tx2, ty1);
	
	GX_Position3s16(x2, y2, zDepth);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2s16(tx2, ty2);
	
	GX_Position3s16(x1, y2, zDepth);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2s16(tx1, ty2);
	
	GX_End();
}

#ifndef NO_DATE_CHECK
static void drawSnowParticles();
const static int colorList[][3] = {
		{ 0xe5, 0x00, 0x00 },
		{ 0xff, 0x8d, 0x00 },
		{ 0xff, 0xee, 0x00 },
		{ 0x02, 0x81, 0x21 },
		{ 0x00, 0x4c, 0xff },
		{ 0x77, 0x00, 0x88 },
		
		{ 0xe5, 0x00, 0x00 },
		{ 0xe5, 0x00, 0x00 },
		{ 0xe5, 0x00, 0x00 },
		{ 0xe5, 0x00, 0x00 },
		{ 0xe5, 0x00, 0x00 }
		
};
void drawDateSpecial(enum DATE_CHECK_LIST date) {
	updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
	int sizeOfQuads = 144;
	switch (date) {
		case DATE_NICE:
			// 1 quad
			GX_Begin(GX_QUADS, VTXFMT_PRIMITIVES_RGB, 4);
			GX_Position3s16(5, 35, -10);
			GX_Color3u8(GX_COLOR_DARKGREEN.r, GX_COLOR_DARKGREEN.g, GX_COLOR_DARKGREEN.b);
			
			GX_Position3s16(9 + 144, 35, -10);
			GX_Color3u8(GX_COLOR_DARKGREEN.r, GX_COLOR_DARKGREEN.g, GX_COLOR_DARKGREEN.b);
			
			GX_Position3s16(9 + 144, 58, -10);
			GX_Color3u8(GX_COLOR_DARKGREEN.r, GX_COLOR_DARKGREEN.g, GX_COLOR_DARKGREEN.b);
			
			GX_Position3s16(5, 58, -10);
			GX_Color3u8(GX_COLOR_DARKGREEN.r, GX_COLOR_DARKGREEN.g, GX_COLOR_DARKGREEN.b);
			
			GX_End();
			break;
		case DATE_PM:
			// 6 quads total across 144 pixels
			sizeOfQuads = 144 / 6;
			GX_Begin(GX_QUADS, VTXFMT_PRIMITIVES_RGB, 4 * 6);
			for (int i = 0; i < 6; i++) {
				GX_Position3s16(5 + (sizeOfQuads * i), 35, -10);
				GX_Color3u8(colorList[i][0], colorList[i][1], colorList[i][2]);
				
				GX_Position3s16(9 + (sizeOfQuads * (i + 1)), 35, -10);
				GX_Color3u8(colorList[i][0], colorList[i][1], colorList[i][2]);
				
				GX_Position3s16(9 + (sizeOfQuads * (i + 1)), 58, -10);
				GX_Color3u8(colorList[i][0], colorList[i][1], colorList[i][2]);
				
				GX_Position3s16(5 + (sizeOfQuads * i), 58, -10);
				GX_Color3u8(colorList[i][0], colorList[i][1], colorList[i][2]);
			}
			GX_End();
			break;
		case DATE_CMAS:
			// call snow code
			drawSnowParticles();
			
			// then obscure them slightly
			// 0x40 -> 64 / 255, ~25% opacity
			setDepth(-24);
			drawSolidBoxAlpha(-10, -10, 700, 500, GXColorAlpha(GX_COLOR_BLACK, 0x40));
			restorePrevDepth();
			
			break;
		default:
			break;
	}
}

static uint16_t numOfParticles = 10;
typedef struct Particle {
	bool active;
	// x y starting coords
	int x;
	int y;
	int progress;
	// direction in degrees
	int direction;
	// num of frames between movements
	int speed;
	// counter for next move
	int moveCounter;
} Particle;

static Particle particles[200];
static int addCounter = 15;
static bool seeded = false;

static void drawSnowParticles() {
	// TODO: if using rand() somewhere else, this probably should be behind a function call
	if (!seeded) {
		srand(time(NULL));
		seeded = true;
		for (int i = 0; i < 200; i++) {
			particles[i].active = false;
			particles[i].x = 0;
			particles[i].y = 0;
			particles[i].progress = 0;
			particles[i].direction = 0;
			particles[i].speed = 0;
			particles[i].moveCounter = 0;
		}
	}
	
	GX_SetPointSize(16, GX_TO_ZERO);
	updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
	GX_Begin(GX_POINTS, VTXFMT_PRIMITIVES_RGB, numOfParticles);
	
	// iterate over our current number of particles
	for (int i = 0; i < numOfParticles; i++) {
		// is the particle ready to be reset?
		if (particles[i].active == false) {
			particles[i].active = true;
			particles[i].progress = 0;
			// can start at any point at the top of the screen, range is (-660, 629)
			// this should let particles drift in from the left of the screen at roughly the same rate as from the top
			particles[i].x = rand() % 1280 - 650;
			// this gives us a bit of variance, (-20, -1)
			particles[i].y = rand() % 20 - 20;
			// direction should be between 15 and 75 degrees
			// 'up' is down-left
			particles[i].direction = (rand() % 60) + 15;
			// how many frames should we wait between movements
			particles[i].speed = rand() % 3 + 1;
			particles[i].moveCounter = particles[i].speed;
			GX_Position3s16(-10, -10, -25);
		} else {
			particles[i].moveCounter--;
			if (particles[i].moveCounter == 0) {
				particles[i].progress += 2;
				particles[i].moveCounter = particles[i].speed;
			}
			
			// we calculate the next position using radians from the starting x and y by progress units
			int destX = particles[i].x + particles[i].progress * (cos(particles[i].direction * M_PI / 180.0));
			int destY = particles[i].y + particles[i].progress * (sin(particles[i].direction * M_PI / 180.0));
			
			GX_Position3s16(destX, destY, -25);
			
			// change direction randomly
			if (rand() % 10 == 0) {
				particles[i].x = destX;
				particles[i].y = destY;
				// +- 5 degrees
				particles[i].direction += (rand() % 10) - 5;
				// clamp if not going down-left
				if (particles[i].direction <= 10) {
					particles[i].direction = 10;
				}
				if (particles[i].direction >= 80) {
					particles[i].direction = 80;
				}
				// new starting point, reset this
				particles[i].progress = 0;
			}
			
			// mark particle as done if outside screen bounds
			if (destX > 650 || destY > 500) {
				particles[i].active = false;
			}
		}
		
		GX_Color3u8(GX_COLOR_WHITE.r, GX_COLOR_WHITE.g, GX_COLOR_WHITE.b);
	}
	
	// slowly add particles
	if (numOfParticles < 200) {
		addCounter--;
		if (addCounter == 0) {
			numOfParticles++;
			addCounter = 15;
		}
	}
	
	GX_End();
	
	GX_SetPointSize(12, GX_TO_ZERO);
}
#endif
