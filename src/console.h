/* console.h
 * Copyright (C) 2025 Jad Altahan (https://github.com/xv)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdio.h>
#include <stdbool.h>
#include <windows.h>
#include <tchar.h>

#define WIN32_LEAN_AND_MEAN

 /*!
  * @brief
  * Represents a console instance.
  */
typedef struct console {
    HANDLE handle;
    WORD attributes;
    bool is_tty;
} console_t;

/*!
 * @brief
 * Console colors that can be used as background and foreground
 */
typedef enum console_color {
    // https://ss64.com/nt/color.html
    CONSOLE_COLOR_NONE = -1,
    CONSOLE_COLOR_BLACK = 0x0,
    CONSOLE_COLOR_BLUE = 0x1,
    CONSOLE_COLOR_GREEN = 0x2,
    CONSOLE_COLOR_AQUA = 0x3,
    CONSOLE_COLOR_RED = 0x4,
    CONSOLE_COLOR_PURPLE = 0x5,
    CONSOLE_COLOR_YELLOW = 0x6,
    CONSOLE_COLOR_WHITE = 0x7,
    CONSOLE_COLOR_GRAY = 0x8,
    CONSOLE_COLOR_LIGHT_BLUE = 0x9,
    CONSOLE_COLOR_LIGHT_GREEN = 0xa,
    CONSOLE_COLOR_LIGHT_AQUA = 0xb,
    CONSOLE_COLOR_LIGHT_RED = 0xc,
    CONSOLE_COLOR_LIGHT_PURPLE = 0xd,
    CONSOLE_COLOR_LIGHT_YELLOW = 0xe,
    CONSOLE_COLOR_BRIGHT_WHITE = 0xf
} console_color_t;

/*!
 * @brief
 * Opens the console and retrieves default attributes.
 *
 * @return
 * Pointer to a new console_t instance, or NULL on allocation failure.
 */
console_t *console_open(void);

/*!
 * @brief
 * Restores original console attributes and frees the instance.
 *
 * @param console
 * Console instance to close.
 */
void console_close(console_t *console);

/*!
 * @brief
 * Sets the foreground and background colors of the console output.
 *
 * @param bg
 * The desired background color to set.
 * 
 * @param fg
 * The desired foreground color to set.
 */
void console_set_colors(console_t *console, console_color_t bg, console_color_t fg);

/*!
 * @brief
 * Restores the console's initial color attributes.
 *
 * @param console
 * Console instance.
 */
void console_reset_colors(console_t *console);

/*!
 * @brief
 * Prints formatted text with the specified background and foreground colors.
 *
 * @param console
 * Console instance.
 *
 * @param bg
 * The background color of the text.
 *
 * @param fg
 * The foreground color of the text.
 * 
 * @param stream
 * Output stream.
 * 
 * @param fmt
 * Format string.
 *
 * @return
 * Number of characters printed.
 */
int console_fprintf_color(console_t *console, console_color_t bg, console_color_t fg,
                          FILE *stream, _Printf_format_string_ const TCHAR *fmt, ...);

/*!
 * @brief
 * Prints a formatted error message in red foreground to stderr.
 *
 * @param console
 * Console instance.
 * 
 * @param fmt
 * Format string.
 *
 * @return
 * Number of characters printed.
 */
int console_printf_error(console_t *console, _Printf_format_string_ const TCHAR *fmt, ...);

#endif // CONSOLE_H