/* main.c
 * Copyright (C) 2023 Jad Altahan (https://github.com/xv)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#define WIN32_LEAN_AND_MEAN

#include "errmsg.h"
#include "getopt.h"
#include "console.h"
#include "version.h"
#include "timeparse.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <windows.h>
#include <limits.h>
#include <tchar.h>
#include <assert.h>

#if defined(_M_ARM64)
#define BUILD_PLAT "arm64"
#elif defined(_WIN64)
#define BUILD_PLAT "x64"
#elif (defined(_WIN32) && !defined(_WIN64))
#define BUILD_PLAT "x86"
#else
#error "Unsupported architecture"
#endif

#define PROGRAM_USAGE_SUMMARY \
"SYNTAX\n\
    touch [OPTION]... FILE...\n\n\
DESCRIPTION\n\
    Updates the access and modification timestamps of each file specified by the\n\
    FILE argument to the current time of day.\n\n\
    The -a and -m flags are set by default.\n\n\
OPTIONS\n\
    -C          Change only the creation timestamp. The last access and last\n\
                write timestamps are not affected unless -a and -m flags are\n\
                also set.\n\n\
    -a          Change only the last access timestamp. The creation and last\n\
                access timestamps are not affected unless -C and -m flags are\n\
                also set.\n\n\
    -m          Change only the last write timestamp. The creation and last\n\
                write timestamps are not affected unless -C and -a flags are\n\
                also set.\n\n\
    -c          Do not create FILE if it does not exist.\n\n\
    -d          Do not dereference symbolic links. If FILE is a symbolic link,\n\
                its timestamp will be changed rather than that of the file it\n\
                refers to.\n\n\
    -A OFFSET   Adjust the timestamps of FILE by OFFSET, which must be in the\n\
                format \"[-][[hh]mm]ss\". The parts of the argument represent the\n\
                following:\n\n\
                    -    Make the adjustment negative, moving time backward.\n\
                    hh   Hours (00-99).\n\
                    mm   Minutes (00-59).\n\
                    ss   Seconds (00-59).\n\n\
                If FILE does not exist, it will be created with adjusted \n\
                timestamps, unless the -c option is specified.\n\n\
    -r REFFILE  Use the timestamp of the file specified by the REFFILE argument\n\
                instead of the current time of day. This option cannot be\n\
                combined with -t.\n\n\
                If -A is specified, the adjustment will be applied to the\n\
                referenced timestamp.\n\n\
    -t STAMP    Use the timestamp specified by the STAMP argument, which must be\n\
                in one of the following ISO 8601 basic or extended formats:\n\n\
                    Calendar date format\n\
                        YYYYMMDD[Thh[mm[ss[.SSS]]][Z|±hh[mm]]]\n\
                        YYYY-MM-DD[Thh:mm[:ss[.SSS]][Z|±hh[:mm]]]\n\n\
                    Ordinal date format\n\
                        YYYYDDD[Thh[mm[ss[.SSS]]][Z|±hh[mm]]]\n\
                        YYYY-DDD[Thh:mm[:ss[.SSS]][Z|±hh[:mm]]]\n\n\
                    Week date format\n\
                        YYYYWwwD[Thh[mm[ss[.SSS]]][Z|±hh[mm]]]\n\
                        YYYY-Www-D[Thh:mm[:ss[.SSS]][Z|±hh[:mm]]]\n\n\
                The parts of the argument represent the following:\n\n\
                    YYYY    Year (1601-9999).\n\
                    MM      Month (01 to 12).\n\
                    D       Day of week (1-7, 1=Monday, 7=Sunday).\n\
                    DD      Day (01-31).\n\
                    DDD     Day of year (001-366).\n\
                    W       Week designator.\n\
                    ww      Week of year (01-53).\n\
                    T       Time designator.\n\
                    hh      Hour of day (00-23).\n\
                    mm      Minute of hour (00-59).\n\
                    ss      Second of minute (00-59).\n\
                    SSS     Millisecond of second (000-999).\n\
                    Z       UTC designator.\n\
                    ±       Plus or minus sign preceding UTC offset.\n\n\
                Date components are validated against calendar rules for the\n\
                specified year. This means \"DD\" must be valid for the given\n\
                month and year (e.g., Feb. 29 is only accepted in leap years);\n\
                \"DDD\" must not exceed 365 in non-leap years; and \"ww\" must not\n\
                exceed the number of ISO weeks in the specified year.\n\n\
                If the time is omitted, midnight local time is assumed.\n\n\
    -h          Display this help information and exit.\n\n\
    -v          Display version information and exit.\n\n\
Source code:\n\
https://github.com/xv/touch-cmd-windows"

typedef enum timestamp_zone {
    TS_ZONE_LOCAL,
    TS_ZONE_UTC
} TimestampZone;

typedef enum timestamp_source {
    TS_SOURCE_NOW,
    TS_SOURCE_EXPLICIT,
    TS_SOURCE_RELATIVE
} TimestampSource;

typedef enum file_time_flags
{
    FT_CREATION = 1 << 0,
    FT_ACCESS = 1 << 1,
    FT_WRITE = 1 << 2
} FileTimeFlags;

typedef struct reference_timestamps {
    FILETIME creation;
    FILETIME access;
    FILETIME write;
} ReferenceTimestamps;

typedef struct timestamp_operation {
    TimestampSource source;
    FileTimeFlags ft_flags;
    FILETIME creation;
    FILETIME access;
    FILETIME write;
    int adjustment_seconds;
} TimestampOperation;

// Specifies that a file's previous last access or write times should be preserved
// when operating with file handles
// https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-filetime
// https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfiletime
const FILETIME ft_preserved = {
    .dwLowDateTime = 0xFFFFFFFF,
    .dwHighDateTime = 0xFFFFFFFF
};

static const TCHAR *prog_name;
static Console *console;

/*!
 * @brief
 * Prints program usage information.
 */
