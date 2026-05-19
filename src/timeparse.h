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

typedef struct utc_offset {
    bool specified;
    int minutes;
} UtcOffset;

/*!
 * @brief
 * Represents a timestamp containing a SYSTEMTIME value and a UTC offset.
 */
typedef struct timestamp {
    SYSTEMTIME st;
    UtcOffset utc_offset;
} Timestamp;

 /*!
  * @brief
  * Translates a timestamp string to a Timestamp struct.
  *
  * @param stamp
  * Timestamp string in ISO 8601 calendar date or week date format.
  * 
  * \par
  * Calendar date formats:
  * - Basic:    YYYYMMDD[Thh[mm[ss[.SSS]]][Z|±hh[mm]]]
  * - Extended: YYYY-MM-DD[Thh:mm[:ss[.SSS]][Z|±hh[:mm]]]
  * 
  * \par
  * Ordinal date formats:
  * - Basic:    YYYYDDD[Thh[mm[ss[.SSS]]][Z|±hh[mm]]]
  * - Extended: YYYY-DDD[Thh:mm[:ss[.SSS]][Z|±hh[:mm]]]
  * 
  * \par
  * Week date formats:
  * - Basic:    YYYYWwwD[Thh[mm[ss[.SSS]]][Z|±hh[mm]]]
  * - Extended: YYYY-Www-D[Thh:mm[:ss[.SSS]][Z|±hh[:mm]]]
  * 
  * @param out
  * Pointer to a Timestamp struct that will receive the translated timestamp.
  *
  * @returns
  * true if the timestamp was successfully parsed and translated; false otherwise.
  */
bool parse_timestamp(const TCHAR *stamp, Timestamp *out);

/*!
 * @brief
 * Parses a time string representing hours, minutes and seconds.
 * 
 * @param hhmmss
 * String in the format [-][[hh]mm]ss.
 * 
 * @param out
 * Pointer to a signed integer to receives the parsed value in number of seconds.
 * 
 * @return
 * true if parsing succeeds; false otherwise.
 */
bool parse_hhmmss(const TCHAR *hhmmss, int *out);

#endif // TIMEPARSE_H