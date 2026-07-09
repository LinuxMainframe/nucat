# nucat

`nucat` is a command-line utility written in C for reading and concatenating file contents. It serves as a modern exploration of low-level system calls, featuring robust sequential error tracking and multi-architecture cross-compilation support.

## Features

* **Multi-File Processing**: Reads and outputs contents from single or multiple file paths sequentially.
* **Ordered Error Reporting**: Logs and stores stream or file access issues (`errno`) in their exact order of occurrence, outputting a summary to `STDERR` at termination.
* **Zero Dependencies**: Can be compiled statically via `musl-gcc` for maximum portability across Linux environments.

## Project Structure

```text
.
├── buildtargets           # List of target triplets for deployment
├── scripts/
│   └── compile_all.sh     # Automation script for cross-compilation
└── v0.0.5/
    ├── build/             # Output directory for compiled binaries
    └── src/
        └── nucat.c        # Core source code

```

### Author
#### Aidan A. Bradley
#### July 2026