//
// Created on 2025/02/21.
//

// sets custom X/Y controller polling values

#ifndef GTS_POLLING_H
#define GTS_POLLING_H

#include <stdbool.h>

void setSamplingRateHigh();
void setSamplingRateNormal();

void setSamplingRate();

bool isUnsupportedMode();

#endif //GTS_POLLING_H
