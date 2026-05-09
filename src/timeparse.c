/* timeparse.c
 * Copyright (C) 2026 Jad Altahan (https://github.com/xv)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include <stdbool.h>
#include <limits.h>

#include "timeparse.h"

 // Win32 epoch is January 1, 1601
#define SYSTIME_YEAR_MIN 1601
#define SYSTIME_YEAR_MAX 30827

#define ISO8601_DATE_BASIC_LEN 8  // yyyyMMdd
#define ISO8601_DATE_EXTEN_LEN 10 // yyyy-MM-dd

#define ISO8601_TIME_BASIC_LEN 6  // HHmmss
#define ISO8601_TIME_EXTEN_LEN 8  // HH:mm:ss

// yyyyMMddTHHmmss
#define ISO8601_TIMESTAMP_BASIC_LEN \
    (ISO8601_DATE_BASIC_LEN + 1 + ISO8601_TIME_BASIC_LEN)

// yyyy-MM-ddTHH:mm:ss
#define ISO8601_TIMESTAMP_EXTEN_LEN \
    (ISO8601_DATE_EXTEN_LEN + 1 + ISO8601_TIME_EXTEN_LEN)

/*!
 * @brief
 * Checks if the given year is a leap year.
 * 
 * @param year
 * The year to check.
 * 
 * @return
 * true if the year is a leap year; false otherwise.
 */
static bool is_leap_year(WORD year) {
    return (year % 400 == 0) || ((year % 4 == 0) && (year % 100 != 0));
}

/*!
 * @brief
 * Checks the number of days in a given month of a given year.
 * 
 * @param year
 * The year to check.
 * 
 * @param month
 * The month to check.
 * 
 * @return
 * The number of days.
 */
static WORD days_in_month(WORD year, WORD month) {
    static const WORD days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if (month == 2 && is_leap_year(year)) {
        return 29;
    }

    return days[month - 1];
}

/*!
 * @brief
 * Validates a SYSTEMTIME struct to ensure all fields are within valid ranges.
 * 
 * @param st
 * Pointer to the SYSTEMTIME struct to validate.
 * 
 * @return
 * true if all fields of the SYSTEMTIME struct are valid; false otherwise.
 */
static bool validate_systemtime(const SYSTEMTIME *st) {
    if (!st) {
        return false;
    }

    if (st->wYear < SYSTIME_YEAR_MIN ||
        st->wYear > SYSTIME_YEAR_MAX) {
        return false;
    }

    if (st->wMonth < 1 ||
        st->wMonth > 12) {
        return false;
    }

    WORD max_day = days_in_month(st->wYear, st->wMonth);

    if (st->wDay < 1 ||
        st->wDay > max_day) {
        return false;
    }

    // ISO 8601 allows 60 for leap seconds, but SYSTEMTIME does not!
    if (st->wHour > 23 ||
        st->wMinute > 59 ||
        st->wSecond > 59 || 
        st->wMilliseconds > 999) {
        return false;
    }

    return true;
}

/*!
 * @brief
 * Parses a string of digits into a WORD value.
 * 
 * @param str
 * Pointer to the string to parse.
 * 
 * @param n
 * Number of characters in the string to parse.
 * 
 * @param out
 * Pointer to a WORD variable that will receive the parsed value.
 * 
 * @return
 * true if parsing was successful; false otherwise.
 */
static bool parse_u16(const TCHAR *str, size_t n, WORD *out) {
    unsigned int v = 0;
    const size_t u16_max_digits = 5;

    if (n == 0 || n > u16_max_digits) {
        return false;
    }

    for (size_t i = 0; i < n; i++) {
        TCHAR c = str[i];

        if (c < '0' || c > '9') {
            return false;
        }

        v = (v * 10) + (unsigned)(c - '0');
    }

    if (v > USHRT_MAX) {
        return false;
    }

    *out = (WORD)v;
    return true;
}

