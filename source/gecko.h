//
// Created on 2025/02/12.
//

// allows sending strings/data to a USB Gecko
// TODO: 1) Replace this with the built-in version ( SYS_Report )
// https://github.com/extremscorner/libogc2/commit/c93feb0a7a588bce13a8d89117d5ff531f1b70a7
// TODO: 2) Can this be configured to print to a network device (eg eth2gc)? Probably not without something custom.

#ifndef GTS_GECKO_H
#define GTS_GECKO_H

void sendMessage(char *msg);

#endif //GTS_GECKO_H
