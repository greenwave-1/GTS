//
// Created on 3/9/26.
//

#include "stickmap.h"

#include <math.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

#include <ogc/pad.h>

#include "util/file.h"
#include "util/print.h"
#include "util/polling.h"

// IF THESE ARE NOT FOUND AT COMPILE TIME, YOU ARE MISSING THE .json FILES IN data/
#include "plot2d_stickmaps_json.h"
#include "coordview_stickmaps_json.h"

static void createSubCoordList(StickmapSubcategory *data) {
	// we do this twice
	for (int iter = 0; iter < 2; iter++) {
		int totalCoords = 0;
		
		// iterate over provided range
		for (int x = data->minX; x <= data->maxX; x++) {
			for (int y = data->minY; y <= data->maxY; y++) {
				// check magnitude
				double mag = sqrt((x * x) + (y * y));
				
				if (mag >= data->magnitudeMin && mag <= data->magnitudeMax) {
					// check angle
					double angle = atan2(y, x) * 180 / M_PI;
					
					if (angle >= data->angleMin && angle <= data->angleMax) {
						// iterate over each valid quadrant
						for (int i = 0; i < 4; i++) {
							if (data->quadrants[i]) {
								// check if we would be storing a "negative zero"
								if (((i == 1 || i == 2) && x == 0) ||
										(i > 1 && y == 0)) {
									continue;
								}

								// store values on second run
								if (iter == 1) {
									data->coordList[totalCoords][0] = x;
									data->coordList[totalCoords][1] = y;
									
									// set sign based on quadrant
									// x
									if (i == 1 || i == 2) {
										data->coordList[totalCoords][0] *= -1;
									}
									
									// y
									if (i > 1) {
										data->coordList[totalCoords][1] *= -1;
									}
								}
								
								// increment
								totalCoords++;
							}
						}
					}
				}
			}
		}
		
		// allocate memory on first run
		if (iter == 0) {
			if (totalCoords == 0) {
				return;
			}
			data->numOfCoords = totalCoords;
			data->coordList = malloc(sizeof(int[data->numOfCoords][2]));
			if (data->coordList == NULL) {
				data->numOfCoords = 0;
				return;
			}
		}
	}
}

enum STICKMAP_JSON_TYPE identifyJson(const char jsonFile[], json_t **root) {
	enum STICKMAP_JSON_TYPE retVal = STICKMAP_TYPE_ERR;
	
	json_error_t jsonError;
	*root = json_loads(jsonFile, 0, &jsonError);
	
	if (json_is_array(*root)) {
		// check if this is formatted for gts
		json_t *data = json_array_get(*root, 0);
		if (json_is_object(data)) {
			json_t *gtsToken = json_object_get(data, "format_gts");
			json_t *normalToken = json_object_get(data, "displayMode");
			
			// check for gts specific token
			if (json_is_boolean(gtsToken) && json_boolean_value(gtsToken)) {
				retVal = STICKMAP_TYPE_GTS;
			}
			// check for random field that is likely to indicate altimor stickmap format
			else if (json_is_integer(normalToken)) {
				retVal = STICKMAP_TYPE_NORMAL;
			}
		}
	}
	
	return retVal;
}

