//
// Created on 3/17/25.
//

#include "continuous.h"
#include "../print.h"

#include <stdio.h>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include "../polling.h"
#include "../draw.h"
#include "../waveform.h"

static sampling_callback cb;
static bool setup = false;
static s8 xValues[500] = {0};
static u16 valueIndex = 0;
static char buf[50];

static void contSamplingCallback() {
	PAD_ScanPads();
	xValues[valueIndex] = PAD_StickX(0);
	valueIndex++;
	if (valueIndex == 500) {
		valueIndex = 0;
	}
	//PAD_SetSamplingCallback(cb);
}

void menu_continuousWaveform(void *currXfb) {
	setSamplingRateHigh();
	u64 startTime = gettime();
	//setSamplingRate();
	cb = PAD_SetSamplingCallback(contSamplingCallback);
	printStr("Hello new menu!\n", currXfb);
	for (int i = 1; i < 500; i++) {
		DrawLine((i - 1) + 50, 240 + xValues[i - 1], i + 50, 240 + xValues[i], COLOR_WHITE, currXfb);
		//DrawDot(i + 50, 240 + xValues[i], COLOR_WHITE, currXfb);
	}
	sprintf(buf, "last value: %d", xValues[valueIndex]);
	printStr(buf, currXfb);
}
