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
    {"-s",        FLAG_LINE_SEEK,    TYPE_INT},
    {"-strict",   FLAG_STRICT,       TYPE_TOGGLE},
    {"-x",        FLAG_STRICT,       TYPE_TOGGLE}
};

#define FLAG_TABLE_SIZE (sizeof(flag_table) / sizeof(flag_table[0]))

// - ERROR MESSAGE TABLE - //
// Designated initializers keep these tied to the enum name, not position,
// so reordering ErrorType later can't silently desync the messages.
static const char *error_messages[] = {
    [ERR_UNKNOWN_FLAG]         = "unrecognized flag",
    [ERR_MISSING_VALUE]        = "flag requires a value",
    [ERR_MISSING_BRACKET]      = "flag requires a bracketed target list",
    [ERR_EXTRA_TOKENS]         = "unexpected extra tokens after flag",
    [ERR_INVALID_TARGET]       = "bracket references a file index that doesn't exist",
    [ERR_FILE_OPEN]            = "could not open file",
    [ERR_FLAG_CONFLICT]        = "conflicting flags on the same file",
    [ERR_INVALID_VALUE]        = "value is not a valid comma-separated list of integers",
    [ERR_CONFLICTING_TARGETS]  = "cannot combine multiple values with multiple target files",
    [ERR_SEEK_LIMIT]           = "too many seek targets for this file",
    [ERR_TOOLWIDE_BRACKET]     = "this flag applies tool-wide and cannot take a bracketed target list",
    [ERR_INVALID_SEEK_VALUE]   = "seek line numbers must be 1 or greater",
    [ERR_DUPLICATE_SEEK]       = "the same line number was specified more than once for this file",
    [ERR_FILE_READ]            = "error reading file",
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
        // Value-flag: mandatory value, then an OPTIONAL bracket. With no
        // bracket, the value applies to every file — same "absent bracket
        // = global" rule as toggles, just shifted by one token since the
        // value itself is never optional.
        if (flag_index + 1 >= boundary_index) {
            return -1; // nothing follows the flag at all
        }
        if (argv[flag_index + 1][0] == '[') {
            return -1; // bracket found where the value should be
        }
        if (flag_index + 2 < boundary_index && argv[flag_index + 2][0] == '[') {
            consumed = 2; // value + bracket
        } else {
            consumed = 1; // value only, applies globally
        }
    }

    // For non-terminal flags, anything left over between what was consumed
    // and the next flag is unaccounted-for input — e.g. "-s 13 [1] extra_junk -v".
    // flag_index + consumed is the LAST token this occurrence uses, so the
    // next free slot is flag_index + consumed + 1, which must equal
    // boundary_index for a clean fit. For the terminal flag, boundary_index
    // is argc (includes files), so this check does not apply.
    if (enforce_exact_boundary && (flag_index + consumed + 1 != boundary_index)) {
        return -1;
    }

    return consumed;
}

