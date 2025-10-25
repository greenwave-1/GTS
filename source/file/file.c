#include "file/file.h"

#include <fat.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <inttypes.h>

#include <ogc/pad.h>

#include "waveform.h"
#include "print.h"

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

int exportData() {
	ControllerRec *data = *(getRecordingData());
	data->dataExported = true;
	// do we have data to begin with?
	if (!data->isRecordingReady || data->recordingType == REC_CLEAR) {
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
	
	// first row is: datetime, number of polls, total time in microseconds, type of recording
	fprintf(fptr, "%s,%u,%" PRIu64 ",%d\n", timeStr, data->sampleEnd, data->totalTimeUs, data->recordingType);
	
	switch (data->recordingType) {
		case REC_OSCILLOSCOPE:
			// X, Y, CX, CY, time from last poll
			fprintf(fptr, "%d,%d,%d,%d,%" PRIu64 "\n",
					data->samples[0].stickX, data->samples[0].stickY,
					data->samples[0].cStickX, data->samples[0].cStickY,
					data->samples[0].timeDiffUs);
			for (int i = 1; i < data->sampleEnd; i++) {
				fprintf(fptr, "%d,%d,%d,%d,%" PRIu64 "\n",
				        data->samples[i].stickX, data->samples[i].stickY,
				        data->samples[i].cStickX, data->samples[i].cStickY,
				        data->samples[i].timeDiffUs);
			}
			break;
		
		case REC_2DPLOT:
			// X, Y, buttons (decimal u16), time from last poll
			fprintf(fptr, "%d,%d,%d,%" PRIu64 "\n",
			        data->samples[0].stickX, data->samples[0].stickY,
			        data->samples[0].buttons, data->samples[0].timeDiffUs);
			for (int i = 1; i < data->sampleEnd; i++) {
				fprintf(fptr, "%d,%d,%d,%" PRIu64 "\n",
				        data->samples[i].stickX, data->samples[i].stickY,
				        data->samples[i].buttons, data->samples[i].timeDiffUs);
			}
			break;
		
		case REC_TRIGGER_L:
			// Analog L, Digital L, time from last poll
			fprintf(fptr, "%u,%" PRIu16 ",%" PRIu64 "\n",
			        data->samples[0].triggerL, data->samples[0].buttons & PAD_TRIGGER_L,
					data->samples[0].timeDiffUs);
			for (int i = 1; i < data->sampleEnd; i++) {
				fprintf(fptr, "%u,%" PRIu16 ",%" PRIu64 "\n",
				        data->samples[i].triggerL, data->samples[i].buttons & PAD_TRIGGER_L,
				        data->samples[i].timeDiffUs);
			}
			break;
		
		case REC_TRIGGER_R:
			// Analog L, Digital L, time from last poll
			fprintf(fptr, "%u,%d,%" PRIu64 "\n",
			        data->samples[0].triggerR, data->samples[0].buttons & PAD_TRIGGER_R,
			        data->samples[0].timeDiffUs);
			for (int i = 1; i < data->sampleEnd; i++) {
				fprintf(fptr, "%u,%d,%" PRIu64 "\n",
				        data->samples[i].triggerR, data->samples[i].buttons & PAD_TRIGGER_R,
				        data->samples[i].timeDiffUs);
			}
			break;
			
		case REC_BUTTONTIME:
			// X, Y, CX, CY, Analog L, Analog R, buttons (decimal u16), time from last poll
			fprintf(fptr, "%d,%d,%d,%d,%u,%u,%" PRIu16 ",%" PRIu64 "\n",
			        data->samples[0].stickX, data->samples[0].stickY,
			        data->samples[0].cStickX, data->samples[0].cStickY,
			        data->samples[0].triggerL, data->samples[0].triggerR,
			        data->samples[0].buttons, data->samples[0].timeDiffUs);
			for (int i = 1; i < data->sampleEnd; i++) {
				fprintf(fptr, "%d,%d,%d,%d,%u,%u,%" PRIu16 ",%" PRIu64 "\n",
				        data->samples[i].stickX, data->samples[i].stickY,
				        data->samples[i].cStickX, data->samples[i].cStickY,
				        data->samples[i].triggerL, data->samples[i].triggerR,
				        data->samples[i].buttons, data->samples[i].timeDiffUs);
			}
			break;
		
		// this shouldn't happen??
		case REC_CLEAR:
		default:
			fclose(fptr);
			return 1;
	}
	
	fclose(fptr);
	
	return 0;
}