/* console.c
 * Copyright (C) 2025 Jad Altahan (https://github.com/xv)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "console.h"

#define FG_MASK (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define BG_MASK (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY)

console_t *console_open(void) {
    console_t *console = calloc(1, sizeof(console_t));
    if (!console) {
        return NULL;
    }

    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!h || h == INVALID_HANDLE_VALUE) {
        return console;
    }

    console->handle = h;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(h, &csbi)) {
        return console;
    }

    console->is_tty = true;
    console->attributes = csbi.wAttributes;

    return console;
}

void console_close(console_t *console) {
    if (!console) {
        return;
    }

    if (console->is_tty) {
        SetConsoleTextAttribute(console->handle, console->attributes);
    }

    free(console);
}

void console_set_colors(console_t *console, console_color_t bg, console_color_t fg) {
    if (!console || !console->is_tty) {
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(console->handle, &csbi)) {
        return;
    }

    WORD attrs = csbi.wAttributes;

    if (bg != CONSOLE_COLOR_NONE) {
        attrs = (attrs & ~BG_MASK) | (bg << 4);
    }

    if (fg != CONSOLE_COLOR_NONE) {
        attrs = (attrs & ~FG_MASK) | fg;
    }

    SetConsoleTextAttribute(console->handle, attrs);
}

void console_reset_colors(console_t *console) {
    if (!console || !console->is_tty) {
        return;
    }

    SetConsoleTextAttribute(console->handle, console->attributes);
}

static int console_vfprintf_color(console_t *console, console_color_t bg, console_color_t fg,
                                  FILE *stream, _Printf_format_string_ const TCHAR *fmt, va_list args) {

    if (!console || !console->is_tty) {
        return _vftprintf(stream, fmt, args);
    }

    // If the format string ends with a new line and a background color is used,
    // it's going to leak onto the next line once the console begins scrolling
    // 
    // It's just how the console buffer appears to work and the only fix is to
    // trim the new line, reset the color, then add it back
    size_t len = _tcslen(fmt);
    bool ends_with_newline = (len > 0) && (fmt[len - 1] == '\n');

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(console->handle, &csbi)) {
        // Fallback
        csbi.wAttributes = console->attributes;
    }

    WORD prev_attr = csbi.wAttributes;

    if (bg != CONSOLE_COLOR_NONE || fg != CONSOLE_COLOR_NONE) {
        console_set_colors(console, bg, fg);
    }

    int ret;

    if (!ends_with_newline || bg == CONSOLE_COLOR_NONE) {
        ret = _vftprintf(stream, fmt, args);
        SetConsoleTextAttribute(console->handle, prev_attr);
    } else {
        TCHAR *tmp = _tcsdup(fmt);
        tmp[len - 1] = '\0';

        ret = _vftprintf(stream, tmp, args);

        free(tmp);
        SetConsoleTextAttribute(console->handle, prev_attr);

        if (_puttc('\n', stream) != _TEOF) {
            ret++;
        }
    }

    return ret;
}

int console_fprintf_color(console_t *console, console_color_t bg, console_color_t fg,
                          FILE *stream, _Printf_format_string_ const TCHAR *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int ret = console_vfprintf_color(
        console, bg, fg, stream, fmt, args);

    va_end(args);
    return ret;
}

int console_printf_error(console_t *console, _Printf_format_string_ const TCHAR *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int ret = console_vfprintf_color(
        console,
        CONSOLE_COLOR_NONE, CONSOLE_COLOR_RED,
        stderr, fmt, args);

    va_end(args);
    return ret;
}