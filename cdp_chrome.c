/**
 * CDP Chrome Module - Unified Chrome Management
 * Merged from cdp_chrome.c + cdp_process.c + cdp_chrome_mgr.c
 * Handles Chrome discovery, launching, and connection management
 */

#include "cdp_internal.h"
#include "cdp_javascript.h"
/* #include "cdp_log.h" - merged into cdp_internal.h */
/* #include "cdp_bus.h" - merged into cdp_internal.h */
/* #include "cdp_process.h" - merged into cdp_internal.h */
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <dirent.h>
#include <sys/resource.h>

/* External global variables */
extern CDPContext g_ctx;
extern int verbose;
extern int ws_sock;
extern int ws_cmd_id;
extern int gui_mode;  // Add support for GUI mode
extern char proxy_server[];  // Proxy server setting
extern int relaunch_chrome;  // Relaunch flag from CLI

/* Static variables - original Chrome module */
static pid_t chrome_pid = -1;

/* ========================================================================== */
/* CDP PROCESS MANAGEMENT MODULE (from cdp_process.c)                        */
/* ========================================================================== */

/* Global registry */
cdp_chrome_registry_t g_chrome_registry = {0};
static int registry_initialized = 0;
static cdp_health_callback_t health_callback = NULL;
static void* health_callback_data = NULL;

/* Forward declarations */
static int allocate_instance_id(void);
static int allocate_debug_port(void);
static int launch_chrome_process(const cdp_chrome_config_t* config, cdp_chrome_instance_t* instance);
static void update_instance_status(cdp_chrome_instance_t* instance, cdp_chrome_status_t status);
static int cleanup_chrome_temp_files(const char* user_data_dir);

/* ========================================================================== */
/* CDP CHROME MANAGER MODULE (from cdp_chrome_mgr.c)                         */
/* ========================================================================== */

/* Chrome process management structure */
typedef struct {
    pid_t pid;
    int port;
    time_t start_time;
    int restart_count;
    int auto_restart;
    char user_data_dir[256];
    char executable_path[512];
    int headless;
    int sandbox;
    int gpu;
} ChromeProcess;

/* Global Chrome process state */
static ChromeProcess g_chrome = {
    .pid = -1,
    .port = CHROME_DEFAULT_PORT,
    .restart_count = 0,
    .auto_restart = 1,
    .headless = 1,
    .sandbox = 0,
    .gpu = 0
};

/* Process monitoring */
static pthread_t monitor_thread;
static int monitor_running = 0;
static pthread_mutex_t chrome_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward declarations - Chrome Manager */
static void* chrome_monitor_thread(void *arg);
static int cleanup_chrome_resources(void);
static int check_chrome_health(void);
static void setup_signal_handlers(void);

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
                    if (verbose) cdp_log(CDP_LOG_INFO, "CHROME", "Found Chrome at: %s", chrome_path);
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
                        if (verbose) cdp_log(CDP_LOG_INFO, "CHROME", "Found Chrome via PATH: %s", chrome_path);
                        return chrome_path;
                    }
                }
                pclose(fp);
            }
        }
    }
    
    // Chrome not found
    if (verbose) cdp_log(CDP_LOG_ERR, "CHROME", "Chrome/Chromium executable not found");
    if (verbose) cdp_log(CDP_LOG_ERR, "CHROME", "Install Chrome or set CDP_CHROME_PATH env var");
    
    return NULL;
}