static void print_usage_info(void) {
    _putts(_T(PROGRAM_USAGE_SUMMARY));
}

/*!
 * @brief
 * Prints program version information.
 */
static void print_version_info(void) {
    _tprintf(_T("touch %s (%s)\n"), _T(VERSION_STR), _T(BUILD_PLAT));
}

/*!
 * @brief
 * Extracts the file name from a full path.
 *
 * @param path
 * Path to the file to process.
 *
 * @param rem_ext
 * If 1, the last occurrence of the file extension will be removed.
 *
 * @return
 * File name without a path.
 */
static const TCHAR *get_name(const TCHAR *path) {
    const TCHAR *name = _tcsrchr(path, '\\');
    return name ? (name + 1) : path;
}

/*!
 * @brief
 * Adjusts the given file time by the given offset. If the offset is negative,
 * time is moved backward. Otherwise, forward.
 *
 * @param ft
 * Pointer to a FILETIME structure containing the time to adjust.
 *
 * @param offset
 * Time represented in seconds.
 */
static void adjust_time_offset(FILETIME *ft, int offset) {
    assert(ft);

    ULARGE_INTEGER uli = { 0 };
    uli.LowPart = ft->dwLowDateTime;
    uli.HighPart = ft->dwHighDateTime;

    LONGLONG ticks = (LONGLONG)uli.QuadPart;
    LONGLONG delta = (LONGLONG)offset * 10000000LL;

    if ((delta > 0 && ticks > (MAXLONGLONG - delta)) ||
        (delta < 0 && ticks < -delta)) {
        return;
    }

    ticks += delta;

    uli.QuadPart = (ULONGLONG)ticks;

    ft->dwLowDateTime = uli.LowPart;
    ft->dwHighDateTime = uli.HighPart;
}

