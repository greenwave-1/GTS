//
// Created on 2025/02/22.
//

// constants for coordinate viewer drawing

#ifndef GTS_STICKMAP_COORDINATES_H
#define GTS_STICKMAP_COORDINATES_H

#include "waveform.h"

enum STICKMAP_LIST { NONE, FF_WD, SHIELDDROP };

// Firefox and Wavedash min/max

// description string
extern const char* STICKMAP_FF_WD_DESC;
// possible return strings
extern const char* STICKMAP_FF_WD_RETVALS[];

// colors for the above responses, separated because it looks better this way
// 0 -> no color
// 1 -> black text on green background
// 2 -> black text on yellow background
extern const u32 STICKMAP_FF_WD_RETCOLORS[][2];

// enum for the two above
enum STICKMAP_FF_WD_ENUM { FF_WD_MISS, FF_WD_SAFE, FF_WD_UNSAFE };
extern const int STICKMAP_FF_WD_ENUM_LEN;

// coordinate tuples
// array of tuples is the best way to think of this
// safe coordinates
extern const int STICKMAP_FF_WD_COORD_SAFE[][2];
extern const int STICKMAP_FF_WD_COORD_SAFE_LEN;

// unsafe coordinates
extern const int STICKMAP_FF_WD_COORD_UNSAFE[][2];
extern const int STICKMAP_FF_WD_COORD_UNSAFE_LEN;


// Shield drop coordinates
extern const char* STICKMAP_SHIELDDROP_DESC;

extern const char* STICKMAP_SHIELDDROP_RETVALS[];

// 0 -> no color
// 1 -> black text on green background
// 2 -> white text on blue background
// 3 -> black text on yellow background
extern const u32 STICKMAP_SHIELDDROP_RETCOLORS[][2];

enum STICKMAP_SHIELDDROP_ENUM { SHIELDDROP_MISS, SHIELDDROP_VANILLA, SHIELDDROP_UCF_LOWER, SHIELDDROP_UCF_UPPER };
extern const int STICKMAP_SHIELDDROP_ENUM_LEN;

extern const int STICKMAP_SHIELDDROP_COORD_VANILLA[][2];
extern const int STICKMAP_SHIELDDROP_COORD_VANILLA_LEN;

extern const int STICKMAP_SHIELDDROP_COORD_UCF_LOWER[][2];
extern const int STICKMAP_SHIELDDROP_COORD_UCF_LOWER_LEN;

extern const int STICKMAP_SHIELDDROP_COORD_UCF_UPPER[][2];
extern const int STICKMAP_SHIELDDROP_COORD_UCF_UPPER_LEN;


int isCoordValid(enum STICKMAP_LIST, WaveformDatapoint);
int toStickmap(int meleeCoord);

#endif //GTS_STICKMAP_COORDINATES_H
