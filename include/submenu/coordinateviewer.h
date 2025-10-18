//
// Created on 10/16/25.
//

// coordinate viewer submenu
// displays coordinates in melee-converted units (0-80)

#ifndef GTS_COORDINATEVIEWER_H
#define GTS_COORDINATEVIEWER_H

enum COORD_VIEW_MENU_STATE { COORD_VIEW_SETUP, COORD_VIEW_POST_SETUP, COORD_VIEW_INSTRUCTIONS };

void menu_coordView();
void menu_coordViewEnd();

#endif //GTS_COORDINATEVIEWER_H