/*!
 * @brief
 * Converts a Timestamp struct to a FILETIME struct.
 *
 * @param ts
 * Pointer to a Timestamp struct to convert.
 *
 * @param out
 * Pointer to a FILETIME struct that will receive the converted Timestamp.
 *
 * @return
 * true if the conversion was successful; false otherwise.
 */
static bool timestamp_to_filetime(const Timestamp *ts, FILETIME *out) {
    assert(ts && out);

    if (ts->utc_offset.specified) {
        FILETIME ft;

        if (!SystemTimeToFileTime(&ts->st, &ft)) {
            return false;
        }

        ULARGE_INTEGER ticks = { 0 };

        ticks.LowPart = ft.dwLowDateTime;
        ticks.HighPart = ft.dwHighDateTime;

        LONGLONG delta =
            (LONGLONG)ts->utc_offset.minutes *
            60LL *
            10000000LL;

        // Prevent underflow if UTC conversion moves time before the FILETIME
        // epoch (Jan 1, 1601). E.g., 1601-01-01T00:00:00+01:00
        //
        // No need to worry about overflow here since the ISO 8601 timestamp
        // parser doesn't allow more than four digits for the year field anyway.
        // An overflow would require the resulting FILETIME to be greater than
        // 0x7FFFFFFFFFFFFFFF, which is equivalent to 30828-09-14T02:48:05.4775807
        // when converted to a timestamp
        if ((delta > 0) && (ticks.QuadPart < (ULONGLONG)delta)) {
            return false;
        }

        ticks.QuadPart -= delta;

        // If the converted timestamp is equivalent to the FILETIME epoch, that
        // is 1601-01-01T00:00:00.000Z, the resulting FILETIME value is {0, 0}.
        // Per SetFileTime() documentation, passing a FILETIME whose members are
        // {0, 0} indicates that the application intends to leave the
        // corresponding timestamp unchanged!
        //
        // I'm going to treat that as "expected behavior" until someone complains :D
        out->dwLowDateTime = ticks.LowPart;
        out->dwHighDateTime = ticks.HighPart;

        return true;
    }

    SYSTEMTIME utc;

    // Interpret timestamp as the local civil time in the current Windows TZ
    // configuration to properly handle TZ-related adjustments like DST transitions
    if (!TzSpecificLocalTimeToSystemTime(NULL, &ts->st, &utc)) {
        return false;
    }

    return SystemTimeToFileTime(&utc, out);
}

 /*!
  * @brief
  * Parses a given timestamp string and translates it into a FILETIME struct.
  *
  * @param stamp
  * The string to parse, which must follow ISO 8601 basic or extended format.
  *
  * @param out
  * Pointer to a FILETIME struct to receive the parsed timestamp.
  *
  * @returns
  * true if the timestamp was successfully parsed and translated; false otherwise.
  */
static bool parse_timestamp_string(const TCHAR *stamp, FILETIME *out) {
    Timestamp ts;

    if (!parse_timestamp(stamp, &ts)) {
        return false;
    }

    return timestamp_to_filetime(&ts, out);
}

/*!
 * @brief
 * Retrieves timestamps from the specified file.
 *
 * @param filename
 * Path to the file to retrieve timestamps from.
 *
 * @param out
 * Pointer to a reference_timestamps struct to store the retrieved timestamps.
 *
 * @return
 * true if the timestamps were successfully retrieved; false otherwise.
 */
static bool get_ref_timestamps(const TCHAR *filename, ReferenceTimestamps *out) {
    assert(filename && out);

    WIN32_FILE_ATTRIBUTE_DATA attr;

    if (!GetFileAttributesEx(filename, GetFileExInfoStandard, (void *)&attr)) {
        return false;
    }

    out->creation = attr.ftCreationTime;
    out->access = attr.ftLastAccessTime;
    out->write = attr.ftLastWriteTime;

    return true;
}