/* Launch Chrome with debugging enabled */
void launch_chrome(void) {
    const char *os = cdp_detect_os();
    if (verbose) cdp_log(CDP_LOG_INFO, "CHROME", "Detected OS: %s", os);
    
    // Check if Chrome is already running on the debug port
    int test_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (test_sock >= 0) {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(g_ctx.config.debug_port);
        addr.sin_addr.s_addr = inet_addr(g_ctx.config.chrome_host);
        
        if (connect(test_sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            close(test_sock);
            if (verbose) cdp_log(CDP_LOG_INFO, "CHROME", "Chrome already running on port %d", g_ctx.config.debug_port);
            return;
        }
        close(test_sock);
    }
    
    // Find Chrome executable
    const char *chrome_path = getenv("CDP_CHROME_PATH");
    if (!chrome_path) {
        chrome_path = find_chrome_executable();
        if (!chrome_path) {
            cdp_log(CDP_LOG_ERR, "CHROME", "Chrome executable not found in PATH");
            cdp_log(CDP_LOG_ERR, "CHROME", "Please install Chrome or specify the path");
            exit(1);
        }
    }
    
    // Fork and launch Chrome
    chrome_pid = fork();
    if (chrome_pid == 0) {
        // Child process - launch Chrome
        
        // Redirect stdout/stderr to suppress Chrome's output
        // But in Windows verbose mode, keep stderr to see errors
        const char *os = cdp_detect_os();
        if (!verbose || !strstr(os, "win")) {
            FILE *devnull = fopen(strstr(os, "win") ? "NUL" : "/dev/null", "w");
            if (devnull) {
                int fd = fileno(devnull);
                dup2(fd, STDOUT_FILENO);
                if (!verbose) {
                    dup2(fd, STDERR_FILENO);
                }
                // Don't close devnull here, let it be closed when process exits
            }
        }
        
        // Build argument array
        char port_arg[32];
        snprintf(port_arg, sizeof(port_arg), "--remote-debugging-port=%d", g_ctx.config.debug_port);

        // Windows may need explicit address binding
        char addr_arg[64] = "";
        if (strstr(cdp_detect_os(), "win")) {
            snprintf(addr_arg, sizeof(addr_arg), "--remote-debugging-address=127.0.0.1");
        }
        
        char user_data_arg[256];
        if (g_ctx.config.user_data_dir) {
            snprintf(user_data_arg, sizeof(user_data_arg), "--user-data-dir=%s", 
                     g_ctx.config.user_data_dir);
        } else {
            // Use a unique profile directory for each port to avoid SingletonLock conflicts
            const char *os = cdp_detect_os();
            if (strstr(os, "win")) {
                // On Windows, get current working directory and build absolute path
                // Chrome on Windows requires an absolute path for user-data-dir
                char cwd[512];
                if (getcwd(cwd, sizeof(cwd))) {
                    // Convert Unix-style path to Windows style if needed
                    // /d/dev/path -> D:\dev\path
                    char win_path[512];
                    if (cwd[0] == '/' && isalpha(cwd[1]) && cwd[2] == '/') {
                        // Unix-style path from MSYS/Cygwin
                        snprintf(win_path, sizeof(win_path), "%c:%s",
                                toupper(cwd[1]), cwd + 2);
                        // Replace forward slashes with backslashes
                        for (char *p = win_path; *p; p++) {
                            if (*p == '/') *p = '\\';
                        }
                        snprintf(user_data_arg, sizeof(user_data_arg),
                                "--user-data-dir=%s\\cdp-chrome-profile-%d",
                                win_path, g_ctx.config.debug_port);
                    } else if (strchr(cwd, '/')) {
                        // Still has forward slashes, replace them
                        strcpy(win_path, cwd);
                        for (char *p = win_path; *p; p++) {
                            if (*p == '/') *p = '\\';
                        }
                        snprintf(user_data_arg, sizeof(user_data_arg),
                                "--user-data-dir=%s\\cdp-chrome-profile-%d",
                                win_path, g_ctx.config.debug_port);
                    } else {
                        // Already Windows format
                        snprintf(user_data_arg, sizeof(user_data_arg),
                                "--user-data-dir=%s\\cdp-chrome-profile-%d",
                                cwd, g_ctx.config.debug_port);
                    }
                } else {
                    // Fallback to relative path if getcwd fails
                    snprintf(user_data_arg, sizeof(user_data_arg),
                            "--user-data-dir=.\\cdp-chrome-profile-%d", g_ctx.config.debug_port);
                }
            } else {
                // Unix/Linux/Mac - use /tmp
                snprintf(user_data_arg, sizeof(user_data_arg),
                        "--user-data-dir=/tmp/cdp-chrome-profile-%d", g_ctx.config.debug_port);
            }
        }
        
        // Prepare proxy argument if specified
        char proxy_arg[600] = "";
        if (strlen(proxy_server) > 0) {
            snprintf(proxy_arg, sizeof(proxy_arg), "--proxy-server=%s", proxy_server);
        }
        
        // Execute Chrome directly (no shell)
        // Note: Chrome may fork and the parent process may exit, which is normal
        // Use execl instead of execlp when we have an absolute path
        // Check for both Unix (/) and Windows (C:) absolute paths
        int is_absolute = (chrome_path[0] == '/' ||
                          (isalpha(chrome_path[0]) && chrome_path[1] == ':'));

        // Determine headless argument based on OS
        const char *headless_arg = "--headless=new";
        if (strstr(os, "win")) {
            headless_arg = "--headless";  // Windows may not support --headless=new
        }

        // Debug: Print the actual command being executed
        if (verbose) {
            cdp_log(CDP_LOG_INFO, "CHROME", "Command: %s", chrome_path);
            cdp_log(CDP_LOG_INFO, "CHROME", "Args: %s %s %s ...", port_arg, user_data_arg, headless_arg);
        }

        if (is_absolute) {
            if (gui_mode || getenv("CDP_GUI_MODE")) {
                // GUI mode - visible Chrome window
                if (strlen(proxy_server) > 0) {
                    execl(chrome_path, chrome_path,
                       port_arg,
                       user_data_arg,
                       proxy_arg,
                       "--no-sandbox",
                       "--disable-dev-shm-usage",
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
                    // Add address argument for Windows if needed
                    if (addr_arg[0]) {
                        execl(chrome_path, chrome_path,
                           port_arg,
                           addr_arg,
                           user_data_arg,
                           "--no-sandbox",
                           "--disable-dev-shm-usage",
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
                           user_data_arg,
                           "--no-sandbox",
                           "--disable-dev-shm-usage",
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
                }
            } else {
                // Headless mode (default)
                if (strlen(proxy_server) > 0) {
                    execl(chrome_path, chrome_path,
                       port_arg,
                       user_data_arg,
                       proxy_arg,
                       "--no-sandbox",
                       "--disable-dev-shm-usage",
                       headless_arg,
                       "--disable-gpu",
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
                    // Add address argument for Windows if needed
                    if (addr_arg[0]) {
                        execl(chrome_path, chrome_path,
                           port_arg,
                           addr_arg,  // Add explicit address binding for Windows
                           user_data_arg,
                           "--no-sandbox",
                           "--disable-dev-shm-usage",
                           headless_arg,
                           "--disable-gpu",
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
                           user_data_arg,
                           "--no-sandbox",
                           "--disable-dev-shm-usage",
                           headless_arg,
                           "--disable-gpu",
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
        } else {
            // Use execlp for PATH lookups
            if (gui_mode || getenv("CDP_GUI_MODE")) {
                // GUI mode - visible Chrome window
                if (strlen(proxy_server) > 0) {
                    execlp(chrome_path, chrome_path,
                           port_arg,
                           user_data_arg,
                           proxy_arg,
                           "--no-sandbox",
                           "--disable-dev-shm-usage",
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
                           user_data_arg,
                           "--no-sandbox",
                           "--disable-dev-shm-usage",
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
                           user_data_arg,
                           proxy_arg,
                           "--no-sandbox",
                           "--disable-dev-shm-usage",
                           headless_arg,
                           "--disable-gpu",
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
                           user_data_arg,
                           "--no-sandbox",
                           "--disable-dev-shm-usage",
                           headless_arg,
                           "--disable-gpu",
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
        cdp_log(CDP_LOG_ERR, "CHROME", "Failed to launch Chrome: %s", strerror(errno));
        exit(1);
    } else if (chrome_pid < 0) {
            cdp_log(CDP_LOG_ERR, "CHROME", "fork failed: %s", strerror(errno));
            exit(1);
    }
    
    // Parent process - wait for Chrome to start
    if (verbose) {
        cdp_log(CDP_LOG_INFO, "CHROME", "Chrome launched with PID %d", chrome_pid);
        cdp_log(CDP_LOG_INFO, "CHROME", "Waiting for Chrome to start listening on port %d...", g_ctx.config.debug_port);
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
                if (verbose && WIFEXITED(status)) cdp_log(CDP_LOG_DEBUG, "CHROME", "Chrome parent exited status %d (normal)", WEXITSTATUS(status));
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
                if (verbose) cdp_log(CDP_LOG_INFO, "CHROME", "DevTools listening on port %d", g_ctx.config.debug_port);
                return;
            }
            close(test_sock);
        }
        
        if (verbose && attempts == 30) {
            cdp_log(CDP_LOG_WARN, "CHROME", "Still waiting for Chrome to start...");
            // Check if process is still alive on Windows
            if (strstr(cdp_detect_os(), "win") && chrome_pid > 0) {
                if (kill(chrome_pid, 0) != 0) {
                    cdp_log(CDP_LOG_ERR, "CHROME", "Chrome process (PID %d) has exited", chrome_pid);
                    return;
                }
            }
        }
        if (verbose && attempts == 60) {
            cdp_log(CDP_LOG_WARN, "CHROME", "Chrome taking longer than usual to start...");
            // Try to get more info about what's happening
            if (strstr(cdp_detect_os(), "win")) {
                cdp_log(CDP_LOG_INFO, "CHROME", "Check if Windows Firewall is blocking port %d", g_ctx.config.debug_port);
                cdp_log(CDP_LOG_INFO, "CHROME", "Try running with --gui flag to see Chrome window", g_ctx.config.debug_port);
            }
        }
        
        attempts++;
    }
    
    cdp_log(CDP_LOG_WARN, "CHROME", "Chrome may not have started correctly (pid=%d)", chrome_pid);
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
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(sock); return NULL; }
    
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
    
    // Use elegant JSON API to extract WebSocket URL
    if (cdp_js_get_websocket_url(buffer, target_id, sizeof(target_id)) != 0) {
        return NULL;
    }
    
    return target_id;
}

/* Backward-compatible alias: return browser endpoint target id (browser/xxx) */
static inline const char* get_browser_target_id(void) {
    return get_chrome_target_id();
}

/* Create a new page via browser endpoint */
char* create_new_page_via_browser(int browser_sock) {
    static char page_target_id[256];

    // Use message bus on the browser websocket
    char params[128];
    cdp_js_build_navigate("about:blank", params, sizeof(params));

    extern int ws_sock;
    int old_ws = ws_sock;
    ws_sock = browser_sock;

    char response[4096];
    int rc = cdp_call_cmd("Target.createTarget", params, response, sizeof(response), DEFAULT_TIMEOUT_MS);
    ws_sock = old_ws;
    if (rc != 0) {
        if (verbose) printf("Target.createTarget failed with rc=%d\n", rc);
        return NULL;
    }
    if (verbose) printf("Target.createTarget response: %.200s\n", response);

    // Use elegant JSON API to extract target ID from nested result
    if (cdp_js_get_nested(response, "result.targetId", page_target_id, sizeof(page_target_id)) != 0) {
        // Fallback: try to parse manually if JSON API fails
        if (verbose) printf("JSON API failed, response: %.500s\n", response);
        const char *tid = strstr(response, "\"targetId\"");
        if (!tid) return NULL;
        tid = strchr(tid, ':');
        if (!tid) return NULL;
        tid++;
        while (*tid == ' ' || *tid == '\t') tid++;
        if (*tid != '"') return NULL;
        tid++;
        const char *end = strchr(tid, '"');
        if (!end) return NULL;
        size_t tid_len = end - tid;
        if (tid_len >= sizeof(page_target_id)) tid_len = sizeof(page_target_id) - 1;
        strncpy(page_target_id, tid, tid_len);
        page_target_id[tid_len] = '\0';
    }
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
        if (!relaunch_chrome) {
            return 0;  // Chrome is running and we don't relaunch
        }

        // Relaunch requested: try to close the existing browser via Browser.close
        if (verbose) cdp_log(CDP_LOG_INFO, "CHROME", "Relaunch requested, attempting Browser.close on existing instance");

        // Obtain browser target id from /json/version
        const char *browser_target = get_browser_target_id();
        if (browser_target && strstr(browser_target, "browser/")) {
            int tmp_sock = connect_chrome_websocket(browser_target);
            if (tmp_sock >= 0) {
                CDPJSONBuilder builder;
                cdp_js_builder_init(&builder);
                cdp_js_builder_add_int(&builder, "id", 1);
                cdp_js_builder_add_string(&builder, "method", "Browser.close");
                char cmd[128]; strncpy(cmd, cdp_js_builder_get(&builder), sizeof(cmd));
                ws_send_text(tmp_sock, cmd);
                // best-effort; socket will likely close from peer
                close(tmp_sock);
            }
        }

        // Wait for port to close
        for (int i = 0; i < 50; i++) { // ~5s
            int s2 = socket(AF_INET, SOCK_STREAM, 0);
            if (s2 >= 0) {
                int ok = connect(s2, (struct sockaddr*)&addr, sizeof(addr));
                close(s2);
                if (ok != 0) break; // closed
            }
            usleep(100 * 1000);
        }
        // Proceed to launch a fresh Chrome below
    }
    close(test_sock);
    
    // Always auto-launch if missing, or when re-launch was requested.
    
    // Auto-launch Chrome by default
    if (verbose) cdp_log(CDP_LOG_INFO, "CHROME", "Chrome not found on port %d, auto-launching...", g_ctx.config.debug_port);
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
            cdp_log(CDP_LOG_INFO, "CHROME", "Verifying Chrome startup...");
        }
        
        usleep(100000);  // 100ms
        verify_attempts++;
    }
    
    cdp_log(CDP_LOG_ERR, "CHROME", "Failed to connect to Chrome after launch. It may still be starting.");
    return -1;
}

/* ========================================================================== */
/* CDP PROCESS MANAGEMENT FUNCTIONS (from cdp_process.c)                     */
/* ========================================================================== */

/* Initialize Chrome registry */
int cdp_init_chrome_registry(void) {
    if (registry_initialized) {
        return CDP_PROCESS_SUCCESS;
    }
    
    memset(&g_chrome_registry, 0, sizeof(g_chrome_registry));
    
    if (pthread_mutex_init(&g_chrome_registry.registry_mutex, NULL) != 0) {
        return CDP_PROCESS_ERR_MEMORY;
    }
    
    g_chrome_registry.next_instance_id = 1;
    g_chrome_registry.next_debug_port = 9222;
    g_chrome_registry.health_check_interval = CDP_DEFAULT_HEALTH_CHECK_INTERVAL;
    g_chrome_registry.auto_cleanup_enabled = 1;
    
    /* Set default temp directory */
    const char* temp_dir = getenv("TMPDIR");
    if (!temp_dir) temp_dir = getenv("TMP");
    if (!temp_dir) temp_dir = "/tmp";
    
    snprintf(g_chrome_registry.temp_dir, sizeof(g_chrome_registry.temp_dir), 
             "%s/cdp_chrome", temp_dir);
    
    /* Create temp directory if it doesn't exist */
    mkdir(g_chrome_registry.temp_dir, 0755);
    
    registry_initialized = 1;
    return CDP_PROCESS_SUCCESS;
}

/* Cleanup Chrome registry */
int cdp_cleanup_chrome_registry(void) {
    if (!registry_initialized) {
        return CDP_PROCESS_SUCCESS;
    }
    
    pthread_mutex_lock(&g_chrome_registry.registry_mutex);
    
    /* Kill all running instances */
    for (int i = 0; i < g_chrome_registry.instance_count; i++) {
        cdp_chrome_instance_t* instance = &g_chrome_registry.instances[i];
        if (instance->status == CDP_CHROME_STATUS_RUNNING) {
            cdp_kill_chrome_instance(instance->instance_id, 1);
        }
        pthread_mutex_destroy(&instance->mutex);
    }
    
    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
    pthread_mutex_destroy(&g_chrome_registry.registry_mutex);
    
    registry_initialized = 0;
    return CDP_PROCESS_SUCCESS;
}

/* Create default Chrome configuration */
int cdp_create_chrome_config(cdp_chrome_config_t* config) {
    if (!config) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }
    
    memset(config, 0, sizeof(cdp_chrome_config_t));
    
    /* Set default values */
    config->debug_port = 0;  /* Will be auto-assigned */
    config->window_width = 1280;
    config->window_height = 720;
    config->headless = 1;  /* Default to headless for automation */
    config->incognito = 0;
    config->disable_gpu = 1;
    config->no_sandbox = 1;  /* Often needed in containerized environments */
    config->disable_dev_shm_usage = 1;
    config->memory_limit_mb = 512;
    config->timeout_sec = CDP_PROCESS_TIMEOUT_SEC;
    config->auto_restart = 1;
    config->max_restart_attempts = CDP_MAX_RESTART_ATTEMPTS;
    config->created_time = time(NULL);
    
    /* Set default user agent */
    snprintf(config->user_agent, sizeof(config->user_agent),
             "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
             "Chrome/120.0.0.0 Safari/537.36 CDP-Client/1.0");
    
    return CDP_PROCESS_SUCCESS;
}

/* Validate Chrome configuration */
int cdp_validate_chrome_config(const cdp_chrome_config_t* config) {
    if (!config) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }
    
    /* Check port range */
    if (config->debug_port != 0 && (config->debug_port < 1024 || config->debug_port > 65535)) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }
    
    /* Check window dimensions */
    if (config->window_width < 100 || config->window_width > 4096 ||
        config->window_height < 100 || config->window_height > 4096) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }
    
    /* Check memory limit */
    if (config->memory_limit_mb < 64 || config->memory_limit_mb > 8192) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }
    
    /* Check timeout */
    if (config->timeout_sec < 5 || config->timeout_sec > 300) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }
    
    /* Validate paths if specified */
    if (strlen(config->profile_dir) > 0) {
        struct stat st;
        if (stat(config->profile_dir, &st) == 0 && !S_ISDIR(st.st_mode)) {
            return CDP_PROCESS_ERR_INVALID_CONFIG;
        }
    }
    
    return CDP_PROCESS_SUCCESS;
}

/* Launch Chrome instance */
int cdp_launch_chrome_instance(const cdp_chrome_config_t* config, cdp_chrome_instance_t* instance) {
    if (!config || !instance) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }
    
    if (!registry_initialized) {
        int ret = cdp_init_chrome_registry();
        if (ret != CDP_PROCESS_SUCCESS) {
            return ret;
        }
    }
    
    /* Validate configuration */
    int ret = cdp_validate_chrome_config(config);
    if (ret != CDP_PROCESS_SUCCESS) {
        return ret;
    }
    
    pthread_mutex_lock(&g_chrome_registry.registry_mutex);
    
    /* Check instance limit */
    if (g_chrome_registry.instance_count >= CDP_MAX_CHROME_INSTANCES) {
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_ERR_INSTANCE_LIMIT_REACHED;
    }
    
    /* Find available slot */
    cdp_chrome_instance_t* new_instance = NULL;
    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        if (g_chrome_registry.instances[i].status == CDP_CHROME_STATUS_UNKNOWN) {
            new_instance = &g_chrome_registry.instances[i];
            break;
        }
    }
    
    if (!new_instance) {
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_ERR_INSTANCE_LIMIT_REACHED;
    }
    
    /* Initialize instance */
    memset(new_instance, 0, sizeof(cdp_chrome_instance_t));
    new_instance->instance_id = allocate_instance_id();
    new_instance->debug_port = (config->debug_port > 0) ? config->debug_port : allocate_debug_port();
    new_instance->config = *config;
    new_instance->start_time = time(NULL);
    new_instance->auto_restart_enabled = config->auto_restart;
    
    if (pthread_mutex_init(&new_instance->mutex, NULL) != 0) {
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_ERR_MEMORY;
    }
    
    /* Launch Chrome process */
    ret = launch_chrome_process(config, new_instance);
    if (ret != CDP_PROCESS_SUCCESS) {
        pthread_mutex_destroy(&new_instance->mutex);
        memset(new_instance, 0, sizeof(cdp_chrome_instance_t));
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return ret;
    }
    
    g_chrome_registry.instance_count++;
    
    /* Copy instance data to output */
    *instance = *new_instance;
    
    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
    
    return CDP_PROCESS_SUCCESS;
}