static bool decodeJsonSubcategory(StickmapSubcategory *target, json_t *data) {
	target->coordList = NULL;
	target->numOfCoords = 0;
	
	if (json_is_object(data)) {
		json_t *dataFromJson = json_object_get(data, "name");
		if (!json_is_string(dataFromJson)) {
			return false;
		}
		target->name = json_string_value(dataFromJson);
		
		dataFromJson = json_object_get(data, "color");
		if (!json_is_array(dataFromJson)) {
			return false;
		}
		if (json_array_size(dataFromJson) != 4) {
			return false;
		}
		target->color.r = json_integer_value(json_array_get(dataFromJson, 0));
		target->color.g = json_integer_value(json_array_get(dataFromJson, 1));
		target->color.b = json_integer_value(json_array_get(dataFromJson, 2));
		target->color.a = json_integer_value(json_array_get(dataFromJson, 3));
		
		dataFromJson = json_object_get(data, "quadrants");
		if (!json_is_array(dataFromJson)) {
			return false;
		}
		if (json_array_size(dataFromJson) != 4) {
			return false;
		}
		int index = 0;
		json_t *value;
		json_array_foreach(dataFromJson, index, value) {
			target->quadrants[index] = json_boolean_value(value);
		}
		
		dataFromJson = json_object_get(data, "displayMode");
		if (!json_is_integer(dataFromJson)) {
			return false;
		}
		target->displayMode = json_integer_value(dataFromJson);
		
		dataFromJson = json_object_get(data, "minX");
		if (!json_is_integer(dataFromJson)) {
			return false;
		}
		target->minX = json_integer_value(dataFromJson);
		
		dataFromJson = json_object_get(data, "minY");
		if (!json_is_integer(dataFromJson)) {
			return false;
		}
		target->minY = json_integer_value(dataFromJson);
		
		dataFromJson = json_object_get(data, "maxX");
		if (!json_is_integer(dataFromJson)) {
			return false;
		}
		target->maxX = json_integer_value(dataFromJson);
		
		dataFromJson = json_object_get(data, "maxY");
		if (!json_is_integer(dataFromJson)) {
			return false;
		}
		target->maxY = json_integer_value(dataFromJson);
		
		dataFromJson = json_object_get(data, "angleMin");
		if (!json_is_integer(dataFromJson)) {
			return false;
		}
		target->angleMin = json_integer_value(dataFromJson);
		
		dataFromJson = json_object_get(data, "angleMax");
		if (!json_is_integer(dataFromJson)) {
			return false;
		}
		target->angleMax = json_integer_value(dataFromJson);
		
		dataFromJson = json_object_get(data, "magnitudeMin");
		if (!json_is_integer(dataFromJson)) {
			return false;
		}
		target->magnitudeMin = json_integer_value(dataFromJson);
		
		dataFromJson = json_object_get(data, "magnitudeMax");
		if (!json_is_integer(dataFromJson)) {
			return false;
		}
		target->magnitudeMax = json_integer_value(dataFromJson);
	}
	
	return true;
}

// dumb
static bool isGTSFile = false;

Stickmap *readJsonNormal(json_t *root, const char *stickmapName) {
	Stickmap *retVal = malloc(sizeof(Stickmap));
	if (retVal == NULL) {
		isGTSFile = false;
		return NULL;
	}
	
	retVal->name = stickmapName;
	
	// this is set to true if called from readJsonGTS()
	// in all other cases, we set a generic description
	// TODO: there's probably a better way to do this...
	if (!isGTSFile) {
		retVal->desc = "Data imported from file";
	}
	isGTSFile = false;
	
	// allocate array based on how many entries
	retVal->subcategoryListLen = 0;
	retVal->subcategoryList = malloc(sizeof(StickmapSubcategory) * (json_array_size(root)));
	
	if (retVal->subcategoryList == NULL) {
		freeStickmap(retVal);
		return NULL;
	}
	
	// decode each entry
	for (int i = 0; i < json_array_size(root); i++) {
		if ( decodeJsonSubcategory(&retVal->subcategoryList[i], json_array_get(root, i)) ) {
			// only increment if we get valid data
			retVal->subcategoryListLen++;
		}
	}
	
	// we need to know how many different "subcategories" actually exist
	const char *prevName = retVal->subcategoryList[0].name;
	retVal->subcategoryDescListLen = 1;
	for (int i = 1; i < json_array_size(root); i++) {
		// increment when we find a _different_ string
		if (strcmp(prevName, retVal->subcategoryList[i].name) != 0) {
			prevName = retVal->subcategoryList[i].name;
			retVal->subcategoryDescListLen++;
		}
	}
	
	// now we allocate space based on the above
	retVal->subcategoryDescList = malloc(sizeof(StickmapSubcategoryDesc) * (retVal->subcategoryDescListLen));
	if (retVal->subcategoryDescList == NULL) {
		freeStickmap(retVal);
		return NULL;
	}
	for (int i = 0; i < retVal->subcategoryDescListLen; i++) {
		retVal->subcategoryDescList[i].name = NULL;
		retVal->subcategoryDescList[i].desc = NULL;
		retVal->subcategoryDescList[i].listIndexStart = -1;
	}
	
	// and set names, and find starting indices
	// set first one manually
	retVal->subcategoryDescList[0].name = retVal->subcategoryList[0].name;
	retVal->subcategoryDescList[0].listIndexStart = 0;
	
	int descIndex = 1;
	prevName = retVal->subcategoryList[0].name;
	
	for (int i = 1; i < json_array_size(root); i++) {
		// do the strings differ?
		if (strcmp(prevName, retVal->subcategoryList[i].name) != 0) {
			// set data
			prevName = retVal->subcategoryList[i].name;
			retVal->subcategoryDescList[descIndex].name = prevName;
			retVal->subcategoryDescList[descIndex].listIndexStart = i;
			descIndex++;
		}
		
		if (descIndex == retVal->subcategoryDescListLen) {
			// we're done
			break;
		}
	}
	
	// return actual pointer if we're good
	if (retVal->subcategoryListLen != 0) {
		return retVal;
	}
	
	// free allocated memory otherwise and return NULL
	freeStickmap(retVal);
	return NULL;
}

