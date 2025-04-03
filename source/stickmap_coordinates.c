//
// Created on 2/22/25.
//

#include "stickmap_coordinates.h"

// init all the extern strings
// refer to stickmap_coordinates.h for descriptions of these values
const char* STICKMAP_FF_WD_DESC = "Min/Max coordinates for Firefox and Wavedash notches around\ncardinals\n\n"
                                         "SAFE / Green -> Ideal coordinates, aim here\n"
                                         "UNSAFE / Yellow -> Risks hitting deadzone, but works\n"
                                         "MISS -> Self explanatory";

const char* STICKMAP_FF_WD_RETVALS[] = {"MISS", "SAFE", "UNSAFE"};

const u32 STICKMAP_FF_WD_RETCOLORS[][2] = { {COLOR_BLACK, COLOR_WHITE},
                                            {COLOR_LIME, COLOR_BLACK},
                                            {COLOR_YELLOW, COLOR_BLACK} };


const int STICKMAP_FF_WD_ENUM_LEN = 3;

const int STICKMAP_FF_WD_COORD_SAFE[][2] = { {9375, 3125},
                                             {9375, 3250} };
const int STICKMAP_FF_WD_COORD_SAFE_LEN = 2;

const int STICKMAP_FF_WD_COORD_UNSAFE[][2] = { {9500, 3000},
                                                      {9500, 2875} };
const int STICKMAP_FF_WD_COORD_UNSAFE_LEN = 2;


const char* STICKMAP_SHIELDDROP_DESC = "Coorinates for Vanilla and UCF Shield drops\n\n"
                                       "VANILLA / Green -> Coordinates that will work without UCF\n"
                                       "UCF LOWER / Blue -> Lower coordinates (idk)\n"
                                       "UCF v0.84 UPPER / Yellow -> v0.84 upper coordinates (idk)";

const char* STICKMAP_SHIELDDROP_RETVALS[] = { "MISS", "VANILLA", "UCF LOWER", "UCF v0.84 UPPER" };

const u32 STICKMAP_SHIELDDROP_RETCOLORS[][2] = { {COLOR_BLACK, COLOR_WHITE},
                                                 {COLOR_LIME, COLOR_BLACK},
                                                 {COLOR_BLUE, COLOR_WHITE},
                                                 {COLOR_YELLOW, COLOR_BLACK} };

const int STICKMAP_SHIELDDROP_ENUM_LEN = 4;

const int STICKMAP_SHIELDDROP_COORD_VANILLA[][2] = { { 7375, 6625 },
													 { 7375, 6750 },
													 { 7250, 6875 } };
const int STICKMAP_SHIELDDROP_COORD_VANILLA_LEN = 3;

const int STICKMAP_SHIELDDROP_COORD_UCF_LOWER[][2] = { { 7000, 7000 },
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
const int STICKMAP_SHIELDDROP_COORD_UCF_LOWER_LEN = 17;

const int STICKMAP_SHIELDDROP_COORD_UCF_UPPER[][2] = { { 7875, 6125 },
                                                       { 7750, 6125 },
													   
													   { 7625, 6250 },
													   { 7750, 6250 },
													   
													   { 7500, 6375 },
													   { 7625, 6375 },
													   
													   { 7375, 6500 },
													   { 7500, 6500 } };
const int STICKMAP_SHIELDDROP_COORD_UCF_UPPER_LEN = 8;

int isCoordValid(enum STICKMAP_LIST test, WaveformDatapoint coords) {
	int ret = 0;
	switch (test) {
		case FF_WD:
			// safe coords
			for (int i = 0; i < STICKMAP_FF_WD_COORD_SAFE_LEN; i++) {
				if ((coords.ax == STICKMAP_FF_WD_COORD_SAFE[i][0] && coords.ay == STICKMAP_FF_WD_COORD_SAFE[i][1]) ||
				    (coords.ay == STICKMAP_FF_WD_COORD_SAFE[i][0] && coords.ax == STICKMAP_FF_WD_COORD_SAFE[i][1])) {
					ret = 1;
					break;
				}
			}
			// unsafe coords
			for (int i = 0; i < STICKMAP_FF_WD_COORD_UNSAFE_LEN; i++) {
				if ((coords.ax == STICKMAP_FF_WD_COORD_UNSAFE[i][0] && coords.ay == STICKMAP_FF_WD_COORD_UNSAFE[i][1]) ||
				    (coords.ay == STICKMAP_FF_WD_COORD_UNSAFE[i][0] && coords.ax == STICKMAP_FF_WD_COORD_UNSAFE[i][1]) ) {
					ret = 2;
					break;
				}
			}
			break;
		case SHIELDDROP:
			if (coords.isAYNegative) {
				// vanilla
				for (int i = 0; i < STICKMAP_SHIELDDROP_COORD_VANILLA_LEN; i++) {
					if (coords.ay == STICKMAP_SHIELDDROP_COORD_VANILLA[i][1] ||
					    (coords.ay * -1) == STICKMAP_SHIELDDROP_COORD_VANILLA[i][1]) {
						ret = 1;
						break;
					}
				}
				// ucf lower
				for (int i = 0; i < STICKMAP_SHIELDDROP_COORD_UCF_LOWER_LEN; i++) {
					if ((coords.ax == STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][0] &&
					     coords.ay == STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][1]) ||
					    ((coords.ax * -1) == STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][0] &&
					     coords.ay == STICKMAP_SHIELDDROP_COORD_UCF_LOWER[i][1])) {
						ret = 2;
						break;
					}
				}
				// ucf v0.84 upper
				for (int i = 0; i < STICKMAP_SHIELDDROP_COORD_UCF_UPPER_LEN; i++) {
					if ((coords.ax == STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][0] &&
					     coords.ay == STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][1]) ||
					    ((coords.ax * -1) == STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][0] &&
					     coords.ay == STICKMAP_SHIELDDROP_COORD_UCF_UPPER[i][1])) {
						ret = 3;
						break;
					}
				}
			}
		case (NONE):
		default:
			break;
	}
	return ret;
}

int toStickmap(int meleeCoord) {
	return ((meleeCoord / 125) * 2);
}
