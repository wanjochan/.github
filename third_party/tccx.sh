#!/bin/bash
# tccx.sh - TCC Architecture-Aware Wrapper
# Automatically selects the correct TCC binary based on current architecture

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Detect current system architecture
detect_architecture() {
    local arch="$(uname -m)"
    case "$arch" in
        x86_64|amd64)
            echo "x86_64"
            ;;
        i386|i686|i486)
            echo "x86_32"
            ;;
        aarch64|arm64)
            echo "arm_64"
            ;;
        armv7*|armv6*)
            echo "arm_32"
            ;;
        *)
            echo "ERROR: Unsupported architecture: $arch" >&2
            exit 1
            ;;
    esac
}

# Detect operating system
detect_os() {
    local os="$(uname -s)"
    case "$os" in
        Linux)
            echo "lnx"
            ;;
        Darwin)
            echo "osx"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            echo "exe"
            ;;
        *)
            echo "ERROR: Unsupported operating system: $os" >&2
            exit 1
            ;;
    esac
}

# Main script logic
main() {
    # Detect architecture and OS
    ARCH=$(detect_architecture)
    OS=$(detect_os)
    
    # Construct TCC binary name based on actual naming convention
    # Map OS to file extension
    case "$OS" in
        lnx)
            TCC_BINARY="tcc_${ARCH}.lnx"
            TCC_BINARY="tcc_${ARCH}.gcc"
            ;;
        osx)
            TCC_BINARY="tcc_${ARCH}.osx"
            ;;
        exe)
            TCC_BINARY="tcc_${ARCH}.exe"
            ;;
        *)
            echo "ERROR: Unsupported operating system: $OS" >&2
            exit 1
            ;;
    esac
    TCC_PATH="$SCRIPT_DIR/$TCC_BINARY"
    
    # Debug output if -v flag is used
    if [[ " $@ " =~ " -v " ]] || [[ " $@ " =~ " --version " ]]; then
        echo "# tccx.sh: Selected $TCC_BINARY for $ARCH on $OS" >&2
    fi
    
    # Check if TCC binary exists
    if [ ! -f "$TCC_PATH" ]; then
        echo "ERROR: TCC binary not found: $TCC_PATH" >&2
        echo "Please run build_tccx.sh to build TCC for your architecture." >&2
        echo "" >&2
        echo "Looking for: $TCC_BINARY" >&2
        echo "Available TCC binaries in $SCRIPT_DIR:" >&2
        ls -1 "$SCRIPT_DIR"/tcc_*.* 2>/dev/null | sed 's/.*\//  /' || echo "  None found"
        exit 1
    fi
    
    # Check if TCC binary is executable
    if [ ! -x "$TCC_PATH" ]; then
        echo "WARNING: TCC binary is not executable: $TCC_PATH" >&2
        echo "Setting executable permission..." >&2
        chmod +x "$TCC_PATH"
        if [ ! -x "$TCC_PATH" ]; then
            echo "ERROR: Failed to make TCC binary executable" >&2
            exit 1
        fi
    fi
    
    # Set environment variables for TCC
    export TCC_CPATH="$SCRIPT_DIR/tcc/include"
    export TCC_LIBRARY_PATH="$SCRIPT_DIR/tcc/lib:$SCRIPT_DIR/tcc/src"
    

    
    # Check if this is a help or version request
    for arg in "$@"; do
        case "$arg" in
            -h|-hh|--help|-v|--version)
                # For help/version, run TCC without extra paths
                exec "$TCC_PATH" "$@"
                ;;
        esac
    done
    
    # Execute TCC with proper library paths
    # Use absolute path for CRT files to avoid search issues
    CRT_PATH="$SCRIPT_DIR/tcc/lib"
    
    # Check if we need to explicitly specify CRT files
    if [ -f "$CRT_PATH/crt1.o" ] && [ -f "$CRT_PATH/crti.o" ] && [ -f "$CRT_PATH/crtn.o" ]; then
        # Check for -run mode first
        RUN_MODE=false
        for arg in "$@"; do
            if [ "$arg" = "-run" ]; then
                RUN_MODE=true
                break
            fi
        done
        
        # For linking operations, explicitly add CRT files
        LINK_MODE=false
        for arg in "$@"; do
            if [ "$arg" = "-c" ]; then
                LINK_MODE=false
                break
            elif [[ "$arg" == *.c ]] || [ "$arg" = "-o" ]; then
                LINK_MODE=true
            fi
        done
        
        if [ "$RUN_MODE" = true ]; then
            # For -run mode, TCC needs -B option to find runtime files
            # -B should point to the directory containing runtime files (tcc/)
            exec "$TCC_PATH" \
                -B"$SCRIPT_DIR/tcc" \
                -I"$SCRIPT_DIR/tcc/include" \
                -I"$SCRIPT_DIR/tcc/src/include" \
                -I"/usr/include/x86_64-linux-gnu" \
                -I"/usr/include" \
                -L"$SCRIPT_DIR/tcc/lib" \
                -L"/usr/lib/x86_64-linux-gnu" \
                -L"/usr/lib" \
                "$@"
        elif [ "$LINK_MODE" = true ]; then
            # Add CRT files explicitly for linking
            exec "$TCC_PATH" \
                -nostdlib \
                "/usr/lib/x86_64-linux-gnu/crt1.o" \
                "/usr/lib/x86_64-linux-gnu/crti.o" \
                -I"$SCRIPT_DIR/tcc/src/include" \
                -I"$SCRIPT_DIR/tcc/include" \
                -I"/usr/include" \
                -I"/usr/include/x86_64-linux-gnu" \
                -L"$SCRIPT_DIR/tcc/lib" \
                -L"$SCRIPT_DIR/tcc/src" \
                -L"/usr/lib" \
                -L"/usr/lib/x86_64-linux-gnu" \
                "$@" \
                "$SCRIPT_DIR/tcc/lib/libtcc1.a" \
                -lc \
                "/usr/lib/x86_64-linux-gnu/crtn.o"
        else
            # For non-linking operations, use standard paths
            exec "$TCC_PATH" \
                -I"$SCRIPT_DIR/tcc/src/include" \
                -I"/usr/include" \
                -I"/usr/include/x86_64-linux-gnu" \
                -L"$SCRIPT_DIR/tcc/lib" \
                -L"$SCRIPT_DIR/tcc/src" \
                -L"/usr/lib" \
                -L"/usr/lib/x86_64-linux-gnu" \
                "$@"
        fi
    else
        # Fallback to standard execution
        exec "$TCC_PATH" \
            -I"$SCRIPT_DIR/tcc/src/include" \
            -I"/usr/include" \
            -I"/usr/include/x86_64-linux-gnu" \
            -L"$SCRIPT_DIR/tcc/lib" \
            -L"$SCRIPT_DIR/tcc/src" \
            -L"/usr/lib" \
            -L"/usr/lib/x86_64-linux-gnu" \
            "$@"
    fi
}

# Run main function with all arguments
main "$@"