/* Allocate unique instance ID */
static int allocate_instance_id(void) {
    return g_chrome_registry.next_instance_id++;
}

/* Allocate available debug port */
static int allocate_debug_port(void) {
    int port = g_chrome_registry.next_debug_port;
    
    /* Check if port is already in use */
    while (port < 65535) {
        int in_use = 0;
        for (int i = 0; i < g_chrome_registry.instance_count; i++) {
            if (g_chrome_registry.instances[i].debug_port == port) {
                in_use = 1;
                break;
            }
        }
        
        if (!in_use) {
            g_chrome_registry.next_debug_port = port + 1;
            return port;
        }
        port++;
    }
    
    return 9222;  /* Fallback to default */
}

/* Update instance status with thread safety */
static void update_instance_status(cdp_chrome_instance_t* instance, cdp_chrome_status_t status) {
    if (!instance) return;
    
    pthread_mutex_lock(&instance->mutex);
    instance->status = status;
    instance->last_activity = time(NULL);
    pthread_mutex_unlock(&instance->mutex);
}

/* Get Chrome status string */
const char* cdp_chrome_status_to_string(cdp_chrome_status_t status) {
    switch (status) {
        case CDP_CHROME_STATUS_UNKNOWN: return "unknown";
        case CDP_CHROME_STATUS_STARTING: return "starting";
        case CDP_CHROME_STATUS_RUNNING: return "running";
        case CDP_CHROME_STATUS_STOPPING: return "stopping";
        case CDP_CHROME_STATUS_STOPPED: return "stopped";
        case CDP_CHROME_STATUS_CRASHED: return "crashed";
        case CDP_CHROME_STATUS_FAILED: return "failed";
        default: return "invalid";
    }
}

