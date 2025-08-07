//
// Created on 2025/02/12.
//

// allows sending strings/data to a USB Gecko
// TODO: 1) Replace this with the built-in version ( SYS_Report )
// https://github.com/extremscorner/libogc2/commit/c93feb0a7a588bce13a8d89117d5ff531f1b70a7
// TODO: 2) Can this be configured to print to a network device (eg eth2gc)? Probably not without something custom.

#ifndef GTS_LOGGING_H
#define GTS_LOGGING_H

#define NETWORKSOCK_PORT 43256

enum LOGGING_DEVICE { USBGECKO_B, NETWORKSOCK };

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
