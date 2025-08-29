//
// Created on 2025/02/21.
//

// sets custom X/Y controller polling values

#ifndef GTS_POLLING_H
#define GTS_POLLING_H

#include <stdint.h>

#include <ogc/pad.h>

// 1 frame is ~16.666 ms
// _technically_ it's supposed to be ~16.833 but timings don't seem to align...
// i really have no idea here
#define FRAME_TIME_MS_F 16.666
#define FRAME_TIME_US 16666
#define FRAME_TIME_US_F (FRAME_TIME_MS_F * 1000)

extern const int FRAME_INTERVAL_MS[];

enum CONT_PORTS_BITFLAGS { CONT_PORT_1, CONT_PORT_2, CONT_PORT_3, CONT_PORT_4 };

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
