/**
 * CDP Chrome Module
 * Handles Chrome discovery, launching, and connection management
 */

#include "cdp_internal.h"
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

/* External global variables */
extern CDPContext g_ctx;
extern int verbose;
extern int ws_sock;
extern int ws_cmd_id;
extern int gui_mode;  // Add support for GUI mode
extern char proxy_server[];  // Proxy server setting

/* Static variables */
static pid_t chrome_pid = -1;

/* Find Chrome/Chromium executable */
char* find_chrome_executable(void) {
    static char chrome_path[512];
    chrome_path[0] = '\0';
    
    const char *os = cdp_detect_os();
    
    // Paths to check based on OS
    const char *paths_linux[] = {
        "/opt/google/chrome/chrome",     // Direct Chrome binary (avoiding wrapper scripts)
        "/usr/bin/google-chrome",
        "/usr/bin/google-chrome-stable",
        "/usr/bin/chromium-browser",
        "/usr/bin/chromium",
        "/snap/bin/chromium",
        "/usr/local/bin/chrome",
        "google-chrome",  // Try PATH
        "chromium",       // Try PATH
        NULL
    };
    
    const char *paths_mac[] = {
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
        "/Applications/Chromium.app/Contents/MacOS/Chromium",
        "/Applications/Google Chrome Canary.app/Contents/MacOS/Google Chrome Canary",
        "/usr/local/bin/chromium",
        "google-chrome",
        "chromium",
        NULL
    };
    
    const char *paths_windows[] = {
        "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
        "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
        "C:\\Program Files\\Chromium\\Application\\chrome.exe",
        "%LOCALAPPDATA%\\Google\\Chrome\\Application\\chrome.exe",
        "%PROGRAMFILES%\\Google\\Chrome\\Application\\chrome.exe",
        "chrome.exe",
        "chrome",
        NULL
    };
    
    // Select paths based on OS
    const char **paths = NULL;
    if (strstr(os, "linux")) {
        paths = paths_linux;
    } else if (strstr(os, "darwin") || strstr(os, "mac")) {
        paths = paths_mac;
    } else if (strstr(os, "win") || strstr(os, "mingw") || strstr(os, "cygwin")) {
        paths = paths_windows;
    } else {
        // Unknown OS, try generic paths
        paths = paths_linux;  // Most likely to work
    }
    
    // Check each path
    for (int i = 0; paths[i]; i++) {
        const char *path = paths[i];
        
        // Expand environment variables for Windows paths
        if (strchr(path, '%')) {
            char expanded[512];
            // Simple expansion for common vars
            if (strstr(path, "%LOCALAPPDATA%")) {
                const char *local = getenv("LOCALAPPDATA");
                if (local) {
                    snprintf(expanded, sizeof(expanded), "%s%s", local, 
                            path + strlen("%LOCALAPPDATA%"));
                    path = expanded;
                }
            } else if (strstr(path, "%PROGRAMFILES%")) {
                const char *prog = getenv("PROGRAMFILES");
                if (prog) {
                    snprintf(expanded, sizeof(expanded), "%s%s", prog,
                            path + strlen("%PROGRAMFILES%"));
                    path = expanded;
                }
            }
        }
        
        // Check if file exists and is executable
        struct stat st;
        if (stat(path, &st) == 0) {
            // Check if it's a regular file or symlink
            if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
                // On Unix, check execute permission
                if (strstr(os, "win") || (st.st_mode & S_IXUSR)) {
                    str_copy_safe(chrome_path, path, sizeof(chrome_path));
                    if (verbose) {
                        printf("Found Chrome at: %s\n", chrome_path);
                    }
                    return chrome_path;
                }
            }
        }
        
        // Try using 'which' command for PATH lookup
        if (!strchr(path, '/') && !strchr(path, '\\')) {
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "which %s 2>/dev/null", path);
            FILE *fp = popen(cmd, "r");
            if (fp) {
                if (fgets(chrome_path, sizeof(chrome_path), fp)) {
                    // Remove newline
                    size_t len = strlen(chrome_path);
                    if (len > 0 && chrome_path[len-1] == '\n') {
                        chrome_path[len-1] = '\0';
                    }
                    pclose(fp);
                    if (chrome_path[0] && stat(chrome_path, &st) == 0) {
                        if (verbose) {
                            printf("Found Chrome via PATH: %s\n", chrome_path);
                        }
                        return chrome_path;
                    }
                }
                pclose(fp);
            }
        }
    }
    
    // Chrome not found
    if (verbose) {
        fprintf(stderr, "Chrome/Chromium executable not found\n");
        fprintf(stderr, "Please install Chrome or set CDP_CHROME_PATH environment variable\n");
    }
    
    return NULL;
}

