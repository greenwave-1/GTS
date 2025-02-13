//
// Created on 2/12/25.
//

#include <stdio.h>
#include <gccore.h>
#include <string.h>
#include <ogc/usbgecko.h>

static u32 sum = 0;

void sendMessage(char *msg, int len) {
	// TODO: is there a better way to do this?
	u32 tmp = 0;
	for (int i = 0; i < len; i++) {
		tmp += msg[i];
	}
	
	// a lot pulled from https://github.com/DacoTaco/priiloader/blob/master/src/priiloader/source/gecko.cpp
	// TODO: there's probably something weird that can happen with strncpy and such here...
	if (usb_isgeckoalive(EXI_CHANNEL_1) && tmp != sum) {
		sum = tmp;
		char tmpMsg[len + 3];
		memset(tmpMsg, 0, sizeof(tmpMsg));
		strncpy(tmpMsg, msg, len);
		strncat(tmpMsg, "\n\0", len + 1);
		usb_flush(EXI_CHANNEL_1);
		usb_sendbuffer(1, tmpMsg, len + 3);
		usb_flush(EXI_CHANNEL_1);
	}
}