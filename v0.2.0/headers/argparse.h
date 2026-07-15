#ifndef ARGPARSE_H
#define ARGPARSE_H

#include <stdbool.h>

// ARGPARSER SETUP FOR NUCAT CMDLINE TOOL
// VERSION: v0.1.0
// AUTHOR: AIDAN A. BRADLEY
//         JULY 2026

// - LIMITS - //
#define MAX_SEEKS     18   // max seek line-targets stored per file
#define MAX_ERRORS    64   // max errors collected before non-strict logging stops
#define MAX_TOKEN_LEN 256  // max length of a token copied into an ErrorEntry

// - FLAG TYPES - //

// Unique ID for every distinct flag/configuration rule
typedef enum {
    FLAG_HELP,
    FLAG_VERBOSE,
    FLAG_LINE_NUMBERS,
    FLAG_LINE_SEEK,
    FLAG_STRICT
} FlagId;

// What shape of trailing value (if any) a flag expects
typedef enum {
    TYPE_TOGGLE,   // no value; optional bracket only
    TYPE_INT,      // expects a succeeding integer
    TYPE_STRING,   // expects a succeeding string
    TYPE_BOOL      // expects a succeeding boolean
} FlagType;

// Blueprint for defining a flag's text match, ID, and expected shape
typedef struct {
    const char *str;
    FlagId id;
    FlagType type;
} FlagDefinition;

// - CONFIG TYPES - //

// Tool-wide settings, not specific to any one file
typedef struct {
    bool help_requested;
    bool verbose_mode;
    bool strict;          // exit immediately on first error, of any severity
} AppConfig;

// Per-file settings, one instance per file (1:1 with num_files)
typedef struct {
    bool line_numbering;
    int  seek[MAX_SEEKS];
    int  seek_count;
    int  file_no;          // 1-indexed position among the files, per bracket refs
    const char *path;      // alias into argv — argv strings outlive main(), so no copy needed
    bool invalid;           // set by flag_conflict_check; main() must skip reading this file
} FileConfig;

// - ERROR HANDLING - //

typedef enum {
    ERR_UNKNOWN_FLAG,
    ERR_MISSING_VALUE,
    ERR_MISSING_BRACKET,
    ERR_EXTRA_TOKENS,
    ERR_INVALID_TARGET,
    ERR_FILE_OPEN,
    ERR_FLAG_CONFLICT,
    ERR_INVALID_VALUE,       // value token wasn't a clean comma-separated int list
    ERR_CONFLICTING_TARGETS, // multiple values AND multiple targets in one occurrence
    ERR_SEEK_LIMIT,           // a file's seek[] array is already full (MAX_SEEKS)
    ERR_TOOLWIDE_BRACKET,     // a tool-wide flag (-h/-v) was given a bracket
    ERR_INVALID_SEEK_VALUE,  // a seek line number was <= 0
    ERR_DUPLICATE_SEEK,      // the same line number appears twice for one file
    ERR_FILE_READ             // read() failed partway through a file
} ErrorType;

typedef struct {
    ErrorType type;
    int  argv_index;               // -1 if not applicable
    char token[MAX_TOKEN_LEN];     // offending token or path, copied verbatim
} ErrorEntry;

typedef struct {
    ErrorEntry entries[MAX_ERRORS];
    int count;
} ErrorLog;

// - PUBLIC FUNCTIONS - //

const FlagDefinition* find_flag(const char *arg);

int find_commas(const char *token, int commalist[]);

int find_flag_positions(char *argv[], int argc, int positionlist[], int *entries);

int validate_flag_shape(char *argv[], int flag_index, int boundary_index,
                         const FlagDefinition *flag, bool enforce_exact_boundary);

// Wraps validate_flag_shape with a semantic check: tool-wide flags
// (FLAG_HELP, FLAG_VERBOSE) cannot carry a bracket, since they don't
// target files. Writes the token-count consumed into *consumed_out on
// success. Returns 0 on success, -1 on a shape failure, -2 specifically
// when a tool-wide flag has a bracket it shouldn't.
int flag_validate(char *argv[], int flag_index, int boundary_index,
                   const FlagDefinition *flag, bool enforce_exact_boundary,
                   int *consumed_out);

// Parses a comma-separated list of integers from `token`, which may
// optionally be wrapped in brackets (e.g. "[2,3]" or bare "13,40" both
// work — brackets are stripped if present). Uses find_commas internally.
// Caller must size `list[]` to at least the number of values that could
// plausibly appear (e.g. strlen(token) is always a safe upper bound).
// Returns the count of integers parsed, or -1 on malformed input
// (non-numeric segment, empty segment, or token too long).
int parse_brackets(const char *token, int list[]);

// Applies one flag's effect to a single target file (by file_no, 1-indexed).
// Branches on flag->id: FLAG_LINE_NUMBERS sets the bool; FLAG_LINE_SEEK
// appends `values` into that file's seek[] array (bounds-checked against
// MAX_SEEKS). Called once per target file from Step 3's loop, not once
// per flag occurrence. Returns 0 on success, -1 on failure (unknown
// flag->id for this purpose, or seek[] capacity exceeded).
int fileconf_mutate(FileConfig fileconfs[], int file_no, const FlagDefinition *flag,
                     const int values[], int value_count);

void report_error(ErrorLog *log, const AppConfig *config,
                   ErrorType type, int argv_index, const char *token);

void print_error_log(const ErrorLog *log);

int argument_parse(int argc, char *argv[], AppConfig *config,
                    FileConfig fileconfs[], int *num_fileconfs,
                    ErrorLog *log);

// Value-sanity check over every prepared FileConfig: rejects non-positive
// seek line numbers (a file has no "line 0") and duplicate seek targets
// within the same file (e.g. "-s 13,13 [1]"). Reports each violation via
// report_error rather than returning early, so every file gets checked.
// Returns 0 if no violations were found, -1 if at least one was reported.
int flag_conflict_check(FileConfig fileconfs[], int num_fileconfs,
                         ErrorLog *log, const AppConfig *config);

#endif // ARGPARSE_H
