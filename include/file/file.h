// file io stuff
#include "waveform.h"
#include <stdbool.h>
//#include <fat.h>

#ifndef GTS_FILE_H
#define GTS_FILE_H

// generic filesystem utils
bool initFilesystem();
FILE *openFile(char *filename, char *modes);

int exportData(WaveformData *data);

#endif //GTS_FILE_H
