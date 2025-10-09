#!/bin/bash

# TCC Cross-Compilation Script
# Builds TCC cross-compilers for 7 target platforms using TCC's native cross-compilation

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TCC_SRC_DIR="$SCRIPT_DIR/tcc/src"
OUTPUT_DIR="$SCRIPT_DIR"

# Detect current system architecture for base compiler
ARCH="$(uname -m)"
case "$ARCH" in
    x86_64) ARCH="x86_64" ;;
    i386|i686) ARCH="x86_32" ;;
    aarch64|arm64) ARCH="arm_64" ;;
    armv7l) ARCH="arm_32" ;;
    *) echo "Warning: Unknown architecture $ARCH, using as-is" ;;
esac

TCC_COMPILER="$SCRIPT_DIR/tcc_${ARCH}.gcc"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

printf "${BLUE}=== TCC Cross-Compilation (Native Method) ===${NC}\n"
echo "Base compiler: $TCC_COMPILER"
echo "Output directory: $OUTPUT_DIR"
echo "Method: Using TCC's built-in cross-compilation with Makefile"
echo ""

# Verify base compiler exists
if [ ! -f "$TCC_COMPILER" ]; then
    printf "${RED}ERROR${NC}: Base TCC compiler not found at $TCC_COMPILER\n"
    printf "Please run ./build_tccx_gcc.sh first.\n"
    exit 1
fi

cd "$TCC_SRC_DIR"

# First, compile the CRT (C Runtime) files that are needed for linking
printf "${BLUE}Preparing CRT files...${NC}\n"
# Ensure lib directory exists
mkdir -p "$SCRIPT_DIR/tcc/lib"

# Check if CRT files exist in lib directory
if [ ! -f "$SCRIPT_DIR/tcc/lib/crt1.o" ] || [ ! -f "$SCRIPT_DIR/tcc/lib/crti.o" ] || [ ! -f "$SCRIPT_DIR/tcc/lib/crtn.o" ]; then
    printf "  Compiling CRT files to lib directory...\n"

    # Compile crt1.o directly to lib
    if ! "$TCC_COMPILER" -c crt1.c -o "$SCRIPT_DIR/tcc/lib/crt1.o"; then
        printf "  ${RED}ERROR${NC} - Failed to compile crt1.o\n"
        exit 1
    fi

    # Compile crti.o directly to lib
    if ! "$TCC_COMPILER" -c crti.c -o "$SCRIPT_DIR/tcc/lib/crti.o"; then
        printf "  ${RED}ERROR${NC} - Failed to compile crti.o\n"
        exit 1
    fi

    # Compile crtn.o directly to lib
    if ! "$TCC_COMPILER" -c crtn.c -o "$SCRIPT_DIR/tcc/lib/crtn.o"; then
        printf "  ${RED}ERROR${NC} - Failed to compile crtn.o\n"
        exit 1
    fi

    printf "  ${GREEN}SUCCESS${NC} - CRT files compiled to lib directory\n"
else
    printf "  ${GREEN}FOUND${NC} - CRT files already exist in lib directory\n"
fi

# Check and compile missing TCC runtime dependencies
printf "${BLUE}Preparing TCC runtime dependencies...${NC}\n"

# Check if runmain.o exists, if not compile it
if [ ! -f "$SCRIPT_DIR/tcc/lib/runmain.o" ]; then
    printf "  Compiling runmain.o...\n"
    if ! gcc -c "$SCRIPT_DIR/tcc/lib/runmain.c" -o "$SCRIPT_DIR/tcc/lib/runmain.o"; then
        printf "  ${RED}ERROR${NC} - Failed to compile runmain.o\n"
        exit 1
    fi
    printf "  ${GREEN}SUCCESS${NC} - runmain.o compiled\n"
else
    printf "  ${GREEN}FOUND${NC} - runmain.o already exists\n"
fi

# Check if libtcc1.a exists, if not compile it
if [ ! -f "$SCRIPT_DIR/tcc/lib/libtcc1.a" ]; then
    printf "  Compiling libtcc1.a runtime library...\n"
    # First compile libtcc1.o from source
    if ! gcc -c "$SCRIPT_DIR/tcc/src/lib/libtcc1.c" -o "$SCRIPT_DIR/tcc/lib/libtcc1.o"; then
        printf "  ${RED}ERROR${NC} - Failed to compile libtcc1.o\n"
        exit 1
    fi
    # Create static library
    if ! ar rcs "$SCRIPT_DIR/tcc/lib/libtcc1.a" "$SCRIPT_DIR/tcc/lib/libtcc1.o"; then
        printf "  ${RED}ERROR${NC} - Failed to create libtcc1.a\n"
        exit 1
    fi
    printf "  ${GREEN}SUCCESS${NC} - libtcc1.a runtime library created\n"
