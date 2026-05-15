/* main.c
 * Copyright (C) 2023 Jad Altahan (https://github.com/xv)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <windows.h>
#include <limits.h>
#include <tchar.h>
#include <assert.h>

#include "errmsg.h"
#include "getopt.h"
#include "console.h"
#include "version.h"

#ifdef _WIN64
#define BUILD_PLAT "x64"
#else
#define BUILD_PLAT "x86"
#endif

#define PROGRAM_USAGE_SUMMARY \
"Syntax:\n\
    touch [options] file [...]\n\n\
Options:\n\
    -A OFFSET    Adjust the timestamps of a file by an offset specified by the\n\
                 OFFSET argument, which must be in the format [-]HH[mm][ss],\n\
                 where HH is a 2-digit numerical representation of hours between\n\
                 0 and 99; mm is a 2-digit numerical representation of minutes\n\
                 between 0 and 59; and ss is a 2-digit numerical representation\n\
                 of seconds between 0 and 59. A negative offset will move time\n\
                 backward. If a file does not exist, it will be created with\n\
                 adjusted timestamps, unless the -c option is specified.\n\n\
    -a           Change only the last access timestamp. The last modified\n\
                 timestamp will not be changed unless -m is specified.\n\n\
    -C           Change only the creation timestamp. Neither the last access or\n\
                 last modified timestamps will be changed unless -a or -m\n\
                 (or both) are specified.\n\n\
    -c           Do not create a new file if the file specified does not exist.\n\
                 The program will not display a diagnostic or error message and\n\
                 the exit value will not be affected.\n\n\
    -d           Do not dereference symbolic links. If this option is specified,\n\
                 the timestamp of the symbolic link itself will be changed\n\
                 rather than the file it refers to.\n\n\
    -m           Change only the last modified timestamp. The last access\n\
                 timestamp will not be changed unless -a is specified.\n\n\
    -r FILE      Use the timestamp of the file specified by the FILE argument\n\
                 instead of the current time of day. This option cannot be\n\
                 combined with -t.\n\n\
    -t STAMP     Use the timestamp specified by the STAMP argument, which must\n\
                 be in the format yyyyMMddHHmm[ss][Z], where yyyy is a 4-digit\n\
                 numerical representation of the year; MM is a 2-digit numerical\n\
                 representation of the month; dd is a 2-digit numerical\n\
                 representation of the day; and ss is an optional 2-digit\n\
                 numerical representation of the seconds. The timestamp is in\n\
                 local time by default; however, you may optionally append Z\n\
                 (case-sensitive) at the end to convert it to UTC time. This\n\
                 option cannot be combined with -r.\n\n\
    -h           Display this help information and exit.\n\n\
    -v           Display version information and exit.\n\n\
This is an open-source utility whose code is found at:\n\
https://github.com/xv/touch-cmd-windows"

typedef unsigned short ushort;

typedef enum timestamp_format {
    TF_LOCAL,
    TF_UTC
} timestamp_format_t;

typedef enum timestamp_mode {
    TM_NOW,
    TM_EXPLICIT,
    TM_RELATIVE
} timestamp_mode_t;

typedef enum file_time_flags
{
    FT_CREATION = 1 << 0,
    FT_ACCESS = 1 << 1,
    FT_WRITE = 1 << 2
} file_time_flags_t;

typedef struct reference_timestamps {
    FILETIME creation;
    FILETIME access;
    FILETIME write;
} reference_timestamps_t;

typedef struct timestamp_operation {
    timestamp_mode_t mode;
    file_time_flags_t ft_flags;
    FILETIME creation;
    FILETIME access;
    FILETIME write;
    int adjustment_seconds;
} timestamp_operation_t;

typedef struct program_config {
    bool no_create;
    bool no_dereference;
    timestamp_format_t stamp_format;
} program_config_t;

static program_config_t config = {
    .no_create = false,
    .no_dereference = false,
    .stamp_format = TF_LOCAL
};

// Specifies that a file's previous last access or write times should be preserved
// when operating with file handles
// https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-filetime
// https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfiletime
const FILETIME ft_preserved = {
    .dwLowDateTime = 0xFFFFFFFF,
    .dwHighDateTime = 0xFFFFFFFF
};

static const TCHAR *prog_name;
static console_t *console;

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
 * Checks if a string consists of digits (0-9) only.
 *
 * @param str
 * The string to check.
 *
 * @return
 * 1 if the given string is all digits; false otherwise.
 */
