/* getopt.c
 * Copyright (C) 2023 Jad Altahan (https://github.com/xv)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "getopt.h"

int opt_index = 1;
int opt_error = 0;

TCHAR opt;
TCHAR *opt_arg = NULL;

int get_opt(int argc, TCHAR *const argv[], const TCHAR *opts) {
    static TCHAR *opts_remaining = _T("\0");
    if (!*opts_remaining) {
        if (opt_index >= argc || *(opts_remaining = argv[opt_index]) != '-') {
            return -1;
        }

        // We got --
        // We don't process long options in this function
        if (opts_remaining[0] == '-' && opts_remaining[1] == '-') {
            return '?';
        }

        opts_remaining++;
    }

    // Store the option character
    opt = *opts_remaining++;
    TCHAR *lookup = _tcschr(opts, opt);

    // Unrecognized option
    if (!lookup) {
        if (!*opts_remaining) {
            opt_index++;
        }

        if (*opts != ':') {
            opt_error = ERROR_ILLEGAL_OPT;
        }

        return '?';
    }

    // Not an option arg
    if (*++lookup != ':') {
        if (!*opts_remaining) {
            opt_index++;
        }
    } else {
        if (*opts_remaining) {
            // The option arg is provided to the option without whitespace
            // separating them. I.e., accepts '-xY' along with '-x Y'
            opt_arg = opts_remaining;
        } else if (argc <= ++opt_index /* arg option not provided */) {
            opt_error = ERROR_OPT_REQ_ARG;
            return '?';
        } else {
            // Disallow '-' for option args
            // XXX: What if an intended option arg is a '-' or starts with it?
            //      Disallowing it might not actually be in the best interest
            // if (argv[opt_index][0] == '-') {
            //     opt_error = ERROR_INVALID_OPT_ARG;
            //     return '?';
            // }

            opt_arg = argv[opt_index];
        }

        opt_index++;
        opts_remaining = _T("\0");
    }

    return opt;
}