/*!
 * @brief
 * Adjusts a file's timestamps.
 *
 * @param file_handle
 * Handle to the file whose timestamps are to be adjusted.
 *
 * @param op
 * Pointer to a TimestampOperation struct containing operation details.
 *
 * @return
 * true if the timestamps were successfully adjusted; false otherwise.
 */
static bool adjust_file_time(HANDLE file_handle, const TimestampOperation *op) {
    assert(op);

    FILETIME creation;
    FILETIME access;
    FILETIME write;

    if (!GetFileTime(file_handle, &creation, &access, &write)) {
        return false;
    }

    const FILETIME *ptr_creation = NULL;
    const FILETIME *ptr_access = &ft_preserved;
    const FILETIME *ptr_write = &ft_preserved;

    if (op->ft_flags & FT_CREATION) {
        adjust_time_offset(&creation, op->adjustment_seconds);
        ptr_creation = &creation;
    }

    if (op->ft_flags & FT_ACCESS) {
        adjust_time_offset(&access, op->adjustment_seconds);
        ptr_access = &access;
    }

    if (op->ft_flags & FT_WRITE) {
        adjust_time_offset(&write, op->adjustment_seconds);
        ptr_write = &write;
    }

    return SetFileTime(file_handle, ptr_creation, ptr_access, ptr_write);
}

/*!
 * @brief
 * Sets the timestamps of the file based on details provide by \p op.
 *
 * @param op
 * Pointer to a TimestampOperation struct containing operation details.
 *
 * @param file_handle
 * An open handle to the file to set its timestamp.
 *
 * @return
 * true if the timestamps were successfully set; false otherwise.
 */
static bool set_file_time(HANDLE file_handle, const TimestampOperation *op) {
    assert(op);

    if (!(op->ft_flags & (FT_CREATION | FT_ACCESS | FT_WRITE))) {
        return true;
    }

    // If there's an adjustment but no explicit timestamp via a reference file
    // or timestamp input, adjust the current file time only
    if (op->source == TS_SOURCE_RELATIVE && op->adjustment_seconds != 0) {
        return adjust_file_time(file_handle, op);
    }

    return SetFileTime(
        file_handle,
        (op->ft_flags & FT_CREATION) ? &op->creation : NULL,
        (op->ft_flags & FT_ACCESS) ? &op->access : &ft_preserved,
        (op->ft_flags & FT_WRITE) ? &op->write : &ft_preserved);
}

/*!
 * @brief
 * Constructs a TimestampOperation struct based on the given parameters.
 *
 * @param ft_stamp
 * Optional pointer to a FILETIME struct containing the timestamp to work with.
 * If specified, then \p ref_stamps should be NULL.
 *
 * @param ref_stamps
 * Optional pointer to ReferenceTimestamps struct containing the timestamps to
 * work with. If specified, then \p ft_stamp should be NULL.
 *
 * @param ft_flags
 * Flags indicating which file timestamps are affected.
 * 
 * @param adjustment_seconds
 * Seconds to add to or subtract (if negative) from the target timestamp.
 *
 * @return
 * A TimestampOperation describing the requested operation.
 * 
 * If both \p ft_stamp and \p ref_stamps are NULL, and \p adjustment_seconds is
 * zero, the current time of day will be used as the base timestamp. However, if
 * \p adjustment_seconds is non-zero, the \c source member of the returned struct
 * will be set to \c TS_SOURCE_RELATIVE, indicating that time adjustment is
 * applied relative to an existing file's timestamps.
 */
