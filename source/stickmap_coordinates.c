//
// Created on 2/22/25.
//

#include "stickmap_coordinates.h"




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