// Wraps validate_flag_shape with a semantic check on top: shape alone
// can't tell you whether a bracket makes sense for this flag's PURPOSE.
// FLAG_HELP/FLAG_VERBOSE are tool-wide (AppConfig-level) and never target
// files, so a bracket on either is meaningless even if it's shaped fine.
//
// Returns 0 on success. Returns -1 on a shape failure (bad tokens).
// Returns -2 specifically when a tool-wide flag carries a bracket it
// shouldn't have — kept distinct from -1 so callers can report an
// accurate message instead of a generic "malformed" one.
int flag_validate(char *argv[], int flag_index, int boundary_index,
                   const FlagDefinition *flag, bool enforce_exact_boundary,
                   int *consumed_out) {
    int consumed = validate_flag_shape(argv, flag_index, boundary_index,
                                        flag, enforce_exact_boundary);
    if (consumed == -1) {
        return -1;
    }

    bool tool_wide = (flag->id == FLAG_HELP || flag->id == FLAG_VERBOSE
                       || flag->id == FLAG_STRICT);

    // Was a bracket actually present in this occurrence? For toggles,
    // consumed==1 means "bracket found" (0 = bare). For value-flags,
    // consumed==2 means "bracket found" (1 = value only, no bracket).
    bool bracket_present = (flag->type == TYPE_TOGGLE) ? (consumed == 1) : (consumed == 2);

    if (tool_wide && bracket_present) {
        return -2; // e.g. "-v [1,2]" — a global flag can't target files
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
        seg_start = (seg_end + 1 < inner_len) ? seg_end + 1 : inner_len;
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
// Step 1: locates and validates the terminal flag, computes file_offset.
// Step 2: prepopulates fileconfs[] with one default entry per file.
// Step 3: walks every flag (non-terminal via positionlist[], then the
//         terminal flag), validates it, parses its value/bracket content,
//         and mutates the targeted fileconfs[] entries accordingly.
int argument_parse(int argc, char *argv[], AppConfig *config,
                    FileConfig fileconfs[], int *num_fileconfs,
                    ErrorLog *log) {
    config->help_requested = false;
    config->verbose_mode = false;
    config->strict = false;

    log->count = 0;
    *num_fileconfs = 0;

    // --- Prescan: does -strict or -x appear anywhere in argv? ---
    // This runs before Step 1 specifically so that strict mode is already
    // active if Step 1's own terminal-flag validation hits an error —
    // otherwise "-strict" placed after the flag that errors would arrive
    // too late to take effect, and position would matter after all.
    // Step 3 still processes -strict/-x normally afterward (shape and
    // tool-wide-bracket validated like any other flag); this prescan only
    // exists to get the *timing* right, not to replace real validation.
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-strict") == 0 || strcmp(argv[i], "-x") == 0) {
            config->strict = true;
            break;
        }
    }

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
            report_error(log, config, ERR_MISSING_VALUE, last_flag, argv[last_flag]);
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
        fileconfs[i].invalid = false;
    }
    *num_fileconfs = num_files;

    // --- Step 3: walk every flag, parse bracket contents, mutate fileconfs[] ---
    // idx runs 0..entries inclusive: 0..entries-1 are the non-terminal flags
    // (positions from positionlist[]), and idx==entries is the terminal flag
    // (already shape-validated in Step 1, but re-run through flag_validate
    // here too for the semantic tool-wide/bracket check, which Step 1 never
    // performed).
    for (int idx = 0; idx <= entries; idx++) {
        int flag_index;
        int boundary_index;
        bool exact;

        if (idx < entries) {
            flag_index = positionlist[idx];
            boundary_index = (idx + 1 < entries) ? positionlist[idx + 1] : last_flag;
            exact = true;
        } else {
            if (last_flag == 0) {
                break; // no terminal flag exists — nothing left to process
            }
            flag_index = last_flag;
            boundary_index = argc;
            exact = false;
        }

        const FlagDefinition *flag = find_flag(argv[flag_index]);
        if (flag == NULL) {
            // find_flag_positions only checks for a leading '-', it never
            // validates the flag is real — this is the first point a
            // non-terminal flag's name actually gets checked.
            report_error(log, config, ERR_UNKNOWN_FLAG, flag_index, argv[flag_index]);
            continue;
        }

        int consumed;
        int flag_rc = flag_validate(argv, flag_index, boundary_index, flag, exact, &consumed);
        if (flag_rc == -2) {
            report_error(log, config, ERR_TOOLWIDE_BRACKET, flag_index, argv[flag_index]);
            continue;
        }
        if (flag_rc == -1) {
            report_error(log, config, ERR_MISSING_VALUE, flag_index, argv[flag_index]);
            continue;
        }

        // Tool-wide flags just set AppConfig — flag_validate already
        // guarantees no bracket was present for these, so there's no
        // file-targeting work to do.
        if (flag->id == FLAG_HELP) {
            config->help_requested = true;
            continue;
        }
        if (flag->id == FLAG_VERBOSE) {
            config->verbose_mode = true;
            continue;
        }
        if (flag->id == FLAG_STRICT) {
            config->strict = true; // already set by the prescan in most cases; idempotent
            continue;
        }

        // From here on, this is a per-file flag (FLAG_LINE_NUMBERS or
        // FLAG_LINE_SEEK) — figure out which files it targets.
        bool has_bracket = (flag->type == TYPE_TOGGLE) ? (consumed == 1) : (consumed == 2);

        int targetlist[MAX_TOKEN_LEN];
        int target_count;

        if (has_bracket) {
            const char *bracket_token = (flag->type == TYPE_TOGGLE)
                                             ? argv[flag_index + 1]
                                             : argv[flag_index + 2];
            target_count = parse_brackets(bracket_token, targetlist);
            if (target_count == -1) {
                report_error(log, config, ERR_INVALID_TARGET, flag_index, bracket_token);
                continue;
            }
        } else {
            // No bracket present — applies to every file (true for a bare
            // toggle, and now also for a value-flag with no bracket).
            target_count = num_files;
            for (int i = 0; i < num_files; i++) {
                targetlist[i] = i + 1;
            }
        }

        // Parse the value list, if this flag type carries one.
        int valuelist[MAX_TOKEN_LEN];
        int value_count = 0;

        if (flag->type != TYPE_TOGGLE) {
            const char *value_token = argv[flag_index + 1];
            value_count = parse_brackets(value_token, valuelist);
            if (value_count == -1) {
                report_error(log, config, ERR_INVALID_VALUE, flag_index, value_token);
                continue;
            }
        }

        // Rule: multiple values may target one file, OR one value may
        // target multiple files — never both in the same occurrence.
        if (value_count > 1 && target_count > 1) {
            report_error(log, config, ERR_CONFLICTING_TARGETS, flag_index, argv[flag_index]);
            continue;
        }

        // Range-check every target against the actual file count before
        // mutating anything — parse_brackets has no notion of num_files,
        // so this is the first point that check can happen.
        bool range_ok = true;
        for (int i = 0; i < target_count; i++) {
            if (targetlist[i] < 1 || targetlist[i] > num_files) {
                report_error(log, config, ERR_INVALID_TARGET, flag_index, argv[flag_index]);
                range_ok = false;
                break;
            }
        }
        if (!range_ok) {
            continue;
        }

        for (int i = 0; i < target_count; i++) {
            if (fileconf_mutate(fileconfs, targetlist[i], flag, valuelist, value_count) == -1) {
                report_error(log, config, ERR_SEEK_LIMIT, flag_index, argv[flag_index]);
            }
        }
    }

    return 0;
}

