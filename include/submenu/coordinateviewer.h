//
// Created on 10/16/25.
//

// coordinate viewer submenu
// displays coordinates in melee-converted units (0-80)

// individual coordinate sets are defined as static consts in coordinateviewer.c, with the following format:
// char * _DESC -> string describing what the coordinates are, and possible options
// char* _RETVALS -> string for a given result from possible coordinates
// GXColor _RETCOLORS[][2] -> array for text background and foreground, if applicable
// _ENUM -> possible return values, the order should match both _RETVALS and _RETCOLORS
// _ENUM_LEN -> self-explanatory

// Then, the actual coordinates are defined in 2d arrays, separated by their type/possible return value
// These are stored as the decimal part of Melee coords (multiples of 0.0125), first column is X, second column is Y
// TODO: the quadrant that a given coordinate set is for is not stored here
//  (IE coordinates are always stored as positive)
// for now, that check is done in isCoordValid() and drawStickmapOverlay() on a case-by-case basis.
// drawStickmapOverlay() also handles what subset of coordinates are drawn, defined by (int which).
// isCoordValid will _always_ return if a given coordinate belongs to a group at all, it does _not_ take chosen subset
// into account.

#ifndef GTS_COORDINATEVIEWER_H
#define GTS_COORDINATEVIEWER_H

enum COORD_VIEW_MENU_STATE { COORD_VIEW_SETUP, COORD_VIEW_POST_SETUP, COORD_VIEW_INSTRUCTIONS };

void menu_coordView();
void menu_coordViewEnd();

void menu_coordViewSetLockState(bool state);

#endif //GTS_COORDINATEVIEWER_H