/* Launch Chrome process implementation */
static int launch_chrome_process(const cdp_chrome_config_t* config, cdp_chrome_instance_t* instance) {
    /* Find Chrome executable */
    char* chrome_path = find_chrome_executable();
    if (!chrome_path || strlen(chrome_path) == 0) {
        snprintf(instance->last_error, sizeof(instance->last_error),
                "Chrome executable not found");
        return CDP_PROCESS_ERR_LAUNCH_FAILED;
    }

    /* Create user data directory if specified */
    if (strlen(config->user_data_dir) > 0) {
        if (mkdir(config->user_data_dir, 0755) != 0 && errno != EEXIST) {
            snprintf(instance->last_error, sizeof(instance->last_error),
                    "Failed to create user data directory: %s", strerror(errno));
            return CDP_PROCESS_ERR_LAUNCH_FAILED;
        }
        strncpy(instance->user_data_dir, config->user_data_dir, sizeof(instance->user_data_dir) - 1);
    } else {
        /* Create temporary user data directory */
        snprintf(instance->user_data_dir, sizeof(instance->user_data_dir),
                "%s/chrome_instance_%d", g_chrome_registry.temp_dir, instance->instance_id);
        if (mkdir(instance->user_data_dir, 0755) != 0 && errno != EEXIST) {
            snprintf(instance->last_error, sizeof(instance->last_error),
                    "Failed to create temp user data directory: %s", strerror(errno));
            return CDP_PROCESS_ERR_LAUNCH_FAILED;
        }
    }

    /* Build Chrome command line arguments */
    char* args[64];
    int argc = 0;

    args[argc++] = chrome_path;

    /* Debug port */
    static char debug_port_arg[64];
    snprintf(debug_port_arg, sizeof(debug_port_arg), "--remote-debugging-port=%d", instance->debug_port);
    args[argc++] = debug_port_arg;

    /* User data directory */
    static char user_data_arg[CDP_MAX_PATH_LENGTH + 32];
    snprintf(user_data_arg, sizeof(user_data_arg), "--user-data-dir=%s", instance->user_data_dir);
    args[argc++] = user_data_arg;

    /* Window size */
    static char window_size_arg[64];
    snprintf(window_size_arg, sizeof(window_size_arg), "--window-size=%d,%d",
             config->window_width, config->window_height);
    args[argc++] = window_size_arg;

    /* Common flags for automation */
    if (config->headless) {
        args[argc++] = "--headless";
    }
    if (config->no_sandbox) {
        args[argc++] = "--no-sandbox";
    }
    if (config->disable_gpu) {
        args[argc++] = "--disable-gpu";
    }
    if (config->disable_dev_shm_usage) {
        args[argc++] = "--disable-dev-shm-usage";
    }
    if (config->incognito) {
        args[argc++] = "--incognito";
    }

    /* Additional flags */
    args[argc++] = "--disable-background-timer-throttling";
    args[argc++] = "--disable-backgrounding-occluded-windows";
    args[argc++] = "--disable-renderer-backgrounding";
    args[argc++] = "--disable-features=TranslateUI";
    args[argc++] = "--disable-ipc-flooding-protection";
    args[argc++] = "--no-first-run";
    args[argc++] = "--no-default-browser-check";

    /* Proxy settings */
    static char proxy_arg[512];
    if (strlen(config->proxy_server) > 0) {
        snprintf(proxy_arg, sizeof(proxy_arg), "--proxy-server=%s", config->proxy_server);
        args[argc++] = proxy_arg;
    }

    /* User agent */
    static char user_agent_arg[768];
    if (strlen(config->user_agent) > 0) {
        snprintf(user_agent_arg, sizeof(user_agent_arg), "--user-agent=%s", config->user_agent);
        args[argc++] = user_agent_arg;
    }

    /* Memory limit */
    static char memory_arg[64];
    snprintf(memory_arg, sizeof(memory_arg), "--max_old_space_size=%d", config->memory_limit_mb);
    args[argc++] = memory_arg;

    /* Extra flags from config */
    static char flags_copy[CDP_MAX_FLAGS_LENGTH];
    if (strlen(config->extra_flags) > 0) {
        /* Simple parsing - split by spaces */
        strncpy(flags_copy, config->extra_flags, sizeof(flags_copy) - 1);
        flags_copy[sizeof(flags_copy) - 1] = '\0';
        char* token = strtok(flags_copy, " ");
        while (token && argc < 60) {  /* Leave room for NULL terminator */
            args[argc++] = token;
            token = strtok(NULL, " ");
        }
    }

    /* Default page */
    args[argc++] = "about:blank";
    args[argc] = NULL;

    /* Update status */
    update_instance_status(instance, CDP_CHROME_STATUS_STARTING);

    /* Fork and exec Chrome */
    pid_t pid = fork();
    if (pid == -1) {
        snprintf(instance->last_error, sizeof(instance->last_error),
                "Failed to fork process: %s", strerror(errno));
        return CDP_PROCESS_ERR_LAUNCH_FAILED;
    }

    if (pid == 0) {
        /* Child process - exec Chrome */
        execv(chrome_path, args);
        /* If we get here, exec failed */
        cdp_log(CDP_LOG_ERR, "PROC", "Failed to exec Chrome: %s", strerror(errno));
        _exit(1);
    }

    /* Parent process */
    instance->pid = pid;
    update_instance_status(instance, CDP_CHROME_STATUS_RUNNING);

    /* Wait a moment for Chrome to start */
    usleep(500000);  /* 500ms */

    /* Verify Chrome is still running */
    if (!cdp_is_chrome_running(pid)) {
        snprintf(instance->last_error, sizeof(instance->last_error),
                "Chrome process exited immediately after launch");
        update_instance_status(instance, CDP_CHROME_STATUS_FAILED);
        return CDP_PROCESS_ERR_LAUNCH_FAILED;
    }

    return CDP_PROCESS_SUCCESS;
}