else
    printf "  ${GREEN}FOUND${NC} - libtcc1.a already exists\n"
fi

# Clean up any CRT files in src directory to avoid confusion
if [ -f "crt1.o" ] || [ -f "crti.o" ] || [ -f "crtn.o" ]; then
    printf "  Cleaning up duplicate CRT files from src directory...\n"
    rm -f crt1.o crti.o crtn.o
fi

# Build cross-compiler using manual compilation with correct target defines
build_cross_compiler() {
    local target="$1"
    local arch="$2"
    local bits="$3"
    local ext="$4"
    local description="$5"

    local output_name="tcc_${arch}.${ext}"

    printf "\n${YELLOW}Building $target ($description)...${NC}\n"

    # Use TCC's standard Makefile to build cross-compiler with system GCC
    if make "${target}-tcc" CC=gcc ONE_SOURCE=yes SILENT=yes 2>/dev/null; then

        # Copy the result to our output directory with standardized name
        if [ -f "${target}-tcc" ]; then
            cp "${target}-tcc" "$OUTPUT_DIR/$output_name"
            printf "  ${GREEN}SUCCESS${NC} - $output_name\n"

            # Show file info
            local file_size=$(ls -lh "$OUTPUT_DIR/$output_name" | awk '{print $5}')
            echo "    Size: $file_size"

            # Test the compiler
            if "$OUTPUT_DIR/$output_name" -v >/dev/null 2>&1; then
                printf "    ${GREEN}VERIFIED${NC} - Compiler is working\n"
            else
                printf "    ${YELLOW}WARNING${NC} - Compiler may not work properly\n"
            fi

            return 0
        else
            printf "  ${RED}FAILED${NC} - Output file ${target}-tcc not found\n"
            return 1
        fi
    else
        printf "  ${RED}FAILED${NC} - Compilation failed for target $target\n"
        return 1
    fi
}

# Main execution function
main() {
    echo "TCC source directory: $TCC_SRC_DIR"
    echo "Output directory: $OUTPUT_DIR"
    echo "Base architecture: $ARCH"
    echo ""

    # Define 7 target platforms according to TCC's Makefile
    # Format: make_target arch bits ext description
    local targets=(
        "x86_64|x86_64|64|lnx|Linux x86_64"
        "i386|x86_32|32|lnx|Linux x86_32"
        "x86_64-win32|x86_64|64|exe|Windows x86_64"
        "i386-win32|x86_32|32|exe|Windows x86_32"
        "arm64|arm_64|64|exe|ARM64"
        "x86_64-osx|x86_64|64|osx|macOS x86_64"
        "arm64-osx|arm_64|64|osx|macOS ARM64"
    )

    # Build cross-compilers
    local success_count=0
    local total_count=${#targets[@]}

    for i in "${!targets[@]}"; do
        local target_info="${targets[$i]}"
        IFS='|' read -r make_target arch bits ext description <<< "$target_info"

        printf "\n${YELLOW}[$(($i+1))/$total_count] Building $make_target...${NC}\n"

        if build_cross_compiler "$make_target" "$arch" "$bits" "$ext" "$description"; then
            ((success_count++))
        fi
    done

    # Final summary
    printf "\n${BLUE}=== TCC Cross-Compilation Results ===${NC}\n"
    printf "Success Rate: ${success_count}/${total_count}\n"
    echo ""

    if [ $success_count -gt 0 ]; then
        printf "${GREEN}Successfully compiled binaries:${NC}\n"
        ls -la "$OUTPUT_DIR"/tcc_*.{lnx,exe,osx} 2>/dev/null | head -10 || echo "  (No binaries found)"
        echo ""
    fi

    # Display CRT files location for debugging
    printf "\n${BLUE}CRT Files Status:${NC}\n"
    echo "Looking for CRT files in project..."
    find "$SCRIPT_DIR/tcc" -name "crt*.o" -type f 2>/dev/null | while read -r file; do
        echo "  Found: $file ($(ls -lh "$file" | awk '{print $5}'))"
    done || echo "  No CRT files found"
    echo ""

    if [ $success_count -eq $total_count ]; then
        printf "${GREEN}Complete success! All $total_count targets compiled.${NC}\n"
    elif [ $success_count -gt 0 ]; then
        printf "${YELLOW}Partial success: ${success_count}/${total_count} targets compiled.${NC}\n"
    else
        printf "${RED}No targets were successfully compiled!${NC}\n"
        exit 1
    fi

    echo ""
    printf "${GREEN}TCC cross-compilation completed!${NC}\n"
    echo "Output directory: $OUTPUT_DIR"
    echo "test tccx.sh -v"
    $OUTPUT_DIR/tccx.sh -v
}

# Execute main function
main "$@"
