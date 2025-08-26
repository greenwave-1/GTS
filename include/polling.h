//
// Created on 2025/02/21.
//

// sets custom X/Y controller polling values

#ifndef GTS_POLLING_H
#define GTS_POLLING_H

#include <stdint.h>

void setSamplingRateHigh();
void setSamplingRateNormal();

void setSamplingRate();

bool isUnsupportedMode();

uint16_t* getButtonsDownPtr();
uint16_t* getButtonsHeldPtr();

#endif //GTS_POLLING_H
