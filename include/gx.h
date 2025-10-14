//
// Created on 10/14/25.
//

#ifndef GTS_GX_H
#define GTS_GX_H

#include <ogc/gx.h>
//#include <stdint.h>
#include <ogc/tpl.h>

#define TEXMAP_NONE GX_TEXMAP_NULL
#define TEXMAP_FONT GX_TEXMAP0
#define TEXMAP_CONTROLLER GX_TEXMAP1
#define TEXMAP_STICKMAPS GX_TEXMAP2

// enum to keep track of the vertex description state
// this _should_ prevent adding unnecessary config commands? idk
enum CURRENT_VTX_MODE { VTX_NONE, VTX_PRIMITIVES, VTX_TEX_NOCOLOR, VTX_TEX_COLOR };

// change vertex descriptions to one of the above, specifying the tev combiner op if necessary
void updateVtxDesc(enum CURRENT_VTX_MODE mode, int tevOp);

// change the texture used for tev op
// (this changes what texture will be drawn on a primitive)
void changeLoadedTexmap(int newTexmap);

// basic initialization stuff
void setupGX(GXRModeObj *rmode);

// start and end of draw
void startDraw(GXRModeObj *rmode);
void finishDraw(void *xfb);

#endif //GTS_GX_H
