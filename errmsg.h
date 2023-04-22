/* errmsg.h
 * Copyright (C) 2023 Jad Altahan (https://github.com/xv)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef ERRMSG_H
#define ERRMSG_H

#include <windows.h>

/*!
 * @brief
 * Gets an error message based on the specified code.
 * 
 * @param code
 * The error code to retrieve its relevant error message.
 * 
 * @return
 * System-formatted string containing an error message.
 */
TCHAR *get_win32_error_msg(unsigned long code);

/*!
 * @brief
 * Gets an error message based on value of GetLastError().
 * 
 * @return
 * System-formatted string containing an error message.
 */
TCHAR *get_win32_last_error_msg(void);

#endif // ERRMSG_H