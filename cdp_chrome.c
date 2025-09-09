/**
 * CDP Chrome Module
 * Handles Chrome discovery, launching, and connection management
 */

#include "cdp_internal.h"
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/utsname.h>
#include <ctype.h>

/* External global variables */
extern CDPContext g_ctx;
extern int verbose;
extern int ws_sock;
extern int ws_cmd_id;

/* Static variables */
static pid_t chrome_pid = -1;

/* Detect operating system */
static const char* detect_os(void) {
    static char os_name[32];
    struct utsname sys_info;
    
    if (uname(&sys_info) == 0) {
        // Convert to lowercase for easier comparison
        for (int i = 0; sys_info.sysname[i]; i++) {
            os_name[i] = tolower(sys_info.sysname[i]);
        }
        return os_name;
    }
    
    // Fallback detection
    #ifdef __linux__
        return "linux";
    #elif __APPLE__
        return "darwin";
    #elif _WIN32
        return "windows";
    #else
        return "unknown";
    #endif
}

/* Find Chrome/Chromium executable */
char* find_chrome_executable(void) {
    static char chrome_path[512];
    chrome_path[0] = '\0';
    
    const char *os = detect_os();
    
    // Paths to check based on OS
    const char *paths_linux[] = {
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
    const char *os = detect_os();
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
            fprintf(stderr, "Error: Chrome is not running on port %d\n", g_ctx.config.debug_port);
            fprintf(stderr, "Please start Chrome with: chrome --remote-debugging-port=%d\n", 
                    g_ctx.config.debug_port);
            fprintf(stderr, "Or set CDP_LAUNCH_CHROME=1 to auto-launch\n");
            exit(1);
        }
    }
    
    // Fork and launch Chrome
    chrome_pid = fork();
    if (chrome_pid == 0) {
        // Child process - launch Chrome
        
        // Build command
        char cmd[2048];
        int len = 0;
        
        // Chrome executable
        len += snprintf(cmd + len, sizeof(cmd) - len, "%s", chrome_path);
        
        // Debugging port
        len += snprintf(cmd + len, sizeof(cmd) - len, " --remote-debugging-port=%d", 
                       g_ctx.config.debug_port);
        
        // Common flags for headless operation
        len += snprintf(cmd + len, sizeof(cmd) - len, " --no-sandbox");
        len += snprintf(cmd + len, sizeof(cmd) - len, " --disable-dev-shm-usage");
        
        // Headless mode (new syntax)
        len += snprintf(cmd + len, sizeof(cmd) - len, " --headless=new");
        
        // Disable GPU for better compatibility
        len += snprintf(cmd + len, sizeof(cmd) - len, " --disable-gpu");
        
        // User data directory
        if (g_ctx.config.user_data_dir) {
            len += snprintf(cmd + len, sizeof(cmd) - len, " --user-data-dir=%s", 
                           g_ctx.config.user_data_dir);
        } else {
            len += snprintf(cmd + len, sizeof(cmd) - len, " --user-data-dir=/tmp/cdp-chrome-profile");
        }
        
        // Additional flags
        len += snprintf(cmd + len, sizeof(cmd) - len, " --disable-extensions");
        len += snprintf(cmd + len, sizeof(cmd) - len, " --disable-background-timer-throttling");
        len += snprintf(cmd + len, sizeof(cmd) - len, " --disable-backgrounding-occluded-windows");
        len += snprintf(cmd + len, sizeof(cmd) - len, " --disable-renderer-backgrounding");
        
        // Start page - use about:blank
        len += snprintf(cmd + len, sizeof(cmd) - len, " about:blank");
        
        // Run in background
        len += snprintf(cmd + len, sizeof(cmd) - len, " > /dev/null 2>&1 &");
        
        if (verbose) {
            printf("Launching Chrome: %s\n", cmd);
        }
        
        // Execute
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        
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
    }
    
    // Wait for Chrome to be ready
    int attempts = 0;
    int max_attempts = 30;  // 3 seconds
    
    while (attempts < max_attempts) {
        usleep(100000);  // 100ms
        
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
        attempts++;
    }
    
    fprintf(stderr, "Warning: Chrome may not have started correctly\n");
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
    
    // Chrome not running, check if we should launch it
    const char *launch_env = getenv("CDP_LAUNCH_CHROME");
    if (!launch_env || strcmp(launch_env, "1") != 0) {
        fprintf(stderr, "Error: Chrome is not running on port %d\n", g_ctx.config.debug_port);
        fprintf(stderr, "Please start Chrome with: chrome --remote-debugging-port=%d\n", 
                g_ctx.config.debug_port);
        fprintf(stderr, "Or set CDP_LAUNCH_CHROME=1 to auto-launch\n");
        return -1;
    }
    
    // Launch Chrome
    launch_chrome();
    
    // Verify it started
    test_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (test_sock >= 0) {
        if (connect(test_sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            close(test_sock);
            return 0;
        }
        close(test_sock);
    }
    
    return -1;
}