/* Check if Chrome process is running */
int cdp_is_chrome_running(pid_t pid) {
    if (pid <= 0) return 0;

    /* Send signal 0 to check if process exists */
    return (kill(pid, 0) == 0);
}

/* Kill Chrome instance */
int cdp_kill_chrome_instance(int instance_id, int force) {
    if (!registry_initialized) {
        return CDP_PROCESS_ERR_INSTANCE_NOT_FOUND;
    }

    pthread_mutex_lock(&g_chrome_registry.registry_mutex);

    cdp_chrome_instance_t* instance = NULL;
    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        if (g_chrome_registry.instances[i].instance_id == instance_id) {
            instance = &g_chrome_registry.instances[i];
            break;
        }
    }

    if (!instance || instance->status == CDP_CHROME_STATUS_UNKNOWN) {
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_ERR_INSTANCE_NOT_FOUND;
    }

    pthread_mutex_lock(&instance->mutex);

    if (instance->status == CDP_CHROME_STATUS_STOPPED) {
        pthread_mutex_unlock(&instance->mutex);
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_SUCCESS;
    }

    update_instance_status(instance, CDP_CHROME_STATUS_STOPPING);

    int result = CDP_PROCESS_SUCCESS;

    if (instance->pid > 0) {
        /* Try graceful shutdown first */
        if (!force && kill(instance->pid, SIGTERM) == 0) {
            /* Wait up to 10 seconds for graceful shutdown */
            for (int i = 0; i < 100; i++) {
                if (!cdp_is_chrome_running(instance->pid)) {
                    break;
                }
                usleep(100000);  /* 100ms */
            }
        }

        /* Force kill if still running */
        if (cdp_is_chrome_running(instance->pid)) {
            if (kill(instance->pid, SIGKILL) != 0) {
                snprintf(instance->last_error, sizeof(instance->last_error),
                        "Failed to kill Chrome process: %s", strerror(errno));
                result = CDP_PROCESS_ERR_KILL_FAILED;
            }
        }

        /* Wait for process to exit */
        int status;
        waitpid(instance->pid, &status, 0);
    }

    /* Cleanup resources */
    cleanup_chrome_temp_files(instance->user_data_dir);

    update_instance_status(instance, CDP_CHROME_STATUS_STOPPED);

    pthread_mutex_unlock(&instance->mutex);

    /* Remove from registry */
    if (result == CDP_PROCESS_SUCCESS) {
        pthread_mutex_destroy(&instance->mutex);
        memset(instance, 0, sizeof(cdp_chrome_instance_t));
        g_chrome_registry.instance_count--;
    }

    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);

    return result;
}