/* Launch Chrome with debugging enabled */
void launch_chrome(void) {
    const char *os = cdp_detect_os();
    if (verbose) {
        printf("Detected OS: %s\n", os);
    }
    
    // Check if Chrome is already running on the debug port
    int test_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (test_sock >= 0) {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(g_ctx.config.debug_port);
        addr.sin_addr.s_addr = inet_addr(g_ctx.config.chrome_host);
        
        if (connect(test_sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            close(test_sock);
            if (verbose) {
                printf("Chrome is already running on port %d\n", g_ctx.config.debug_port);
            }
            return;
        }
        close(test_sock);
    }
    
    // Find Chrome executable
    const char *chrome_path = getenv("CDP_CHROME_PATH");
    if (!chrome_path) {
        chrome_path = find_chrome_executable();
        if (!chrome_path) {
            fprintf(stderr, "Error: Chrome executable not found in PATH\n");
            fprintf(stderr, "Please install Chrome or specify the path\n");
            fprintf(stderr, "To disable auto-launch, set CDP_NOLAUNCH_CHROME=1\n");
            exit(1);
        }
    }
    
    // Fork and launch Chrome
    chrome_pid = fork();
    if (chrome_pid == 0) {
        // Child process - launch Chrome
        
        // Always redirect stdout/stderr to /dev/null for Chrome
        // Chrome's output is too noisy even for verbose mode
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        
        // Build argument array
        char port_arg[32];
        snprintf(port_arg, sizeof(port_arg), "--remote-debugging-port=%d", g_ctx.config.debug_port);
        
        char user_data_arg[256];
        if (g_ctx.config.user_data_dir) {
            snprintf(user_data_arg, sizeof(user_data_arg), "--user-data-dir=%s", 
                     g_ctx.config.user_data_dir);
        } else {
            // Use a unique profile directory for each port to avoid SingletonLock conflicts
            snprintf(user_data_arg, sizeof(user_data_arg), "--user-data-dir=/tmp/cdp-chrome-profile-%d", 
                     g_ctx.config.debug_port);
        }
        
        // Prepare proxy argument if specified
        char proxy_arg[600] = "";
        if (strlen(proxy_server) > 0) {
            snprintf(proxy_arg, sizeof(proxy_arg), "--proxy-server=%s", proxy_server);
        }
        
        // Execute Chrome directly (no shell)
        // Note: Chrome may fork and the parent process may exit, which is normal
        // Use execl instead of execlp when we have an absolute path
        if (chrome_path[0] == '/') {
            if (gui_mode || getenv("CDP_GUI_MODE")) {
                // GUI mode - visible Chrome window
                if (strlen(proxy_server) > 0) {
                    execl(chrome_path, chrome_path,
                       port_arg,
                       proxy_arg,
                       "--no-sandbox",
                       "--disable-dev-shm-usage",
                       user_data_arg,
                       "--disable-extensions",
                       "--disable-background-timer-throttling",
                       "--disable-renderer-backgrounding",
                       "--disable-features=TranslateUI",
                       "--disable-ipc-flooding-protection",
                       "--no-first-run",
                       "--enable-automation",
                       "about:blank",
                       NULL);
                } else {
                    execl(chrome_path, chrome_path,
                       port_arg,
                       "--no-sandbox",
                       "--disable-dev-shm-usage",
                       user_data_arg,
                       "--disable-extensions",
                       "--disable-background-timer-throttling",
                       "--disable-renderer-backgrounding",
                       "--disable-features=TranslateUI",
                       "--disable-ipc-flooding-protection",
                       "--no-first-run",
                       "--enable-automation",
                       "about:blank",
                       NULL);
                }
            } else {
                // Headless mode (default)
                if (strlen(proxy_server) > 0) {
                    execl(chrome_path, chrome_path,
                       port_arg,
                       proxy_arg,
                       "--no-sandbox",
                       "--disable-dev-shm-usage",
                       "--headless=new",
                       "--disable-gpu",
                       user_data_arg,
                       "--disable-extensions",
                       "--disable-background-timer-throttling",
                       "--disable-backgrounding-occluded-windows",
                       "--disable-renderer-backgrounding",
                       "--disable-features=TranslateUI",
                       "--disable-ipc-flooding-protection",
                       "--no-first-run",
                       "--disable-default-apps",
                       "--disable-sync",
                       "--enable-automation",
                       "--password-store=basic",
                       "--use-mock-keychain",
                       "about:blank",
                       NULL);
                } else {
                    execl(chrome_path, chrome_path,
                       port_arg,
                       "--no-sandbox",
                       "--disable-dev-shm-usage",
                       "--headless=new",
                       "--disable-gpu",
                       user_data_arg,
                       "--disable-extensions",
                       "--disable-background-timer-throttling",
                       "--disable-backgrounding-occluded-windows",
                       "--disable-renderer-backgrounding",
                       "--disable-features=TranslateUI",
                       "--disable-ipc-flooding-protection",
                       "--no-first-run",
                       "--disable-default-apps",
                       "--disable-sync",
                       "--enable-automation",
                       "--password-store=basic",
                       "--use-mock-keychain",
                       "about:blank",
                       NULL);
                }
            }
        } else {
            // Use execlp for PATH lookups
            if (gui_mode || getenv("CDP_GUI_MODE")) {
                // GUI mode - visible Chrome window
                if (strlen(proxy_server) > 0) {
                    execlp(chrome_path, chrome_path,
                           port_arg,
                           proxy_arg,
                           "--no-sandbox",
                           "--disable-dev-shm-usage",
                           user_data_arg,
                           "--disable-extensions",
                           "--disable-background-timer-throttling",
                           "--disable-renderer-backgrounding",
                           "--disable-features=TranslateUI",
                           "--disable-ipc-flooding-protection",
                           "--no-first-run",
                           "--enable-automation",
                           "about:blank",
                           NULL);
                } else {
                    execlp(chrome_path, chrome_path,
                           port_arg,
                           "--no-sandbox",
                           "--disable-dev-shm-usage",
                           user_data_arg,
                           "--disable-extensions",
                           "--disable-background-timer-throttling",
                           "--disable-renderer-backgrounding",
                           "--disable-features=TranslateUI",
                           "--disable-ipc-flooding-protection",
                           "--no-first-run",
                           "--enable-automation",
                           "about:blank",
                           NULL);
                }
            } else {
                // Headless mode (default)
                if (strlen(proxy_server) > 0) {
                    execlp(chrome_path, chrome_path,
                           port_arg,
                           proxy_arg,
                           "--no-sandbox",
                           "--disable-dev-shm-usage",
                           "--headless=new",
                           "--disable-gpu",
                           user_data_arg,
                           "--disable-extensions",
                           "--disable-background-timer-throttling",
                           "--disable-backgrounding-occluded-windows",
                           "--disable-renderer-backgrounding",
                           "--disable-features=TranslateUI",
                           "--disable-ipc-flooding-protection",
                           "--no-first-run",
                           "--disable-default-apps",
                           "--disable-sync",
                           "--enable-automation",
                           "--password-store=basic",
                           "--use-mock-keychain",
                           "about:blank",
                           NULL);
                } else {
                    execlp(chrome_path, chrome_path,
                           port_arg,
                           "--no-sandbox",
                           "--disable-dev-shm-usage",
                           "--headless=new",
                           "--disable-gpu",
                           user_data_arg,
                           "--disable-extensions",
                           "--disable-background-timer-throttling",
                           "--disable-backgrounding-occluded-windows",
                           "--disable-renderer-backgrounding",
                           "--disable-features=TranslateUI",
                           "--disable-ipc-flooding-protection",
                           "--no-first-run",
                           "--disable-default-apps",
                           "--disable-sync",
                           "--enable-automation",
                           "--password-store=basic",
                           "--use-mock-keychain",
                           "about:blank",
                           NULL);
                }
            }
        }
        
        // If we get here, exec failed
        perror("Failed to launch Chrome");
        exit(1);
    } else if (chrome_pid < 0) {
        perror("fork");
        exit(1);
    }
    
    // Parent process - wait for Chrome to start
    if (verbose) {
        printf("Chrome launched with PID %d\n", chrome_pid);
        printf("Waiting for Chrome to start listening on port %d...\n", g_ctx.config.debug_port);
    }
    
    // Wait for Chrome to be ready
    int attempts = 0;
    int max_attempts = 100;  // 10 seconds - Chrome can be slow to start in headless mode
    int child_exited = 0;
    
    while (attempts < max_attempts) {
        usleep(100000);  // 100ms
        
        // Check if Chrome process exited (this is normal - Chrome may fork and parent exits)
        if (!child_exited && attempts > 0 && attempts % 5 == 0) {
            int status;
            pid_t result = waitpid(chrome_pid, &status, WNOHANG);
            if (result == chrome_pid) {
                child_exited = 1;
                if (verbose) {
                    if (WIFEXITED(status)) {
                        printf("Chrome parent process exited with status %d (this is normal)\n", 
                               WEXITSTATUS(status));
                    }
                }
                // Don't return here - Chrome may have forked and parent exited
                // Continue checking if port is open
            }
        }
        
        int test_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (test_sock >= 0) {
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(g_ctx.config.debug_port);
            addr.sin_addr.s_addr = inet_addr(g_ctx.config.chrome_host);
            
            if (connect(test_sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                close(test_sock);
                if (verbose) {
                    printf("Chrome DevTools is listening on port %d\n", g_ctx.config.debug_port);
                }
                return;
            }
            close(test_sock);
        }
        
        if (verbose && attempts == 30) {
            printf("Still waiting for Chrome to start...\n");
        }
        if (verbose && attempts == 60) {
            printf("Chrome is taking longer than usual to start...\n");
        }
        
        attempts++;
    }
    
    fprintf(stderr, "Warning: Chrome may not have started correctly\n");
    fprintf(stderr, "Chrome process (PID %d) may still be starting\n", chrome_pid);
}

/* Get Chrome target ID from /json/version endpoint */
char* get_chrome_target_id(void) {
    static char target_id[256];
    target_id[0] = '\0';
    
    // Connect to Chrome HTTP endpoint
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return NULL;
    }
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(g_ctx.config.debug_port);
    addr.sin_addr.s_addr = inet_addr(g_ctx.config.chrome_host);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return NULL;
    }
    
    // Send HTTP request for /json/version
    const char *request = "GET /json/version HTTP/1.1\r\nHost: localhost\r\n\r\n";
    send(sock, request, strlen(request), 0);
    
    // Read response
    char buffer[4096];
    int len = recv(sock, buffer, sizeof(buffer)-1, 0);
    close(sock);
    
    if (len <= 0) {
        return NULL;
    }
    buffer[len] = '\0';
    
    // Parse webSocketDebuggerUrl
    const char *ws_url = strstr(buffer, "\"webSocketDebuggerUrl\"");
    if (!ws_url) {
        return NULL;
    }
    
    // Extract the URL
    ws_url = strchr(ws_url, ':');
    if (!ws_url) return NULL;
    ws_url += 2;  // Skip ': "'
    
    const char *url_start = strchr(ws_url, '"');
    if (!url_start) return NULL;
    url_start++;
    
    const char *url_end = strchr(url_start, '"');
    if (!url_end) return NULL;
    
    // Extract target ID from URL (e.g., "ws://localhost/devtools/browser/xxx")
    const char *devtools = strstr(url_start, "/devtools/");
    if (!devtools) return NULL;
    devtools += 10;  // Skip "/devtools/"
    
    size_t id_len = url_end - devtools;
    if (id_len >= sizeof(target_id)) {
        return NULL;
    }
    
    strncpy(target_id, devtools, id_len);
    target_id[id_len] = '\0';
    
    return target_id;
}

