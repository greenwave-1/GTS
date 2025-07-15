//
// Created on 7/4/25.
//

#ifndef GTS_GATE_H
#define GTS_GATE_H

#include <gctypes.h>

enum GATE_MENU_STATE { GATE_SETUP, GATE_POST_SETUP, GATE_INSTRUCTIONS };
enum GATE_STATE { GATE_INIT, GATE_POST_INIT };

void menu_gateMeasure(void *currXfb, u32 *p, u32 *h);
void menu_gateControllerDisconnected();
void menu_gateMeasureEnd();

#endif //GTS_GATE_H
