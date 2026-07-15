#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "argparse.h"

// ARGPARSER SETUP FOR NUCAT CMDLINE TOOL
// VERSION: v0.1.0
// AUTHOR: AIDAN A. BRADLEY
//         JULY 2026

// - FLAG DEFINITIONS - //
static const FlagDefinition flag_table[] = {
    {"-h",        FLAG_HELP,         TYPE_TOGGLE},
    {"--help",    FLAG_HELP,         TYPE_TOGGLE},
    {"-v",        FLAG_VERBOSE,      TYPE_TOGGLE},
    {"--verbose", FLAG_VERBOSE,      TYPE_TOGGLE},
    {"-n",        FLAG_LINE_NUMBERS, TYPE_TOGGLE},
    {"-s",        FLAG_LINE_SEEK,    TYPE_INT}
};

#define FLAG_TABLE_SIZE (sizeof(flag_table) / sizeof(flag_table[0]))

// - ERROR MESSAGE TABLE - //
// Designated initializers keep these tied to the enum name, not position,
// so reordering ErrorType later can't silently desync the messages.
static const char *error_messages[] = {
    [ERR_UNKNOWN_FLAG]    = "unrecognized flag",
    [ERR_MISSING_VALUE]   = "flag requires a value",
    [ERR_MISSING_BRACKET] = "flag requires a bracketed target list",
    [ERR_EXTRA_TOKENS]    = "unexpected extra tokens after flag",
    [ERR_INVALID_TARGET]  = "bracket references a file index that doesn't exist",
    [ERR_FILE_OPEN]       = "could not open file",
    [ERR_FLAG_CONFLICT]   = "conflicting flags on the same file",
};

// Looks up a flag definition by its literal text (e.g. "-n", "--help").
// Returns NULL if no match is found.
const FlagDefinition* find_flag(const char *arg) {
    for (size_t i = 0; i < FLAG_TABLE_SIZE; i++) {
        if (strcmp(flag_table[i].str, arg) == 0) {
            return &flag_table[i];
        }
    }
    return NULL;
}

// Finds every comma in `token`, recording each comma's index into
// `commalist`. Caller must size commalist to at least strlen(token),
// which is always a safe upper bound (every character could be a comma).
// Returns the number of commas found.
int find_commas(const char *token, int commalist[]) {
    int commasfound = 0;
    int length = (int)strlen(token);

    for (int i = 0; i < length; i++) {
        if (token[i] == ',') {
            commalist[commasfound] = i;
            commasfound++;
        }
    }
    return commasfound;
}

// Scans argv (skipping argv[0]) and records the number of non-flag tokens
// found between each pair of adjacent flags into spanlist. The span for
// the terminal flag (the last one found) is NOT recorded here — that is
// handled separately by validate_flag_shape, since a terminal flag's true
// consumption can't be determined by counting alone (it must be validated
// against expected shape, bounded by argc rather than another flag).
//
// *entries is set to the number of spans written into spanlist.
// Returns the argv index of the last (terminal) flag found, or 0 if none.
int find_span(char *argv[], int argc, int spanlist[], int *entries) {
    int entry = 0;
    int span = 0;
    int last_flag = 0;
    bool seen_flag = false;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (seen_flag) {
                spanlist[entry] = span;
                entry++;
            }
            span = 0;
            seen_flag = true;
            last_flag = i;
        } else {
            span++;
        }
    }

    *entries = entry;
    return last_flag;
}