Stickmap **readJsonGTS(json_t *root, int *len) {
	int decodedEntries = 0;
	Stickmap **target = malloc(sizeof(Stickmap*) * json_array_size(root));
	if (target == NULL) {
		return NULL;
	}

	// index 0 had the "gts_format" token, so we start at index 1
	for (int i = 1; i < json_array_size(root); i++) {
		json_t *data = json_array_get(root, i);
		
		json_t *dataFromJson = json_object_get(data, "stickmap_name");
		if (!json_is_string(dataFromJson)) {
			continue;
		}
		const char *stickmapName = json_string_value(dataFromJson);
		
		// ok this is slightly unintuitive
		// some subcategories/zones require multiple entries/"rules" to describe fully,
		// we use listIndexStart to determine where a given set starts
		// this assumes that the data in the json is _in order_
		
		dataFromJson = json_object_get(data, "subcategory_data");
		if (!json_is_array(dataFromJson)) {
			continue;
		}
		
		// read in normal data
		isGTSFile = true;
		// subcategory_data is literally the unmodified altimor stickmap format, so we just call readJsonNormal()
		Stickmap *temp = readJsonNormal(dataFromJson, stickmapName);
		
		if (temp == NULL) {
			continue;
		}
		
		dataFromJson = json_object_get(data, "stickmap_desc");
		if (!json_is_string(dataFromJson)) {
			freeStickmap(temp);
			continue;
		}
		temp->desc = json_string_value(dataFromJson);
		
		// get subcategory names and descriptions
		dataFromJson = json_object_get(data, "stickmap_subcategories");
		if (!json_is_array(dataFromJson)) {
			freeStickmap(temp);
			continue;
		}
		
		// do we have descriptions?
		if (json_array_size(dataFromJson) == temp->subcategoryDescListLen) {
			for (int j = 0; j < json_array_size(dataFromJson); j++) {
				json_t *entry = json_array_get(dataFromJson, j);
				
				// set names optionally if we have them, and they are shorter
				json_t *subName = json_object_get(entry, "subcategory_name");
				if (json_is_string(subName)) {
					const char *subDescName = json_string_value(subName);
					// DescList.name shouldn't ever be null due to readJsonNormal(), so this should be fine??
					if (strlen(subDescName) < strlen(temp->subcategoryDescList[j].name)) {
						temp->subcategoryDescList[j].name = subDescName;
					}
				}
				
				// set description
				// note that this is _NOT_ optional
				// TODO: have logic here to check if a description exists
				//  if not, and we have a replacement name, move the original name to the description
				json_t *subDesc = json_object_get(entry, "subcategory_desc");
				if (!json_is_string(subDesc)) {
					continue;
				}
				temp->subcategoryDescList[j].desc = json_string_value(subDesc);
			}
		}
		
		target[decodedEntries] = temp;
		decodedEntries++;
	}
	
	// return actual pointer if we're good
	if (decodedEntries != 0) {
		*len = decodedEntries;
		return target;
	}

	// don't need to call freeStickmap() here since decodedEntries will be 0
	free(target);
	
	return NULL;
}

