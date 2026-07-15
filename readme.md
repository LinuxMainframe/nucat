# nucat

`nucat` is a targeted file-reading command-line utility written in C. Built as a low-level, high-performance exploration of direct system calls, it features a unique two-pass argument parser, bracket-based file targeting, and robust error tracking.

## Features

* **Targeted Line Seeking**: Use `-s` to output only specific line numbers from your files, avoiding unnecessary reads on large files[cite: 6].
* **Bracket-Based Targeting**: Limit line numbering (`-n`) or seeking (`-s`) to specific files using bracket syntax (e.g., `-s 12 [1] -n [2]` targets file 1 and file 2 differently)[cite: 6].
* **Deferred Error Log**: Collects, categorizes, and reports system and syntax errors in order of occurrence at termination[cite: 5, 6], unless run in `-strict` / `-x` mode for immediate exit[cite: 5].
* **Zero Dependencies**: Compiles directly against the standard library with no external dependencies; highly portable.

## Project Structure

```text
.
├── buildtargets           # Target triplets for cross-compilation
├── scripts/
│   └── compile_all.sh     # Automation script for building releases
├── v0.1.0/                # Legacy milestone
└── v0.2.0/                # Current stable release
    ├── build/             # Output binaries
    ├── headers/
    │   └── argparse.h     # Parser structures and definitions
    └── src/
        ├── argparse.c     # Two-pass validation & bracket logic
        └── nucat.c        # Core file I/O & output stream loop

```

## Quick Start

### Build

Compile the latest version:

```bash
gcc -Wall -Wextra v0.2.0/src/nucat.c v0.2.0/src/argparse.c -I v0.2.0/headers -o nucat

```

### Usage

```bash
# Line-number only the first file, but print only line 42 of the second file
./nucat -n [1] -s 42 [2] file1.txt file2.txt

```

---

### Author

**Aidan A. Bradley**

*July 2026*
