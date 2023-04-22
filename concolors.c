/* concolors.c
 * Copyright (C) 2020 Jad Altahan (https://github.com/xv)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "concolors.h"

void console_init(console_screen_t **ci)
{
    *ci = (console_screen_t *)calloc(1, sizeof(console_screen_t));
    if (*ci == NULL) {
        return;
    }

    (*ci)->std_console = GetStdHandle(STD_OUTPUT_HANDLE);
    if ((*ci)->std_console != NULL) {
        if (!GetConsoleScreenBufferInfo((*ci)->std_console, &(*ci)->buff_info)) {
            (*ci)->std_console = NULL;
        }
    }
}

void console_set_colors(console_color_t fg, console_color_t bg) {
    if (bg == COLOR_NONE && fg == COLOR_NONE) {
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (console->std_console == NULL ||
        !GetConsoleScreenBufferInfo(console->std_console, &csbi)) {
        return;
    }

    WORD color = csbi.wAttributes;

    if (fg != COLOR_NONE) {
        color = (color & 0xFFF0) | fg;
    }

    if (bg != COLOR_NONE) {
        color = (color & 0xFF0F) | (bg << 4);
    }

    SetConsoleTextAttribute(console->std_console, color);
}

void console_reset_colors(void) {
    if (console == NULL) {
        return;
    }

    if (console->std_console != NULL) {
        SetConsoleTextAttribute(
            console->std_console, 
            console->buff_info.wAttributes
        );
    }
}