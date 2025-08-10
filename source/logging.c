//
// Created on 2025/02/12.
//

// most network stuff in here is pulled from the sockettest example

#include "logging.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <network.h>
#include <stdarg.h>

#include <ogc/lwp.h>
#include <ogc/exi.h>
#include <ogc/system.h>

#include "file/file.h"

static lwp_t socket_thread = (lwp_t) NULL;

static enum LOGGING_DEVICE dev = USBGECKO_B;

static bool deviceSet = false;

static bool networkSetupSuccess = false, connectionEstablished = false;

static char localip[16] = {0};
static char gateway[16] = {0};
static char netmask[16] = {0};

static int32_t sock, csock;
static struct sockaddr_in client, server;
static uint32_t clientlen;

static FILE *logFile = NULL;

static void attemptConnect() {
	if (networkSetupSuccess) {
		clientlen = sizeof(client);
		sock = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		if (sock == INVALID_SOCKET) {
			return;
		}
		memset(&server, 0, sizeof(server));
		memset(&client, 0, sizeof(client));

		server.sin_family = AF_INET;
		server.sin_port = htons(NETWORKSOCK_PORT);
		server.sin_addr.s_addr = INADDR_ANY;
		
		if (net_bind(sock, (struct sockaddr*) &server, sizeof(server))) {
			return;
		}
		if (net_listen(sock, 5)) {
			return;
		}
		
		while (true) {
			csock = net_accept(sock, (struct sockaddr *) &client, &clientlen);
			if (csock < 0) {
				continue;
			} else {
				break;
			}
		}
	}
	connectionEstablished = true;
	return;
}

static void *setupNetwork(void *args) {
	if (if_config(localip, netmask, gateway, true) >= 0) {
		networkSetupSuccess = true;
		attemptConnect();
	}
	return NULL;
}

static void networkMessage(char *msg, va_list list) {
	static char msgBuffer[500];
	if (connectionEstablished) {
		vsnprintf(msgBuffer, 500, msg, list);
		net_send(csock, msgBuffer, strlen(msgBuffer), 0);
		net_send(csock, "\n", 2, 0);
	}
}

enum LOGGING_DEVICE getLoggingType() {
	return dev;
}

void setupLogging(enum LOGGING_DEVICE device) {
	dev = device;
	switch (dev) {
		case USBGECKO_B:
			SYS_EnableGecko(EXI_CHANNEL_1, true);
			deviceSet = true;
			break;
		case NETWORKSOCK:
			if (!networkSetupSuccess) {
				// called in a thread so that we can print while we wait
				LWP_CreateThread(&socket_thread, setupNetwork, NULL, NULL, 2048, 50);
				deviceSet = true;
			}
			break;
		case LOGFILE:
			if (initFilesystem()) {
				logFile = openFile("/GTS/debug.log", "a");
			}
			if (logFile != NULL) {
				deviceSet = true;
			}
			break;
		default:
			break;
	}
}

static uint32_t sum = 0;

void debugLog(char *msg, ...) {
	if (deviceSet) {
		// ensure we don't send duplicate messages
		uint32_t temp = 0;
		for (int i = 0; i < strlen(msg); i++) {
			temp += msg[i];
		}
		if (temp == sum) {
			return;
		}
		sum = temp;
		
		// actually send message
		va_list list;
		va_start(list, msg);
		switch (dev) {
			case USBGECKO_B:
				SYS_Reportv(msg, list);
				SYS_Report("\n");
				break;
			case NETWORKSOCK:
				networkMessage(msg, list);
				break;
			case LOGFILE:
				vfprintf(logFile, msg, list);
				fprintf(logFile, "\n");
				break;
		}
		va_end(list);
	}
}

void stopLogging() {
	debugLog("End of log.");
	switch (dev) {
		case NETWORKSOCK:
			net_close(csock);
			net_close(sock);
			break;
		case LOGFILE:
			fclose(logFile);
			break;
		default:
			break;
	}
}

char* getConfiguredIP() {
	return localip;
}

bool isNetworkConfigured() {
	return networkSetupSuccess;
}

bool isConnectionMade() {
	return connectionEstablished;
}