static TimestampOperation prepare_timestamp(
    const FILETIME *ft_stamp,
    const ReferenceTimestamps *ref_stamps,
    FileTimeFlags ft_flags, int adjustment_seconds) {

    TimestampOperation op = { 0 };

    op.ft_flags = ft_flags;
    op.adjustment_seconds = adjustment_seconds;

    if (!(ft_stamp || ref_stamps) && adjustment_seconds != 0) {
        op.source = TS_SOURCE_RELATIVE;
        // Nothing to do here since relative adjustment is handled in
        // set_file_time()
        return op;
    }

    if (ref_stamps) {
        op.source = TS_SOURCE_EXPLICIT;

        op.creation = ref_stamps->creation;
        op.access = ref_stamps->access;
        op.write = ref_stamps->write;
    } else if (ft_stamp) {
        op.source = TS_SOURCE_EXPLICIT;
        op.creation = op.access = op.write = *ft_stamp;
    } else {
        FILETIME now;
        GetSystemTimeAsFileTime(&now);

        op.source = TS_SOURCE_NOW;
        op.creation = op.access = op.write = now;
    }

    if (op.adjustment_seconds != 0) {
        if (op.ft_flags & FT_CREATION) {
            adjust_time_offset(&op.creation, op.adjustment_seconds);
        }

        if (op.ft_flags & FT_ACCESS) {
            adjust_time_offset(&op.access, op.adjustment_seconds);
        }

        if (op.ft_flags & FT_WRITE) {
            adjust_time_offset(&op.write, op.adjustment_seconds);
        }
    }

    return op;
}

/*!
 * @brief
 * Changes the timestamp of the given file.
 *
 * @param path
 * Path to the file to touch.
 *
 * @param existing_only
 * Specifies whether to operate on an existing file only, or create the file if
 * it does not exist.
 * 
 * @param follow_symlinks
 * Specifies whether to follow symbolic links, or operate on the links themselves.
 *
 * @return
 * true if the timestamps were successfully changed; false otherwise.
 */
static bool touch(
    const TCHAR *path,
    bool existing_only, bool follow_symlinks,
    const TimestampOperation *op) {

    assert(path && op);

    DWORD cw_flags = FILE_ATTRIBUTE_NORMAL;

    if (!follow_symlinks) {
        cw_flags |= FILE_FLAG_OPEN_REPARSE_POINT;
    }

    HANDLE file_handle = CreateFile(
        path,                                        // lpFileName
        GENERIC_READ | FILE_WRITE_ATTRIBUTES,        // dwDesiredAccess
        FILE_SHARE_READ,                             // dwShareMode
        NULL,                                        // lpSecurityAttributes
        existing_only ? OPEN_EXISTING : OPEN_ALWAYS, // dwCreationDisposition
        cw_flags,                                    // dwFlagsAndAttributes
        NULL                                         // hTemplateFile
    );

    if (file_handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();

        bool missing_file =
            (err == ERROR_FILE_NOT_FOUND ||
             err == ERROR_PATH_NOT_FOUND);

        return (existing_only && missing_file);
    }

    bool ok = set_file_time(file_handle, op);
    CloseHandle(file_handle);

    return ok;
}

/*!
 * @brief
 * Prints an error message to stderr and exits the program with a failure status.
 * 
 * @param print_help_hint
 * Specifies whether to print a help hint.
 * 
 * @param fmt
 * Format string for the error message.
 * 
 * @param ...
 * Arguments for the format string.
 */
static noreturn void die(bool print_help_hint, const TCHAR *fmt, ...) {
    va_list args;

    va_start(args, fmt);

    console_vfprintf_color(console,
        CONSOLE_COLOR_NONE, CONSOLE_COLOR_RED,
        stderr, fmt, args);

    va_end(args);

    if (print_help_hint) {
        _tprintf(_T("Try '%s -h' to show help information.\n"), prog_name);
    }

    console_close(console);

    exit(EXIT_FAILURE);
}

