//
// Created on 7/4/25.
//

// Gate Visualizer submenu
// shows the bounds of a given stick gate, helpful for checking wear

#ifndef GTS_GATE_H
#define GTS_GATE_H

#include <stdint.h>

enum GATE_MENU_STATE { GATE_SETUP, GATE_POST_SETUP, GATE_INSTRUCTIONS };
enum GATE_STATE { GATE_INIT, GATE_POST_INIT };

void menu_gateMeasure(void *currXfb, uint16_t *p, uint16_t *h);
void menu_gateControllerDisconnected();
void menu_gateMeasureEnd();

#endif //GTS_GATE_H
