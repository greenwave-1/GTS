#ifndef GTS_FILE_H
#define GTS_FILE_H

// file io stuff
#include "waveform.h"

#include <stdio.h>

// generic filesystem utils
bool initFilesystem();
FILE *openFile(char *filename, char *modes);

int exportData();

#endif //GTS_FILE_H
