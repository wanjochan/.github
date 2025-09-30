#!/usr/bin/env bash
set -euo pipefail

#Bash(wget -q https://bellard.org/quickjs/quickjs-2024-01-13.tar.xz 2>&1)
#Bash(tar xf quickjs-2024-01-13.tar.xz && ls -la  | head -20)
#Bash(cd quickjs-2024-01-13 && make libquickjs.a -j4 2>&1 | tail -20)

cd ../self-evolve-ai/

# Build main CDP executable with all enhanced modules
echo "Building CDP with QuickJS integration..."
# Build embedded JS header first
bash ./build_js_resources.sh

# Check if we can use cached QuickJS library
QJS_DIR="quickjs-2024-01-13"
LIB_DIR="lib"

if [ -f "$LIB_DIR/libquickjs.a" ]; then
    # Check if library is still up-to-date
    REBUILD=0
    for src in $QJS_DIR/quickjs.c $QJS_DIR/cutils.c $QJS_DIR/libregexp.c $QJS_DIR/libunicode.c $QJS_DIR/libbf.c; do
        if [ "$src" -nt "$LIB_DIR/libquickjs.a" ]; then
            REBUILD=1
            break
        fi
    done

    if [ $REBUILD -eq 0 ]; then
        echo "  → Using cached libquickjs.a (faster compilation)"
        echo "  Compiling CDP with cached QuickJS library..."
        ./cosmocc/bin/cosmocc \
          cdp.c cdp_utils.c cdp_error.c cdp_websocket.c cdp_javascript.c cdp_chrome.c cdp_user_interface.c cdp_notify.c \
          cdp_quickjs.c cdp_http.c \
          -Iquickjs-2024-01-13 -Llib \
          -lquickjs -lpthread -lm \
          -o cdp.exe || { echo "[ERROR] cosmocc build failed" >&2; exit 1; }
    else
        REBUILD=1
    fi
else
    REBUILD=1
fi

# If no cached library or it's outdated, build it first
if [ "$REBUILD" = "1" ]; then
    if [ ! -f "$LIB_DIR/libquickjs.a" ]; then
        echo "  Building QuickJS library for faster future compilations..."
    else
        echo "  Rebuilding QuickJS library (sources changed)..."
    fi

    mkdir -p "$LIB_DIR"

    # Compile QuickJS object files
    ./cosmocc/bin/cosmocc -c $QJS_DIR/quickjs.c -o $LIB_DIR/quickjs.o -I$QJS_DIR -DCONFIG_VERSION=\"2024-01-13\" -DCONFIG_BIGNUM
    ./cosmocc/bin/cosmocc -c $QJS_DIR/cutils.c -o $LIB_DIR/cutils.o -I$QJS_DIR -DCONFIG_VERSION=\"2024-01-13\"
    ./cosmocc/bin/cosmocc -c $QJS_DIR/libregexp.c -o $LIB_DIR/libregexp.o -I$QJS_DIR -DCONFIG_VERSION=\"2024-01-13\"
    ./cosmocc/bin/cosmocc -c $QJS_DIR/libunicode.c -o $LIB_DIR/libunicode.o -I$QJS_DIR -DCONFIG_VERSION=\"2024-01-13\"
    ./cosmocc/bin/cosmocc -c $QJS_DIR/libbf.c -o $LIB_DIR/libbf.o -I$QJS_DIR -DCONFIG_VERSION=\"2024-01-13\" -DCONFIG_BIGNUM

    # Create static library
    ./cosmocc/bin/cosmoar rcs $LIB_DIR/libquickjs.a $LIB_DIR/*.o
    rm -f $LIB_DIR/*.o
    echo "  ✓ Created libquickjs.a ($(du -h $LIB_DIR/libquickjs.a | cut -f1))"

    # Now compile CDP with the library
    echo "  Compiling CDP with QuickJS library..."
    ./cosmocc/bin/cosmocc \
      cdp.c cdp_utils.c cdp_error.c cdp_websocket.c cdp_javascript.c cdp_chrome.c cdp_user_interface.c cdp_notify.c \
      cdp_quickjs.c cdp_http.c \
      -Iquickjs-2024-01-13 -Llib \
      -lquickjs -lpthread -lm \
      -o cdp.exe || { echo "[ERROR] cosmocc build failed" >&2; exit 1; }
fi

ls -al *cdp*.exe

echo "Testing basic CDP functionality..."
./cdp.exe -h

echo "Testing JavaScript execution..."
#echo 'JSON.stringify(Object.keys(window))' | ./cdp.exe -d 9922
echo 'Object.keys(window)' | ./cdp.exe -d 9922
echo '(async () => { const x = await Promise.resolve(42); return x; })()' | timeout 5 ./cdp.exe -d 9922 2>&1 | tail -5

echo "Copying existing files to .github..."
echo ""
echo "Testing Enhanced JS injection..."
if ! timeout 8s echo "window.CDP_Enhanced?.version || 'INJECTION_FAILED'" | ./cdp.exe -d 9922 >/tmp/cdp_test.out 2>&1; then
    echo "⚠️  Warning: CDP injection test timeout"
else
    if grep -q "INJECTION_FAILED\|NOT_LOADED\|undefined" /tmp/cdp_test.out; then
        echo "❌ Enhanced JS injection test failed!"
        if grep -q "SyntaxError\|error" /tmp/cdp_test.out; then
            echo "🐛 Found JS syntax error in injection"
            grep "SyntaxError\|DEBUG.*injection" /tmp/cdp_test.out 2>/dev/null || true
        fi
    elif grep -q "2.1" /tmp/cdp_test.out; then
        echo "✅ Enhanced JS injection test passed (v2.1 loaded)!"
    else
        echo "⚠️  Enhanced JS injection test inconclusive"
    fi
fi
rm -f /tmp/cdp_test.out

echo ""
echo "Copying files to .github..."
cp -v cdp*.c ../.github/ 2>/dev/null || true
cp -v cdp*.h ../.github/ 2>/dev/null || true
cp -v cdp_js_resources.h ../.github/ 2>/dev/null || true
cp -v cdp*.sh ../.github/ 2>/dev/null || true
cp -v test_*.c ../.github/ 2>/dev/null || true
cp -v cdp.exe ../.github/ 2>/dev/null || true

echo ""
echo "🎉 Build completed successfully!"