void genStickmapCoords(Stickmap *target) {
	for (int i = 0; i < target->subcategoryListLen; i++) {
		createSubCoordList(&target->subcategoryList[i]);
	}
}

int getCoordSubcategory(MeleeCoordinates coord, Stickmap *stickmap) {
	// iterate over each subcategory
	for (int i = 0; i < stickmap->subcategoryListLen; i++) {
		StickmapSubcategory *sub = &stickmap->subcategoryList[i];
		// check magnitude
		if (coord.magnitude >= sub->magnitudeMin &&
			coord.magnitude <= sub->magnitudeMax) {
			int x = abs(coord.stickX);
			int y = abs(coord.stickY);
			// initial range check
			if (x >= sub->minX && x <= sub->maxX &&
				y >= sub->minY && y <= sub->maxY) {
				// check quadrants
				for (int j = 0; j < 4; j++) {
					if (sub->quadrants[j]) {
						// set sign based on quadrant
						if (j == 1 || j == 2) {
							x *= -1;
						}
						if (j > 1) {
							y *= -1;
						}
						// do signs match?
						if (x == coord.stickX && y == coord.stickY) {
							// check angle (done here so that we don't do atan2() as often(?)
							x = abs(x);
							y = abs(y);
							double angle = atan2(y, x) * 180 / M_PI;
							if (angle >= sub->angleMin && angle <= sub->angleMax) {
								return i;
							}
						}
					}
					// return values to normal
					x = abs(x);
					y = abs(y);
				}
			}
		}
	}
	
	return -1;
}

static Stickmap **plot2dStickmaps = NULL;
static int plot2dStickmapsLen = 0;
static Stickmap **coordViewStickmaps = NULL;
static int coordViewStickmapsLen = 0;
void loadBuiltinStickmaps() {
	if (plot2dStickmaps == NULL && coordViewStickmaps == NULL) {
		json_t *root;
		enum STICKMAP_JSON_TYPE jsonType = identifyJson((char *) plot2d_stickmaps_json, &root);
		
		assert(jsonType == STICKMAP_TYPE_GTS);
		plot2dStickmaps = readJsonGTS(root, &plot2dStickmapsLen);
		
		jsonType = identifyJson((char *) coordview_stickmaps_json, &root);
		
		assert(jsonType == STICKMAP_TYPE_GTS);
		coordViewStickmaps = readJsonGTS(root, &coordViewStickmapsLen);
		for (int i = 0; i < coordViewStickmapsLen; i++) {
			genStickmapCoords(coordViewStickmaps[i]);
		}
	}
}

Stickmap **getBuiltinStickmap(enum STICKMAP_BUILTIN_LIST list, int *len) {
	Stickmap **ret = NULL;
	switch (list) {
		case STICKMAP_PLOT2D:
			*len = plot2dStickmapsLen;
			ret = plot2dStickmaps;
			break;
		case STICKMAP_COORDVIEW:
			*len = coordViewStickmapsLen;
			ret = coordViewStickmaps;
			break;
		default:
			break;
	}
	
	return ret;
}

static char **externalJsonFiles = NULL;
static int externalJsonNum = 0;
static bool readFilesystemForJson = false;

static ExternalStickmap *externalStickmaps = NULL;
static int externalStickmapsLen = 0;

