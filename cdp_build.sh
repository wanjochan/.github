cd ../self-evolve-ai/
./cosmocc/bin/cosmocc cdp_main.c cdp_utils.c cdp_error.c cdp_websocket.c cdp_javascript.c cdp_chrome.c cdp_async.c cdp_chrome_mgr.c cdp_json_parser.c cdp_user_features.c -o cdp.exe -lpthread -lm
ls -al cdp.exe
./cdp.exe -h
echo 'JSON.stringify(Object.keys(window))' | ./cdp.exe -v
cp -v cdp*.c ../.github/
cp -v cdp*.h ../.github/
cp -v cdp*.sh ../.github/
cp -v cdp.exe ../.github/
