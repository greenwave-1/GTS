//
// Created on 4/13/26.
//

#include "submenu/errordisplay.h"

#ifdef DEBUGGDB
#include <debug.h>
#endif

#include "util/print.h"

static enum CURRENT_MENU errorSource = ERR;
static char *menuStr = NULL;
static char errorStr[1024] = "No error string provided";

void menu_errorDisplay() {
	setCursorPos(3, 0);
	printStr("Error encountered in menu: ");
	if (menuStr != NULL) {
		printStr("\"%s\"", menuStr);
	} else {
		printStr("(ID %d)", errorSource);
	}
	setCursorPos(5, 0);
	printStr("Provided error string:\n");
	printStr(errorStr);
}

void menu_errorDisplaySetError(char *str, va_list list) {
	#ifdef DEBUGGDB
	_break();
	#endif
	errorSource = menu_getCurrentMenu();
	vsnprintf(errorStr, 1023, str, list);
	menu_setCurrentMenu(ERR);
}
