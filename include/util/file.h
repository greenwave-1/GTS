#ifndef GTS_FILE_H
#define GTS_FILE_H

// file io stuff

// TODO: define folder paths here instead of hardcoded in file.c

#include <stdio.h>

#include "waveform.h"

// generic filesystem utils
bool initFilesystem();
void deinitFilesystem();
FILE *createFile(char *filename, char *modes);
FILE *openFile(char *filename, char *modes);

char* readFile(FILE *inFile, int *length);

int exportData();

#endif //GTS_FILE_H