/* Cleanup Chrome temporary files */
static int cleanup_chrome_temp_files(const char* user_data_dir) {
    if (!user_data_dir || strlen(user_data_dir) == 0) {
        return CDP_PROCESS_SUCCESS;
    }

    /* Only cleanup temp directories we created */
    if (strstr(user_data_dir, g_chrome_registry.temp_dir) != user_data_dir) {
        return CDP_PROCESS_SUCCESS;
    }

    /* Simple recursive directory removal */
    char command[CDP_MAX_PATH_LENGTH + 32];
    snprintf(command, sizeof(command), "rm -rf \"%s\"", user_data_dir);

    int result = system(command);
    return (result == 0) ? CDP_PROCESS_SUCCESS : CDP_PROCESS_ERR_CLEANUP_FAILED;
}

/* List Chrome instances */
int cdp_list_chrome_instances(cdp_chrome_instance_t** instances, int* count) {
    if (!instances || !count) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }

    if (!registry_initialized) {
        *instances = NULL;
        *count = 0;
        return CDP_PROCESS_SUCCESS;
    }

    pthread_mutex_lock(&g_chrome_registry.registry_mutex);

    /* Count active instances */
    int active_count = 0;
    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        if (g_chrome_registry.instances[i].status != CDP_CHROME_STATUS_UNKNOWN) {
            active_count++;
        }
    }

    if (active_count == 0) {
        *instances = NULL;
        *count = 0;
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_SUCCESS;
    }

    /* Allocate result array */
    cdp_chrome_instance_t* result = malloc(active_count * sizeof(cdp_chrome_instance_t));
    if (!result) {
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_ERR_MEMORY;
    }

    /* Copy active instances */
    int result_index = 0;
    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        if (g_chrome_registry.instances[i].status != CDP_CHROME_STATUS_UNKNOWN) {
            result[result_index++] = g_chrome_registry.instances[i];
        }
    }

    *instances = result;
    *count = active_count;

    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
    return CDP_PROCESS_SUCCESS;
}

/* Get instance status */
int cdp_get_instance_status(int instance_id, cdp_chrome_instance_t* status) {
    if (!status) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }

    if (!registry_initialized) {
        return CDP_PROCESS_ERR_INSTANCE_NOT_FOUND;
    }

    pthread_mutex_lock(&g_chrome_registry.registry_mutex);

    cdp_chrome_instance_t* instance = NULL;
    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        if (g_chrome_registry.instances[i].instance_id == instance_id) {
            instance = &g_chrome_registry.instances[i];
            break;
        }
    }

    if (!instance || instance->status == CDP_CHROME_STATUS_UNKNOWN) {
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_ERR_INSTANCE_NOT_FOUND;
    }

    pthread_mutex_lock(&instance->mutex);
    *status = *instance;
    pthread_mutex_unlock(&instance->mutex);

    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
    return CDP_PROCESS_SUCCESS;
}

/* Check instance health */
int cdp_check_instance_health(int instance_id) {
    if (!registry_initialized) {
        return CDP_PROCESS_ERR_INSTANCE_NOT_FOUND;
    }

    pthread_mutex_lock(&g_chrome_registry.registry_mutex);

    cdp_chrome_instance_t* instance = NULL;
    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        if (g_chrome_registry.instances[i].instance_id == instance_id) {
            instance = &g_chrome_registry.instances[i];
            break;
        }
    }

    if (!instance || instance->status == CDP_CHROME_STATUS_UNKNOWN) {
        pthread_mutex_unlock(&g_chrome_registry.registry_mutex);
        return CDP_PROCESS_ERR_INSTANCE_NOT_FOUND;
    }

    pthread_mutex_lock(&instance->mutex);

    int result = CDP_PROCESS_SUCCESS;

    /* Check if process is still running */
    if (!cdp_is_chrome_running(instance->pid)) {
        update_instance_status(instance, CDP_CHROME_STATUS_CRASHED);
        instance->health_check_failures++;

        /* Trigger health callback if set */
        if (health_callback) {
            health_callback(instance, health_callback_data);
        }

        result = CDP_PROCESS_ERR_HEALTH_CHECK_FAILED;
    } else {
        /* Process is running - update last health check time */
        instance->last_health_check = time(NULL);
        instance->health_check_failures = 0;
    }

    pthread_mutex_unlock(&instance->mutex);
    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);

    return result;
}

/* Set health check callback */
int cdp_set_health_check_callback(cdp_health_callback_t callback, void* user_data) {
    health_callback = callback;
    health_callback_data = user_data;
    return CDP_PROCESS_SUCCESS;
}

/* Emergency cleanup all Chrome processes */
int cdp_emergency_cleanup_chrome_processes(void) {
    if (!registry_initialized) {
        return CDP_PROCESS_SUCCESS;
    }

    pthread_mutex_lock(&g_chrome_registry.registry_mutex);

    int cleaned_count = 0;

    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        cdp_chrome_instance_t* instance = &g_chrome_registry.instances[i];

        if (instance->status != CDP_CHROME_STATUS_UNKNOWN &&
            instance->status != CDP_CHROME_STATUS_STOPPED) {

            /* Force kill the process */
            if (instance->pid > 0 && cdp_is_chrome_running(instance->pid)) {
                kill(instance->pid, SIGKILL);
                waitpid(instance->pid, NULL, 0);
            }

            /* Cleanup temp files */
            cleanup_chrome_temp_files(instance->user_data_dir);

            /* Reset instance */
            pthread_mutex_destroy(&instance->mutex);
            memset(instance, 0, sizeof(cdp_chrome_instance_t));
            cleaned_count++;
        }
    }

    g_chrome_registry.instance_count = 0;

    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);

    return cleaned_count;
}

/* Copy Chrome configuration */
int cdp_copy_chrome_config(const cdp_chrome_config_t* src, cdp_chrome_config_t* dst) {
    if (!src || !dst) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }

    *dst = *src;
    return CDP_PROCESS_SUCCESS;
}

/* Get registry statistics */
int cdp_get_registry_stats(int* total_instances, int* running_instances, int* failed_instances) {
    if (!total_instances || !running_instances || !failed_instances) {
        return CDP_PROCESS_ERR_INVALID_CONFIG;
    }

    if (!registry_initialized) {
        *total_instances = 0;
        *running_instances = 0;
        *failed_instances = 0;
        return CDP_PROCESS_SUCCESS;
    }

    pthread_mutex_lock(&g_chrome_registry.registry_mutex);

    int total = 0, running = 0, failed = 0;

    for (int i = 0; i < CDP_MAX_CHROME_INSTANCES; i++) {
        cdp_chrome_instance_t* instance = &g_chrome_registry.instances[i];
        if (instance->status != CDP_CHROME_STATUS_UNKNOWN) {
            total++;
            if (instance->status == CDP_CHROME_STATUS_RUNNING) {
                running++;
            } else if (instance->status == CDP_CHROME_STATUS_CRASHED ||
                      instance->status == CDP_CHROME_STATUS_FAILED) {
                failed++;
            }
        }
    }

    *total_instances = total;
    *running_instances = running;
    *failed_instances = failed;

    pthread_mutex_unlock(&g_chrome_registry.registry_mutex);

    return CDP_PROCESS_SUCCESS;
}

