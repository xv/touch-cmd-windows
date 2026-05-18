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

#define ISO8601_DATE_EXTEN_LEN 10 // yyyy-MM-dd

/*!
 * @brief
 * Specifies the format style for parsing timestamps.
 */
typedef enum format_style
{
    FORMAT_STYLE_BASIC,
    FORMAT_STYLE_EXTENDED,
} FormatStyle;

/*!
 * @brief
 * Cursor-like context for parsing timestamps.
 */
typedef struct parse_context {
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

/*!
 * @brief
 * Checks whether a character is a timezone designator ('Z', '+' or '-').
 * 
 * @param ch
 * The character to check.
 * 
 * @return
 * true if \p ch is a timezone designator; false otherwise.
 */
static inline bool is_tz_start(TCHAR ch) {
    return ch == 'Z' || ch == '+' || ch == '-';
}

/*!
 * @brief
 * Checks whether the current character in the parse context equals the
 * specified character.
 * 
 * @param ctx
 * Pointer to the current parse context.
 * 
 * @param ch
 * Character to match.
 *
 * @return
 * true if the current character equals \p ch; otherwise false.
 */
static bool peek_char(const ParseContext *ctx, TCHAR ch) {
    return (ctx->len > 0) && (*ctx->ptr == ch);
}

/*!
 * @brief
 * Consumes the current character in the parse context if it equals the
 * specified character.
 * 
 * @param ctx
 * Pointer to the current parse context.
 * 
 * @param ch
 * The character to match and consume.
 * 
 * @return
 * true if the character matched and was consumed; false otherwise.
 */
static bool consume_char(ParseContext *ctx, TCHAR ch) {
    if (!peek_char(ctx, ch)) {
        return false;
    }

    ctx->ptr++;
    ctx->len--;

    return true;
}

/*!
 * @brief
 * Parses and consumes a 16-bit unsigned value represented by the specified
 * number of digits from the parse context.
 * 
 * @param ctx
 * Pointer to the current parse context.
 * 
 * @param digits
 * Number of characters/digits to parse and consume.
 * 
 * @param out
 * Pointer to a WORD where the parsed 16-bit value is written on success; not
 * modified if parsing fails.
 * 
 * @return
 * true if the digits in the parse context were successfully parsed and
 * consumed; false otherwise.
 */
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

/*!
 * @brief
 * Parses a date from the parse context and, if present and valid, writes the
 * year, month and day to \p st.
 * 
 * @param ctx
 * Pointer to the current parse context.
 * 
 * @param st
 * Pointer to a SYSTEMTIME structure that will receive the parsed date.
 * 
 * @return
 * true if the date was successfully parsed and stored; false otherwise.
 */
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

/*!
 * @brief
 * Parses time from the parse context and, if present and valid, writes the
 * hour, minute, second, and millisecond to \p st.
 *
 * @param ctx
 * Pointer to the current parse context.
 *
 * @param st
 * Pointer to a SYSTEMTIME structure that will receive the parsed time.
 *
 * @return
 * true if the time was successfully parsed and stored; false otherwise.
 */
static bool parse_time(ParseContext *ctx, SYSTEMTIME *st) {
    if (!consume_u16(ctx, 2, &st->wHour)) {
        return false;
    }

    bool extended = ctx->fmt_style == FORMAT_STYLE_EXTENDED;

    // Reduced precision; HH only
    //
    // ISO 8601-1:2019§4.2.2.3 forbids hours-only expressions in extended format
    // 
    // ISO 8601-1:2019§5.3.3,5.3.4.1 forbid UTC designator "Z" and offsets in
    // extended format with hours-only expressions
    if (ctx->len == 0 || is_tz_start(*ctx->ptr)) {
        return !extended;
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

/*!
 * @brief
 * Parses a UTC designator or offset from the parse context and, if present and
 * valid, writes it to \p out.
 * 
 * @param ctx
 * Pointer to the current parse context.
 * 
 * @param out
 * Pointer to a UtcOffset struct that will receive the parsed offset.
 * 
 * @return
 * true if the UTC designator or offset is empty or a valid offset was
 * successfully parsed and stored; false on parse or validation error.
 */
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

    bool extended = ctx->fmt_style == FORMAT_STYLE_EXTENDED;

    // ISO 8601-1:2019§5.3.4.1 forbids hours-only offsets in extended format
    // but allows them in basic format
    if (extended && !consume_char(ctx, ':')) {
        return false;
    }

    if (ctx->len > 0 && !consume_u16(ctx, 2, &minutes)) {
        return false;
    }

    if (hours > 23 || minutes > 59) {
        return false;
    }

    // ISO 8601-1:2019§3.2.4 states: a plus sign ["+"] to represent a positive
    // value or zero (the plus sign shall not be omitted), or a minus sign ["-"]
    // otherwise.
    //
    // Wikipedia for ISO 8601 states: It is not permitted to state a zero value
    // time offset with a negative sign, as "−00:00", "−0000", or "−00".
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

    // Optional time component
    if (ctx.len != 0) {
        // ISO 8601-1:2019§5.3.2: "T" is always present in basic format, but for
        // extended format, it may be omitted in time-only expressions
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

bool parse_hhmmss(const TCHAR *hhmmss, int *out) {
    if (!hhmmss || *hhmmss == '\0' || !out) {
        return false;
    }

    ParseContext ctx = {
        .ptr = hhmmss,
        .len = _tcslen(hhmmss)
    };

    bool negative = consume_char(&ctx, '-');

    if (ctx.len != 2 && // SS
        ctx.len != 4 && // MMSS
        ctx.len != 6) { // HHMMSS
        return false;
    }
    
    WORD hh, mm, ss;
    hh = mm = ss = 0;

    switch (ctx.len) {
        case 2:
            if (!consume_u16(&ctx, 2, &ss)) {
                return false;
            }
            break;
        case 4:
            if (!consume_u16(&ctx, 2, &mm) ||
                !consume_u16(&ctx, 2, &ss)) {
                return false;
            }
            break;
        case 6:
            if (!consume_u16(&ctx, 2, &hh) ||
                !consume_u16(&ctx, 2, &mm) ||
                !consume_u16(&ctx, 2, &ss)) {
                return false;
            }
            break;
        default:
            return false;
    }

    if (mm > 59 || ss > 59) {
        return false;
    }

    int total = (hh * 3600) + (mm * 60) + ss;

    if (negative) {
        total = -total;
    }

    *out = total;
    return true;
}