bool parse_timestamp(const TCHAR *stamp, Timestamp *out) {
    if (!stamp || *stamp == '\0' || !out) {
        return false; 
    }

    SYSTEMTIME st = { 0 };

    bool extended = false;
    bool zulu = false;

    const TCHAR *ptr = stamp;
    size_t len = _tcslen(stamp);

    if (ptr[len - 1] == 'Z') {
        zulu = true;
        len--; // Subtract it now to simplify length checks later
    }

    if (len >= ISO8601_DATE_EXTEN_LEN && ptr[4] == '-' /* yyyy[-] */) {
        extended = true;
    }

    if (extended) {
        if (ptr[7] != '-' /* yyyy-MM[-] */) {
            return false;
        }

        if (!parse_u16(ptr + 0, 4, &st.wYear) ||
            !parse_u16(ptr + 5, 2, &st.wMonth) ||
            !parse_u16(ptr + 8, 2, &st.wDay)) {
            return false;
        }

        ptr += ISO8601_DATE_EXTEN_LEN;
        len -= ISO8601_DATE_EXTEN_LEN;
    } else {
        if (len < ISO8601_DATE_BASIC_LEN /* yyyyMMdd */) {
            return false;
        }

        if (!parse_u16(ptr + 0, 4, &st.wYear) ||
            !parse_u16(ptr + 4, 2, &st.wMonth) ||
            !parse_u16(ptr + 6, 2, &st.wDay)) {
            return false;
        }

        ptr += ISO8601_DATE_BASIC_LEN;
        len -= ISO8601_DATE_BASIC_LEN;
    }

    // Time component
    if (len > 0) {
        if (*ptr++ != 'T') {
            return false;
        }

        len--;

        if (extended) {
            const size_t min_time_len = ISO8601_TIME_EXTEN_LEN - 3; // HH:mm

            if (len < min_time_len || ptr[2] != ':' /* HH[:] */) {
                return false;
            }

            if (!parse_u16(ptr + 0, 2, &st.wHour) ||
                !parse_u16(ptr + 3, 2, &st.wMinute)) {
                return false;
            }

            ptr += min_time_len;
            len -= min_time_len;

            if (len > 0 /* Assume seconds remain */) {
                const size_t sec_len = 3; // :ss

                if (len < sec_len || ptr[0] != ':' /* [:]ss */) {
                    return false;
                }

                if (!parse_u16(ptr + 1, 2, &st.wSecond)) {
                    return false;
                }

                ptr += sec_len;
                len -= sec_len;
            }  
        } else {
            const size_t min_time_len = ISO8601_TIME_BASIC_LEN - 2; // HHmm

            if (len < min_time_len) {
                return false;
            }

            if (!parse_u16(ptr + 0, 2, &st.wHour) ||
                !parse_u16(ptr + 2, 2, &st.wMinute)) {
                return false;
            }

            ptr += min_time_len;
            len -= min_time_len;

            if (len > 0 /* Assume seconds remain */) {
                const size_t sec_len = 2; // ss

                if (len < sec_len) {
                    return false;
                }

                if (!parse_u16(ptr, 2, &st.wSecond)) {
                    return false;
                }

                ptr += sec_len;
                len -= sec_len;
            }
        }

        if (len > 0 /* Assume fractional seconds remain */) {
            const size_t ms_len = 4; // .sss

            if (len < ms_len || ptr[0] != '.') {
                return false;
            }

            if (!parse_u16(ptr + 1, 3, &st.wMilliseconds)) {
                return false;
            }

            ptr += ms_len;
            len -= ms_len;
        }
    } else if (zulu) {
        // Reject Zulu specifier since there's no time component
        return false;
    }

    // There should be no more characters at this point; otherwise assume format
    // is invalid since Zulu specifier was already handled earlier and UTC
    // offsets are not supported
    if (len != 0) {
        return false;
    }

    if (!validate_systemtime(&st)) {
        return false;
    }

    out->st = st;
    out->zulu = zulu;

    return true;
}