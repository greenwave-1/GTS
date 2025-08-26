//
// Created on 2025/02/21.
//

// sets custom X/Y controller polling values

#ifndef GTS_POLLING_H
#define GTS_POLLING_H

#include <stdint.h>

#include <ogc/pad.h>

enum CONT_PORTS_BITFLAGS { CONT_PORT_1 = 1, CONT_PORT_2 = 2, CONT_PORT_3 = 4, CONT_PORT_4 = 8};

void setSamplingRateHigh();
void setSamplingRateNormal();

void setSamplingRate();

bool isUnsupportedMode();

uint16_t* getButtonsDownPtr();
uint16_t* getButtonsHeldPtr();

PADStatus getOriginStatus(enum CONT_PORTS_BITFLAGS port);

void attemptReadOrigin();
bool isControllerConnected(enum CONT_PORTS_BITFLAGS port);

#endif //GTS_POLLING_H
