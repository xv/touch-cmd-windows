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
    FILETIME access_time;
    FILETIME modified_time;
} reference_timestamps_t;

typedef struct program_config {
    int change_time_flags;
    int time_offset;
    bool no_create;
    bool no_dereference;
    reference_timestamps_t *ref_stamps;
    SYSTEMTIME *custom_stamp;
    timestamp_format_t stamp_format;
} program_config_t;

#define FLAG_CHANGE_TIME_MOD (1 << 0)
#define FLAG_CHANGE_TIME_ACC (1 << 1)
#define HAS_FLAG(flags, mask) (flags & mask) != 0

program_config_t config = {
    .change_time_flags = 0,
    .time_offset = 0,
    .no_create = false,
    .no_dereference = false,
    .ref_stamps = NULL,
    .custom_stamp = NULL,
    .stamp_format = TF_LOCAL
};

// Specifies that a file's previous access and modification times
// should be preserved
const FILETIME ft_preserved = {
    0xFFFFFFFF,
    0xFFFFFFFF
};

/*!
 * @brief
 * Prints program usage information.
 */
void print_usage_info(void) {
    puts(
"\
Syntax:\n\
  touch [options] file [...]\n\n\
Options:\n\
  -A OFFSET    Adjust the timestamp time by an offset in the format\n\
               [-]HH[mm][ss]. A Negative offset moves time backward.\n\n\
  -a           Change access time only.\n\n\
  -c           Do not create files.\n\n\
  -d           Do not follow symbolic links.\n\n\
  -m           Change modification time only.\n\n\
  -r FILE      Set the timestamp from a reference file.\n\n\
  -t STAMP     Set a timestamp in the format yyyyMMddHHmm[ss][Z].\n\
               You may append 'Z' at the end of the timestamp to\n\
               convert it from local time to UTC.\n\n\
  -h           Display this help information and exit.\n\n\
  -v           Display version information and exit.\n\n\
Refer to the detailed documentation at:\n\
https://github.com/xv/touch-cmd-windows");
}

/*!
 * @brief
 * Prints program version information.
 */
