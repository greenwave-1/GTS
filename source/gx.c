//
// Created on 10/14/25.
//

// custom wrapper to try and make dealing with gx easier

// most of this is pulled from the provided examples, other projects that use gx (swiss-gc),
// and this https://devkitpro.org/wiki/libogc/GX (that last one is very old tho).
// most of this is likely incorrect in some way...

#include "gx.h"

#include <malloc.h>
#include <memory.h>
#include <stdlib.h>

#include <ogc/tpl.h>

#include "textures.h"
#include "textures_tpl.h"


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

// matrix math stuff
static Mtx view, model, modelview;
static Mtx44 projectionMatrix;

// tpl object and texture objects
static TPLFile tpl;
static GXTexObj fontTex;
static GXTexObj controllerTex;
static GXTexObj stickmapTexArr[6];

// change vertex descriptions to one of the above, specifying the tev combiner op if necessary
void updateVtxDesc(enum CURRENT_VTX_MODE mode, int tevOp) {
	// only run if we are drawing something different
	if (mode != currentVtxMode) {
		// we always provide direct data for position
		GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
		
		switch (mode) {
			case VTX_PRIMITIVES:
				GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
				GX_SetVtxDesc(GX_VA_TEX0, GX_DISABLE);
				
				GX_SetTevOp(GX_TEVSTAGE0, tevOp);
				
				break;
			case VTX_TEX_NOCOLOR:
				GX_SetVtxDesc(GX_VA_CLR0, GX_DISABLE);
				GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
				
				GX_SetTevOp(GX_TEVSTAGE0, tevOp);
				
				break;
			case VTX_TEX_COLOR:
				GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
				GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

				GX_SetTevOp(GX_TEVSTAGE0, tevOp);
				
				break;
			case VTX_NONE:
			default:
				break;
		}
	}
	
	GX_SetTevDirect(GX_TEVSTAGE0);
	
	currentVtxMode = mode;
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
	
	// setup gx to clear background on each new frame
	GXColor background = {0,0,0,0xff};
	//GXColor background = {0xff, 0xff, 0xff, 0xff};
	GX_SetCopyClear(background, GX_MAX_Z24);
	
	// other gx setup stuff
	GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight,0,1);
	float yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
	uint32_t xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopySrc(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth, xfbHeight);
	GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	
	GX_SetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL);
	//GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering,((rmode->viHeight==2*rmode->xfbHeight)?GX_ENABLE:GX_DISABLE));
	
	GX_SetCullMode(GX_CULL_NONE);
	
	// clear vertex stuff
	GX_ClearVtxDesc();
	
	// setup vertex stuff
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	
	// VTXFMT0, configured for primitive drawing
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGB, GX_RGB8, 0);
	
	// VTXFMT1, configured for display textures, while also modifying colors
	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_S16, 0);
	
	// VTXFMT2, configured to display texture, no modification of color directly
	GX_SetVtxAttrFmt(GX_VTXFMT2, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT2, GX_VA_TEX0, GX_TEX_ST, GX_S16, 0);
	
	currentVtxMode = VTX_NONE;
	
	// TODO: most of this is where I lose my understanding of gx, something's probably wrong in here...
	
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
	GX_LoadTexObj(&(stickmapTexArr[0]), TEXMAP_STICKMAPS);
	TPL_GetTexture(&tpl, await, &stickmapTexArr[1]);
	TPL_GetTexture(&tpl, movewait, &stickmapTexArr[2]);
	TPL_GetTexture(&tpl, crouch, &stickmapTexArr[3]);
	TPL_GetTexture(&tpl, ledgel, &stickmapTexArr[4]);
	TPL_GetTexture(&tpl, ledger, &stickmapTexArr[5]);
	
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
	guOrtho(projectionMatrix, 0, rmode->viHeight, 0, rmode->viWidth, 0.1f, 300.0f);
	
	// perspecive matrix, z distance affects apparent size
	//guPerspective(projectionMatrix, 45, (float) rmode->viWidth / rmode->viHeight, 0.1f, 300.0f);
	
	GX_LoadProjectionMtx(projectionMatrix, GX_ORTHOGRAPHIC);
	
}

/*
uint8_t getFifoVal() {
	return GX_GetFifoWrap(gxFifoObj);
}
*/

// start and end of draw
void startDraw(GXRModeObj *rmode) {
	GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	
	guMtxIdentity(model);
	guMtxTransApply(model, model, 0.0f, 0.0f, -1.0f);
	guMtxConcat(view,model,modelview);
	
	GX_LoadPosMtxImm(modelview, GX_PNMTX0);
	
	GX_SetLineWidth(12, GX_TO_ZERO);
}

void finishDraw(void *xfb) {
	//GX_Flush();
	GX_DrawDone();
	
	//GX_SetAlphaUpdate(GX_TRUE);
	//GX_SetColorUpdate(GX_TRUE);
	
	GX_CopyDisp(xfb, GX_TRUE);
}

void drawLine(int x1, int y1, int x2, int y2, GXColor color) {
	updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
	
	GX_Begin(GX_LINES, GX_VTXFMT0, 2);
	
	GX_Position3s16(x1, y1, -5);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x2, y2, -5);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_End();
}

void drawBox(int x1, int y1, int x2, int y2, GXColor color) {
	updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
	
	GX_Begin(GX_LINESTRIP, GX_VTXFMT0, 5);
	
	GX_Position3s16(x1, y1, -5);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x2, y1, -5);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x2, y2, -5);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x1, y2, -5);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x1, y1, -5);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_End();
	
}

void drawSolidBox(int x1, int y1, int x2, int y2, GXColor color) {
	updateVtxDesc(VTX_PRIMITIVES, GX_PASSCLR);
	
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	
	GX_Position3s16(x1, y1, -5);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x2, y1, -5);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x2, y2, -5);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_Position3s16(x1, y2, -5);
	GX_Color3u8(color.r, color.g, color.b);
	
	GX_End();
}