static bool is_digits(const TCHAR *str) {
    if (!str || *str == '\0') {
        return false;
    }

    while (*str) {
        if (*str < '0' || *str > '9') {
            return false;
        }

        ++str;
    }

    return true;
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
 * Clamps an unsigned short value between a minimum and a maximum.
 *
 * @param n
 * Original value.
 *
 * @param min
 * Minimum value.
 *
 * @param max
 * Maximum value.
 *
 * @returns
 * Clamped value if the original is outside of the min-max boundary.
 */
static inline ushort clamp_ushort(ushort n, ushort min, ushort max) {
    const ushort u = n < min ? min : n;
    return u > max ? max : u;
}

/*!
 * @brief
 * Converts a local time specified in a SYSTEMTIME struct to file time.
 *
 * @param system_time
 * Pointer to a SYSTEMTIME struct containing the time to convert.
 *
 * @param file_time
 * Pointer to a FILETIME struct to receive the converted time.
 *
 * @return
 * true if the function succeeds; false otherwise.
 */
static bool local_time_to_file_time(const SYSTEMTIME *system_time, FILETIME *file_time) {
    assert(system_time && file_time);

    FILETIME ft_local;
    SystemTimeToFileTime(system_time, &ft_local);
    return LocalFileTimeToFileTime(&ft_local, file_time);
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

    ULARGE_INTEGER uli = {
        ft->dwLowDateTime,
        ft->dwHighDateTime
    };

    uli.QuadPart += (offset * 10000000ULL);

    ft->dwLowDateTime = uli.LowPart;
    ft->dwHighDateTime = uli.HighPart;
}

/*!
 * @brief
 * Translates a timestamp to a FILETIME struct.
 *
 * @param stamp
 * Timestamp string in the format yyyyMMddHHmm[ss].
 *
 * @param out
 * Pointer to a FILETIME struct to receive the translated timestamp.
 *
 * @returns
 * true if the timestamp was successfully translated; false otherwise.
 */
static bool string_to_filetime(const TCHAR *stamp, FILETIME *out) {
    assert(stamp && out);

    SYSTEMTIME st = { 0 };

    int num_fields_converted = _stscanf(
        stamp,
        _T("%4hu%2hu%2hu%2hu%2hu%2hu"),
        &st.wYear,
        &st.wMonth,
        &st.wDay,
        &st.wHour,
        &st.wMinute,
        &st.wSecond
    );

    if (num_fields_converted < 5) {
        return false;
    }

    st.wYear = clamp_ushort(st.wYear, 1601, 30827);
    st.wMonth = clamp_ushort(st.wMonth, 1, 12);
    st.wDay = clamp_ushort(st.wDay, 1, 31);
    st.wHour = clamp_ushort(st.wHour, 0, 23);
    st.wMinute = clamp_ushort(st.wMinute, 0, 59);
    st.wSecond = clamp_ushort(st.wSecond, 0, 59);

    if (config.stamp_format == TF_LOCAL) {
        local_time_to_file_time(&st, out);
    } else {
        // UTC timestamp
        SystemTimeToFileTime(&st, out);
    }

    return true;
}

/*!
 * @brief
 * Parses a string in the format [-]HH[mm][ss].
 *
 * @param hhmmss
 * The string to parse.
 *
 * @return
 * Number of seconds if successful, either positive or negative depending
 * on the operator parsed. Otherwise, INT_MIN is returned on failure.
 */
static int parse_hhmmss(TCHAR *hhmmss) {
    assert(hhmmss);

    bool neg = false;

    if (*hhmmss == '-') {
        hhmmss++;
        neg = true;
    }

    if (_tcslen(hhmmss) > 6) {
        // Longer than HHmmss
        return INT_MIN;
    }

    int hh, mm, ss;
    hh = mm = ss = 0;

    int num_fields_converted = _stscanf(
        hhmmss,
        _T("%2d%2d%2d"),
        &hh,
        &mm,
        &ss
    );

    if (num_fields_converted < 1 || !is_digits(hhmmss)) {
        // Invalid; the hour as a bare minimum wasn't parsed or the string
        // has a non-digit
        return INT_MIN;
    }

    mm = clamp_ushort(mm, 0, 59);
    ss = clamp_ushort(ss, 0, 59);

    int offset = hh * 3600 + mm * 60 + ss;
    if (neg) {
        offset *= -1;
    }

    return offset;
}

/*!
 * @brief
 * Parses a given timestamp to translate into a FILETIME struct.
 *
 * @param stamp
 * The string to parse, in the format yyyyMMddHHmm[ss][Z].
 *
 * @param out
 * Pointer to a FILETIME struct to receive the parsed timestamp.
 *
 * @returns
 * true if the timestamp was successfully parsed; false otherwise.
 */
static bool parse_timestamp_string(TCHAR *stamp, FILETIME *out) {
    assert(stamp && out);

    TCHAR *zulu = _tcschr(stamp, 'Z');

    if (zulu != NULL) {
        size_t spec_len = _tcslen(zulu);
        if (spec_len > 1) {
            return false;
        }

        config.stamp_format = TF_UTC;
        stamp[_tcslen(stamp) - spec_len] = '\0';
    }

    if (!is_digits(stamp)) {
        return false;
    }

    return string_to_filetime(stamp, out);
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
static bool get_ref_timestamps(const TCHAR *filename, reference_timestamps_t *out) {
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
 * Pointer to a timestamp_operation_t struct containing operation details.
 *
 * @return
 * true if the timestamps were successfully adjusted; false otherwise.
 */
static bool adjust_file_time(HANDLE file_handle, const timestamp_operation_t *op) {
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
 * Pointer to a timestamp_operation_t struct containing operation details.
 *
 * @param file_handle
 * An open handle to the file to set its timestamp.
 *
 * @return
 * true if the timestamps were successfully set; false otherwise.
 */
static bool set_file_time(HANDLE file_handle, const timestamp_operation_t *op) {
    assert(op);

    if (!(op->ft_flags & (FT_CREATION | FT_ACCESS | FT_WRITE))) {
        return true;
    }

    // If there's an adjustment but no explicit timestamp via a reference file
    // or timestamp input, adjust the current file time only
    if (op->mode == TM_RELATIVE && op->adjustment_seconds != 0) {
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
 * Constructs a timestamp_operation_t struct based on the given parameters.
 *
 * @param ft_stamp
 * Optional pointer to a FILETIME struct containing the timestamp to work with.
 * If specified, then \p ref_stamps should be NULL.
 *
 * @param ref_stamps
 * Optional pointer to reference_timestamps_t struct containing the timestamps
 * to work with. If specified, then \p ft_stamp should be NULL.
 *
 * @param ft_flags
 * Flags indicating which file timestamps are affected.
 * 
 * @param adjustment_seconds
 * Seconds to add to or subtract (if negative) from the target timestamp.
 *
 * @return
 * A timestamp_operation_t describing the requested operation.
 * 
 * If both \p ft_stamp and \p ref_stamps are NULL, and \p adjustment_seconds is
 * zero, the current time of day will be used as the base timestamp. However, if
 * \p adjustment_seconds is non-zero, the \c mode member of the returned struct
 * will be set to \c TM_RELATIVE, indicating that time adjustment is applied
 * relative to an existing file's timestamps.
 */
static timestamp_operation_t prepare_timestamp(
    const FILETIME *ft_stamp,
    const reference_timestamps_t *ref_stamps,
    file_time_flags_t ft_flags, int adjustment_seconds) {

    timestamp_operation_t op = { 0 };

    op.ft_flags = ft_flags;
    op.adjustment_seconds = adjustment_seconds;

    if (!(ft_stamp || ref_stamps) && adjustment_seconds != 0) {
        op.mode = TM_RELATIVE;
        // Nothing to do here since relative adjustment is handled in
        // set_file_time()
        return op;
    }

    if (ref_stamps) {
        op.mode = TM_EXPLICIT;

        op.creation = ref_stamps->creation;
        op.access = ref_stamps->access;
        op.write = ref_stamps->write;
    } else if (ft_stamp) {
        op.mode = TM_EXPLICIT;
        op.creation = op.access = op.write = *ft_stamp;
    } else {
        FILETIME now;
        GetSystemTimeAsFileTime(&now);

        op.mode = TM_NOW;
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
 * @param create_new
 * Specifies whether to create a new file if it does not exist.
 * 
 * @param follow_symlinks
 * Specifies whether to follow symbolic links.
 *
 * @return
 * true if the timestamps were successfully changed; false otherwise.
 */
static bool touch(
    const TCHAR *path,
    bool create_new, bool follow_symlinks,
    const timestamp_operation_t *op) {

    assert(path && op);

    DWORD cw_flags = FILE_ATTRIBUTE_NORMAL;

    if (!follow_symlinks) {
        cw_flags |= FILE_FLAG_OPEN_REPARSE_POINT;
    }

    HANDLE file_handle = CreateFile(
        path,                                     // lpFileName
        GENERIC_READ | FILE_WRITE_ATTRIBUTES,     // dwDesiredAccess
        FILE_SHARE_READ,                          // dwShareMode
        NULL,                                     // lpSecurityAttributes
        create_new ? OPEN_ALWAYS : OPEN_EXISTING, // dwCreationDisposition
        cw_flags,                                 // dwFlagsAndAttributes
        NULL                                      // hTemplateFile
    );

    if (file_handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();

        bool missing_file =
            (err == ERROR_FILE_NOT_FOUND ||
             err == ERROR_PATH_NOT_FOUND);

        return (!create_new && missing_file);
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

    if (console) {
        free(console);
    }

    exit(EXIT_FAILURE);
}

int _tmain(int argc, TCHAR **argv) {
    console = console_open();
    prog_name = get_name(argv[0]);

    // Store option arguments to process later before touching
    TCHAR *offset_input = NULL;
    TCHAR *stamp_input = NULL;
    TCHAR *stamp_ref_file_input = NULL;

    file_time_flags_t ft_flags = 0;
    int adjustment_seconds = 0;

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
                config.no_create = true;
                break;
            case 'd':
                config.no_dereference = true;
                break;
            case 'h':
                print_usage_info();
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
                exit(EXIT_SUCCESS);
            default:
                if (opt_error == ERROR_ILLEGAL_OPT) {
                    die(true, _T("%s: Option -%c is illegal.\n"), prog_name, opt);
                } else if (opt_error == ERROR_OPT_REQ_ARG) {
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
        int offset = parse_hhmmss(offset_input);

        if (offset == INT_MIN) {
            die(true, _T("%s: Adjustment offset is invalid.\n"), prog_name);
        }

        adjustment_seconds = offset;
    }

    // Disallow timestamp inputs for multiple sources as it makes no sense
    if (stamp_input && stamp_ref_file_input) {
        die(false, _T("%s: Cannot set timestamp from multiple sources.\n"), prog_name);
    }

    FILETIME ft_stamp, *ft_stamp_ptr = NULL;
    reference_timestamps_t ref_stamps, *ref_stamps_ptr = NULL;

    if (stamp_input) {
        if (!parse_timestamp_string(stamp_input, &ft_stamp)) {
            die(true, _T("%s: Timestamp does not respect format.\n"), prog_name);
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

    timestamp_operation_t op = prepare_timestamp(
        ft_stamp_ptr, ref_stamps_ptr,
        ft_flags, adjustment_seconds);

    bool all_ok = true;

    for (; opt_index < argc; opt_index++) {
        bool ok = touch(argv[opt_index], !config.no_create, !config.no_dereference, &op);

        all_ok &= ok;

        if (!ok) {
            TCHAR *err_msg = get_win32_last_error_msg();
            console_printf_error(console, _T("%s: Could not open '%s' - %s"), prog_name, argv[opt_index], err_msg);
            HeapFree(GetProcessHeap(), 0, err_msg);
        };
    }

    if (console) {
        free(console);
    }

    return all_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}