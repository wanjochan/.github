

set -euo pipefail

# Paths relative to cosmorun/ directory
COSMO_BIN="../third_party/cosmocc/bin"
COSMO_X86="$COSMO_BIN/x86_64-unknown-cosmo-cc"
COSMO_ARM="$COSMO_BIN/aarch64-unknown-cosmo-cc"
APELINK="$COSMO_BIN/apelink"
##COMMON_FLAGS="-DCONFIG_TCC_SEMLOCK=1 -pthread"
COMMON_FLAGS=""
ARM64_FLAGS="-DTCC_TARGET_ARM64"
COSMO_RELEASE_DIR="../../.github/cosmorun/"

SRC_FILES=(cosmorun.c cosmo_trampoline.c cosmo_tcc.c cosmo_utils.c ../third_party/tinycc.hack/libtcc.c)
INC_FLAGS=(-I ../third_party/tinycc.hack/)

rm -fv cosmorun.exe cosmorun.*.tmp cosmorun.x86_64 cosmorun.arm64

echo "[cosmorun] building x86_64 variant"
"$COSMO_X86" "${SRC_FILES[@]}" "${INC_FLAGS[@]}" $COMMON_FLAGS -o cosmorun.x86_64

echo "[cosmorun] building arm64 variant"
"$COSMO_ARM" "${SRC_FILES[@]}" "${INC_FLAGS[@]}" $COMMON_FLAGS -o cosmorun.arm64

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

## original tcc (not yet solve osx jit problem):
#../third_party/cosmocc/bin/cosmocc $COMMON_FLAGS "${SRC_FILES[@]}" -I ../third_party/tinycc/ -I. -o cosmorun_raw.exe

## direct build (has problem for osx ape_link?)
../third_party/cosmocc/bin/cosmocc $COMMON_FLAGS cosmorun.c cosmo_trampoline.c cosmo_tcc.c cosmo_utils.c ../third_party/tinycc.hack/libtcc.c -I ../third_party/tinycc.hack/ -o cosmorun_direct.exe

ls -al cosmorun*.exe
rm -fv *.o

## sync to the dir of release
# Files to sync (supports wildcards, add/remove as needed)
RELEASE_FILES=(
    # Build scripts
    build_cosmorun.sh

    # Executables
    cosmorun.exe
    cosmorun_direct.exe

    # Core source files
    cosmorun.c
    cosmo_*.h
    cosmo_*.c

    # Documentation (optional)
    cosmorun.md

    test_duckdb_api.c
)

echo "[cosmorun] syncing to release dir: $COSMO_RELEASE_DIR"
for file_pattern in "${RELEASE_FILES[@]}"; do
    # Skip empty lines and comments
    [[ -z "$file_pattern" || "$file_pattern" =~ ^[[:space:]]*# ]] && continue
    cp -v $file_pattern "$COSMO_RELEASE_DIR/" 2>/dev/null || true
done

touch config.h

# core dump test
#COSMORUN_TRACE=1 ./cosmorun.exe --eval 'int main() { int x = 1/0; return x; }'
#COSMORUN_TRACE=0 ./cosmorun.exe --eval 'int main() { int x = 1/0; return x; }'

./cosmorun.exe --eval 'main(){printf("./cosmorun.exe: Hello World!\n");}'

# echo `date -Ins`
./cosmorun.exe test_libm.c
./cosmorun.exe test_libc.c

## quick dev osx
##../third_party/cosmocc/bin/aarch64-unknown-cosmo-cc -DTCC_TARGET_ARM64 -I ../third_party/tinycc.hack cosmorun.c ../third_party/tinycc.hack/libtcc.c 
