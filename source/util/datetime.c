//
// Created on 10/29/25.
//

#include "util/datetime.h"

#include <time.h>
#include <malloc.h>

#include "util/file.h"

// YY-MM-DD_HH-MM-SS format
char *getDateTimeStr() {
	// 32 chars max
	char *retStr = malloc(sizeof(char) * 32);
	
	// get current time
	time_t currTime;
	struct tm * timeinfo;
	time(&currTime);
	timeinfo = localtime(&currTime);
	
	// YYYY-MM-DD_HH-MM-SS_microS
	strftime(retStr, 32, "%Y-%m-%d_%H-%M-%S", timeinfo);
	
	return retStr;
}

#ifndef NO_DATE_CHECK

static struct tm * getCurrTimeInfo() {
	time_t currTime;
	struct tm * timeinfo;
	
	time(&currTime);
	timeinfo = localtime(&currTime);
	
	return timeinfo;
}

bool forceDateSet = false;
enum DATE_CHECK_LIST forcedDate = DATE_NONE;

void forceDate(enum DATE_CHECK_LIST dateToForce) {
	forceDateSet = true;
	forcedDate = dateToForce;
}

enum DATE_CHECK_LIST checkDate() {
	if (forceDateSet) {
		return forcedDate;
	}
	
	if (initFilesystem()) {
		FILE *fileCheck = fopen("/GTS/color.txt", "r");
		
		if (fileCheck != NULL) {
			fclose(fileCheck);
			return DATE_PM;
		}
	}
	
	struct tm * timeinfo = getCurrTimeInfo();
	
	if (timeinfo->tm_mon == 3) {
		if (timeinfo->tm_mday == 20) {
			return DATE_NICE;
		}
	}
	
	if (timeinfo->tm_mon == 5) {
		return DATE_PM;
	}
	
	if (timeinfo->tm_mon == 11) {
		if (timeinfo->tm_mday == 24 || timeinfo->tm_mday == 25) {
			return DATE_CMAS;
		}
	}
	
	return DATE_NONE;
}
#endif
