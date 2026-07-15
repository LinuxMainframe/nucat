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
    FLAG_LINE_SEEK
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
} FileConfig;

// - ERROR HANDLING - //

typedef enum {
    ERR_UNKNOWN_FLAG,
    ERR_MISSING_VALUE,
    ERR_MISSING_BRACKET,
    ERR_EXTRA_TOKENS,
    ERR_INVALID_TARGET,
    ERR_FILE_OPEN,
    ERR_FLAG_CONFLICT
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

int find_span(char *argv[], int argc, int spanlist[], int *entries);

int validate_flag_shape(char *argv[], int flag_index, int boundary_index,
                         const FlagDefinition *flag);

void report_error(ErrorLog *log, const AppConfig *config,
                   ErrorType type, int argv_index, const char *token);

void print_error_log(const ErrorLog *log);

int argument_parse(int argc, char *argv[], AppConfig *config,
                    FileConfig fileconfs[], int *num_fileconfs,
                    ErrorLog *log);

#endif // ARGPARSE_H