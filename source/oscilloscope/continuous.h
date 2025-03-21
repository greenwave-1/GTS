//
// Created on 3/17/25.
//

#ifndef FOSSSCOPE_R2_CONTINUOUS_H
#define FOSSSCOPE_R2_CONTINUOUS_H

#include <gccore.h>

enum CONT_MENU_STATE { CONT_SETUP, CONT_POST_SETUP };
enum CONT_STATE { INPUT, INPUT_LOCK };

void menu_continuousWaveform(void *currXfb, u32 *p, u32 *h);
void menu_continuousEnd();



#endif //FOSSSCOPE_R2_CONTINUOUS_H
