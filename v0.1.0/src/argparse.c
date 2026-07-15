#include <string.h>
#include <stdio.h>
#include "argparse.h"

// ARGPARSER SETUP FOR NUCAT CMDLINE TOOL
// AUTHOR: AIDAN A. BRADLEY
// 			  JULY 2026

static const FlagDefinition flag_table[] = {
    {"-h",                FLAG_HELP,            KIND_TOGGLE},
    {"--help",            FLAG_HELP,            KIND_TOGGLE},
    {"-v",                FLAG_VERBOSE,         KIND_TOGGLE},
    {"--verbose",         FLAG_VERBOSE,         KIND_TOGGLE},
    {"-n",				  FLAG_LINE_NUMBERS,	KIND_TOGGLE},
    {"-A", 				  FLAG_REVEAL_HIDDEN,	KIND_TOGGLE}
};

#define FLAG_TABLE_SIZE (sizeof(flag_table) / sizeof(flag_table[0]))

// Private helper function to find a flag definition in our table
const FlagDefinition* find_flag(const char *arg) {
    for (size_t i = 0; i < FLAG_TABLE_SIZE; i++) {
        if (strcmp(flag_table[i].str, arg) == 0) {
            return &flag_table[i];
        }
    }
    return NULL; // Not found
}

// Private dispatcher
// Delegated role: Modifies application state based on the evaluated Flag ID
static void execute_flag_action(FlagId id, char *value, AppConfig *config) {
    switch (id) {
        case FLAG_HELP:				config->help_requested = 1;  break;
        case FLAG_VERBOSE: 			config->verbose_enabled = 1; break;
        case FLAG_LINE_NUMBERS: 	config->line_numbering = 1;  break;
        case FLAG_REVEAL_HIDDEN: 	config->reveal_hidden = 1; 	 break;
        default: 												 break;
    }
}

// Public facing engine
int parse_arguments(int argc, char *argv[], AppConfig *config) {
	// Init the struct safely to zero/NULL defaults
	config->help_requested 	= 0;
	config->verbose_enabled = 0;
	config->line_numbering 	= 0;
	config->reveal_hidden	= 0;

	for (int i = 1; i < argc; i++) {
		char *arg = argv[i];

		if (arg[0] == '-') {
			const FlagDefinition *flag = find_flag(arg);

			if (flag == NULL) {
				fprintf(stderr, "Error: Unknown option '%s'\n", arg);
				return -1;
			}

			char *associated_value = NULL;
			if (flag->kind == KIND_VALUE) {
				if (i + 1 < argc) {
					associated_value = argv[++i]; //increment before fetching argument string
				} else {
					fprintf(stderr, "Error: Option '%s' requires a value\n", flag->str);
					return -1;
				}
			}

			execute_flag_action(flag->id, associated_value, config);
		}
	}
	return 0;
}
