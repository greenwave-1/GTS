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

void pauseLogging(bool state);
void setAllowDuplicateMessages(bool state);

enum LOGGING_NETWORK_STATUS { NETLOG_INIT, NETLOG_CONF_FAIL, NETLOG_CONF_SUCCESS };

enum LOGGING_NETWORK_STATUS getNetworkSetupState();
bool isConnectionMade();

char* getConfiguredIP();

//bool setupNetwork();

//bool attemptConnect();

#endif //GTS_LOGGING_H
