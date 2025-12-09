/* main.c
 * Copyright (C) 2023 Jad Altahan (https://github.com/xv)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdbool.h>
#include <tchar.h>

#include "errmsg.h"
#include "getopt.h"
#include "concolors.h"
#include "version.h"

#ifdef _WIN64
#define BUILD_PLAT "x64"
#else
#define BUILD_PLAT "x86"
#endif

#define puts(str) _putts(_T(str))

#define printf(fmt, ...) _tprintf(_T(fmt), ##__VA_ARGS__)

#define printf_error(fmt, ...) {\
    console_set_colors(COLOR_RED, COLOR_NONE);\
    _ftprintf(stderr, _T(fmt), ##__VA_ARGS__);\
    console_reset_colors();\
};

typedef unsigned short ushort;

typedef enum timestamp_format {
    TF_LOCAL,
    TF_UTC
} timestamp_format_t;

typedef struct reference_timestamps {
    FILETIME creation_time;
    FILETIME last_access_time;
    FILETIME last_write_time;
} reference_timestamps_t;

typedef struct program_config {
    int change_time_flags;
    int time_offset;
    bool no_create;
    bool no_dereference;
    reference_timestamps_t *ref_stamps;
    FILETIME *stamp;
    timestamp_format_t stamp_format;
} program_config_t;

#define FLAG_CHANGE_TIME_CREATION    (1 << 0)
#define FLAG_CHANGE_TIME_LAST_ACCESS (1 << 1)
#define FLAG_CHANGE_TIME_LAST_WRITE  (1 << 2)
#define HAS_FLAG(flags, mask) (flags & mask) != 0

program_config_t config = {
    .change_time_flags = 0,
    .time_offset = 0,
    .no_create = false,
    .no_dereference = false,
    .ref_stamps = NULL,
    .stamp = NULL,
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

console_screen_t *console;

/*!
 * @brief
 * Prints program usage information.
 */
static void print_usage_info(void) {
    puts(
"\
Syntax:\n\
  touch [options] file [...]\n\n\
Options:\n\
  -A OFFSET    Adjust the timestamp time by an offset in the format\n\
               [-]HH[mm][ss]. A Negative offset moves time backward.\n\n\
  -a           Change last access time only.\n\
  -C           Change creation time only\n\
  -c           Do not create files.\n\
  -d           Do not follow symbolic links.\n\
  -m           Change last modified time only.\n\
  -r FILE      Set the timestamp from a reference file.\n\n\
  -t STAMP     Set a timestamp in the format yyyyMMddHHmm[ss][Z].\n\
               You may append 'Z' at the end of the timestamp to\n\
               convert it from local time to UTC.\n\n\
  -h           Display this help information and exit.\n\
  -v           Display version information and exit.\n\n\
Refer to the detailed documentation at:\n\
https://github.com/xv/touch-cmd-windows");
}

/*!
 * @brief
 * Prints program version information.
 */ 
