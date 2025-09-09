/**
 * CDP Chrome Process Manager
 * Enhanced Chrome lifecycle management with auto-restart and cleanup
 */

#include "cdp_internal.h"
#include <sys/wait.h>
#include <signal.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <errno.h>

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

/* Forward declarations */
static void* chrome_monitor_thread(void *arg);
static int cleanup_chrome_resources(void);
static int check_chrome_health(void);
static void setup_signal_handlers(void);
int cdp_chrome_stop(void);

/* Initialize Chrome manager */
int cdp_chrome_mgr_init(void) {
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
        printf("Chrome manager initialized\n");
    }
    
    return 0;
}

/* Shutdown Chrome manager */
void cdp_chrome_mgr_shutdown(void) {
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
        printf("Chrome manager shutdown\n");
    }
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
        
        // Redirect output to /dev/null
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        
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
                printf("Chrome started successfully (PID: %d)\n", pid);
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
        printf("Chrome stopped (PID: %d)\n", pid);
    }
    
    return 0;
}

/* Restart Chrome */
int cdp_chrome_restart(void) {
    if (verbose) {
        printf("Restarting Chrome...\n");
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
                    printf("Chrome process died unexpectedly\n");
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
                            printf("Chrome using too much memory (%luMB), restarting...\n", 
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
                printf("Chrome process exited (PID: %d)\n", pid);
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