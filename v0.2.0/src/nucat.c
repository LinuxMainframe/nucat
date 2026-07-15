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
//      >> ./nucat [-h] [-v] [-n [targets]] [-s <lines> [targets]] PATH...
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
        printf("Usage: nucat [-h] [-v] [-n [targets]] [-s <lines> [targets]] PATH...\n");
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