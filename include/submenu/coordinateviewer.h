//
// Created on 10/16/25.
//

// coordinate viewer submenu
// displays coordinates in melee-converted units (0-80)

// coordinate stickmaps are decoded from altimor json format/slightly extended format for gts (see data/*.json)
// "Builtin" stickmaps is from data/coordview_stickmaps.json. This file is compiled into gts, and is accessible
// via "coordview_stickmaps_json.h"

// other stickmaps can be loaded via storage. more specifically, gts will look for any json files in /gts/stickmaps/
// on every boot, and attempt to decode them. if successful, they are accessible via the file picker (L+A).

#ifndef GTS_COORDINATEVIEWER_H
#define GTS_COORDINATEVIEWER_H

enum COORD_VIEW_MENU_STATE { COORD_VIEW_SETUP, COORD_VIEW_POST_SETUP, COORD_VIEW_INSTRUCTIONS, COORD_VIEW_FILE_PICKER };

void menu_coordView();
void menu_coordViewEnd();

void menu_coordViewSetLockState(bool state);

#endif //GTS_COORDINATEVIEWER_H