void loadExternalJsonList() {
	if (!readFilesystemForJson) {
		if (initFilesystem()) {
			externalJsonFiles = getFilesystemJson(&externalJsonNum);
			// allocate if we actually get entries
			if (externalJsonNum != 0 && externalJsonFiles != NULL) {
				externalStickmaps = calloc(sizeof(ExternalStickmap) * externalJsonNum, sizeof(ExternalStickmap));
				for (int i = 0; i < externalJsonNum; i++) {
					// build path to open file
					char *filePath = calloc((sizeof(char) * 256) + 16, sizeof(char));
					strcat(filePath, "/gts/stickmaps/");
					strcat(filePath, externalJsonFiles[i]);
					
					// open and read file
					FILE *inFile = openFile(filePath, "r");
					int len = 0;
					if (inFile == NULL) {
						continue;
					}
					char *buf = readFile(inFile, &len);
					fclose(inFile);
					free(filePath);
					
					// did something error?
					if (buf == NULL || len == 0) {
						continue;
					}
					
					json_t *root;
					externalStickmaps[externalStickmapsLen].stickmapType = identifyJson(buf, &root);
					free(buf);
					externalStickmaps[externalStickmapsLen].fileName = externalJsonFiles[i];
					externalStickmaps[externalStickmapsLen].stickmapArrLen = -1;
					
					// determine type of json
					switch (externalStickmaps[externalStickmapsLen].stickmapType) {
						case STICKMAP_TYPE_GTS:
							Stickmap **gtsTemp = readJsonGTS(root, &len);
							if (gtsTemp != NULL) {
								externalStickmaps[externalStickmapsLen].stickmapArr = gtsTemp;
								externalStickmaps[externalStickmapsLen].stickmapArrLen = len;
							}
							break;
						// TODO: do better error detection in identifyJson()
						//  this is kinda the "default" case, since identifyJson() doesn't really output _TYPE_ERR
						//  unless the file outright isn't json
						case STICKMAP_TYPE_NORMAL:
							Stickmap *normalTemp = readJsonNormal(root, externalJsonFiles[i]);
							if (normalTemp != NULL) {
								externalStickmaps[externalStickmapsLen].stickmapArrLen = 1;
								externalStickmaps[externalStickmapsLen].stickmapArr = malloc(sizeof(Stickmap*));
								if (externalStickmaps[externalStickmapsLen].stickmapArr == NULL) {
									freeStickmap(normalTemp);
									continue;
								}
								externalStickmaps[externalStickmapsLen].stickmapArr[0] = normalTemp;
							}
							break;
						case STICKMAP_TYPE_ERR:
						default:
							break;
					}
					
					// only increment our index if we actually got data...
					if (externalStickmaps[externalStickmapsLen].stickmapArrLen > 0) {
						for (int j = 0; j < externalStickmaps[externalStickmapsLen].stickmapArrLen; j++) {
							genStickmapCoords(externalStickmaps[externalStickmapsLen].stickmapArr[j]);
						}
						externalStickmapsLen++;
					}
				}
			}
		}
		readFilesystemForJson = true;
	}
}

ExternalStickmap* getExternalJsonList(int *length) {
	// only return if the above has ran
	if (readFilesystemForJson) {
		*length = externalStickmapsLen;
		return externalStickmaps;
	}
	*length = -1;
	return NULL;
}
// var for counting how long the stick has been held away from neutral
static uint8_t stickheld = 0;
static int stickYPos = 0, stickYPrevPos = 0;