static void print_version_info(void) {
    printf("touch %s (%s)\n", _T(VERSION_STR), _T(BUILD_PLAT));
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
    if (*str == '\0') {
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
static TCHAR *get_name(TCHAR *path, bool rem_ext) {
    TCHAR *name = _tcsrchr(path, '\\');
    if (!name) {
        return path;
    }

    name++;

    if (rem_ext) {
        TCHAR *dot = _tcsrchr(name, '.');
        if (dot) {
            *dot = '\0';
        }
    }

    return name;
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
 * 1 if the function succeeds; 0 otherwise.
 */
static bool local_time_to_file_time(const LPSYSTEMTIME system_time, LPFILETIME file_time) {
    if (!system_time || !file_time) {
        return false;
    }

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
 * Adjusts a file's timestamp by an offset.
 * 
 * @param file_handle
 * Handle to the file whose timestamp is to be adjusted.
 * 
 * @param offset
 * Time represented in seconds.
 */
static void adjust_file_timestamp(HANDLE file_handle, int offset) {
    FILETIME ft_creation, ft_access, ft_write;

    if (!GetFileTime(file_handle, &ft_creation, &ft_access, &ft_write)) {
        return;
    }

    FILETIME *ptr_creation, *ptr_access, *ptr_write;
    ptr_creation = NULL;
    ptr_access = ptr_write = &ft_preserved;

    if (HAS_FLAG(config.change_time_flags, FLAG_CHANGE_TIME_CREATION)) {
        adjust_time_offset(&ft_creation, offset);
        ptr_creation = &ft_creation;
    }

    if (HAS_FLAG(config.change_time_flags, FLAG_CHANGE_TIME_LAST_ACCESS)) {
        adjust_time_offset(&ft_access, offset);
        ptr_access = &ft_access;
    }

    if (HAS_FLAG(config.change_time_flags, FLAG_CHANGE_TIME_LAST_WRITE)) {
        adjust_time_offset(&ft_write, offset);
        ptr_write = &ft_write;
    }

    SetFileTime(file_handle, ptr_creation, ptr_access, ptr_write);
}

/*!
 * @brief
 * Translates a timestamp to a FILETIME struct.
 *
 * @param stamp
 * Timestamp string in the format yyyyMMddHHmm[ss].
 * 
 * @returns
 * Pointer to a FILETIME struct containing the translated timestamp.
 * Caller is responsible for freeing allocated memory.
 */
static LPFILETIME string_to_filetime(const TCHAR *stamp) {
    LPFILETIME result = malloc(sizeof(FILETIME));
    if (!result) {
        return NULL;
    }

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
        free(result);
        return NULL;
    }

    st.wYear = clamp_ushort(st.wYear, 1601, 30827);
    st.wMonth = clamp_ushort(st.wMonth, 1, 12);
    st.wDay = clamp_ushort(st.wDay, 1, 31);
    st.wHour = clamp_ushort(st.wHour, 0, 23);
    st.wMinute = clamp_ushort(st.wMinute, 0, 59);
    st.wSecond = clamp_ushort(st.wSecond, 0, 59);

    if (config.stamp_format == TF_LOCAL) {
        local_time_to_file_time(&st, result);
    } else {
        // UTC timestamp
        SystemTimeToFileTime(&st, result);
    }

    if (config.time_offset != 0) {
        adjust_time_offset(result, config.time_offset);
    }

    return result;
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
 * @returns
 * Pointer to a FILETIME struct containing the parsed timestamp.
 */
static LPFILETIME parse_timestamp_string(TCHAR *stamp) {
    TCHAR *zulu = _tcschr(stamp, 'Z');

    if (zulu != NULL) {
        size_t spec_len = _tcslen(zulu);
        if (spec_len > 1) {
            return NULL;
        }

        config.stamp_format = TF_UTC;
        stamp[_tcslen(stamp) - spec_len] = '\0';
    }

    if (!is_digits(stamp)) {
        return NULL;
    }

    return string_to_filetime(stamp);
}

/*!
 * @brief
 * Retrieves timestamps from the specified file.
 * 
 * @param filename
 * Path to the file to retrieve timestamps from.
 * 
 * @return
 * Pointer to a reference_timestamps struct containing timestamps retrieved
 * from the specified file.
 */
static reference_timestamps_t *get_ref_timestamp(const TCHAR *filename) {
    reference_timestamps_t *result = malloc(sizeof(reference_timestamps_t));
    if (!result) {
        return NULL;
    }

    WIN32_FILE_ATTRIBUTE_DATA attr;

    if (!GetFileAttributesEx(filename, GetFileExInfoStandard, (void *)&attr)) {
        free(result);
        return NULL;
    }

    result->creation_time = attr.ftCreationTime;
    result->last_access_time = attr.ftLastAccessTime;
    result->last_write_time = attr.ftLastWriteTime;

    int offset = config.time_offset;

    if (offset != 0) {
        if (HAS_FLAG(config.change_time_flags, FLAG_CHANGE_TIME_CREATION)) {
            adjust_time_offset(&result->creation_time, offset);
        }

        if (HAS_FLAG(config.change_time_flags, FLAG_CHANGE_TIME_LAST_ACCESS)) {
            adjust_time_offset(&result->last_access_time, offset);
        }

        if (HAS_FLAG(config.change_time_flags, FLAG_CHANGE_TIME_LAST_WRITE)) {
            adjust_time_offset(&result->last_write_time, offset);
        }
    }

    return result;
}

/*!
 * @brief
 * Retrieves the current system date and time.
 *
 * @return
 * Pointer to a FILETIME struct containing the current system date and time.
 */
static LPFILETIME get_current_filetime(void) {
    LPFILETIME result = malloc(sizeof(FILETIME));
    if (!result) {
        return NULL;
    }

    SYSTEMTIME st;
    GetSystemTime(&st);
    SystemTimeToFileTime(&st, result);

    return result;
}

/*!
 * @brief
 * Sets the timestamps of the file based on the specified options
 * 
 * @param file_handle
 * An open handle to the file to set its timestamp.
 */
static void set_file_timestamp(HANDLE file_handle) {
    bool change_creation = HAS_FLAG(config.change_time_flags, FLAG_CHANGE_TIME_CREATION);
    bool change_access = HAS_FLAG(config.change_time_flags, FLAG_CHANGE_TIME_LAST_ACCESS);
    bool change_write = HAS_FLAG(config.change_time_flags, FLAG_CHANGE_TIME_LAST_WRITE);

    if (!(change_creation || change_access || change_write)) {
        return;
    }

    if (config.ref_stamps) {
        SetFileTime(
            file_handle,
            change_creation ? &config.ref_stamps->creation_time : NULL,
            change_access ? &config.ref_stamps->last_access_time : &ft_preserved,
            change_write ? &config.ref_stamps->last_write_time : &ft_preserved
        );
    } else {
        // Custom timestamp not specified but an adjustment option is
        if (config.time_offset != 0 && !config.stamp) {
            adjust_file_timestamp(file_handle, config.time_offset);
            return;
        }

        SetFileTime(
            file_handle,
            change_creation ? config.stamp : NULL,
            change_access ? config.stamp : &ft_preserved,
            change_write ? config.stamp : &ft_preserved
        );
    }
}

/*!
 * @brief
 * Changes the timestamp of the given file.
 * 
 * @param filename
 * Path to the file to touch.
 */
static bool touch(const TCHAR *filename, bool follow_symlinks) {
    bool create_new = !config.no_create;
    int cw_flags = FILE_ATTRIBUTE_NORMAL;

    if (!follow_symlinks) {
        cw_flags |= FILE_FLAG_OPEN_REPARSE_POINT;
    }
    
    HANDLE file_handle = CreateFile(
        filename,                                 // lpFileName
        GENERIC_READ | FILE_WRITE_ATTRIBUTES,     // dwDesiredAccess
        FILE_SHARE_READ,                          // dwShareMode
        NULL,                                     // lpSecurityAttributes
        create_new ? OPEN_ALWAYS : OPEN_EXISTING, // dwCreationDisposition
        cw_flags,                                 // dwFlagsAndAttributes
        NULL                                      // hTemplateFile
    );

    if (file_handle == INVALID_HANDLE_VALUE) {
        return create_new ? false : true;
    }

    set_file_timestamp(file_handle);

    CloseHandle(file_handle);
    return true;
}

int _tmain(int argc, TCHAR **argv) {
    bool status_ok = true;
    bool print_get_help = true;

    TCHAR *prog_name = get_name(argv[0], true);

    // Store option arguments to process later before touching
    TCHAR *hhmmss_adjustment = NULL;
    TCHAR *stamp_input = NULL;
    TCHAR *stamp_ref_file_input = NULL;

    console_init(&console);

    if (argc < 2) {
        printf_error("%s: No argument is supplied.\n", prog_name);

        status_ok = false;
        goto clean_exit;
    }

    int option;
    while ((option = get_opt(argc, argv, _T("A:aCcdhmr:t:v"))) != -1) {
        switch (option) {
            case 'A':
                hhmmss_adjustment = opt_arg;
                break;
            case 'a':
                config.change_time_flags |= FLAG_CHANGE_TIME_LAST_ACCESS;
                break;
            case 'C':
                config.change_time_flags |= FLAG_CHANGE_TIME_CREATION;
                break;
            case 'c':
                config.no_create = true;
                break;
            case 'd':
                config.no_dereference = true;
                break;
            case 'h':
                print_usage_info();
                goto clean_exit;
            case 'm':
                config.change_time_flags |= FLAG_CHANGE_TIME_LAST_WRITE;
                break;
            case 'r':
                stamp_ref_file_input = opt_arg;
                break;
            case 't':
                stamp_input = opt_arg;
                break;
            case 'v':
                print_version_info();
                goto clean_exit;
            default:
                if (opt_error == ERROR_ILLEGAL_OPT) {
                    printf_error("%s: Option -%c is illegal.\n", prog_name, opt);
                } else if (opt_error == ERROR_OPT_REQ_ARG) {
                    printf_error("%s: Option -%c requires an argument.\n", prog_name, opt);
                } else {
                    printf_error("%s: Option is missing or the format is invalid.\n", prog_name);
                }

                status_ok = false;
                goto clean_exit;
        }
    }

    // Didn't receive any files to touch
    if (opt_index == argc) {
        printf_error("%s: Missing file operand.\n", prog_name);

        status_ok = false;
        goto clean_exit;
    }

    // Process time adjustment offset
    if (hhmmss_adjustment) {
        config.time_offset = parse_hhmmss(hhmmss_adjustment);
        if (config.time_offset == INT_MIN) {
            printf_error("%s: Adjustment offset is invalid.\n", prog_name);

            status_ok = false;
            goto clean_exit;
        }
    }

    // Disallow timestamp inputs for multiple sources as it makes no sense
    if (stamp_input && stamp_ref_file_input) {
        printf_error("%s: Cannot set timestamp from multiple sources.\n", prog_name);

        status_ok = false;
        goto clean_exit;
    }

    // Process user-provided timestamp
    if (stamp_input) {
        config.stamp = parse_timestamp_string(stamp_input);
        if (!config.stamp) {
            printf_error("%s: Timestamp does not respect format.\n", prog_name);

            status_ok = false;
            goto clean_exit;
        }
    } else {
        // No need for current time if we're using a ref or only adjusting
        // a file's timestamps
        if (!(stamp_ref_file_input || hhmmss_adjustment)) {
            config.stamp = get_current_filetime();
        }
    }

    // Process file-referenced timestamp
    if (stamp_ref_file_input) {
        config.ref_stamps = get_ref_timestamp(stamp_ref_file_input);
        if (!config.ref_stamps) {
            printf_error("%s: Reference timestamp could not be set.\n", prog_name);

            status_ok = false;
            goto clean_exit;
        }
    }

    // Activate both flags if none are specified in options
    if (config.change_time_flags == 0) {
        config.change_time_flags = (FLAG_CHANGE_TIME_LAST_ACCESS | FLAG_CHANGE_TIME_LAST_WRITE);
    }

    for (; opt_index < argc; opt_index++) {
        status_ok &= touch(argv[opt_index], !config.no_dereference);

        if (!status_ok) {
            TCHAR *err_msg = get_win32_last_error_msg();
            printf_error("%s: Could not open '%s' - %s", prog_name, argv[opt_index], err_msg);
            HeapFree(GetProcessHeap(), 0, err_msg);
        };
    }

    print_get_help = false;

clean_exit:

    if (!status_ok && print_get_help) {
        printf("Try '%s -h' to show help information.\n", prog_name);
    }

    free(console);

    if (config.ref_stamps) {
        free(config.ref_stamps);
    }

    if (config.stamp) {
        free(config.stamp);
    }

    int exit_code = status_ok ? EXIT_SUCCESS : EXIT_FAILURE;
    return exit_code;
}