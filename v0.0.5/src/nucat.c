#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>


// nucat
// A new CLI tool, not to replace cat, but to do something different
// v0.0.5
// Author: Aidan A. Bradley
//
// Handles single and multi-file output
// Does not yet handle command line flags
// usage:
//      >> ./cat <PATH/TO/FILE1> <PATH/TO/FILE2> ... <PATH/TO/FILE>
//
// Changelog:
//      v0.0.1 - Initial code with error handling
//      v0.0.2 - Swapped from STDOUT to STDERR streaming
//      v0.0.3 - Condensed if statement chain
//      v0.0.4 - Condensed further and removed if blocks
//      v0.0.5 - Added multifile support and robust error
//               handling
int main(int argc, char *argv[]) {

    // We need to keep track of the errors in the proper order
    // so we init an array with slots for CLI argmuent added to cat
    // hence the -1
    int errors[argc];
    errors[0] = 0; // Buffer the zeroth index because we capture that error separately
    char *msg; // placeholder for our messages
    int buffsize = 4096;
    int errcnt = 0;

    // Lets check to make sure we get more than one argument in the CLI
    // This does not need anything special, just ensure at least one
    // PATH is present in the command line arguments
    // (THE COMMAND ITSELF COUNTS AS ONE ARGUMENT)
    if (argc < 2) {
        msg = "cat: Error: no PATH(s) supplied!\n";
        write(2, msg, strlen(msg));
        return 1;
    }

    // So we can ignore the zeroth element since the command
    // lies in the zeroth index. And since we know that C
    // guarantees a NULL terminator, we can use that to find the end
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY); // Attempt the file opening

        // If there is an error:
        if (fd == -1) {
            errors[i] = errno; // Store the errno in the correct index
            errcnt++; // incrememnt error counter
                continue; // And move on, we will print these out later
        }

        // setup the storage buffer for the characters read
        char buffer[buffsize];
        // ensure we have a default for the amount of lines read, can change later
        ssize_t lnread = 4096;

        // while we are still pulling characters out
        while (lnread > 0) {
            lnread = read(fd, buffer, buffsize); // store the lines read and output to buffer

            // Now ensure we dont accidentally get a -1 from read(), which is unsigned so becomes max size buffer
            if (lnread == -1) {
                break;
            }
            write(1, buffer, lnread); // write buffer contents to STDOUT
        }

        // When we reach lnread <= 0:
        if (lnread == 0) {
            errors[i] = 0; // not an error
            close(fd); // We close if the NULL terminator is found
            continue;
        } else if (lnread == -1) {
            // we have reached an error
            errors[i] = errno; // store the error
            errcnt++; // increment error counter
            close(fd); // close the file
            continue; // move on
        }
    }

    // if errors found at all
    if (errcnt > 0) {
        for (int i = 1; i < argc; i++) {
            // iterate through the error messages
            if (errors[i] == 0) {
                continue; // dont say anything
            } else {
                msg = "cat: Error: ";
                write(2, msg, strlen(msg));
                msg = "command line argument \"";
                write(2, msg, strlen(msg));
                msg = argv[i];
                write(2, msg, strlen(msg));
                msg = "\" : ";
                write(2, msg, strlen(msg));
                msg = strerror(errors[i]);
                write(2, msg, strlen(msg));
                write(2, "\n", 1);
            }
        }
        return 1; // we found at least one error, so return 1 to let the kernel know whats up
    }
    return 0; // otherwise, tell the kernel everything went well

}