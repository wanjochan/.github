#!/bin/bash

set -euo pipefail

echo "=========================================="
echo "CosmoRun Build Script (TCC Runtime Mode)"
echo "=========================================="
echo "Date: $(date)"
echo "Working dir: $(pwd)"
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "[Step 1] Detecting paths..."

# Detect if we're in a worktree and adjust paths accordingly
if [ -f "../.git" ]; then
    # We're in a worktree, use main repo paths
    MAIN_REPO="/workspace/self-evolve-ai"
    COSMO_BIN="$MAIN_REPO/third_party/cosmocc/bin"
    TINYCC_DIR="$MAIN_REPO/third_party/tinycc.hack"
else
    # Normal repo layout - use $SCRIPT_DIR to get absolute paths
    COSMO_BIN="$SCRIPT_DIR/../third_party/cosmocc/bin"
    TINYCC_DIR="$SCRIPT_DIR/../third_party/tinycc.hack"
fi

COSMO_X86="$COSMO_BIN/x86_64-unknown-cosmo-cc"
COSMO_ARM="$COSMO_BIN/aarch64-unknown-cosmo-cc"
APELINK="$COSMO_BIN/apelink"
##COMMON_FLAGS="-DCONFIG_TCC_SEMLOCK=1 -pthread"
COMMON_FLAGS="-D BUILD_COSMO_RUN -mtiny -Os"
X86_64_FLAGS="-DTCC_TARGET_X86_64"
ARM64_FLAGS="-DTCC_TARGET_ARM64"
COSMO_RELEASE_DIR="../../.github/cosmorun/"

# Source files in src/ directory
SRC_DIR="$SCRIPT_DIR/src"
# System Layer files (now at repo root)
SYSTEM_LAYER_DIR="$SCRIPT_DIR/../cosmorun_system"
SYSTEM_LAYER_FILES=(
    "$SYSTEM_LAYER_DIR/cosmo_system.c"
    "$SYSTEM_LAYER_DIR/libc_shim/sys_libc_shim.c"
)

SRC_FILES=($SRC_DIR/cosmo_run.c $SRC_DIR/cosmo_tcc.c $SRC_DIR/cosmo_co.c $SRC_DIR/cosmo_co_asm.S $SRC_DIR/cosmo_utils.c $SRC_DIR/cosmo_cc.c $SRC_DIR/cosmo_got_plt_reloc.c $SRC_DIR/cosmo_analyzer.c $SRC_DIR/cosmo_formatter.c $SRC_DIR/cosmo_cache.c $SRC_DIR/cosmo_mem_profiler.c $SRC_DIR/cosmo_profiler.c $SRC_DIR/cosmo_parallel.c $SRC_DIR/cosmo_parallel_link.c $SRC_DIR/cosmo_debugger.c $SRC_DIR/cosmo_coverage.c $SRC_DIR/cosmo_mutate.c $SRC_DIR/cosmo_symbols.c $SRC_DIR/cosmo_lsp.c $SRC_DIR/cosmo_deps.c $SRC_DIR/cosmo_env.c $SRC_DIR/cosmo_publish.c $SRC_DIR/cosmo_lock.c $SRC_DIR/cosmo_ffi.c $SRC_DIR/cosmo_sandbox.c $SRC_DIR/cosmo_sign.c $SRC_DIR/cosmo_audit.c $SRC_DIR/cosmo_target.c $SRC_DIR/cosmo_bootstrap.c "$SCRIPT_DIR/c_modules/mod_crypto.c" "$SCRIPT_DIR/c_modules/third_party/cJSON.c" $SRC_DIR/xdl.c "$TINYCC_DIR/libtcc.c")
# Add include paths for src/, c_modules/, and cosmorun_system/ to resolve relative includes
INC_FLAGS=(-I "$TINYCC_DIR" -I "$SRC_DIR" -I "$SCRIPT_DIR" -I "$SCRIPT_DIR/c_modules" -I "$SCRIPT_DIR/..")

rm -fv cosmorun.exe cosmorun.*.tmp cosmorun.x86_64 cosmorun.arm64 cosmorun-*-64.exe

echo "[cosmorun] building x86_64 variant (TCC Runtime mode)"
"$COSMO_X86" "${SYSTEM_LAYER_FILES[@]}" "${SRC_FILES[@]}" "${INC_FLAGS[@]}" $COMMON_FLAGS $X86_64_FLAGS -o cosmorun.x86_64

echo "[cosmorun] building arm64 variant (TCC Runtime mode)"
"$COSMO_ARM" "${SYSTEM_LAYER_FILES[@]}" "${SRC_FILES[@]}" "${INC_FLAGS[@]}" $COMMON_FLAGS $ARM64_FLAGS -o cosmorun.arm64

