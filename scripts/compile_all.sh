#!/usr/bin/env bash

# Exit immediately if a command exits prematurely
set -e

# Define the directories
SRC_FILE="./v0.0.5/src/nucat.c"
BUILD_DIR="./v0.0.5/build"

# Ensure the build directory exists
mkdir -p "$BUILD_DIR"

if [ ! -f "buildtargets" ]; then
    echo "Error: 'buildtargets' file not found in the specified directory."
    exit 1
fi

# --- DEPENDENCY PRE-CHECK & APT PROMPT GENERATION ---
declare -A COMPILER_PKGS
COMPILER_PKGS["musl-gcc"]="musl-tools"
COMPILER_PKGS["aarch64-linux-gnu-gcc"]="gcc-aarch64-linux-gnu"
COMPILER_PKGS["arm-linux-gnueabihf-gcc"]="gcc-arm-linux-gnueabihf"
COMPILER_PKGS["x86_64-w64-mingw32-gcc"]="gcc-mingw-w64-x86-64"

MISSING_PKGS=""

# Scan through the expected compilers to see what is missing
for comp in "${!COMPILER_PKGS[@]}"; do
    if ! command -v "$comp" >/dev/null 2>&1; then
        MISSING_PKGS="$MISSING_PKGS ${COMPILER_PKGS[$comp]}"
    fi
done

# If packages are missing, give the user a clear copy-paste prompt
if [ -n "$MISSING_PKGS" ]; then
    echo "Warning: Missing Cross-Compilation Toolchains Detected!"
    echo "To install all necessary tools for your target matrix, run this command:"
    echo ""
    echo "    sudo apt update && sudo apt install -y$MISSING_PKGS"
    echo ""
    echo "------------------------------------------------------"
fi

echo "Starting automated compilation across target matrix..."
echo "------------------------------------------------------"

# Read buildtargets line by line
while IFS= read -r triplet || [ -n "$triplet" ]; do
    # Skip empty lines or comments
    [[ -z "$triplet" || "$triplet" =~ ^# ]] && continue

    # Strip any trailing carriage returns
    triplet=$(echo "$triplet" | tr -d '\r')

    # Determine the compiler and output binary name based on the triplet
    case "$triplet" in
        "x86_64-unknown-linux-gnu")
            COMPILER="gcc"
            OUT_BIN="nucat-linux-x86_64"
            ;;
        "x86_64-unknown-linux-musl")
            COMPILER="musl-gcc"
            OUT_BIN="nucat-linux-musl-x86_64"
            ;;
        "aarch64-unknown-linux-gnu")
            COMPILER="aarch64-linux-gnu-gcc"
            OUT_BIN="nucat-linux-arm64"
            ;;
        "armv7-unknown-linux-gnueabihf")
            COMPILER="arm-linux-gnueabihf-gcc"
            OUT_BIN="nucat-linux-armhf"
            ;;
        "x86_64-pc-windows-gnu")
            COMPILER="x86_64-w64-mingw32-gcc"
            OUT_BIN="nucat-windows-x86_64.exe"
            ;;
        *)
            # Silently skip unmapped entries to prevent terminal noise
            continue
            ;;
    esac

    echo "Processing target: $triplet"

    # Check if the required cross-compiler toolchain is installed
    if ! command -v "$COMPILER" >/dev/null 2>&1; then
        echo "   Skipping: '$COMPILER' is not installed."
        echo "-------------------------------------------------------"
        continue
    fi

    # Compile the binary
    echo "     Compiling with $COMPILER..."
    
    # Static linking flag adjustment for musl or standard environments
    if [ "$COMPILER" = "musl-gcc" ]; then
        $COMPILER -O2 -Wall -Wextra -static "$SRC_FILE" -o "$BUILD_DIR/$OUT_BIN"
    else
        $COMPILER -O2 -Wall -Wextra "$SRC_FILE" -o "$BUILD_DIR/$OUT_BIN"
    fi

    echo "     Generated: $BUILD_DIR/$OUT_BIN"
    echo "-------------------------------------------------------"

done < buildtargets

echo "!!! Compilation cycle complete. Check '$BUILD_DIR' for outputs."