/* Create a new page via browser endpoint */
char* create_new_page_via_browser(int browser_sock) {
    static char page_target_id[256];
    
    // Send Target.createTarget to create a new page
    CDPMessage* msg = cdp_message_new("Target.createTarget");
    cdp_message_add_param(msg, "url", "about:blank");
    char *command = cdp_message_build(msg);
    
    int msg_id = msg->id;
    cdp_message_free(msg);
    
    if (ws_send_text(browser_sock, command) < 0) {
        return NULL;
    }
    
    // Wait for response
    char response[4096];
    int len = receive_response_by_id(response, sizeof(response), msg_id, 10);
    
    if (len <= 0) {
        return NULL;
    }
    
    // Parse targetId from response
    const char *target_start = strstr(response, "\"targetId\":\"");
    if (!target_start) {
        return NULL;
    }
    
    target_start += 12;  // Skip past "targetId":"
    const char *target_end = strchr(target_start, '"');
    if (!target_end) {
        return NULL;
    }
    
    size_t id_len = target_end - target_start;
    if (id_len >= sizeof(page_target_id)) {
        return NULL;
    }
    
    strncpy(page_target_id, target_start, id_len);
    page_target_id[id_len] = '\0';
    
    return page_target_id;
}

/* Ensure Chrome is running and ready */
int ensure_chrome_running(void) {
    // Check if Chrome is accessible
    int test_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (test_sock < 0) {
        return -1;
    }
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(g_ctx.config.debug_port);
    addr.sin_addr.s_addr = inet_addr(g_ctx.config.chrome_host);
    
    if (connect(test_sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        close(test_sock);
        return 0;  // Chrome is running
    }
    close(test_sock);
    
    // Chrome not running, check if we should NOT launch it
    const char *no_launch_env = getenv("CDP_NOLAUNCH_CHROME");
    if (no_launch_env && strcmp(no_launch_env, "1") == 0) {
        fprintf(stderr, "Error: Chrome is not running on port %d\n", g_ctx.config.debug_port);
        fprintf(stderr, "Please start Chrome with: chrome --remote-debugging-port=%d\n", 
                g_ctx.config.debug_port);
        fprintf(stderr, "(Auto-launch disabled by CDP_NOLAUNCH_CHROME=1)\n");
        return -1;
    }
    
    // Auto-launch Chrome by default
    if (verbose) {
        printf("Chrome not found on port %d, auto-launching...\n", g_ctx.config.debug_port);
    }
    launch_chrome();
    
    // Verify it started - give it a bit more time if needed
    int verify_attempts = 0;
    while (verify_attempts < 30) {  // 3 more seconds
        test_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (test_sock >= 0) {
            if (connect(test_sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                close(test_sock);
                return 0;
            }
            close(test_sock);
        }
        
        if (verify_attempts == 0 && verbose) {
            printf("Verifying Chrome startup...\n");
        }
        
        usleep(100000);  // 100ms
        verify_attempts++;
    }
    
    fprintf(stderr, "Error: Failed to connect to Chrome after launch\n");
    fprintf(stderr, "Chrome may still be starting. Try running again in a few seconds.\n");
    return -1;
}