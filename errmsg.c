/* errmsg.c
 * Copyright (C) 2023 Jad Altahan (https://github.com/xv)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "errmsg.h"

TCHAR *get_win32_error_msg(unsigned long code) {
    TCHAR *msg = NULL;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&msg,
        0,
        NULL
    );

    return msg;
}

TCHAR *get_win32_last_error_msg(void) {
    DWORD code = GetLastError();
    return get_win32_error_msg(code);
}