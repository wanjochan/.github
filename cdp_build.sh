cd ../self-evolve-ai/

# Build main CDP executable with all enhanced modules
echo "Building CDP with process management, filesystem, system integration and concurrent operations..."
./cosmocc/bin/cosmocc cdp.c cdp_utils.c cdp_error.c cdp_websocket.c cdp_javascript.c cdp_chrome.c cdp_async.c cdp_chrome_mgr.c cdp_user_features.c cdp_process.c cdp_filesystem.c cdp_system.c cdp_concurrent.c -o cdp.exe -lpthread -lm

ls -al *cdp*.exe

echo "Testing basic CDP functionality..."
./cdp.exe -h

# Chrome auto-launches by default now, use CDP_NOLAUNCH_CHROME=1 to disable
echo "Testing JavaScript execution..."
echo 'JSON.stringify(Object.keys(window))' | ./cdp.exe -d 9922

echo "Copying existing files to .github..."
cp -v cdp*.c ../.github/ 2>/dev/null || true
cp -v cdp*.h ../.github/ 2>/dev/null || true
cp -v cdp*.sh ../.github/ 2>/dev/null || true
cp -v test_*.c ../.github/ 2>/dev/null || true
cp -v cdp.exe ../.github/ 2>/dev/null || true