void print_version_info(void) {
    puts(
"touch - Windows alternative to the Unix command\n\
Copyright(C) Jad Altahan 2023\n\
Version 1.0.0\n\n\
This software may be modified And distributed under the terms of\n\
the MIT license. For a copy of the license, consult this webpage:\n\
https://github.com/xv/touch-cmd-windows/blob/master/LICENSE");
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
bool is_digits(const TCHAR *str) {
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
TCHAR *get_name(TCHAR *path, bool rem_ext) {
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
 * Adjusts the given file time by the given offset. If the offset is negative,
 * time is moved backward. Otherwise, forward.
 *
 * @param ft
 * Pointer to a FILETIME structure containing the time to adjust.
 *
 * @param offset
 * Time represented in seconds.
 */
void adjust_time_offset(FILETIME *ft, int offset) {
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
void adjust_file_timestamp(HANDLE file_handle, int offset) {
    bool chg_acc = HAS_FLAG(config.change_time_flags, FLAG_CHANGE_TIME_ACC);
    bool chg_mod = HAS_FLAG(config.change_time_flags, FLAG_CHANGE_TIME_MOD);

    FILETIME ft_acc = { 0 };
    FILETIME ft_mod = { 0 };

    if (chg_acc && chg_mod) {
        GetFileTime(file_handle, NULL, &ft_acc, &ft_mod);
        adjust_time_offset(&ft_acc, offset);
        adjust_time_offset(&ft_mod, offset);
        SetFileTime(file_handle, NULL, &ft_acc, &ft_mod);
    } else if (chg_acc) {
        GetFileTime(file_handle, NULL, &ft_acc, NULL);
        adjust_time_offset(&ft_acc, offset);
        SetFileTime(file_handle, NULL, &ft_acc, &ft_preserved);
    } else {
        GetFileTime(file_handle, NULL, NULL, &ft_mod);
        adjust_time_offset(&ft_mod, offset);
        SetFileTime(file_handle, NULL, &ft_preserved, &ft_mod);
    }
}

/*!
 * @brief
 * Translates a timestamp to a SYSTEMTIME struct.
 *
 * @param stamp
 * Timestamp string in the format yyyyMMddHHmm[ss].
 * 
 * @returns
 * Pointer to a SYSTEMTIME struct, containing the timestamp info.
 * Caller is responsible for freeing allocated memory.
 */
LPSYSTEMTIME string_to_systime(const TCHAR *stamp) {
    LPSYSTEMTIME result = malloc(sizeof(SYSTEMTIME));
    if (!result) {
        return NULL;
    }

    result->wSecond = 0;
    result->wMilliseconds = 0;

    int num_fields_converted = _stscanf(
        stamp,
        _T("%4hu%2hu%2hu%2hu%2hu%2hu"),
        &result->wYear,
        &result->wMonth,
        &result->wDay,
        &result->wHour,
        &result->wMinute,
        &result->wSecond
    );

    if (num_fields_converted < 5) {
        free(result);
        return NULL;
    }

    result->wYear = clamp_ushort(result->wYear, 1601, 30827);
    result->wMonth = clamp_ushort(result->wMonth, 1, 12);
    result->wDay = clamp_ushort(result->wDay, 1, 31);
    result->wHour = clamp_ushort(result->wHour, 0, 23);
    result->wMinute = clamp_ushort(result->wMinute, 0, 59);
    result->wSecond = clamp_ushort(result->wSecond, 0, 59);

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
int parse_hhmmss(TCHAR *hhmmss) {
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
 * Parses a given timestamp to translate into a SYSTEMTIME struct.
 * 
 * @param stamp
 * The string to parse, in the format yyyyMMddHHmm[ss][Z].
 * 
 * @returns
 * Pointer to a SYSTEMTIME struct, containing the parsed timestamp info.
 */
LPSYSTEMTIME parse_timestamp_string(TCHAR *stamp) {
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

    return string_to_systime(stamp);
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
bool local_time_to_file_time(const LPSYSTEMTIME system_time, LPFILETIME file_time) {
    if (!system_time || !file_time) {
        return false;
    }

    FILETIME file_time_local;
    SystemTimeToFileTime(system_time, &file_time_local);
    return LocalFileTimeToFileTime(&file_time_local, file_time);
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
reference_timestamps_t *get_ref_timestamp(const TCHAR *filename) {
    reference_timestamps_t *result = malloc(sizeof(reference_timestamps_t));
    if (!result) {
        return NULL;
    }

    WIN32_FILE_ATTRIBUTE_DATA attr;

    if (!GetFileAttributesEx(filename, GetFileExInfoStandard, (void *)&attr)) {
        free(result);
        return NULL;
    }

    result->access_time = attr.ftLastAccessTime;
    result->modified_time = attr.ftLastWriteTime;

    return result;
}

/*!
 * @brief
 * Sets the file timestamp from a user-supplied input. If this input is not
 * supplied via the appropriate option, the timestamp will use the current
 * time.
 *
 * @param file_handle
 * An open handle to the file to set its timestamp.
 */
void set_file_timestamp_from_input(HANDLE file_handle) {
    FILETIME file_time;

    if (config.custom_stamp == NULL) {
        if (config.time_offset != 0) {
            adjust_file_timestamp(file_handle, config.time_offset);
            return;
        }

        SYSTEMTIME sys_time;
        GetSystemTime(&sys_time);
        SystemTimeToFileTime(&sys_time, &file_time);
    } else {
        if (config.stamp_format == TF_LOCAL) {
            local_time_to_file_time(config.custom_stamp, &file_time);
        } else {
            // UTC timestamp
            SystemTimeToFileTime(config.custom_stamp, &file_time);
        }

        if (config.time_offset != 0) {
            adjust_time_offset(&file_time, config.time_offset);
        }
    }

    bool chg_acc = HAS_FLAG(config.change_time_flags, FLAG_CHANGE_TIME_ACC);
    bool chg_mod = HAS_FLAG(config.change_time_flags, FLAG_CHANGE_TIME_MOD);

    SetFileTime(
        file_handle,
        NULL,
        chg_acc ? &file_time : &ft_preserved,
        chg_mod ? &file_time : &ft_preserved
    );
}

/*!
 * @brief
 * Sets the file timestamp from a reference file.
 * 
 * @param file_handle
 * An open handle to the file to set its timestamp.
 */
void set_file_timestamp_from_ref(HANDLE file_handle) {
    bool chg_acc = HAS_FLAG(config.change_time_flags, FLAG_CHANGE_TIME_ACC);
    bool chg_mod = HAS_FLAG(config.change_time_flags, FLAG_CHANGE_TIME_MOD);

    if (config.time_offset != 0) {
        if (chg_acc && chg_mod) {
            adjust_time_offset(&config.ref_stamps->access_time, config.time_offset);
            adjust_time_offset(&config.ref_stamps->modified_time, config.time_offset);
        } else if (chg_acc) {
            adjust_time_offset(&config.ref_stamps->access_time, config.time_offset);
        } else {
            adjust_time_offset(&config.ref_stamps->modified_time, config.time_offset);
        }
    }

    SetFileTime(
        file_handle,
        NULL,
        chg_acc ? &config.ref_stamps->access_time : &ft_preserved,
        chg_mod ? &config.ref_stamps->modified_time : &ft_preserved
    );
}

/*!
 * @brief
 * Changes the timestamp of the given file.
 * 
 * @param filename
 * Path to the file to touch.
 */
bool touch(const TCHAR *filename, bool follow_symlinks) {
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

    if (config.ref_stamps) {
        set_file_timestamp_from_ref(file_handle);
    } else {
        set_file_timestamp_from_input(file_handle);
    }

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
    TCHAR *stamp_ref_filename = NULL;

    console_init(&console);

    if (argc < 2) {
        printf_error("%s: No argument is supplied.\n", prog_name);

        status_ok = false;
        goto clean_exit;
    }

    int option;
    while ((option = get_opt(argc, argv, _T("A:acdhmr:t:v"))) != -1) {
        switch (option) {
            case 'A':
                hhmmss_adjustment = opt_arg;
                break;
            case 'a':
                config.change_time_flags |= FLAG_CHANGE_TIME_ACC;
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
                config.change_time_flags |= FLAG_CHANGE_TIME_MOD;
                break;
            case 'r':
                stamp_ref_filename = opt_arg;
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

    // Activate both flags if none are specified in options
    if (config.change_time_flags == 0) {
        config.change_time_flags = (FLAG_CHANGE_TIME_MOD | FLAG_CHANGE_TIME_ACC);
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
    if (stamp_input && stamp_ref_filename) {
        printf_error("%s: Cannot set timestamp from multiple sources.\n", prog_name);

        status_ok = false;
        goto clean_exit;
    }

    // Didn't receive any files to touch
    if (opt_index == argc) {
        printf_error("%s: Missing file operand.\n", prog_name);

        status_ok = false;
        goto clean_exit;
    }

    // Process user-provided timestamp
    if (stamp_input) {
        config.custom_stamp = parse_timestamp_string(stamp_input);
        if (!config.custom_stamp) {
            printf_error("%s: Timestamp does not respect format.\n", prog_name);

            status_ok = false;
            goto clean_exit;
        }
    }

    // Process file-referenced timestamp
    if (stamp_ref_filename) {
        config.ref_stamps = get_ref_timestamp(stamp_ref_filename);
        if (!config.ref_stamps) {
            printf_error("%s: Reference timestamp could not be set.\n", prog_name);

            status_ok = false;
            goto clean_exit;
        }
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

    if (config.custom_stamp) {
        free(config.custom_stamp);
    }

    int exit_code = status_ok ? EXIT_SUCCESS : EXIT_FAILURE;
    return exit_code;
}