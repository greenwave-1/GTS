
#include "export.h"

#include <fat.h>

#include <stdio.h>
#include <stdlib.h>
#include "waveform.h"
#include <stdbool.h>


bool exportData(WaveformData *data) {
	if (!fatInitDefault()) {
		printf("Fat init fail");
		return false;
	}
	FILE *fptr = fopen("test.csv", "w");
	fprintf(fptr, "test data please ignore\n");
	fclose(fptr);
	return true;
}