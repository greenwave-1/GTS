
#include "file.h"

#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include "waveform.h"
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// appended to the file, in order to prevent files from being overwritten
// technically this can only occur if someone exports multiple in one second
static unsigned int increment = 0;

bool exportData(WaveformData *data, bool exportAsMeleeValues) {
	// do we have data to begin with?
	if (!data->isDataReady) {
		return false;
	}
	
	if (!fatInitDefault()) {
		printf("Fat init fail");
		return false;
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
		if (stat("/FossScope", &st) == -1) {
			if (mkdir("/FossScope", 0700) == -1) {
				printf("directory creation fail");
				return false;
			}
		}
	}
	
	// create filepath
	char fileStr[64] = "/FossScope/";
	strcat(fileStr, timeStr);  // in theory this is right, idk if its actually right tho...
	strcat(fileStr, "_");
	{
		char numBuf[4];
		sprintf(numBuf, "%u", increment);
		strcat(fileStr, numBuf);
		increment++;
		increment %= 10;
	}
	
	strcat(fileStr, ".csv");
	
	{
		struct stat st = {0};
		// check if file already exists
		if (stat(fileStr, &st) == 0) {
			printf("file exists");
			return false;
		}
	}
	
	FILE *fptr = fopen(fileStr, "w");
	
	// first row is: datetime, number of polls, are the polls being exported as melee coords
	fprintf(fptr, "%s,%u,%d\n", timeStr, data->endPoint, exportAsMeleeValues);
	
	// actual data
	// second row is x coords, third row is y coords, fourth row is time from last poll
	
	// x
	for (int i = 0; i < data->endPoint; i++) {
		if (exportAsMeleeValues) {
			WaveformDatapoint temp = convertStickValues(&(data->data[i]));
			if (i == 0) {
				fprintf(fptr, "%d", temp.ax);
			} else {
				fprintf(fptr, ",%d", temp.ax);
			}
		} else {
			if (i == 0) {
				fprintf(fptr, "%d", data->data[i].ax);
			} else {
				fprintf(fptr, ",%d", data->data[i].ax);
			}
		}
	}
	fprintf(fptr, "\n");
	
	// y
	for (int i = 0; i < data->endPoint; i++) {
		if (exportAsMeleeValues) {
			WaveformDatapoint temp = convertStickValues(&(data->data[i]));
			if (i == 0) {
				fprintf(fptr, "%d", temp.ay);
			} else {
				fprintf(fptr, ",%d", temp.ay);
			}
		} else {
			if (i == 0) {
				fprintf(fptr, "%d", data->data[i].ay);
			} else {
				fprintf(fptr, ",%d", data->data[i].ay);
			}
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
	return true;
}