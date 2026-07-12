#ifndef ARGPARSE_H
#define ARGPARSE_H

#include <stddef.h> // For size_t

// Types and Templates here (No memory allocation yet)

// Unique IDs for every single flag/configuration rule
typedef enum {
    FLAG_UNKNOWN = 0,
    FLAG_HELP,
    FLAG_VERBOSE,
    FLAG_LINE_NUMBERS,
    FLAG_REVEAL_HIDDEN
} FlagId;

// Flags can behave differently: some are simple toggles, others require an argument string
typedef enum {
    KIND_TOGGLE,   // Example: -v or --FDISABLE_LOGGING
    KIND_VALUE     // Example: -o <path>
} FlagKind;

// The blueprint for defining new options
typedef struct {
    const char *str;   // The text match (e.g., "-o" or "--help")
    FlagId id;         // The unique uint-adjacent enum ID
    FlagKind kind;     // Does it require a trailing value?
} FlagDefinition;

typedef struct {
    int help_requested;
    int verbose_enabled;
    int line_numbering;
    int reveal_hidden;
} AppConfig;

// Public function blueprints

int parse_arguments(int argc, char *argv[], AppConfig *config);

#endif // ARGPARSE_H