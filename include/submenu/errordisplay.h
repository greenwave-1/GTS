//
// Created on 4/13/26.
//

#ifndef GTS_ERRORDISPLAY_H
#define GTS_ERRORDISPLAY_H

#include "menu.h"

#include <stdarg.h>

void menu_errorDisplay();

void menu_errorDisplaySetError(char *str, va_list list);

#endif //GTS_ERRORDISPLAY_H
