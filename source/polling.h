//
// Created on 2/21/25.
//

#ifndef FOSSSCOPE_R2_POLLING_H
#define FOSSSCOPE_R2_POLLING_H

#include <stdbool.h>

void setSamplingRateHigh();
void setSamplingRateNormal();

void setSamplingRate();

bool isUnsupportedMode();

#endif //FOSSSCOPE_R2_POLLING_H
