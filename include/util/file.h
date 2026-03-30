#ifndef GTS_FILE_H
#define GTS_FILE_H

// file io stuff

// TODO: define folder paths here instead of hardcoded in file.c

#include <stdio.h>

#include "waveform.h"

enum FS_DEVICE_LIST { DEVICE_NONE, DEVICE_CARD_A, DEVICE_CARD_B, DEVICE_GC_SP2, DEVICE_WII_SD, DEVICE_WII_USB };

// generic filesystem utils
bool initFilesystem();
void deinitFilesystem();

enum FS_DEVICE_LIST getCurrentDevice();
bool attemptOpenDevice(enum FS_DEVICE_LIST device);
char *getDeviceString(enum FS_DEVICE_LIST device);

FILE *createFile(char *filename, char *modes);
FILE *openFile(char *filename, char *modes);

char* readFile(FILE *inFile, int *length);

char** getFilesystemJson(int *len);

int exportData();

#endif //GTS_FILE_H