/* Get process error string */
const char* cdp_process_error_to_string(cdp_process_error_t error) {
    switch (error) {
        case CDP_PROCESS_SUCCESS: return "success";
        case CDP_PROCESS_ERR_INVALID_CONFIG: return "invalid configuration";
        case CDP_PROCESS_ERR_LAUNCH_FAILED: return "launch failed";
        case CDP_PROCESS_ERR_INSTANCE_NOT_FOUND: return "instance not found";
        case CDP_PROCESS_ERR_INSTANCE_LIMIT_REACHED: return "instance limit reached";
        case CDP_PROCESS_ERR_KILL_FAILED: return "kill failed";
        case CDP_PROCESS_ERR_HEALTH_CHECK_FAILED: return "health check failed";
        case CDP_PROCESS_ERR_CLEANUP_FAILED: return "cleanup failed";
        case CDP_PROCESS_ERR_PORT_CONFLICT: return "port conflict";
        case CDP_PROCESS_ERR_PERMISSION_DENIED: return "permission denied";
        case CDP_PROCESS_ERR_TIMEOUT: return "timeout";
        case CDP_PROCESS_ERR_MEMORY: return "memory error";
        default: return "unknown error";
    }
}

/* ========================================================================== */
/* CDP CHROME MANAGER FUNCTIONS (from cdp_chrome_mgr.c)                      */
/* ========================================================================== */

/* Initialize Chrome manager */
int cdp_start_chrome_manager(void) {
    pthread_mutex_lock(&chrome_mutex);
    
    // Set default values
    g_chrome.port = g_ctx.config.debug_port;
    
    // Use temp directory for user data if not specified
    if (g_chrome.user_data_dir[0] == '\0') {
        snprintf(g_chrome.user_data_dir, sizeof(g_chrome.user_data_dir),
                "/tmp/cdp-chrome-profile-%d", getpid());
    }
    
    // Find Chrome executable
    char *chrome_path = find_chrome_executable();
    if (chrome_path) {
        str_copy_safe(g_chrome.executable_path, chrome_path, 
                     sizeof(g_chrome.executable_path));
    }
    
    // Setup signal handlers
    setup_signal_handlers();
    
    // Start monitor thread
    monitor_running = 1;
    if (pthread_create(&monitor_thread, NULL, chrome_monitor_thread, NULL) != 0) {
        monitor_running = 0;
        pthread_mutex_unlock(&chrome_mutex);
        return cdp_error_push(CDP_ERR_CHROME_NOT_FOUND, 
                             "Failed to start Chrome monitor thread");
    }
    
    pthread_mutex_unlock(&chrome_mutex);
    
    if (verbose) {
        cdp_log(CDP_LOG_INFO, "CHROME", "Chrome manager initialized");
    }
    
    return 0;
}

/* Shutdown Chrome manager */
int cdp_stop_chrome_manager(void) {
    pthread_mutex_lock(&chrome_mutex);
    monitor_running = 0;
    pthread_mutex_unlock(&chrome_mutex);
    
    // Wait for monitor thread
    pthread_join(monitor_thread, NULL);
    
    // Stop Chrome if we started it
    cdp_chrome_stop();
    
    // Cleanup resources
    cleanup_chrome_resources();
    
    if (verbose) {
        cdp_log(CDP_LOG_INFO, "CHROME", "Chrome manager shutdown");
    }
    
    return 0;
}

/* Start Chrome with optimized settings */
int cdp_chrome_start(void) {
    pthread_mutex_lock(&chrome_mutex);
    
    // Check if already running
    if (g_chrome.pid > 0 && check_chrome_health() == 0) {
        pthread_mutex_unlock(&chrome_mutex);
        return 0;
    }
    
    // Check executable
    if (g_chrome.executable_path[0] == '\0') {
        pthread_mutex_unlock(&chrome_mutex);
        return cdp_error_push(CDP_ERR_CHROME_NOT_FOUND, 
                             "Chrome executable not found");
    }
    
    // Fork and launch Chrome
    pid_t pid = fork();
    if (pid < 0) {
        pthread_mutex_unlock(&chrome_mutex);
        return cdp_error_push(CDP_ERR_CHROME_NOT_FOUND, 
                             "Failed to fork: %s", strerror(errno));
    }
    
    if (pid == 0) {
        // Child process - launch Chrome
        
        // Set resource limits to prevent memory leaks
        struct rlimit limit;
        limit.rlim_cur = 512 * 1024 * 1024; // 512MB
        limit.rlim_max = 1024 * 1024 * 1024; // 1GB
        setrlimit(RLIMIT_AS, &limit);
        
        // Build argument list
        char *args[64];
        int argc = 0;
        
        args[argc++] = g_chrome.executable_path;
        
        // Debug port
        char port_arg[32];
        snprintf(port_arg, sizeof(port_arg), "--remote-debugging-port=%d", g_chrome.port);
        args[argc++] = port_arg;
        
        // User data directory
        char data_dir_arg[512];
        snprintf(data_dir_arg, sizeof(data_dir_arg), "--user-data-dir=%s", 
                g_chrome.user_data_dir);
        args[argc++] = data_dir_arg;
        
        // Headless mode
        if (g_chrome.headless) {
            args[argc++] = "--headless=new";
        }
        
        // Sandbox
        if (!g_chrome.sandbox) {
            args[argc++] = "--no-sandbox";
            args[argc++] = "--disable-setuid-sandbox";
        }
        
        // GPU
        if (!g_chrome.gpu) {
            args[argc++] = "--disable-gpu";
            args[argc++] = "--disable-software-rasterizer";
        }
        
        // Performance optimizations
        args[argc++] = "--disable-dev-shm-usage";
        args[argc++] = "--disable-extensions";
        args[argc++] = "--disable-plugins";
        args[argc++] = "--disable-images";
        args[argc++] = "--disable-background-timer-throttling";
        args[argc++] = "--disable-backgrounding-occluded-windows";
        args[argc++] = "--disable-renderer-backgrounding";
        args[argc++] = "--disable-features=TranslateUI";
        args[argc++] = "--disable-ipc-flooding-protection";
        
        // Memory optimizations
        args[argc++] = "--max_old_space_size=512";
        args[argc++] = "--memory-pressure-off";
        args[argc++] = "--disable-background-networking";
        
        // Security
        args[argc++] = "--disable-web-security";
        args[argc++] = "--allow-running-insecure-content";
        
        // Initial page
        args[argc++] = "about:blank";
        
        args[argc] = NULL;
        
        // // Redirect output to /dev/null
        // int devnull = open("/dev/null", O_RDWR);
        // if (devnull >= 0) {
        //     dup2(devnull, STDOUT_FILENO);
        //     dup2(devnull, STDERR_FILENO);
        //     close(devnull);
        // }
        
        // Execute Chrome
        execv(g_chrome.executable_path, args);
        
        // If we get here, exec failed
        exit(1);
    }
    
    // Parent process
    g_chrome.pid = pid;
    g_chrome.start_time = time(NULL);
    
    pthread_mutex_unlock(&chrome_mutex);
    
    // Wait for Chrome to be ready
    int attempts = 0;
    while (attempts < 30) { // 3 seconds
        usleep(100000); // 100ms
        
        if (check_chrome_health() == 0) {
            if (verbose) {
                cdp_log(CDP_LOG_INFO, "CHROME", "Chrome started successfully (PID: %d)", pid);
            }
            return 0;
        }
        attempts++;
    }
    
    return cdp_error_push(CDP_ERR_CHROME_NOT_FOUND, 
                         "Chrome failed to start within timeout");
}

