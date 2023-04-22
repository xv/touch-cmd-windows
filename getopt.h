#ifndef GETOPT_H
#define GETOPT_H

#include <stddef.h>
#include <tchar.h>

#define ERROR_ILLEGAL_OPT 1
#define ERROR_OPT_REQ_ARG 2
#define ERROR_INVALID_OPT_ARG 3

// Stores of the index next option to be processed in argv
extern int opt_index;

// Sets an one of the ERROR_* codes if  an error is encountered
// during option parsing. The caller can use this code to display
// an appropriate message
extern int opt_error;

// Stores a character representing the last parsed option
extern TCHAR opt;

// If an option requires an argument, the parsed argument value
// will be stored here
extern TCHAR *opt_arg;

/*!
 * @brief
 * Parses the command-line arguments.
 *
 * @param argc
 * Number of command-line arguments passed to the program.
 * 
 * @param argv
 * Array of command-line arguments passed to the program.
 *
 * @param opts
 * String containing the options. Options that take an arguments should
 * be succeeded by a colon (:).
 * 
 * @returns
 * If an option was successfully parsed, the return values is the option
 * character; -1 otherwise.
 */
int get_opt(int argc, TCHAR *const argv[], const TCHAR *opts);

#endif // GETOPT_H