#ifndef CUSTOM_COLORS_H
#define CUSTOM_COLORS_H

#include <ogc/color.h>

// colors are in this format, but with luminance stored twice, as per libogc
// https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
// https://github.com/devkitPro/libogc/blob/master/gc/ogc/color.h

static const u32 CUSTOM_COLORS[] = {
		COLOR_BLACK, 0x24802480, 0x38803880,
		0x4c804c80, 0x60806080, 0x74807480,
		0x88808880, 0x9c809c80, 0xb080b080,
		0xc480c480, 0xd880d880
};

#endif