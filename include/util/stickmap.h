//
// Created on 3/9/26.
//

// stickmap json parsing
// decodes individual coordinates from json
// format expected is altimor stickmap

#ifndef GTS_STICKMAP_H
#define GTS_STICKMAP_H

#include "waveform.h"

#include <stdint.h>
#include <jansson.h>

#include "util/gx.h"

enum TEX_GEN_ASYNC_STATE { TEX_ASYNC_INIT, TEX_ASYNC_GEN, TEX_ASYNC_DONE };
enum TEX_GEN_ASYNC_STATE isExternalStickmapReady();

enum STICKMAP_WHICH_STICK { STICKMAP_NOT_SPECIFIED, STICKMAP_A_STICK, STICKMAP_C_STICK };

// OK SO THIS IS DUMB BUT I HOPE IT WORKS
// i'm doing a "bad thing" here, where the coordinate buffer can be in either:
// stickmap or ExternalStickmap
// the idea is that with external files, i can allocate one block of memory, then subdivide it to each stickmap
// each subcategory will get a pointer to the start of its buffer
// done this way so that I don't risk fragmentation when loading a new external file, since i only have to
// free a single buffer. i still have to traverse the structure and unset pointers tho...

typedef struct StickmapSubcategory {
	// info from json itself
	const char *name;
	GXColor color;
	bool quadrants[4];
	int displayMode;
	int minX;
	int minY;
	int maxX;
	int maxY;
	int angleMin;
	int angleMax;
	int magnitudeMin;
	int magnitudeMax;
	
	// info derived from above
	int numOfCoords;
	int8_t (*coordList)[2];
} StickmapSubcategory;

typedef struct StickmapSubcategoryDesc {
	const char *name;
	const char *desc;
	int listIndexStart;
} StickmapSubcategoryDesc;

typedef struct Stickmap {
	const char *name;
	const char *desc;
	enum STICKMAP_WHICH_STICK whichStick;
	int subcategoryListLen;
	StickmapSubcategory *subcategoryList;
	// optional data
	int subcategoryDescListLen;
	TextureStruct texture;
	int totalCoords;
	void *stickmapBufferStart;
	StickmapSubcategoryDesc *subcategoryDescList;
} Stickmap;

void generateStickmapTextureAsync(Stickmap **stickmapList, int len);

enum STICKMAP_JSON_TYPE { STICKMAP_TYPE_ERR, STICKMAP_TYPE_NORMAL, STICKMAP_TYPE_GTS };

// returns json object, and what type it is
enum STICKMAP_JSON_TYPE identifyJson(const char jsonFile[], json_t **root);

// one of these are called, depending on return type of above
Stickmap *readJsonNormal(json_t *root, const char *stickmapName);
Stickmap **readJsonGTS(json_t *root, int *len);

// identify what subcategory a given coordinate pair belongs to, if at all
int getCoordSubcategory(MeleeCoordinates coord, enum STICKMAP_WHICH_STICK whichStick, Stickmap *stickmap);

void loadBuiltinStickmaps();

enum STICKMAP_BUILTIN_LIST { STICKMAP_PLOT2D, STICKMAP_COORDVIEW };
Stickmap **getBuiltinStickmap(enum STICKMAP_BUILTIN_LIST list, int *len);

// iterable enum for use in menus
typedef struct ExternalStickmap {
	char *fileName;
	enum STICKMAP_JSON_TYPE stickmapType;
	bool generatedTextures;
	void *coordBuffer;
	int coordBufferSize;
	// 'array' of stickmaps
	// done this way to simplify things a bit
	// basically if a json is a 'normal' json, it will be treated
	// as a single Stickmap, so we just make this a pointer to that pointer
	// instead of an array
	int stickmapArrLen;
	Stickmap **stickmapArr;
} ExternalStickmap;

// get the list of .json files in /gts/stickmaps/ on the first mounted storage
// uses getFilesystemJson() in file.h
void loadExternalJsonList();
// return pointer to what was loaded above
ExternalStickmap* getExternalJsonList(int *length);

// draw json picker
int drawJsonFilePicker(ExternalStickmap *list);

void freeStickmap(Stickmap *target);
void freeBuiltinJsonList();
void freeExternalJsonList();

#endif //GTS_STICKMAP_H
