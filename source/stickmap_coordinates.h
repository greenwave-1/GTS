//
// Created on 2/22/25.
//

#include "waveform.h"

#ifndef FOSSSCOPE_R2_STICKMAP_COORDINATES_H
#define FOSSSCOPE_R2_STICKMAP_COORDINATES_H


enum STICKMAP_LIST { NONE, FF_WD, SHIELDDROP };


// Firefox and Wavedash min/max

// description string
static const char* STICKMAP_FF_WD_DESC = "Min/Max coordinates for Firefox and Wavedash notches around\ncardinals\n\n"
										 "SAFE / Green -> Ideal coordinates, aim here\n"
										 "UNSAFE / Yellow -> Risks hitting deadzone, but works\n"
										 "MISS -> Self explanatory";
// possible return strings
static const char* STICKMAP_FF_WD_RETVALS[] = {"MISS", "SAFE", "UNSAFE"};

// colors for the above responses, separated because it looks better this way
// 0 -> no color
// 1 -> black text on green background
// 2 -> black text on yellow background
static const u32 STICKMAP_FF_WD_RETCOLORS[][2] = { {COLOR_BLACK, COLOR_WHITE},
												   {COLOR_LIME, COLOR_BLACK},
												   {COLOR_YELLOW, COLOR_BLACK} };

// enum for the two above
enum STICKMAP_FF_WD_ENUM { FF_WD_MISS, FF_WD_SAFE, FF_WD_UNSAFE };
static const int STICKMAP_FF_WD_ENUM_LEN = 3;

// coordinate tuples
// array of tuples is the best way to think of this
// safe coordinates
static const int STICKMAP_FF_WD_COORD_SAFE[][2] = { {9375, 3125},
													{9375, 3250} };
static const int STICKMAP_FF_WD_COORD_SAFE_LEN = 2;

// unsafe coordinates
static const int STICKMAP_FF_WD_COORD_UNSAFE[][2] = { {9500, 3000},
													  {9500, 2875} };
static const int STICKMAP_FF_WD_COORD_UNSAFE_LEN = 2;


// Shield drop coordinates
static const char* STICKMAP_SHIELDDROP_DESC = "Coorinates for Vanilla and UCF Shield drops\n\n"
											  "VANILLA / Green -> Coordinates that will work without UCF\n"
											  "UCF LOWER / Blue -> Lower coordinates (idk)\n"
											  "UCF v0.84 UPPER / Yellow -> v0.84 upper coordinates (idk)";

static const char* STICKMAP_SHIELDDROP_RETVALS[] = { "MISS", "VANILLA", "UCF LOWER", "UCF v0.84 UPPER" };

// 0 -> no color
// 1 -> black text on green background
// 2 -> white text on blue background
// 3 -> black text on yellow background
static const u32 STICKMAP_SHIELDDROP_RETCOLORS[][2] = { {COLOR_BLACK, COLOR_WHITE},
                                                        {COLOR_LIME, COLOR_BLACK},
                                                        {COLOR_BLUE, COLOR_WHITE},
                                                        {COLOR_YELLOW, COLOR_BLACK} };

enum STICKMAP_SHIELDDROP_ENUM { SHIELDDROP_MISS, SHIELDDROP_VANILLA, SHIELDDROP_UCF_LOWER, SHIELDDROP_UCF_UPPER };
static const int STICKMAP_SHIELDDROP_ENUM_LEN = 4;

static const int STICKMAP_SHIELDDROP_COORD_VANILLA[][2] = { { 7375, 6625},
                                                            { 7375, 6750},
                                                            { 7250, 6875} };
static const int STICKMAP_SHIELDDROP_COORD_VANILLA_LEN = 3;

static const int STICKMAP_SHIELDDROP_COORD_UCF_LOWER[][2] = { { 7000, 7000 },
                                                              { 7125, 7000 },
                                                              { 6875, 7125 },
                                                              { 7000, 7125 },
                                                              { 6750, 7250 },
                                                              { 6875, 7250 },
                                                              { 6500, 7375 },
                                                              { 6625, 7375 },
                                                              { 6750, 7375 },
                                                              { 6375, 7500 },
                                                              { 6500, 7500 },
                                                              { 6250, 7625 },
                                                              { 6375, 7625 },
                                                              { 6125, 7785 },
                                                              { 6250, 7785 },
                                                              { 6000, 7875 },
                                                              { 6125, 7875 } };
static const int STICKMAP_SHIELDDROP_COORD_UCF_LOWER_LEN = 17;

static const int STICKMAP_SHIELDDROP_COORD_UCF_UPPER[][2] = { { 7625, 6250 },
                                                              { 7750, 6250 },
                                                              { 7500, 6375 },
                                                              { 7625, 6375 },
                                                              { 7375, 6250 },
                                                              { 7500, 6250 } };
static const int STICKMAP_SHIELDDROP_COORD_UCF_UPPER_LEN = 6;


int isCoordValid(enum STICKMAP_LIST, WaveformDatapoint);
int toStickmap(int meleeCoord);

#endif //FOSSSCOPE_R2_STICKMAP_COORDINATES_H
