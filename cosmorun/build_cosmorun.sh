
set -euo pipefail

# Paths relative to cosmorun/ directory
COSMO_BIN="../third_party/cosmocc/bin"
COSMO_X86="$COSMO_BIN/x86_64-unknown-cosmo-cc"
COSMO_ARM="$COSMO_BIN/aarch64-unknown-cosmo-cc"
APELINK="$COSMO_BIN/apelink"
##COMMON_FLAGS="-DCONFIG_TCC_SEMLOCK=1 -pthread"
COMMON_FLAGS="-D BUILD_COSMO_RUN -mtiny -Os"
X86_64_FLAGS="-DTCC_TARGET_X86_64"
ARM64_FLAGS="-DTCC_TARGET_ARM64"
COSMO_RELEASE_DIR="../../.github/cosmorun/"

SRC_FILES=(cosmo_run.c cosmo_tcc.c cosmo_utils.c xdl.c ../third_party/tinycc.hack/libtcc.c)
INC_FLAGS=(-I ../third_party/tinycc.hack/)

rm -fv cosmorun.exe cosmorun.*.tmp cosmorun.x86_64 cosmorun.arm64 cosmorun-*-64.exe

echo "[cosmorun] building x86_64 variant"
"$COSMO_X86" "${SRC_FILES[@]}" "${INC_FLAGS[@]}" $COMMON_FLAGS $X86_64_FLAGS -o cosmorun.x86_64

echo "[cosmorun] building arm64 variant"
"$COSMO_ARM" "${SRC_FILES[@]}" "${INC_FLAGS[@]}" $COMMON_FLAGS $ARM64_FLAGS -o cosmorun.arm64

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

## Build architecture-specific binaries (for cosmo_mini.c)
echo "[cosmorun] building architecture-specific binaries with -mtiny -Os"

echo "[cosmorun] building cosmorun-x86-64.exe"
"$COSMO_X86" -mtiny -Os "${SRC_FILES[@]}" "${INC_FLAGS[@]}" $COMMON_FLAGS $X86_64_FLAGS -o cosmorun-x86-64.exe

echo "[cosmorun] building cosmorun-arm-64.exe"
"$COSMO_ARM" -mtiny -Os "${SRC_FILES[@]}" "${INC_FLAGS[@]}" $COMMON_FLAGS $ARM64_FLAGS -o cosmorun-arm-64.exe

# RISC-V architecture support (stub - toolchain not available)
# Current cosmocc only provides: x86_64-unknown-cosmo-cc, aarch64-unknown-cosmo-cc
# RISC-V would require: riscv64-unknown-cosmo-cc (not yet released)
#
# Uncomment when RISC-V toolchain becomes available:
# COSMO_RISCV="$COSMO_BIN/riscv64-unknown-cosmo-cc"
# if [ -f "$COSMO_RISCV" ]; then
#     echo "[cosmorun] building cosmorun-risc-64.exe"
#     RISCV_FLAGS="-DTCC_TARGET_RISCV64"
#     "$COSMO_RISCV" -mtiny -Os "${SRC_FILES[@]}" "${INC_FLAGS[@]}" $COMMON_FLAGS $RISCV_FLAGS -o cosmorun-risc-64.exe
# else
#     echo "[cosmorun] skipping RISC-V build (toolchain not available)"
# fi

## original tcc (not yet solve osx jit problem):
#../third_party/cosmocc/bin/cosmocc $COMMON_FLAGS "${SRC_FILES[@]}" -I ../third_party/tinycc/ -I. -o cosmorun_raw.exe

## direct build (maybe has problem for osx ape_link??)
../third_party/cosmocc/bin/cosmocc $COMMON_FLAGS cosmo_run.c cosmo_tcc.c cosmo_utils.c xdl.c ../third_party/tinycc.hack/libtcc.c -I ../third_party/tinycc.hack/ -o cosmorun_direct.exe

ls -al cosmorun*.exe
rm -fv *.o

## sync to the dir of release
./sync_cosmorun.sh "$COSMO_RELEASE_DIR"

touch config.h

# core dump test
#COSMORUN_TRACE=1 ./cosmorun.exe --eval 'int main() { int x = 1/0; return x; }'
#COSMORUN_TRACE=0 ./cosmorun.exe --eval 'int main() { int x = 1/0; return x; }'

./cosmorun.exe --eval 'main(){printf("./cosmorun.exe: Hello World!\n");}'

# echo `date -Ins`
./cosmorun.exe test_libm.c
./cosmorun.exe test_libc.c

## quick dev osx
##../third_party/cosmocc/bin/aarch64-unknown-cosmo-cc -DTCC_TARGET_ARM64 -I ../third_party/tinycc.hack cosmo_run.c ../third_party/tinycc.hack/libtcc.c 