/* Stop Chrome gracefully */
int cdp_chrome_stop(void) {
    pthread_mutex_lock(&chrome_mutex);
    
    if (g_chrome.pid <= 0) {
        pthread_mutex_unlock(&chrome_mutex);
        return 0;
    }
    
    pid_t pid = g_chrome.pid;
    
    // Try graceful shutdown first
    kill(pid, SIGTERM);
    
    pthread_mutex_unlock(&chrome_mutex);
    
    // Wait for process to exit
    int status;
    int attempts = 0;
    while (attempts < 50) { // 5 seconds
        if (waitpid(pid, &status, WNOHANG) == pid) {
            break;
        }
        usleep(100000); // 100ms
        attempts++;
    }
    
    // Force kill if still running
    if (kill(pid, 0) == 0) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    
    pthread_mutex_lock(&chrome_mutex);
    g_chrome.pid = -1;
    pthread_mutex_unlock(&chrome_mutex);
    
    // Cleanup resources
    cleanup_chrome_resources();
    
    if (verbose) {
        cdp_log(CDP_LOG_INFO, "CHROME", "Chrome stopped (PID: %d)", pid);
    }
    
    return 0;
}

/* Restart Chrome */
int cdp_chrome_restart(void) {
    if (verbose) {
        cdp_log(CDP_LOG_INFO, "CHROME", "Restarting Chrome...");
    }
    
    cdp_chrome_stop();
    usleep(500000); // 500ms delay
    
    pthread_mutex_lock(&chrome_mutex);
    g_chrome.restart_count++;
    pthread_mutex_unlock(&chrome_mutex);
    
    return cdp_chrome_start();
}

/* Chrome monitor thread */
static void* chrome_monitor_thread(void *arg) {
    (void)arg;
    
    while (monitor_running) {
        sleep(5); // Check every 5 seconds
        
        pthread_mutex_lock(&chrome_mutex);
        
        if (g_chrome.pid > 0) {
            // Check if process is still alive
            if (kill(g_chrome.pid, 0) != 0) {
                // Process died
                if (verbose) {
                    cdp_log(CDP_LOG_WARN, "CHROME", "Chrome process died unexpectedly");
                }
                
                g_chrome.pid = -1;
                
                // Auto-restart if enabled
                if (g_chrome.auto_restart && g_chrome.restart_count < 10) {
                    pthread_mutex_unlock(&chrome_mutex);
                    cdp_chrome_restart();
                    continue;
                }
            } else {
                // Check memory usage
                char stat_path[256];
                snprintf(stat_path, sizeof(stat_path), 
                        "/proc/%d/statm", g_chrome.pid);
                
                FILE *fp = fopen(stat_path, "r");
                if (fp) {
                    unsigned long rss_pages;
                    fscanf(fp, "%*lu %lu", &rss_pages);
                    fclose(fp);
                    
                    unsigned long rss_mb = (rss_pages * 4096) / (1024 * 1024);
                    
                    // Restart if using too much memory (>1GB)
                    if (rss_mb > 1024) {
                        if (verbose) {
                            cdp_log(CDP_LOG_WARN, "CHROME", "Chrome using too much memory (%luMB), restarting...", 
                                  rss_mb);
                        }
                        pthread_mutex_unlock(&chrome_mutex);
                        cdp_chrome_restart();
                        continue;
                    }
                }
            }
        }
        
        pthread_mutex_unlock(&chrome_mutex);
    }
    
    return NULL;
}

/* Check Chrome health */
static int check_chrome_health(void) {
    // Try to connect to debug port
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(g_chrome.port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    // Set timeout
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
    
    return result;
}

/* Cleanup Chrome resources */
static int cleanup_chrome_resources(void) {
    // Remove user data directory if it's temporary
    if (strstr(g_chrome.user_data_dir, "/tmp/cdp-chrome-profile-")) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s' 2>/dev/null", 
                g_chrome.user_data_dir);
        system(cmd);
    }
    
    return 0;
}

/* Signal handler for child processes */
void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;
    
    // Reap all dead children
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        pthread_mutex_lock(&chrome_mutex);
        if (pid == g_chrome.pid) {
            g_chrome.pid = -1;
            if (verbose) {
                cdp_log(CDP_LOG_INFO, "CHROME", "Chrome process exited (PID: %d)", pid);
            }
        }
        pthread_mutex_unlock(&chrome_mutex);
    }
}

/* Setup signal handlers */
static void setup_signal_handlers(void) {
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
}

/* Get Chrome statistics */
void cdp_chrome_stats(int *uptime, int *restarts, unsigned long *memory_mb) {
    pthread_mutex_lock(&chrome_mutex);
    
    if (uptime && g_chrome.pid > 0) {
        *uptime = time(NULL) - g_chrome.start_time;
    }
    
    if (restarts) {
        *restarts = g_chrome.restart_count;
    }
    
    if (memory_mb && g_chrome.pid > 0) {
        char stat_path[256];
        snprintf(stat_path, sizeof(stat_path), "/proc/%d/statm", g_chrome.pid);
        
        FILE *fp = fopen(stat_path, "r");
        if (fp) {
            unsigned long rss_pages;
            fscanf(fp, "%*lu %lu", &rss_pages);
            fclose(fp);
            *memory_mb = (rss_pages * 4096) / (1024 * 1024);
        }
    }
    
    pthread_mutex_unlock(&chrome_mutex);
}

/* Check if Chrome is running - Chrome manager version */
int cdp_chrome_is_running(void) {
    pthread_mutex_lock(&chrome_mutex);
    int running = g_chrome.pid > 0 && kill(g_chrome.pid, 0) == 0;
    pthread_mutex_unlock(&chrome_mutex);
    return running;
}
