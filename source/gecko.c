//
// Created on 2025/02/12.
//

#include <stdio.h>
#include <gccore.h>
#include <string.h>
#include <ogc/usbgecko.h>

static u32 sum = 0;

void sendMessage(char *msg) {
	// TODO: is there a better way to do this?
	u32 tmp = 0;
	for (int i = 0; i < strlen(msg); i++) {
		tmp += msg[i];
	}
	
	// a lot pulled from https://github.com/DacoTaco/priiloader/blob/master/src/priiloader/source/gecko.cpp
	if (usb_isgeckoalive(EXI_CHANNEL_1) && tmp != sum) {
		sum = tmp;
		usb_flush(EXI_CHANNEL_1);
		usb_sendbuffer(1, msg, strlen(msg));
		usb_sendbuffer(1, "\n", 1);
		usb_flush(EXI_CHANNEL_1);
	}
}