// Value-sanity check over every prepared FileConfig. Checks each file's
// seek[] independently: every value must be >= 1 (there is no line 0),
// and no value may repeat within the same file's list. On any violation,
// that specific file is marked invalid (fc->invalid = true) so main()
// can skip reading it while still processing every other file normally —
// one bad file's config should not block correct output from the rest.
// (In strict mode, report_error itself exits immediately on the first
// violation, so this per-file marking only matters in non-strict mode.)
// Every violation found is still reported, not just the first.
// Returns 0 if no violations were found, -1 if at least one was reported.
int flag_conflict_check(FileConfig fileconfs[], int num_fileconfs,
                         ErrorLog *log, const AppConfig *config) {
    int had_error = 0;

    for (int i = 0; i < num_fileconfs; i++) {
        FileConfig *fc = &fileconfs[i];

        for (int s = 0; s < fc->seek_count; s++) {
            if (fc->seek[s] < 1) {
                report_error(log, config, ERR_INVALID_SEEK_VALUE, -1, fc->path);
                fc->invalid = true;
                had_error = 1;
            }

            for (int t = s + 1; t < fc->seek_count; t++) {
                if (fc->seek[t] == fc->seek[s]) {
                    report_error(log, config, ERR_DUPLICATE_SEEK, -1, fc->path);
                    fc->invalid = true;
                    had_error = 1;
                }
            }
        }
    }

    return had_error ? -1 : 0;
}
