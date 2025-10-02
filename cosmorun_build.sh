# 20250928
set -euo pipefail

COSMO_BIN="third_party/cosmocc/bin"
COSMO_X86="$COSMO_BIN/x86_64-unknown-cosmo-cc"
COSMO_ARM="$COSMO_BIN/aarch64-unknown-cosmo-cc"
APELINK="$COSMO_BIN/apelink"
##COMMON_FLAGS="-DCONFIG_TCC_SEMLOCK=1 -pthread"
COMMON_FLAGS=""
ARM64_FLAGS="-DTCC_TARGET_ARM64"

# original tcc not yet solve osx jit problem:
#third_party/cosmocc/bin/cosmocc $COMMON_FLAGS cosmorun.c third_party/tinycc/libtcc.c -I third_party/tinycc/ -I. -o cosmorun_raw.exe

SRC_FILES=(cosmorun.c third_party/tinycc.hack/libtcc.c)
INC_FLAGS=(-I third_party/tinycc.hack/)

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

## direct build (has problem)
##third_party/cosmocc/bin/cosmocc $COMMON_FLAGS cosmorun.c third_party/tinycc.hack/libtcc.c -I third_party/tinycc.hack/ -o cosmorun_hack.exe


ls -al cosmorun*.exe
./cosmorun.exe --eval 'main(){printf("./cosmorun.exe: Hello World!\n");}'

# core dump test
#COSMORUN_TRACE=1 ./cosmorun.exe --eval 'int main() { int x = 1/0; return x; }'
#COSMORUN_TRACE=0 ./cosmorun.exe --eval 'int main() { int x = 1/0; return x; }'
