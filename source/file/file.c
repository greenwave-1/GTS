
#include "file/file.h"

#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include "waveform.h"
#include "print.h"
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// appended to the file, in order to prevent files from being overwritten
// technically this can only occur if someone exports multiple in one second
static unsigned int increment = 0;

static bool initSuccess = false, initAttempted = false;

bool initFilesystem() {
	if (!initAttempted) {
		initSuccess = fatInitDefault();
		initAttempted = true;
	}
	return initSuccess;
}

FILE *openFile(char *filename, char *modes) {
	if (!initFilesystem()) {
		return NULL;
	}
	
	{
		struct stat st = {0};
		// check if file already exists
		if (stat(filename, &st) == 0) {
			return NULL;
		}
	}
	
	FILE *retFile = fopen(filename, modes);
	
	if (!retFile) {
		return NULL;
	}
	
	return retFile;
}

int exportData(WaveformData *data) {
	data->exported = true;
	// do we have data to begin with?
	if (!data->isDataReady) {
		return 1;
	}
	
	if (!initFilesystem()) {
		return 2;
	}
	
	// get current time in YY-MM-DD_HH-MM-SS format
	time_t currTime;
	struct tm * timeinfo;
	
	char timeStr[32];
	{
		time(&currTime);
		timeinfo = localtime(&currTime);
		// YYYY-MM-DD_HH-MM-SS_microS
		strftime(timeStr, 32, "%Y-%m-%d_%H-%M-%S", timeinfo);
	}
	
	// create directory if it doesn't exist
	// https://stackoverflow.com/questions/7430248/creating-a-new-directory-in-c
	{
		struct stat st = {0};
		if (stat("/GTS", &st) == -1) {
			if (mkdir("/GTS", 0700) == -1) {
				return 3;
			}
		}
	}
	
	// create filepath
	char fileStr[64] = "/GTS/";
	strncat(fileStr, timeStr, 32);  // in theory this is right, idk if its actually right tho...
	strcat(fileStr, "_");
	{
		char numBuf[2];
		sprintf(numBuf, "%u", increment);
		strcat(fileStr, numBuf);
		// increment will only ever be 1-9
		increment++;
		increment %= 10;
	}
	
	strcat(fileStr, ".csv");
	
	{
		struct stat st = {0};
		// check if file already exists
		if (stat(fileStr, &st) == 0) {
			return 4;
		}
	}
	
	FILE *fptr = openFile(fileStr, "w");
	
	// first row is: datetime, number of polls
	fprintf(fptr, "%s,%u\n", timeStr, data->endPoint);
	
	// actual data
	// second row is x coords, third row is y coords, fourth row is time from last poll
	
	// x
	for (int i = 0; i < data->endPoint; i++) {
		if (i == 0) {
				fprintf(fptr, "%d", data->data[i].ax);
		} else {
			fprintf(fptr, ",%d", data->data[i].ax);
		}
	}
	fprintf(fptr, "\n");
	
	// y
	for (int i = 0; i < data->endPoint; i++) {
		if (i == 0) {
			fprintf(fptr, "%d", data->data[i].ay);
		} else {
			fprintf(fptr, ",%d", data->data[i].ay);
		}
	}
	fprintf(fptr, "\n");
	
	// time between polls
	for (int i = 0; i < data->endPoint; i++) {
		if (i == 0) {
			fprintf(fptr, "%llu", data->data[i].timeDiffUs);
		} else {
			fprintf(fptr, ",%llu", data->data[i].timeDiffUs);
		}
	}
	fprintf(fptr, "\n");
	fclose(fptr);
	
	return 0;
}