echo "[cosmorun] linking fat binary via apelink"
"$APELINK" \
  -s \
  -l "$COSMO_BIN/ape-x86_64.elf" \
  -l "$COSMO_BIN/ape-aarch64.elf" \
  -M "$COSMO_BIN/ape-m1.c" \
  -o cosmorun.exe \
  cosmorun.x86_64 \
  cosmorun.arm64

#rm -f cosmorun.x86_64 cosmorun.arm64

echo "[cosmorun] building architecture-specific binaries with -mtiny -Os (TCC Runtime mode)"

echo "[cosmorun] building cosmorun-x86-64.exe"
"$COSMO_X86" -mtiny -Os "${SYSTEM_LAYER_FILES[@]}" "${SRC_FILES[@]}" "${INC_FLAGS[@]}" $COMMON_FLAGS $X86_64_FLAGS -o cosmorun-x86-64.exe

echo "[cosmorun] building cosmorun-arm-64.exe"
"$COSMO_ARM" -mtiny -Os "${SYSTEM_LAYER_FILES[@]}" "${SRC_FILES[@]}" "${INC_FLAGS[@]}" $COMMON_FLAGS $ARM64_FLAGS -o cosmorun-arm-64.exe

# RISC-V architecture support (stub - toolchain not available)
# Current cosmocc only provides: x86_64-unknown-cosmo-cc, aarch64-unknown-cosmo-cc
# RISC-V would require: riscv64-unknown-cosmo-cc (not yet released)
#
# Uncomment when RISC-V toolchain becomes available:
# COSMO_RISCV="$COSMO_BIN/riscv64-unknown-cosmo-cc"
# if [ -f "$COSMO_RISCV" ]; then
#     echo "[cosmorun] building cosmorun-risc-64.exe"
#     RISCV_FLAGS="-DTCC_TARGET_RISCV64"
#     "$COSMO_RISCV" -mtiny -Os "${SYSTEM_LAYER_FILES[@]}" "${SRC_FILES[@]}" "${INC_FLAGS[@]}" $COMMON_FLAGS $RISCV_FLAGS -o cosmorun-risc-64.exe
# else
#     echo "[cosmorun] skipping RISC-V build (toolchain not available)"
# fi

## original tcc (not yet solve osx jit problem):
#../third_party/cosmocc/bin/cosmocc $COMMON_FLAGS "${SYSTEM_LAYER_FILES[@]}" "${SRC_FILES[@]}" -I ../third_party/tinycc/ -I. -o cosmorun_raw.exe

## direct build (maybe has problem for osx ape_link??)
# Note: cosmo_co_asm.S not included - direct build doesn't support .S files
# "$COSMO_BIN/cosmocc" $COMMON_FLAGS $SRC_DIR/cosmo_run.c $SRC_DIR/cosmo_tcc.c $SRC_DIR/cosmo_co.c $SRC_DIR/cosmo_utils.c $SRC_DIR/cosmo_cc.c $SRC_DIR/cosmo_got_plt_reloc.c $SRC_DIR/cosmo_analyzer.c $SRC_DIR/cosmo_formatter.c $SRC_DIR/cosmo_cache.c $SRC_DIR/cosmo_mem_profiler.c $SRC_DIR/cosmo_profiler.c $SRC_DIR/cosmo_parallel.c $SRC_DIR/cosmo_parallel_link.c $SRC_DIR/cosmo_debugger.c $SRC_DIR/cosmo_coverage.c $SRC_DIR/cosmo_mutate.c $SRC_DIR/cosmo_symbols.c $SRC_DIR/cosmo_lsp.c $SRC_DIR/cosmo_deps.c $SRC_DIR/cosmo_env.c $SRC_DIR/cosmo_publish.c $SRC_DIR/cosmo_lock.c $SRC_DIR/cosmo_ffi.c $SRC_DIR/cosmo_sandbox.c $SRC_DIR/cosmo_sign.c $SRC_DIR/cosmo_audit.c c_modules/mod_crypto.c $SRC_DIR/xdl.c "$TINYCC_DIR/libtcc.c" -I "$TINYCC_DIR" -I "$SRC_DIR" -o cosmorun_direct.exe 2>&1 | grep -v "undefined reference to .__co_ctx" || true

ls -al cosmorun*.exe
rm -fv *.o

## sync to the dir of release
"$SCRIPT_DIR/sync_cosmorun.sh" "$COSMO_RELEASE_DIR"

touch config.h

## core dump test
#./cosmorun.exe -vv --eval 'int main() { int x = 1/0; return x; }'
#./cosmorun.exe --eval 'int main() { int x = 1/0; return x; }'

./cosmorun.exe --eval 'main(){printf("./cosmorun.exe: Hello World!\n");}'

# echo `date -Ins`
./cosmorun.exe tests/test_libm.c
./cosmorun.exe tests/test_libc.c

