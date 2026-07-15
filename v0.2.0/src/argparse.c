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

// Scans argv (skipping argv[0]) and records the argv index of every
// non-terminal flag found into positionlist. Unlike the old span-counting
// approach, a flag's own position is known the instant it's found —
// no deferred write needed, since we're not measuring a gap to the next
// flag, just recording where we currently stand.
//
// The terminal flag (the last one found) is deliberately NOT included in
// positionlist — same scoping as before: this function only locates
// flags, it never determines what a flag consumes. That's
// validate_flag_shape's job, called separately by the caller for both
// non-terminal flags (using the next flag's position as boundary) and
// the terminal flag (using argc as boundary).
//
// *entries is set to the number of positions written into positionlist.
// Returns the argv index of the last (terminal) flag found, or 0 if none.
int find_flag_positions(char *argv[], int argc, int positionlist[], int *entries) {
    int entry = 0;
    int last_flag = 0;
    bool seen_flag = false;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (seen_flag) {
                // The PREVIOUS flag we saw is now known to be non-terminal
                // (since we just found one after it) — record its position.
                positionlist[entry] = last_flag;
                entry++;
            }
            seen_flag = true;
            last_flag = i;
        }
    }

    *entries = entry;
    return last_flag;
}

// Validates the token shape of ONE flag occurrence, searching only within
// [flag_index, boundary_index). For a non-terminal flag, boundary_index is
// the index of the NEXT flag, and must be reached EXACTLY (nothing can be
// left over between this flag's consumption and the next flag). For the
// terminal flag, boundary_index is argc — but argc includes the files that
// follow, so it is only a search ceiling here, not something the flag's
// consumption must reach exactly. enforce_exact_boundary distinguishes
// these two cases; pass true for non-terminal flags, false for the
// terminal flag.
//
// Returns the number of tokens consumed (0, 1, or 2) on success.
// Returns -1 on any malformed shape (missing value, missing mandatory
// bracket, or — for non-terminal flags only — leftover unconsumed tokens).
int validate_flag_shape(char *argv[], int flag_index, int boundary_index,
                         const FlagDefinition *flag, bool enforce_exact_boundary) {
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

    // For non-terminal flags, anything left over between what was consumed
    // and the next flag is unaccounted-for input — e.g. "-s 13 [1] extra_junk -v".
    // For the terminal flag, boundary_index is argc (includes files), so
    // this check does not apply.
    if (enforce_exact_boundary && (flag_index + consumed != boundary_index)) {
        return -1;
    }

    return consumed;
}

// Wraps validate_flag_shape with a semantic check on top: shape alone
// can't tell you whether a bracket makes sense for this flag's PURPOSE.
// FLAG_HELP/FLAG_VERBOSE are tool-wide (AppConfig-level) and never target
// files, so a bracket on either is meaningless even if it's shaped fine.
int flag_validate(char *argv[], int flag_index, int boundary_index,
                   const FlagDefinition *flag, bool enforce_exact_boundary,
                   int *consumed_out) {
    int consumed = validate_flag_shape(argv, flag_index, boundary_index,
                                        flag, enforce_exact_boundary);
    if (consumed == -1) {
        return -1;
    }

    bool tool_wide = (flag->id == FLAG_HELP || flag->id == FLAG_VERBOSE);

    // Was a bracket actually present in this occurrence? For toggles,
    // consumed==1 means "bracket found". Value-flags always require a
    // bracket to succeed at all (validate_flag_shape enforces that), so
    // reaching here with a value-flag means a bracket is guaranteed present.
    bool bracket_present = (flag->type == TYPE_TOGGLE) ? (consumed == 1) : true;

    if (tool_wide && bracket_present) {
        return -1; // e.g. "-v [1,2]" — a global flag can't target files
    }

    *consumed_out = consumed;
    return 0;
}

