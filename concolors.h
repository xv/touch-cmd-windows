/* concolors.h
 * Copyright (C) 2020 Jad Altahan (https://github.com/xv)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef CONCOLORS_H
#define CONCOLORS_H

#include <windows.h>

#define WIN32_LEAN_AND_MEAN

typedef struct console_screen {
    HANDLE std_console;
    CONSOLE_SCREEN_BUFFER_INFO buff_info;
} console_screen_t;

typedef enum _console_color {
    // https://ss64.com/nt/color.html
    COLOR_NONE = -1,
    COLOR_BLACK = 0x0,
    COLOR_BLUE = 0x1,
    COLOR_GREEN = 0x2,
    COLOR_AQUA = 0x3,
    COLOR_RED = 0x4,
    COLOR_PURPLE = 0x5,
    COLOR_YELLOW = 0x6,
    COLOR_WHITE = 0x7,
    COLOR_GRAY = 0x8,
    COLOR_LIGHT_BLUE = 0x9,
    COLOR_LIGHT_GREEN = 0xa,
    COLOR_LIGHT_AQUA = 0xb,
    COLOR_LIGHT_RED = 0xc,
    COLOR_LIGHT_PURPLE = 0xd,
    COLOR_LGIHT_YELLOW = 0xe,
    COLOR_BRIGHT_WHITE = 0xf
} console_color_t;

console_screen_t *console;

/**
 * @brief
 * Initializes the console screen buffer.
 */
void console_init(console_screen_t **ci);

/*!
 * @brief
 * Changes the foreground and background colors of the console output.
 *
 * @param fg
 * The desired foreground color.
 * 
 * @param bg
 * The desired background color.
 */
void console_set_colors(console_color_t fg, console_color_t bg);

/**
 * @brief
 * Resets the foreground and background colors of the console output to
 * their default system values.
 */
void console_reset_colors(void);

#endif // CONCOLORS_H