// Validates the token shape of ONE flag occurrence, searching only within
// [flag_index, boundary_index). boundary_index is the index of the NEXT
// flag for a non-terminal flag, or argc for the terminal flag — this is
// the generalization that replaces the old parse_terminal_flag: the same
// toggle/value logic applies either way, only the search ceiling differs.
//
// Returns the number of tokens consumed (0, 1, or 2) on success.
// Returns -1 on any malformed shape (missing value, missing mandatory
// bracket, or leftover tokens beyond what the flag actually consumes).
int validate_flag_shape(char *argv[], int flag_index, int boundary_index,
                         const FlagDefinition *flag) {
    int consumed;

    if (flag->type == TYPE_TOGGLE) {
        // Toggle: optional bracket immediately after, nothing else.
        if (flag_index + 1 < boundary_index && argv[flag_index + 1][0] == '[') {
            consumed = 1;
        } else {
            consumed = 0;
        }
    } else {
        // Value-flag: mandatory value, then mandatory bracket.
        if (flag_index + 1 >= boundary_index) {
            return -1; // nothing follows the flag at all
        }
        if (argv[flag_index + 1][0] == '[') {
            return -1; // bracket found where the value should be
        }
        if (flag_index + 2 >= boundary_index || argv[flag_index + 2][0] != '[') {
            return -1; // value found, but no mandatory bracket after it
        }
        consumed = 2;
    }

    // Anything left over between what was consumed and the boundary is
    // unaccounted-for input — e.g. "-s 13 [1] extra_junk" before the next flag.
    if (flag_index + consumed != boundary_index) {
        return -1;
    }

    return consumed;
}

// Records an error. In strict mode, prints immediately to stderr and exits
// the program with status 1. Otherwise, appends to the log for the caller
// to report later (see print_error_log), so parsing/processing can continue
// and every file gets a fair attempt regardless of earlier failures.
void report_error(ErrorLog *log, const AppConfig *config,
                   ErrorType type, int argv_index, const char *token) {

    if (config->strict) {
        fprintf(stderr, "nucat: fatal: %s", error_messages[type]);
        if (token != NULL) {
            fprintf(stderr, " ('%s')", token);
        }
        fprintf(stderr, "\n");
        exit(1);
    }

    if (log->count >= MAX_ERRORS) {
        return; // cap reached; drop further errors silently
    }

    ErrorEntry *entry = &log->entries[log->count];
    entry->type = type;
    entry->argv_index = argv_index;

    if (token != NULL) {
        strncpy(entry->token, token, MAX_TOKEN_LEN - 1);
        entry->token[MAX_TOKEN_LEN - 1] = '\0'; // strncpy doesn't guarantee this
    } else {
        entry->token[0] = '\0';
    }

    log->count++;
}

// Prints every collected error in the order they were recorded.
void print_error_log(const ErrorLog *log) {
    for (int i = 0; i < log->count; i++) {
        const ErrorEntry *e = &log->entries[i];
        fprintf(stderr, "nucat: %s", error_messages[e->type]);
        if (e->token[0] != '\0') {
            fprintf(stderr, " ('%s')", e->token);
        }
        if (e->argv_index >= 0) {
            fprintf(stderr, " [arg %d]", e->argv_index);
        }
        fprintf(stderr, "\n");
    }
}

// Full two-pass argument parser. Populates *config, fills fileconfs[] (up
// to *num_fileconfs entries), and records any errors into *log.
//
// TODO: this is currently a skeleton. The pieces below are the ones this
// session has designed and verified in isolation (find_span,
// validate_flag_shape, report_error) — what's still unwritten is the loop
// that ties them together: walking each flag via the spans/boundaries,
// calling validate_flag_shape per flag, breaking bracket contents apart
// with find_commas + strtol, and writing/merging into fileconfs[] by
// file_no. That orchestration is intentionally left for you to build next,
// since the bracket-to-FileConfig wiring hasn't been designed in code yet.
int argument_parse(int argc, char *argv[], AppConfig *config,
                    FileConfig fileconfs[], int *num_fileconfs,
                    ErrorLog *log) {
    config->help_requested = false;
    config->verbose_mode = false;
    config->strict = false;

    log->count = 0;
    *num_fileconfs = 0;

    // TODO: first pass — find_span() to get spanlist/entries/last_flag,
    //       then walk each non-terminal flag using its neighbor's index
    //       as boundary_index, plus the terminal flag with boundary=argc.
    // TODO: second pass — for each validated flag, parse its bracket
    //       contents (find_commas + strtol per segment), then create or
    //       merge into the matching FileConfig by file_no.

    return 0;
}