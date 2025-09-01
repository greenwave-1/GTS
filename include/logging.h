//
// Created on 2025/02/12.
//

// debug logging helper
// can be configured to print messages to a usb gecko, a network socket, or a file on mounted storage

#ifndef GTS_LOGGING_H
#define GTS_LOGGING_H

#define NETWORKSOCK_PORT 43256

enum LOGGING_DEVICE { USBGECKO_B, NETWORKSOCK, LOGFILE };

enum LOGGING_DEVICE getLoggingType();

void setupLogging(enum LOGGING_DEVICE device);

void debugLog(char *msg, ...);

void stopLogging();

bool isNetworkConfigured();
bool isConnectionMade();

char* getConfiguredIP();

//bool setupNetwork();

//bool attemptConnect();

#endif //GTS_LOGGING_H
