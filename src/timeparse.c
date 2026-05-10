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
 * Specifies the format style for parsing timestamps.
 */
typedef enum
{
    FORMAT_STYLE_BASIC,
    FORMAT_STYLE_EXTENDED,
} FormatStyle;

/*!
 * @brief
 * Cursor-like context for parsing timestamps.
 */
typedef struct {
    // Pointer to the current position in the input string being parsed
    const TCHAR *ptr;
    // Number of characters remaining in the input string from the current position
    size_t len;
    // Indicates the format style being parsed
    FormatStyle fmt_style;
} ParseContext;

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
    // Appendix C https://www.rfc-editor.org/rfc/rfc3339
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

static inline bool is_tz_start(TCHAR ch) {
    return ch == 'Z' || ch == '+' || ch == '-';
}

static bool peek_char(const ParseContext *ctx, TCHAR ch) {
    return (ctx->len > 0) && (*ctx->ptr == ch);
}

static bool consume_char(ParseContext *ctx, TCHAR ch) {
    if (!peek_char(ctx, ch)) {
        return false;
    }

    ctx->ptr++;
    ctx->len--;

    return true;
}

static bool consume_u16(ParseContext *ctx, size_t digits, WORD *out) {
    if (ctx->len < digits) {
        return false;
    }

    if (!parse_u16(ctx->ptr, digits, out)) {
        return false;
    }

    ctx->ptr += digits;
    ctx->len -= digits;

    return true;
}

static bool parse_date(ParseContext *ctx, SYSTEMTIME *st) {
    if (!consume_u16(ctx, 4, &st->wYear)) {
        return false;
    }

    bool extended = ctx->fmt_style == FORMAT_STYLE_EXTENDED;

    if (extended && !consume_char(ctx, '-')) {
        return false;
    }

    if (!consume_u16(ctx, 2, &st->wMonth)) {
        return false;
    }

    if (extended && !consume_char(ctx, '-')) {
        return false;
    }

    if (!consume_u16(ctx, 2, &st->wDay)) {
        return false;
    }

    return true;
}

static bool parse_time(ParseContext *ctx, SYSTEMTIME *st) {
    if (!consume_u16(ctx, 2, &st->wHour)) {
        return false;
    }

    bool extended = ctx->fmt_style == FORMAT_STYLE_EXTENDED;

    // Reduced precision; HH only
    // 
    // Note: ISO 8601-1:2019§5.3.3 states that reduced precision is not allowed
    //       when a time zone designator "Z" is present. But I'm going to allow
    //       it here since this parser is meant to be based on ISO 8601's format
    //       but not necessarily as strictly conforming
    if (ctx->len == 0 || is_tz_start(*ctx->ptr)) {
        return true;
    }

    if (extended && !consume_char(ctx, ':')) {
        return false;
    }

    if (!consume_u16(ctx, 2, &st->wMinute)) {
        return false;
    }

    // Reduced precision; HH:mm only
    if (ctx->len == 0 || is_tz_start(*ctx->ptr)) {
        return true;
    }

    if (extended && !consume_char(ctx, ':')) {
        return false;
    }

    if (!consume_u16(ctx, 2, &st->wSecond)) {
        return false;
    }

    if (consume_char(ctx, '.') &&
       !consume_u16(ctx, 3, &st->wMilliseconds)) {
        return false;
    }

    return true;
}

static bool parse_utc_offset(ParseContext *ctx, UtcOffset *out) {
    if (ctx->len == 0) {
        return true;
    }

    if (consume_char(ctx, 'Z')) {
        out->specified = true;
        out->minutes = 0;

        return ctx->len == 0;
    }

    int sign;

    if (consume_char(ctx, '+')) {
        sign = 1;
    } else if (consume_char(ctx, '-')) {
        sign = -1;
    } else {
        // No timezone specified
        return ctx->len == 0;
    }

    WORD hours;
    WORD minutes = 0;

    if (!consume_u16(ctx, 2, &hours)) {
        return false;
    }

    bool extended =
        ctx->fmt_style == FORMAT_STYLE_EXTENDED &&
        consume_char(ctx, ':');

    // Minutes optional
    if (ctx->len > 0) {
        if (!consume_u16(ctx, 2, &minutes)) {
            return false;
        }
    }

    if (extended && minutes == 0 && ctx->len != 0) {
        return false;
    }

    if (hours > 23 || minutes > 59) {
        return false;
    }

    // ISO 8601:2004§3.4.2 states: [±] represents a plus sign [+] if in
    // combination with the following element a positive value or zero needs to
    // be represented (in this case, unless explicitly stated otherwise, the
    // plus sign shall not be omitted), or a minus sign [−] if in combination
    // with the following element a negative value needs to be represented.
    //
    // ISO 8601-1:2019§3.2.4 states: a plus sign ["+"] to represent a positive
    // value or zero (the plus sign shall not be omitted), or a minus sign ["-"]
    // otherwise.
    //
    // Wikipedia for ISO 8601 states: It is not permitted to state a zero value
    // time offset with a negative sign, as "−00:00", "−0000", or "−00".
    //
    // RFC 3339§4.3 states: If the time in UTC is known, but the offset to local
    // time is unknown, this can be represented with an offset of "-00:00". This
    // differs semantically from an offset of "Z" or "+00:00", which imply that
    // UTC is the preferred reference point for the specified time.
    //
    // I'm going to reject it here since it doesn't make much sense to have a
    // negative offset of zero
    if (sign < 0 && hours == 0 && minutes == 0) {
        return false;
    }

    if (ctx->len != 0) {
        return false;
    }

    out->specified = true;
    out->minutes = sign * ((int)hours * 60 + (int)minutes);

    return true;
}

bool parse_timestamp(const TCHAR *stamp, Timestamp *out) {
    if (!stamp || *stamp == '\0' || !out) {
        return false; 
    }

    SYSTEMTIME st = { 0 };

    UtcOffset utc_offset = { .specified = false, .minutes = 0 };

    ParseContext ctx = {
        .ptr = stamp,
        .len = _tcslen(stamp)
    };

    bool extended =
        ctx.len >= ISO8601_DATE_EXTEN_LEN &&
        stamp[4] == '-' &&
        stamp[7] == '-';

    ctx.fmt_style = extended ? FORMAT_STYLE_EXTENDED : FORMAT_STYLE_BASIC;

    if (!parse_date(&ctx, &st)) {
        return false;
    }

    // Time component
    if (ctx.len != 0) {
        if (!consume_char(&ctx, 'T')) {
            return false;
        }

        if (!parse_time(&ctx, &st)) {
            return false;   
        }

        if (!parse_utc_offset(&ctx, &utc_offset)) {
            return false;
        }
    }

    if (!validate_systemtime(&st)) {
        return false;
    }

    *out = (Timestamp) {
        .st = st,
        .utc_offset = utc_offset
    };

    return true;
}