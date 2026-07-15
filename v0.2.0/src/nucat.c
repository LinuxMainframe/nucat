#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "argparse.h"

// nucat
// A new CLI tool, not to replace cat, but to do something different
// v0.2.0
// Author: Aidan A. Bradley
//
// usage:
//      >> ./nucat [-h] [-v] [-strict|-x] [-n [targets]] [-s <lines> [targets]] PATH...
//
// Style note: AppConfig-wide flags (-h, -v, -strict/-x) don't need any
// particular order to function correctly, but favor writing them before
// the per-file flags (-n, -s) in a command line — it reads more clearly
// as "tool-wide settings, then per-file settings."
//
// Changelog:
//      v0.0.1 - v0.0.5  See prior versions.
//      v0.1.0 - v0.2.0 Restructuring
//      v0.2.0 - Rebuilt on the new two-pass argparse system: strict
//               flags-before-files grammar, bracket-based per-file
//               targeting, -n and -s may now be combined (seeking still
//               filters output to matched lines; -n adds the prefix only
//               to lines that are actually shown).

int main(int argc, char *argv[]) {
    AppConfig config;
    FileConfig fileconfs[argc]; // upper bound: num_files can never exceed argc
    int num_fileconfs;
    ErrorLog log;

    int rc = argument_parse(argc, argv, &config, fileconfs, &num_fileconfs, &log);
    if (rc != 0) {
        print_error_log(&log);
        return 1;
    }

    if (config.help_requested) {
        printf(
            "nucat v0.2.0 — a targeted file-reading tool, not a cat replacement\n"
            "\n"
            "USAGE:\n"
            "  nucat [flags] PATH [PATH...]\n"
            "\n"
            "  Flags must come before all file paths. Nothing after the first\n"
            "  file path is treated as a flag, even if it starts with '-'.\n"
            "\n"
            "FLAGS:\n"
            "  -h, --help      Show this help message and exit.\n"
            "  -v, --verbose   Print extra diagnostic info before running.\n"
            "  -strict, -x     Exit immediately on the first error of any kind,\n"
            "                  instead of collecting errors and continuing.\n"
            "  -n [targets]    Prefix output lines with their line number.\n"
            "                  With no bracket, applies to every file.\n"
            "  -s <lines> [targets]\n"
            "                  Show only the given line number(s), instead of\n"
            "                  the whole file. With no bracket, applies to\n"
            "                  every file (same as -n's default behavior).\n"
            "\n"
            "TARGETING FILES WITH [brackets]:\n"
            "  Files are numbered by their position on the command line,\n"
            "  starting at 1. A bracket after a flag limits that flag to\n"
            "  specific files: [1], [2,3], etc.\n"
            "\n"
            "  -s accepts either multiple lines for ONE file, or one line for\n"
            "  MULTIPLE files — never both at once:\n"
            "    -s 13,40 [1]      OK   (two lines, one file)\n"
            "    -s 13 [1,2]       OK   (one line, two files)\n"
            "    -s 13,40 [1,2]    ERROR (ambiguous — pick one form)\n"
            "\n"
            "  This still applies with NO bracket at all: 'no bracket' means\n"
            "  'every file', so multiple -s values with more than one file\n"
            "  on the command line is the same ambiguity. Add a bracket to\n"
            "  disambiguate in that case.\n"
            "\n"
            "EXAMPLES:\n"
            "  nucat file.txt\n"
            "      Plain output, same as cat.\n"
            "\n"
            "  nucat -n file.txt\n"
            "      Every line prefixed with its line number.\n"
            "\n"
            "  nucat -s 42 [1] file.txt\n"
            "      Print only line 42.\n"
            "\n"
            "  nucat -n -s 572 file.txt\n"
            "      No bracket needed with a single file — -s applies\n"
            "      globally here, same as -n's default behavior.\n"
            "\n"
            "  nucat -n -s 42 [1] file.txt\n"
            "      Print only line 42, with its real line number prefixed.\n"
            "\n"
            "  nucat -n [1] -s 10 [2] file1.txt file2.txt\n"
            "      file1.txt fully numbered; file2.txt shows only line 10.\n"
            "\n"
            "  nucat -strict -s 5,5 [1] file.txt\n"
            "      Duplicate seek value — exits immediately with an error\n"
            "      instead of skipping the file and continuing.\n"
        );
        return 0;
    }

    if (config.verbose_mode) {
        printf("  [DEBUG] Verbose reporting initialized.\n\n");
    }

    if (num_fileconfs < 1) {
        fprintf(stderr, "nucat: Error: no PATH(s) supplied!\n");
        return 1;
    }

    // Value-sanity check (non-positive / duplicate seek targets) before
    // touching the filesystem at all.
    flag_conflict_check(fileconfs, num_fileconfs, &log, &config);

    for (int f = 0; f < num_fileconfs; f++) {
        FileConfig *fc = &fileconfs[f];

        if (fc->invalid) {
            // Already reported by flag_conflict_check — skip entirely so
            // no partial/incorrect output is produced for this file.
            continue;
        }

        int fd = open(fc->path, O_RDONLY);
        if (fd == -1) {
            report_error(&log, &config, ERR_FILE_OPEN, -1, fc->path);
            continue; // move on to the next file
        }

        int buffsize = 4096;
        char buffer[buffsize];
        ssize_t lnread;

        int start_of_line = 1;
        int line_number = 1;
        bool show_line = true; // recomputed once per line, at start_of_line

        while ((lnread = read(fd, buffer, buffsize)) > 0) {
            for (ssize_t b = 0; b < lnread; b++) {
                if (start_of_line) {
                    if (fc->seek_count > 0) {
                        show_line = false;
                        for (int s = 0; s < fc->seek_count; s++) {
                            if (fc->seek[s] == line_number) {
                                show_line = true;
                                break;
                            }
                        }
                    } else {
                        show_line = true; // no seeking — every line shows
                    }

                    if (show_line && fc->line_numbering) {
                        printf("%6d ", line_number);
                        fflush(stdout); // force printf's buffer out before write() hits
                    }

                    start_of_line = 0;
                }

                if (show_line) {
                    write(1, &buffer[b], 1);
                }

                if (buffer[b] == '\n') {
                    start_of_line = 1;
                    line_number++;
                }
            }
        }

        if (lnread == -1) {
            report_error(&log, &config, ERR_FILE_READ, -1, fc->path);
        }

        close(fd);
    }

    if (log.count > 0) {
        print_error_log(&log);
        return 1;
    }

    return 0;
}
