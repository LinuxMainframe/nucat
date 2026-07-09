#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>


// nucat CLI tool
// mimicks the real CLI tool 'cat'
// v0.0.4
// Author: Aidan A. Bradley
//
// Only handles single file "catenation"
// usage:
//      >> ./cat <PATH/TO/FILE>
//
// Changelog:
//      v0.0.1 - Initial code with error handling
//      v0.0.2 - Swapped from STDOUT to STDERR streaming
//      v0.0.3 - Condensed if statement chain
//      v0.0.4 - Condensed further and removed if blocks
//
int main(int argc, char *argv[]) {
    // Check to see if the caller passed the PATH as an argument
    if (argc < 2) {
        char *msg = "PATH missing from command line arguments.\n";
        write(2, "cat: ERROR: ", 12);
        write(2, msg, strlen(msg));
        return 1;
    }

    // Attempt an open() syscall
    int fd = open(argv[1], O_RDONLY);
    
    // If there is an error, handle it here
    if (fd == -1) {

        int op_err = errno;
        write(2, "cat: ERROR: ", 12);

        char *msg = strerror(op_err);
        write(2, msg, strlen(msg));
        write(2, "\n", 1);

        return 1;
    }

    // If we get a positive non-zero FD, move forwards with reading
    char buffer[4096];
    ssize_t lnread = 4096;

    while (lnread > 0) {
        // attempt a read
        lnread = read(fd, buffer, 4096);

        // if it works, then go ahead and read until we reach EOF
        if (lnread > 0) {
            write(1, buffer, lnread);
        }
    } 
    if (lnread < 0) {
        // otherwise we need to handle the error
        int rd_err = errno;

        char *msg = "cat: ERROR: ";
        write(2, msg, strlen(msg));
        msg = strerror(rd_err);
        write(2, msg, strlen(msg));
        write(2, "\n", 1);

        close(fd);
        return 1; // Return failure to the OS!
    }

    close(fd);
    return 0;
}