// Parses a comma-separated list of integers out of `token`. Strips a
// leading '[' and trailing ']' if present, so the same function serves
// both bracket contents ("[2,3]") and bare value lists ("13,40").
int parse_brackets(const char *token, int list[]) {
    int len = (int)strlen(token);
    int start = 0;
    int end = len;

    if (len > 0 && token[0] == '[') {
        start++;
    }
    if (len > 0 && token[len - 1] == ']') {
        end--;
    }

    int inner_len = end - start;
    if (inner_len <= 0 || inner_len >= MAX_TOKEN_LEN) {
        return -1; // empty ("[]"), reversed, or too long to safely buffer
    }

    char buffer[MAX_TOKEN_LEN];
    strncpy(buffer, token + start, inner_len);
    buffer[inner_len] = '\0';

    int commalist[MAX_TOKEN_LEN]; // inner_len < MAX_TOKEN_LEN, safe upper bound
    int commasfound = find_commas(buffer, commalist);

    int count = 0;
    int seg_start = 0;
    for (int i = 0; i <= commasfound; i++) {
        int seg_end = (i < commasfound) ? commalist[i] : inner_len;
        int seg_len = seg_end - seg_start;

        if (seg_len <= 0 || seg_len >= 64) {
            return -1; // empty segment (e.g. "1,,3") or absurdly long one
        }

        char segbuf[64];
        strncpy(segbuf, buffer + seg_start, seg_len);
        segbuf[seg_len] = '\0';

        char *endptr;
        long val = strtol(segbuf, &endptr, 10);
        if (endptr == segbuf || *endptr != '\0') {
            return -1; // segment wasn't a clean integer
        }

        list[count] = (int)val;
        count++;
        seg_start = seg_end + 1;
    }

    return count;
}

// Applies flag's effect to a single file, identified by file_no (1-indexed,
// matching FileConfig.file_no — a direct index, no search needed thanks to
// the prepopulation in argument_parse's Step 2).
int fileconf_mutate(FileConfig fileconfs[], int file_no, const FlagDefinition *flag,
                     const int values[], int value_count) {
    if (file_no < 1) {
        return -1;
    }

    FileConfig *fc = &fileconfs[file_no - 1];

    switch (flag->id) {
        case FLAG_LINE_NUMBERS:
            fc->line_numbering = true;
            break;

        case FLAG_LINE_SEEK:
            for (int i = 0; i < value_count; i++) {
                if (fc->seek_count >= MAX_SEEKS) {
                    return -1; // seek[] capacity exceeded for this file
                }
                fc->seek[fc->seek_count] = values[i];
                fc->seek_count++;
            }
            break;

        default:
            return -1; // not a per-file flag — shouldn't reach here
    }

    return 0;
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
// TODO: Step 3 — walk every flag (non-terminal, then terminal), parse
// bracket contents with find_commas + strtol, and mutate the matching
// fileconfs[] entries by file_no. Not yet designed at the code level.
int argument_parse(int argc, char *argv[], AppConfig *config,
                    FileConfig fileconfs[], int *num_fileconfs,
                    ErrorLog *log) {
    config->help_requested = false;
    config->verbose_mode = false;
    config->strict = false;

    log->count = 0;
    *num_fileconfs = 0;

    // --- Step 1: locate and validate the terminal flag ---
    int positionlist[argc];
    int entries;
    int last_flag = find_flag_positions(argv, argc, positionlist, &entries);
    int file_offset = 1;
    int num_files = 0;

    if (last_flag == 0) {
        // No flags at all — every argument past argv[0] is a file.
        file_offset = 1;
        num_files = argc - 1;
    } else {
        const FlagDefinition *terminal = find_flag(argv[last_flag]);
        if (terminal == NULL) {
            // Can't compute file_offset without knowing what the terminal
            // flag consumes — this is unrecoverable for this invocation.
            report_error(log, config, ERR_UNKNOWN_FLAG, last_flag, argv[last_flag]);
            return -1;
        }

        // boundary=argc is a search ceiling here, not an exact target —
        // files legitimately follow, so enforce_exact_boundary is false.
        int consumed = validate_flag_shape(argv, last_flag, argc, terminal, false);
        if (consumed == -1) {
            report_error(log, config, ERR_MISSING_BRACKET, last_flag, argv[last_flag]);
            return -1;
        }

        file_offset = last_flag + 1 + consumed;
        num_files = argc - file_offset;
    }

    // --- Step 2: prepopulate fileconfs[] with one default entry per file ---
    for (int i = 0; i < num_files; i++) {
        fileconfs[i].file_no = i + 1;
        fileconfs[i].path = argv[file_offset + i];
        fileconfs[i].line_numbering = false;
        fileconfs[i].seek_count = 0;
    }
    *num_fileconfs = num_files;

    // --- Step 3: walk every flag, parse bracket contents, mutate fileconfs[] ---
    // TODO: not yet designed at the code level — see comment above.

    return 0;
}