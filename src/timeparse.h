/* timeparse.h
 * Copyright (C) 2026 Jad Altahan (https://github.com/xv)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef TIMEPARSE_H
#define TIMEPARSE_H

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <tchar.h>

/*!
 * @brief
 * Represents a timestamp containing a SYSTEMTIME value and a flag indicating
 * whether the time is UTC.    
 */
typedef struct {
    SYSTEMTIME st;
    bool zulu;
} Timestamp;

 /*!
  * @brief
  * Translates a timestamp string to a Timestamp struct.
  *
  * @param stamp
  * Timestamp string in ISO 8601 basic (yyyyMMdd[THHmm[ss][.sss][Z]]) or
  * extended format (yyyy-MM-dd[THH:mm[:ss][.sss][Z]]).
  *
  * @param out
  * Pointer to a Timestamp struct that will receive the translated timestamp.
  *
  * @returns
  * true if the timestamp was successfully parsed and translated; false otherwise.
  */
bool parse_timestamp(const TCHAR *stamp, Timestamp *out);

#endif // TIMEPARSE_H