int _tmain(int argc, TCHAR **argv) {
    SetConsoleOutputCP(1252);

    console = console_open();
    prog_name = get_name(argv[0]);

    // Store option arguments to process later before touching
    TCHAR *offset_input = NULL;
    TCHAR *stamp_input = NULL;
    TCHAR *stamp_ref_file_input = NULL;

    FileTimeFlags ft_flags = 0;
    int adjustment_seconds = 0;

    int file_must_exist = false;
    int follow_symlinks = true;

    if (argc < 2) {
        die(true, _T("%s: No argument is supplied.\n"), prog_name);
    }

    int option;
    while ((option = get_opt(argc, argv, _T("A:aCcdhmr:t:v"))) != -1) {
        switch (option) {
            case 'A':
                offset_input = opt_arg;
                break;
            case 'a':
                ft_flags |= FT_ACCESS;
                break;
            case 'C':
                ft_flags |= FT_CREATION;
                break;
            case 'c':
                file_must_exist = true;
                break;
            case 'd':
                follow_symlinks = false;
                break;
            case 'h':
                print_usage_info();
                console_close(console);
                exit(EXIT_SUCCESS);
            case 'm':
                ft_flags |= FT_WRITE;
                break;
            case 'r':
                stamp_ref_file_input = opt_arg;
                break;
            case 't':
                stamp_input = opt_arg;
                break;
            case 'v':
                print_version_info();
                console_close(console);
                exit(EXIT_SUCCESS);
            default:
                if (opt_error == GETOPT_ERR_OPT_UNKNOWN) {
                    die(true, _T("%s: Option -%c is illegal.\n"), prog_name, opt);
                } else if (opt_error == GETOPT_ERR_OPT_REQ_ARG) {
                    die(true, _T("%s: Option -%c requires an argument.\n"), prog_name, opt);
                } else {
                    die(true, _T("%s: Option is missing or the format is invalid.\n"), prog_name);
                }
        }
    }

    // Activate both flags if none are specified in options
    if (!(ft_flags & (FT_CREATION | FT_ACCESS | FT_WRITE))) {
        ft_flags |= (FT_ACCESS | FT_WRITE);
    }

    // Didn't receive any files to touch
    if (opt_index == argc) {
        die(true, _T("%s: Missing file operand.\n"), prog_name);
    }

    // Process time adjustment offset
    if (offset_input) {
        int offset;

        if (!parse_hhmmss(offset_input, &offset)) {
            die(true, _T("%s: Adjustment offset is invalid.\n"), prog_name);
        }

        adjustment_seconds = offset;
    }

    // Disallow timestamp inputs for multiple sources as it makes no sense
    if (stamp_input && stamp_ref_file_input) {
        die(false, _T("%s: Cannot set timestamp from multiple sources.\n"), prog_name);
    }

    FILETIME ft_stamp, *ft_stamp_ptr = NULL;
    ReferenceTimestamps ref_stamps, *ref_stamps_ptr = NULL;

    if (stamp_input) {
        if (!parse_timestamp_string(stamp_input, &ft_stamp)) {
            die(true, _T("%s: Timestamp is invalid or not in the expected format.\n"), prog_name);
        }

        ft_stamp_ptr = &ft_stamp;
    }

    if (stamp_ref_file_input) {
        if (!get_ref_timestamps(stamp_ref_file_input, &ref_stamps)) {
            DWORD err = GetLastError();

            if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
                die(false, _T("%s: Reference file does not exist.\n"), prog_name);
            } else {
                die(false, _T("%s: Reference timestamp could not be retrieved.\n"), prog_name);
            }
        }

        ref_stamps_ptr = &ref_stamps;
    }

    TimestampOperation op = prepare_timestamp(
        ft_stamp_ptr, ref_stamps_ptr,
        ft_flags, adjustment_seconds);

    bool all_ok = true;

    for (; opt_index < argc; opt_index++) {
        bool ok = touch(argv[opt_index], file_must_exist, follow_symlinks, &op);

        all_ok &= ok;

        if (!ok) {
            TCHAR *err_msg = get_win32_last_error_msg();
            console_printf_error(console, _T("%s: Could not open '%s' - %s"), prog_name, argv[opt_index], err_msg);
            HeapFree(GetProcessHeap(), 0, err_msg);
        };
    }

    console_close(console);

    return all_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}