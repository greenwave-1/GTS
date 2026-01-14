#ifndef GTS_FILE_H
#define GTS_FILE_H

// file io stuff

#include <stdio.h>

#include "waveform.h"

// generic filesystem utils
bool initFilesystem();
void deinitFilesystem();
FILE *openFile(char *filename, char *modes);

int exportData();

#endif //GTS_FILE_H