static int jsonPickerCursor = 0;
int drawJsonFilePicker(ExternalStickmap *list) {
	printStr("Choose a file:\n\n");
	if ((externalStickmaps == NULL || externalStickmapsLen == 0) && readFilesystemForJson) {
		printStr("No files found");
	} else {
		int totalPages = ceil(externalStickmapsLen / 16.0);
		
		// page logic
		int printIndexStart = (jsonPickerCursor / 16) * 16;
		int printIndexEnd = printIndexStart + 16;
		if (printIndexEnd > externalStickmapsLen) {
			printIndexEnd = externalStickmapsLen;
		}
		
		if (printIndexStart != 0) {
			setCursorPos(3, 10);
			printStr("<-- L");
			drawFontButton(FONT_L);
		}
		setCursorPos(3, 20);
		printStr("Page %3d/%3d", (jsonPickerCursor / 16) + 1, totalPages);
		if (printIndexEnd != externalStickmapsLen) {
			setCursorPos(3, 35);
			drawFontButton(FONT_R);
			printStr("R -->");
		}
		
		
		setCursorPos(4, 0);
		
		for (int i = printIndexStart; i < printIndexEnd; i++) {
			if (jsonPickerCursor == i) {
				printStr(" > %.*s", 45, list[i].fileName);
			} else {
				printStr("   %.*s", 45, list[i].fileName);
			}
			printStr("\n");
		}
		
		stickYPrevPos = stickYPos;
		stickYPos = PAD_StickY(0);
		
		// flags which tell whether the stick is held in an up or down position
		uint8_t up = stickYPos > 10;
		uint8_t down = stickYPos < -10;
		
		// only move the stick if it wasn't already held for the last 10 ticks
		uint8_t movable = stickheld % 10 == 0;
		
		uint16_t *pressed = getButtonsDownPtr();
		// does the user move the cursor?
		if (*pressed & PAD_BUTTON_UP || (up && movable)) {
			if (jsonPickerCursor > 0) {
				jsonPickerCursor--;
			} else {
				jsonPickerCursor = externalStickmapsLen - 1;
			}
		} else if (*pressed & PAD_BUTTON_DOWN || (down && movable)) {
			if (jsonPickerCursor < externalStickmapsLen - 1) {
				jsonPickerCursor++;
			} else {
				jsonPickerCursor = 0;
			}
		} else if (*pressed == PAD_TRIGGER_L) {
			if (printIndexStart != 0) {
				jsonPickerCursor -= 16;
			}
		} else if (*pressed == PAD_TRIGGER_R) {
			if (printIndexEnd != externalStickmapsLen) {
				jsonPickerCursor += 16;
				if (jsonPickerCursor > externalStickmapsLen) {
					jsonPickerCursor = externalStickmapsLen - 1;
				}
			}
		}

		else if (*pressed & PAD_BUTTON_A) {
			return jsonPickerCursor;
		}
		
		// increase or reset counter for how long stick has been held
		if (up || down) {
			stickheld++;
		} else {
			stickheld = 0;
		}
	}
	return -1;
}

void freeStickmap(Stickmap *target) {
	if (target != NULL) {
		// free coordinate array if applicable
		for (int i = 0; i < target->subcategoryListLen; i++) {
			free(target->subcategoryList[i].coordList);
		}
		// free other arrays
		free(target->subcategoryList);
		free(target->subcategoryDescList);
		
		free(target);
		
		target = NULL;
	}
}

void freeBuiltinJsonList() {
	if (coordViewStickmaps != NULL && plot2dStickmaps != NULL) {
		for (int i = 0; i < coordViewStickmapsLen; i++) {
			freeStickmap(coordViewStickmaps[i]);
		}
		for (int i = 0; i < plot2dStickmapsLen; i++) {
			freeStickmap(plot2dStickmaps[i]);
		}
	}
}

void freeExternalJsonList() {
	if (externalJsonNum != 0) {
		for (int i = 0; i < externalJsonNum; i++) {
			ExternalStickmap *ptr = &externalStickmaps[i];
			for (int j = 0; j < ptr->stickmapArrLen; j++) {
				freeStickmap(ptr->stickmapArr[j]);
			}
		}
		free(externalStickmaps);
		free(externalJsonFiles);
		externalStickmaps = NULL;
		externalJsonNum = 0;
		externalStickmapsLen = 0;
	}
}
