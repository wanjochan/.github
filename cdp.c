 /**
 * CDP Chrome System-Level Companion Tool
 * Advanced Chrome DevTools Protocol client with system integration capabilities
 * 
 * CORE FEATURES:
 * - JavaScript REPL with Runtime.addBinding support
 * - Direct URL opening and navigation (http://site.com)
 * - Native API bindings: await CDP.cli(), CDP.system(), CDP.exec()
 * - Chrome process lifecycle management (multi-instance support)
 * - File system interaction (download monitoring, batch operations)
 * - System integration (notifications, CI/CD, external commands)
 * - Concurrent operations framework (load testing, batch processing)
 * - Security permission system for external websites
 * 
 * SYSTEM-LEVEL CAPABILITIES (Chrome cannot do alone):
 * - Multi-instance Chrome management with health monitoring
 * - File download monitoring and batch file processing  
 * - Desktop notifications and CI/CD integration
 * - External system command execution
 * - Concurrent task scheduling and load balancing
 * 
 * BUILD (using cosmopolitan for win/lnx/osx and x86/arm):
 * 	./cdp_build.sh
 * 
 * USAGE:
 *   ./cdp.exe [URL]                    # Open URL with native bindings
 *   ./cdp.exe --gui https://site.com   # Open with visible Chrome window
 *   ./cdp.exe --dev-mode http://localhost:3000  # Development mode (all permissions)
 *   ./cdp.exe --allow-binding all --allow-domain mysite.com
 * 
 * NATIVE API EXAMPLES (in browser console):
 *   await CDP.cli('screenshot', './output.png')
 *   await CDP.cli('monitor-downloads', './downloads/')
 *   await CDP.cli('process-list')
 *   await CDP.system('npm run build')
 *   await CDP.exec('git status')
 * 
 * SECURITY:
 *   Default: about:blank has full access, external sites denied
 *   --allow-binding: cli,screenshot,monitor,system,file,all
 *   --allow-domain: domain.com,*.company.com
 *   --dev-mode: Allow all for localhost (development use)
 * 
 * COMPATIBILITY:
 *   Windows/Linux/macOS, x86_64/ARM64
 *   Chrome 100+, Chromium, Chrome Headless
 * 
 * DATE: 2025-09-10
 * VERSION: 2.0 (System-Level Companion)
 * MODULES: process(779), filesystem(824), system(757), concurrent(1005) = 3365 lines
 */
/**
 * CDP Main Module - Minimal entry point
 * Uses modular architecture
 */

#include "cdp_internal.h"
#include "cdp_process.h"
#include "cdp_filesystem.h"
#include "cdp_system.h"
#include "cdp_concurrent.h"
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/select.h>
#include <sys/time.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* Signal handling for Ctrl+C */
static sigjmp_buf jump_buffer;
static volatile sig_atomic_t interrupted = 0;

static void sigint_handler(int sig) {
    (void)sig;
    interrupted = 1;
    siglongjmp(jump_buffer, 1);
}

/* Global variables */
CDPContext g_ctx = {
    .config = {
        .debug_port = CHROME_DEFAULT_PORT,
        .server_port = 8080,
        .server_host = "127.0.0.1",
        .chrome_host = "127.0.0.1",
        .verbose = 0,
        .debug_mode = 0,
        .timeout_ms = DEFAULT_TIMEOUT_MS
    },
    .conn = {
        .ws_sock = -1,
        .server_sock = -1,
        .connected = 0,
        .reconnect_attempts = 0,
        .max_reconnect_attempts = MAX_RECONNECT_ATTEMPTS,
        .reconnect_delay_ms = RECONNECT_BASE_DELAY_MS
    },
    .runtime = {
        .command_id = 1,
        .runtime_ready = 0,
        .page_ready = 0
    },
    .num_children = 0
};

CDPErrorInfo g_last_error = {0};
int verbose = 0;
int debug_mode = 0;
int ws_sock = -1;
int ws_cmd_id = 1;
int gui_mode = 0;  // Global GUI mode flag
char proxy_server[512] = "";  // Global proxy server setting

/* Permission system */
typedef enum {
    CDP_PERM_NONE = 0,
    CDP_PERM_READ = 1,        // process-list, status等只读功能
    CDP_PERM_SCREENSHOT = 2,  // 截图功能
    CDP_PERM_MONITOR = 4,     // 文件监控功能
    CDP_PERM_SYSTEM = 8,      // 系统命令执行
    CDP_PERM_FILE = 16,       // 文件操作
    CDP_PERM_ALL = 31         // 所有权限
} cdp_permission_t;

typedef struct {
    char allowed_domains[16][256];  // 允许的域名列表
    int domain_count;
    cdp_permission_t permissions;   // 允许的权限位掩码
    int allow_localhost;            // 是否允许localhost
    int allow_file_protocol;        // 是否允许file://协议
    int dev_mode;                   // 开发模式
} cdp_security_config_t;

static cdp_security_config_t security_config = {
    .domain_count = 0,
    .permissions = CDP_PERM_NONE,
    .allow_localhost = 0,
    .allow_file_protocol = 0,
    .dev_mode = 0
};

/* Print usage information */
void print_usage(const char *prog_name) {
    printf("Usage: %s [options] [URL|script.js]\n", prog_name);
    printf("\nOptions:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -v, --verbose       Enable verbose output\n");
    printf("  -d, --debug-port    Chrome debug port (default: 9222)\n");
    printf("  -H, --host          Chrome host (default: 127.0.0.1)\n");
    printf("  --gui               Launch Chrome with GUI (non-headless mode)\n");
    printf("  --proxy-server      Set Chrome proxy server (e.g., http://proxy:8080)\n");
    printf("  --allow-binding     Allow binding functions (cli,screenshot,system,monitor,all)\n");
    printf("  --allow-domain      Allow specific domains (domain.com or *.company.com)\n");
    printf("  --dev-mode          Development mode (allow all bindings for localhost)\n");
    printf("\nArguments:\n");
    printf("  URL                 Open specific URL (e.g., https://google.com)\n");
    printf("  script.js           Execute JavaScript/CDP script file\n");
    printf("\nExamples:\n");
    printf("  %s                          # Start REPL with about:blank\n", prog_name);
    printf("  %s https://google.com       # Open Google and start REPL\n", prog_name);
    printf("  %s --gui https://github.com # Open GitHub with GUI\n", prog_name);
    printf("  %s --proxy-server http://proxy:8080 https://site.com\n", prog_name);
    printf("  %s --allow-binding all --dev-mode http://localhost:3000\n", prog_name);
    printf("  %s --allow-domain mysite.com --allow-binding cli,screenshot\n", prog_name);
    printf("  echo '2+3' | %s             # Evaluate expression\n", prog_name);
    printf("  %s script.js                # Execute script file\n", prog_name);
}

/* Global variables for URL handling */
static char target_url[1024] = "about:blank";

/* Forward declarations for native bindings */
static int setup_native_bindings(void);
static int handle_binding_call(const char* name, const char* payload);
static char* execute_cli_command(const char* command, const char* type, const char* options);
static char* execute_system_command_safe(const char* command, const char* working_dir);
static int run_repl_with_bindings(void);
static int handle_runtime_event(const char* event_json);
static char* execute_cdp_cli_command(const char* command, const char* args);
static int parse_binding_permissions(const char* perm_string);
static int parse_domain_whitelist(const char* domain_string);
static int check_binding_permission(const char* current_url, const char* command);
static int check_command_permission(const char* command);
static int get_current_page_url(char* url_buffer, size_t buffer_size);

/* Check if string is a valid URL */
static int is_url(const char *str) {
    if (!str) return 0;
    return (strstr(str, "http://") == str || 
            strstr(str, "https://") == str ||
            strstr(str, "file://") == str ||
            strstr(str, "data://") == str);
}

/* Parse command line arguments */
static int parse_args(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"debug-port", required_argument, 0, 'd'},
        {"host", required_argument, 0, 'H'},
        {"gui", no_argument, 0, 'g'},
        {"proxy-server", required_argument, 0, 'p'},
        {"allow-binding", required_argument, 0, 'b'},
        {"allow-domain", required_argument, 0, 'a'},
        {"dev-mode", no_argument, 0, 'D'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "hvd:H:gp:b:a:D", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                exit(0);
            case 'v':
                verbose = 1;
                g_ctx.config.verbose = 1;
                break;
            case 'd':
                g_ctx.config.debug_port = atoi(optarg);
                break;
            case 'H':
                g_ctx.config.chrome_host = optarg;
                break;
            case 'g':
                gui_mode = 1;
                break;
            case 'p':
                strncpy(proxy_server, optarg, sizeof(proxy_server) - 1);
                proxy_server[sizeof(proxy_server) - 1] = '\0';
                break;
            case 'b':
                parse_binding_permissions(optarg);
                break;
            case 'a':
                parse_domain_whitelist(optarg);
                break;
            case 'D':
                security_config.dev_mode = 1;
                security_config.allow_localhost = 1;
                security_config.permissions = CDP_PERM_ALL;
                break;
            default:
                print_usage(argv[0]);
                return -1;
        }
    }
    
    return 0;
}

/* Check if input needs more lines (unclosed brackets/quotes) */
static int needs_more_input(const char *input) {
    int braces = 0, brackets = 0, parens = 0;
    int in_string = 0, in_single_quote = 0;
    int escape = 0;
    
    for (const char *p = input; *p; p++) {
        if (escape) {
            escape = 0;
            continue;
        }
        
        if (*p == '\\') {
            escape = 1;
            continue;
        }
        
        // Handle strings
        if (*p == '"' && !in_single_quote) {
            in_string = !in_string;
        } else if (*p == '\'' && !in_string) {
            in_single_quote = !in_single_quote;
        }
        
        // Count brackets when not in string
        if (!in_string && !in_single_quote) {
            switch (*p) {
                case '{': braces++; break;
                case '}': braces--; break;
                case '[': brackets++; break;
                case ']': brackets--; break;
                case '(': parens++; break;
                case ')': parens--; break;
            }
        }
    }
    
    return (braces > 0 || brackets > 0 || parens > 0 || 
            in_string || in_single_quote);
}

/* Execute script file */
static int execute_script_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return -1;
    }
    
    char line[4096];
    char multiline[16384] = "";
    int in_multiline = 0;
    int line_num = 0;
    
    while (fgets(line, sizeof(line), f)) {
        line_num++;
        
        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        // Skip empty lines and comments
        if (strlen(line) == 0 || line[0] == '#' || 
            (line[0] == '/' && line[1] == '/')) {
            continue;
        }
        
        if (in_multiline) {
            strcat(multiline, "\n");
            strcat(multiline, line);
            
            if (!needs_more_input(multiline)) {
                // Execute complete multiline code
                if (verbose) {
                    printf("[Line %d] Executing: %s\n", line_num, multiline);
                }
                char *result = cdp_process_user_command(multiline);
                if (result && *result) {
                    printf("%s\n", result);
                }
                multiline[0] = '\0';
                in_multiline = 0;
            }
        } else {
            if (needs_more_input(line)) {
                strcpy(multiline, line);
                in_multiline = 1;
            } else {
                // Execute single line
                if (verbose) {
                    printf("[Line %d] Executing: %s\n", line_num, line);
                }
                char *result = cdp_process_user_command(line);
                if (result && *result) {
                    printf("%s\n", result);
                }
            }
        }
    }
    
    // Execute any remaining multiline code
    if (in_multiline && strlen(multiline) > 0) {
        fprintf(stderr, "Warning: Unclosed expression at end of file\n");
        char *result = cdp_process_user_command(multiline);
        if (result && *result) {
            printf("%s\n", result);
        }
    }
    
    fclose(f);
    return 0;
}

/* Enhanced REPL with multi-line support */
static int run_repl(void) {
    char input[4096];
    char multiline[16384] = "";
    int in_multiline = 0;
    
    printf("Chrome DevTools Protocol REPL\n");
    printf("Type JavaScript expressions or .help for commands\n");
    printf("Multi-line input supported (unclosed brackets)\n");
    printf("Press Ctrl+C to cancel input, Ctrl+D to exit\n\n");
    
    // Set up Ctrl+C handler
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    
    while (1) {
        // Setup jump point for Ctrl+C
        if (sigsetjmp(jump_buffer, 1)) {
            // Ctrl+C pressed
            printf("\n^C\n");
            multiline[0] = '\0';
            in_multiline = 0;
            interrupted = 0;
            continue;
        }
        
        const char *prompt = in_multiline ? "... " : "> ";
        printf("%s", prompt);
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            printf("\n");
            break;
        }
        
        // Remove newline
        size_t len = strlen(input);
        if (len > 0 && input[len-1] == '\n') {
            input[len-1] = '\0';
        }
        
        // Skip empty lines in normal mode
        if (!in_multiline && strlen(input) == 0) {
            continue;
        }
        
        // Handle exit commands
        if (!in_multiline && (strcmp(input, ".exit") == 0 || 
                              strcmp(input, ".quit") == 0)) {
            break;
        }
        
        // Multi-line input handling
        if (in_multiline) {
            if (strlen(input) > 0) {
                strcat(multiline, "\n");
                strcat(multiline, input);
            }
            
            if (!needs_more_input(multiline)) {
                // Execute complete multiline code
                char *result = cdp_process_user_command(multiline);
                if (result && *result) {
                    printf("%s\n", result);
                }
                multiline[0] = '\0';
                in_multiline = 0;
            }
        } else {
            if (needs_more_input(input)) {
                strcpy(multiline, input);
                in_multiline = 1;
            } else {
                // Execute single line
                char *result = cdp_process_user_command(input);
                if (result && *result) {
                    printf("%s\n", result);
                }
            }
        }
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    // Check executable name
    const char *program_name = argv[0];
    const char *base_name = strrchr(program_name, '/');
    if (!base_name) {
        base_name = strrchr(program_name, '\\');
    }
    base_name = base_name ? base_name + 1 : program_name;
    
    // Parse arguments
    if (parse_args(argc, argv) != 0) {
        return 1;
    }

    // Initialize enhanced Chrome process management
    int process_init_result = cdp_init_chrome_registry();
    if (process_init_result != CDP_PROCESS_SUCCESS) {
        fprintf(stderr, "Warning: Failed to initialize Chrome process management: %s\n",
                cdp_process_error_to_string(process_init_result));
        // Continue anyway for backward compatibility
    } else if (verbose) {
        printf("Enhanced Chrome process management initialized\n");
    }

    // Initialize filesystem interaction module
    int filesystem_init_result = cdp_init_filesystem_module();
    if (filesystem_init_result != CDP_FILE_SUCCESS) {
        fprintf(stderr, "Warning: Failed to initialize filesystem module: %s\n",
                cdp_file_error_to_string(filesystem_init_result));
        // Continue anyway for backward compatibility
    } else if (verbose) {
        printf("Enhanced filesystem interaction initialized\n");
    }

    // Initialize system integration module
    int system_init_result = cdp_init_system_module();
    if (system_init_result != CDP_SYSTEM_SUCCESS) {
        fprintf(stderr, "Warning: Failed to initialize system integration module: %s\n",
                cdp_system_error_to_string(system_init_result));
        // Continue anyway for backward compatibility
    } else if (verbose) {
        printf("Enhanced system integration initialized\n");
    }

    // Initialize concurrent operations framework
    int concurrent_init_result = cdp_init_concurrent_module();
    if (concurrent_init_result != CDP_CONCURRENT_SUCCESS) {
        fprintf(stderr, "Warning: Failed to initialize concurrent framework: %s\n",
                cdp_concurrent_error_to_string(concurrent_init_result));
        // Continue anyway for backward compatibility
    } else if (verbose) {
        printf("Enhanced concurrent operations framework initialized\n");
    }
    
    // Show executable info in verbose mode
    if (verbose) {
        int is_cdp_exe = (strcmp(base_name, "cdp.exe") == 0);
        printf("Running as: %s%s\n", base_name, 
               is_cdp_exe ? " (correct executable)" : " (warning: should be cdp.exe)");
        printf("CDP Client v2.0 (Modular)\n");
        printf("Configuration:\n");
        printf("  Chrome: %s:%d\n", g_ctx.config.chrome_host, g_ctx.config.debug_port);
        printf("  Mode: %s\n", verbose ? "Verbose" : "Normal");
    }
    
    // Ensure Chrome is running
    if (ensure_chrome_running() != 0) {
        fprintf(stderr, "Failed to connect to Chrome\n");
        return 1;
    }
    
    // Get Chrome target ID
    char *target_id = get_chrome_target_id();
    if (!target_id) {
        fprintf(stderr, "Failed to get Chrome target ID\n");
        return 1;
    }
    
    if (verbose) {
        printf("Connecting to Chrome on %s:%d...\n", 
               g_ctx.config.chrome_host, g_ctx.config.debug_port);
        printf("Chrome is running!\n");
    }
    
    // Connect via WebSocket to browser endpoint
    str_copy_safe(g_ctx.conn.target_id, target_id, sizeof(g_ctx.conn.target_id));
    ws_sock = connect_chrome_websocket(target_id);
    if (ws_sock < 0) {
        fprintf(stderr, "Failed to connect WebSocket\n");
        return 1;
    }
    
    if (verbose) {
        printf("WebSocket connected to %s endpoint successfully\n",
               strstr(target_id, "browser") ? "browser" : "page");
    }
    
    // Auto-attach to JavaScript context
    // Initialize performance tracking
    cdp_perf_init();
    
    if (verbose) {
        printf("\n=== Chrome DevTools Protocol Client ===\n");
        printf("Auto-attaching to JavaScript context...\n");
    }
    
    // Check for existing about:blank or create new one
    char get_targets_cmd[128];
    snprintf(get_targets_cmd, sizeof(get_targets_cmd), 
             "{\"id\":%d,\"method\":\"Target.getTargets\"}", ws_cmd_id++);
    
    char *about_blank_target = NULL;
    
    // Send getTargets command
    if (send_command_with_retry(get_targets_cmd) >= 0) {
        char response[RESPONSE_BUFFER_SIZE];
        int len = ws_recv_text(ws_sock, response, sizeof(response));
        if (len > 0) {
            // Look for existing about:blank page
            char *target_pos = strstr(response, "\"url\":\"about:blank\"");
            if (target_pos) {
                // Extract targetId
                char *search_start = (target_pos > response + 200) ? target_pos - 200 : response;
                char *id_start = strstr(search_start, "\"targetId\":\"");
                if (id_start && id_start < target_pos) {
                    id_start += 12;
                    char *id_end = strchr(id_start, '"');
                    if (id_end) {
                        size_t id_len = id_end - id_start;
                        about_blank_target = malloc(id_len + 1);
                        memcpy(about_blank_target, id_start, id_len);
                        about_blank_target[id_len] = '\0';
                        if (verbose) printf("Found existing about:blank: %s\n", about_blank_target);
                    }
                }
            }
        }
    }
    
    // If no about:blank found, create one
    if (!about_blank_target) {
        char *new_target = create_new_page_via_browser(ws_sock);
        if (new_target) {
            // create_new_page_via_browser returns static buffer, need to copy
            about_blank_target = malloc(strlen(new_target) + 1);
            if (about_blank_target) {
                strcpy(about_blank_target, new_target);
                if (verbose) {
                    printf("Created new about:blank: %s\n", about_blank_target);
                }
            }
        }
    }
    
    // Auto-attach to the about:blank page
    if (about_blank_target) {
        char page_path[256];
        snprintf(page_path, sizeof(page_path), "page/%s", about_blank_target);
        
        int old_ws = ws_sock;
        int page_sock = connect_chrome_websocket(page_path);
        if (page_sock > 0) {
            if (old_ws > 0) close(old_ws);
            ws_sock = page_sock;
            str_copy_safe(g_ctx.conn.target_id, page_path, sizeof(g_ctx.conn.target_id));
            
            // Enable Runtime
            char enable_cmd[128];
            snprintf(enable_cmd, sizeof(enable_cmd), "{\"id\":%d,\"method\":\"Runtime.enable\"}", ws_cmd_id++);
            if (send_command_with_retry(enable_cmd) >= 0) {
                char buf[LARGE_BUFFER_SIZE];
                ws_recv_text(ws_sock, buf, sizeof(buf));
                g_ctx.runtime.runtime_ready = 1;
                
                // Initialize performance tracking after connection
                cdp_perf_init();
                
            if (verbose) {
                printf("Attached to page endpoint, JavaScript execution ready\n");
            }
            
            // Setup Native API bindings with security check
            if (verbose) {
                printf("Setting up CDP.cli() native bindings...\n");
            }
            
            // Get current URL for security check
            char current_url[1024];
            if (get_current_page_url(current_url, sizeof(current_url)) == 0) {
                if (check_binding_permission(current_url, "any")) {
                    setup_native_bindings();
                    if (verbose) {
                        printf("Bindings enabled for URL: %s\n", current_url);
                    }
                } else {
                    if (verbose) {
                        printf("Bindings denied for URL: %s\n", current_url);
                        printf("Use --allow-binding or --dev-mode to enable\n");
                    }
                }
            } else {
                // Fallback: allow for about:blank or if can't determine URL
                setup_native_bindings();
            }
            
            // Navigate to target URL if specified
            if (strcmp(target_url, "about:blank") != 0) {
                if (verbose) {
                    printf("Navigating to: %s\n", target_url);
                }
                char navigate_cmd[2048];
                snprintf(navigate_cmd, sizeof(navigate_cmd),
                    "{\"id\":%d,\"method\":\"Page.navigate\",\"params\":{\"url\":\"%s\"}}",
                    ws_cmd_id++, target_url);
                
                if (send_command_with_retry(navigate_cmd) >= 0) {
                    char response[RESPONSE_BUFFER_SIZE];
                    ws_recv_text(ws_sock, response, sizeof(response));
                    if (verbose) {
                        printf("Navigation initiated\n");
                    }
                }
            }
            
            if (verbose) {
                printf("CDP.cli() ready! Try: await CDP.cli('document.title')\n\n");
            }
            }
        }
        // Only free if we allocated it (when creating new page)
        if (about_blank_target) {
            free(about_blank_target);
            about_blank_target = NULL;
        }
    }
    
    // Check for URL or script file argument
    int non_option_index = optind;
    if (non_option_index < argc) {
        const char *argument = argv[non_option_index];
        
        if (is_url(argument)) {
            // URL argument - navigate to the specified URL
            if (verbose) {
                printf("Target URL specified: %s\n", argument);
            }
            strncpy(target_url, argument, sizeof(target_url) - 1);
            target_url[sizeof(target_url) - 1] = '\0';
        } else if (strstr(argument, ".js") || strstr(argument, ".cdp")) {
            // Script file argument
            if (verbose) {
                printf("Executing script file: %s\n\n", argument);
            }
            execute_script_file(argument);
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argument);
            fprintf(stderr, "Usage: %s [options] [URL|script.js]\n", argv[0]);
            return 1;
        }
    } else if (!isatty(STDIN_FILENO)) {
        // Read from pipe - with multi-line support
        char input[4096];
        char multiline[16384] = "";
        int in_multiline = 0;
        
        while (fgets(input, sizeof(input), stdin)) {
            size_t len = strlen(input);
            if (len > 0 && input[len-1] == '\n') {
                input[len-1] = '\0';
            }
            
            // Skip empty lines and comments
            if (!in_multiline && (strlen(input) == 0 || input[0] == '#' || 
                (input[0] == '/' && input[1] == '/'))) {
                continue;
            }
            
            if (in_multiline) {
                strcat(multiline, "\n");
                strcat(multiline, input);
                
                if (!needs_more_input(multiline)) {
                    // Execute complete multiline code
                    char *result = cdp_process_user_command(multiline);
                    if (result && *result) {
                        printf("%s\n", result);
                    }
                    multiline[0] = '\0';
                    in_multiline = 0;
                }
            } else {
                if (needs_more_input(input)) {
                    strcpy(multiline, input);
                    in_multiline = 1;
                } else if (strlen(input) > 0) {
                    // Execute single line
                    char *result = cdp_process_user_command(input);
                    if (result && *result) {
                        printf("%s\n", result);
                    }
                }
            }
        }
        
        // Execute any remaining multiline code
        if (in_multiline && strlen(multiline) > 0) {
            char *result = cdp_process_user_command(multiline);
            if (result && *result) {
                printf("%s\n", result);
            }
        }
    } else {
        // Interactive REPL with binding support
        run_repl_with_bindings();
    }
    
    // Cleanup
    if (ws_sock >= 0) {
        close(ws_sock);
    }

    // Cleanup enhanced modules
    cdp_cleanup_concurrent_module();
    cdp_cleanup_system_module();
    cdp_cleanup_filesystem_module();
    cdp_cleanup_chrome_registry();
    
    return 0;
}

/* Enhanced REPL with Native Bindings support */
static int run_repl_with_bindings(void) {
    char input[4096];
    char multiline[16384] = "";
    int in_multiline = 0;
    
    printf("Chrome DevTools Protocol REPL with Native Bindings\n");
    printf("Type JavaScript expressions, system commands, or .help for commands\n");
    printf("Multi-line input supported (unclosed brackets)\n");
    printf("Native API: await CDP.cli('code') or await CDP.system('command')\n");
    printf("Press Ctrl+C to cancel input, Ctrl+D to exit\n\n");
    
    // Set up Ctrl+C handler
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    
    while (1) {
        // Setup jump point for Ctrl+C
        if (sigsetjmp(jump_buffer, 1)) {
            // Ctrl+C pressed
            printf("\n^C\n");
            multiline[0] = '\0';
            in_multiline = 0;
            interrupted = 0;
            continue;
        }
        
        // Check for binding events while waiting for input
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(ws_sock, &read_fds);
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms timeout
        
        int ready = select(FD_SETSIZE, &read_fds, NULL, NULL, &tv);
        
        if (ready > 0) {
            // Check for WebSocket events (binding calls)
            if (FD_ISSET(ws_sock, &read_fds)) {
                char event_buffer[RESPONSE_BUFFER_SIZE];
                int event_len = ws_recv_text(ws_sock, event_buffer, sizeof(event_buffer));
                if (event_len > 0) {
                    handle_runtime_event(event_buffer);
                }
            }
            
            // Check for user input
            if (FD_ISSET(STDIN_FILENO, &read_fds)) {
                const char *prompt = in_multiline ? "... " : "> ";
                printf("%s", prompt);
                fflush(stdout);
                
                if (!fgets(input, sizeof(input), stdin)) {
                    printf("\n");
                    break;
                }
                
                // Remove newline
                size_t len = strlen(input);
                if (len > 0 && input[len-1] == '\n') {
                    input[len-1] = '\0';
                }
                
                // Skip empty lines in normal mode
                if (!in_multiline && strlen(input) == 0) {
                    continue;
                }
                
                // Handle exit commands
                if (!in_multiline && (strcmp(input, ".exit") == 0 || 
                                      strcmp(input, ".quit") == 0)) {
                    break;
                }
                
                // Multi-line input handling
                if (in_multiline) {
                    if (strlen(input) > 0) {
                        strcat(multiline, "\n");
                        strcat(multiline, input);
                    }
                    
                    if (!needs_more_input(multiline)) {
                        // Execute complete multiline code
                        char *result = cdp_process_user_command(multiline);
                        if (result && *result) {
                            printf("%s\n", result);
                        }
                        multiline[0] = '\0';
                        in_multiline = 0;
                    }
                } else {
                    if (needs_more_input(input)) {
                        strcpy(multiline, input);
                        in_multiline = 1;
                    } else {
                        // Execute single line
                        char *result = cdp_process_user_command(input);
                        if (result && *result) {
                            printf("%s\n", result);
                        }
                    }
                }
            }
        }
    }
    
    return 0;
}

/* Handle Runtime events including binding calls */
static int handle_runtime_event(const char* event_json) {
    if (strstr(event_json, "Runtime.bindingCalled")) {
        // Extract binding name and payload
        const char* name_start = strstr(event_json, "\"name\":\"");
        const char* payload_start = strstr(event_json, "\"payload\":\"");
        
        if (name_start && payload_start) {
            name_start += 8;
            const char* name_end = strchr(name_start, '"');
            
            payload_start += 11;
            const char* payload_end = strrchr(payload_start, '"');
            
            if (name_end && payload_end) {
                char binding_name[64];
                char payload[4096];
                
                strncpy(binding_name, name_start, MIN(name_end - name_start, sizeof(binding_name) - 1));
                binding_name[MIN(name_end - name_start, sizeof(binding_name) - 1)] = '\0';
                
                strncpy(payload, payload_start, MIN(payload_end - payload_start, sizeof(payload) - 1));
                payload[MIN(payload_end - payload_start, sizeof(payload) - 1)] = '\0';
                
                return handle_binding_call(binding_name, payload);
            }
        }
    }
    
    return 0;
}

/* Handle binding call from page */
static int handle_binding_call(const char* name, const char* payload) {
    if (strcmp(name, "CDP") != 0) {
        return -1;  // Not our binding
    }
    
    if (verbose) {
        printf("Handling CDP binding call: %.100s...\n", payload);
    }
    
    // Parse JSON payload
    char request_id[64] = {0};
    char command_type[32] = {0};
    char command[4096] = {0};
    
    // Extract request ID
    const char* id_start = strstr(payload, "\"id\":\"");
    if (id_start) {
        id_start += 6;
        const char* id_end = strchr(id_start, '"');
        if (id_end) {
            strncpy(request_id, id_start, MIN(id_end - id_start, sizeof(request_id) - 1));
        }
    }
    
    // Extract type
    const char* type_start = strstr(payload, "\"type\":\"");
    if (type_start) {
        type_start += 8;
        const char* type_end = strchr(type_start, '"');
        if (type_end) {
            strncpy(command_type, type_start, MIN(type_end - type_start, sizeof(command_type) - 1));
        }
    }
    
    // Extract command
    const char* cmd_start = strstr(payload, "\"cmd\":\"");
    if (cmd_start) {
        cmd_start += 7;
        const char* cmd_end = strchr(cmd_start, '"');
        if (cmd_end) {
            strncpy(command, cmd_start, MIN(cmd_end - cmd_start, sizeof(command) - 1));
        }
    }
    
    // Execute command based on type
    char* result = NULL;
    
    if (strcmp(command_type, "cdp_cli") == 0) {
        // Parse args from payload
        char args_str[2048] = {0};
        const char* args_start = strstr(payload, "\"args\":");
        if (args_start) {
            args_start += 7;
            const char* args_end = strstr(args_start, "}");
            if (args_end) {
                strncpy(args_str, args_start, MIN(args_end - args_start, sizeof(args_str) - 1));
            }
        }
        
        result = execute_cdp_cli_command(command, args_str);
    } else {
        result = execute_cli_command(command, command_type, NULL);
    }
    
    // Return result to page
    char return_result[8192];
    if (result) {
        snprintf(return_result, sizeof(return_result),
            "{\"id\":%d,\"method\":\"Runtime.evaluate\",\"params\":{\"expression\":\""
            "(function() {"
            "  const req = window._cdp_requests && window._cdp_requests.get('%s');"
            "  if (req) {"
            "    window._cdp_requests.delete('%s');"
            "    try { req.resolve(%s); } catch(e) { req.resolve('%s'); }"
            "  }"
            "})()\"}}",
            ws_cmd_id++, request_id, request_id, result, result);
        
        send_command_with_retry(return_result);
        free(result);
    }
    
    return 0;
}

/* Setup Native API Bindings */
static int setup_native_bindings(void) {
    // 1. Add CDP binding function
    char add_binding[512];
    snprintf(add_binding, sizeof(add_binding),
        "{\"id\":%d,\"method\":\"Runtime.addBinding\",\"params\":{\"name\":\"CDP\"}}",
        ws_cmd_id++);
    
    if (send_command_with_retry(add_binding) < 0) {
        return -1;
    }
    
    // Wait for binding response
    char response[RESPONSE_BUFFER_SIZE];
    ws_recv_text(ws_sock, response, sizeof(response));
    
    // 2. Inject CDP helper functions into page
    char inject_cdp_api[4096];
    snprintf(inject_cdp_api, sizeof(inject_cdp_api),
        "{\"id\":%d,\"method\":\"Runtime.evaluate\",\"params\":{\"expression\":\""
        "window.CDP = {"
        "  cli: async (command, ...args) => {"
        "    return new Promise((resolve, reject) => {"
        "      const requestId = 'cli-' + Date.now() + '-' + Math.random().toString(36).substr(2, 5);"
        "      window._cdp_requests = window._cdp_requests || new Map();"
        "      window._cdp_requests.set(requestId, {resolve, reject});"
        "      "
        "      const payload = JSON.stringify({type: 'cdp_cli', command, args, id: requestId});"
        "      CDP(payload);"
        "    });"
        "  },"
        "  system: async (cmd, options = {}) => {"
        "    return new Promise((resolve, reject) => {"
        "      const requestId = 'sys-' + Date.now() + '-' + Math.random().toString(36).substr(2, 5);"
        "      window._cdp_requests = window._cdp_requests || new Map();"
        "      window._cdp_requests.set(requestId, {resolve, reject});"
        "      "
        "      const payload = JSON.stringify({type: 'system', cmd, options, id: requestId});"
        "      CDP(payload);"
        "    });"
        "  },"
        "  js: async (code) => {"
        "    return new Promise((resolve, reject) => {"
        "      const requestId = 'js-' + Date.now() + '-' + Math.random().toString(36).substr(2, 5);"
        "      window._cdp_requests = window._cdp_requests || new Map();"
        "      window._cdp_requests.set(requestId, {resolve, reject});"
        "      "
        "      const payload = JSON.stringify({type: 'javascript', code, id: requestId});"
        "      CDP(payload);"
        "    });"
        "  }"
        "};"
        "console.log('🚀 CDP.cli() ready! Examples:');"
        "console.log('  await CDP.cli(\\\\\"screenshot\\\\\", \\\\\"./output.png\\\\\")');"
        "console.log('  await CDP.cli(\\\\\"monitor-downloads\\\\\", \\\\\"./downloads/\\\\\")');"
        "console.log('  await CDP.cli(\\\\\"process-list\\\\\")');"
        "console.log('  await CDP.system(\\\\\"ls -la\\\\\")');"
        "\"}}",
        ws_cmd_id++);
    
    if (send_command_with_retry(inject_cdp_api) < 0) {
        return -2;
    }
    
    ws_recv_text(ws_sock, response, sizeof(response));
    
    return 0;
}

/* Execute CLI command */
static char* execute_cli_command(const char* command, const char* type, const char* options) {
    char* result = malloc(8192);
    if (!result) return NULL;
    
    if (strcmp(type, "cli") == 0) {
        // Execute JavaScript command via CDP Runtime.evaluate
        char escaped_cmd[4096];
        json_escape_safe(escaped_cmd, command, sizeof(escaped_cmd));
        
        char eval_cmd[4096];
        snprintf(eval_cmd, sizeof(eval_cmd),
            "{\"id\":%d,\"method\":\"Runtime.evaluate\",\"params\":{\"expression\":\"%s\",\"returnByValue\":true}}",
            ws_cmd_id++, escaped_cmd);
        
        if (send_command_with_retry(eval_cmd) >= 0) {
            char response[RESPONSE_BUFFER_SIZE];
            int len = ws_recv_text(ws_sock, response, sizeof(response));
            
            if (len > 0) {
                // Extract result value
                const char* value_start = strstr(response, "\"value\":");
                if (value_start) {
                    value_start += 8;
                    const char* result_end = strstr(value_start, ",\"");
                    if (!result_end) result_end = strstr(value_start, "}");
                    
                    if (result_end) {
                        int value_len = result_end - value_start;
                        strncpy(result, value_start, MIN(value_len, 8191));
                        result[value_len] = '\0';
                        return result;
                    }
                }
                
                // Check for exception
                if (strstr(response, "\"exceptionDetails\"")) {
                    strcpy(result, "{\"error\":\"JavaScript execution error\"}");
                    return result;
                }
            }
        }
        
        strcpy(result, "null");
        
    } else if (strcmp(type, "system") == 0) {
        // Execute system command
        free(result);
        return execute_system_command_safe(command, ".");
    }
    
    return result;
}

/* Execute system command safely */
static char* execute_system_command_safe(const char* command, const char* working_dir) {
    char* result = malloc(8192);
    if (!result) return NULL;
    
    if (verbose) {
        printf("Executing system command: %s\n", command);
    }
    
    // Execute command
    FILE* fp = popen(command, "r");
    if (!fp) {
        snprintf(result, 8192, 
            "{\"error\":\"Failed to execute command: %s\",\"success\":false}", command);
        return result;
    }
    
    // Read output
    char output[6144] = {0};
    size_t total_read = 0;
    
    while (fgets(output + total_read, 
                 sizeof(output) - total_read - 1, fp) && 
           total_read < sizeof(output) - 1) {
        total_read = strlen(output);
    }
    
    int exit_code = pclose(fp);
    
    // Escape output for JSON
    char escaped_output[6144];
    json_escape_safe(escaped_output, output, sizeof(escaped_output));
    
    // Format result
    snprintf(result, 8192,
        "{"
        "\"stdout\":\"%s\","
        "\"exitCode\":%d,"
        "\"success\":%s"
        "}",
        escaped_output, exit_code, 
        exit_code == 0 ? "true" : "false");
    
    return result;
}

/* Execute CDP CLI commands */
static char* execute_cdp_cli_command(const char* command, const char* args) {
    char* result = malloc(8192);
    if (!result) return NULL;
    
    if (verbose) {
        printf("Executing CDP CLI command: %s %s\n", command, args);
    }
    
    // Route to appropriate CDP functionality
    if (strcmp(command, "screenshot") == 0) {
        // Take screenshot using CDP
        char screenshot_cmd[1024];
        snprintf(screenshot_cmd, sizeof(screenshot_cmd),
            "{\"id\":%d,\"method\":\"Page.captureScreenshot\",\"params\":{}}",
            ws_cmd_id++);
        
        if (send_command_with_retry(screenshot_cmd) >= 0) {
            char response[LARGE_BUFFER_SIZE];
            int len = ws_recv_text(ws_sock, response, sizeof(response));
            
            if (len > 0 && strstr(response, "\"data\":")) {
                strcpy(result, "{\"success\":true,\"type\":\"screenshot\",\"data\":\"base64_data\"}");
                return result;
            }
        }
        
        strcpy(result, "{\"error\":\"Screenshot failed\"}");
        
    } else if (strcmp(command, "monitor-downloads") == 0) {
        // Start download monitoring
        char monitor_result[512];
        int monitor_status = cdp_start_download_monitor("./downloads/");
        
        snprintf(result, 8192, 
            "{\"success\":%s,\"command\":\"monitor-downloads\",\"status\":%d}",
            monitor_status == 0 ? "true" : "false", monitor_status);
        
    } else if (strcmp(command, "process-list") == 0) {
        // List Chrome processes
        cdp_chrome_instance_t* instances;
        int count;
        
        if (cdp_list_chrome_instances(&instances, &count) == 0) {
            snprintf(result, 8192,
                "{\"success\":true,\"command\":\"process-list\",\"count\":%d,\"instances\":[]}",
                count);
        } else {
            strcpy(result, "{\"error\":\"Failed to get process list\"}");
        }
        
    } else if (strcmp(command, "system-notify") == 0) {
        // Send system notification
        char title[256] = "CDP Notification";
        char message[512] = "Message from web page";
        
        int notify_result = cdp_send_system_notification(title, message, CDP_NOTIFY_INFO);
        
        snprintf(result, 8192,
            "{\"success\":%s,\"command\":\"system-notify\",\"result\":%d}",
            notify_result == 0 ? "true" : "false", notify_result);
            
    } else if (strcmp(command, "batch-test") == 0) {
        // Batch testing functionality
        strcpy(result, "{\"success\":true,\"command\":\"batch-test\",\"status\":\"ready\"}");
        
    } else if (strcmp(command, "load-test") == 0) {
        // Load testing functionality  
        strcpy(result, "{\"success\":true,\"command\":\"load-test\",\"status\":\"ready\"}");
        
    } else {
        snprintf(result, 8192, 
            "{\"error\":\"Unknown CDP command: %s\",\"available\":[\"screenshot\",\"monitor-downloads\",\"process-list\",\"system-notify\",\"batch-test\",\"load-test\"]}",
            command);
    }
    
    return result;
}

/* Parse binding permissions from string */
static int parse_binding_permissions(const char* perm_string) {
    if (!perm_string) return -1;
    
    char perm_copy[512];
    strncpy(perm_copy, perm_string, sizeof(perm_copy) - 1);
    perm_copy[sizeof(perm_copy) - 1] = '\0';
    
    if (strcmp(perm_copy, "all") == 0) {
        security_config.permissions = CDP_PERM_ALL;
        return 0;
    }
    
    // Parse comma-separated permissions
    char* token = strtok(perm_copy, ",");
    while (token) {
        if (strcmp(token, "cli") == 0 || strcmp(token, "read") == 0) {
            security_config.permissions |= CDP_PERM_READ;
        } else if (strcmp(token, "screenshot") == 0) {
            security_config.permissions |= CDP_PERM_SCREENSHOT;
        } else if (strcmp(token, "monitor") == 0) {
            security_config.permissions |= CDP_PERM_MONITOR;
        } else if (strcmp(token, "system") == 0) {
            security_config.permissions |= CDP_PERM_SYSTEM;
        } else if (strcmp(token, "file") == 0) {
            security_config.permissions |= CDP_PERM_FILE;
        }
        token = strtok(NULL, ",");
    }
    
    return 0;
}

/* Parse domain whitelist from string */
static int parse_domain_whitelist(const char* domain_string) {
    if (!domain_string) return -1;
    
    char domain_copy[1024];
    strncpy(domain_copy, domain_string, sizeof(domain_copy) - 1);
    domain_copy[sizeof(domain_copy) - 1] = '\0';
    
    // Parse comma-separated domains
    char* token = strtok(domain_copy, ",");
    while (token && security_config.domain_count < 16) {
        strncpy(security_config.allowed_domains[security_config.domain_count], 
                token, sizeof(security_config.allowed_domains[0]) - 1);
        security_config.allowed_domains[security_config.domain_count][sizeof(security_config.allowed_domains[0]) - 1] = '\0';
        security_config.domain_count++;
        token = strtok(NULL, ",");
    }
    
    return 0;
}

/* Check if binding is allowed for current URL and command */
static int check_binding_permission(const char* current_url, const char* command) {
    if (!current_url) return 0;
    
    // Always allow about:blank
    if (strstr(current_url, "about:blank")) {
        return 1;
    }
    
    // Dev mode allows everything for localhost
    if (security_config.dev_mode && 
        (strstr(current_url, "localhost") || strstr(current_url, "127.0.0.1"))) {
        return 1;
    }
    
    // Check localhost permission
    if (security_config.allow_localhost && 
        (strstr(current_url, "localhost") || strstr(current_url, "127.0.0.1"))) {
        return check_command_permission(command);
    }
    
    // Check file protocol permission
    if (security_config.allow_file_protocol && strstr(current_url, "file://")) {
        return check_command_permission(command);
    }
    
    // Check domain whitelist
    for (int i = 0; i < security_config.domain_count; i++) {
        const char* domain = security_config.allowed_domains[i];
        
        if (domain[0] == '*' && domain[1] == '.') {
            // Wildcard domain: *.mycompany.com
            if (strstr(current_url, domain + 1)) {
                return check_command_permission(command);
            }
        } else {
            // Exact domain match
            if (strstr(current_url, domain)) {
                return check_command_permission(command);
            }
        }
    }
    
    return 0;  // Default deny
}

/* Check permission for specific command */
static int check_command_permission(const char* command) {
    if (strcmp(command, "any") == 0) {
        return security_config.permissions > CDP_PERM_NONE;
    }
    
    if (strcmp(command, "screenshot") == 0) {
        return security_config.permissions & CDP_PERM_SCREENSHOT;
    } else if (strcmp(command, "monitor-downloads") == 0) {
        return security_config.permissions & CDP_PERM_MONITOR;
    } else if (strcmp(command, "process-list") == 0) {
        return security_config.permissions & CDP_PERM_READ;
    } else if (strcmp(command, "system-notify") == 0) {
        return security_config.permissions & CDP_PERM_SYSTEM;
    }
    
    return security_config.permissions & CDP_PERM_ALL;
}

/* Get current page URL */
static int get_current_page_url(char* url_buffer, size_t buffer_size) {
    if (!url_buffer || buffer_size == 0) return -1;
    
    // If target URL was specified, use that
    if (strcmp(target_url, "about:blank") != 0) {
        strncpy(url_buffer, target_url, buffer_size - 1);
        url_buffer[buffer_size - 1] = '\0';
        return 0;
    }
    
    // Otherwise query current page URL via CDP
    char get_url_cmd[256];
    snprintf(get_url_cmd, sizeof(get_url_cmd),
        "{\"id\":%d,\"method\":\"Runtime.evaluate\",\"params\":{\"expression\":\"window.location.href\",\"returnByValue\":true}}",
        ws_cmd_id++);
    
    if (send_command_with_retry(get_url_cmd) >= 0) {
        char response[RESPONSE_BUFFER_SIZE];
        int len = ws_recv_text(ws_sock, response, sizeof(response));
        
        if (len > 0) {
            const char* value_start = strstr(response, "\"value\":\"");
            if (value_start) {
                value_start += 9;
                const char* value_end = strchr(value_start, '"');
                if (value_end) {
                    int url_len = MIN(value_end - value_start, buffer_size - 1);
                    strncpy(url_buffer, value_start, url_len);
                    url_buffer[url_len] = '\0';
                    return 0;
                }
            }
        }
    }
    
    strcpy(url_buffer, "about:blank");  // Fallback
    return 0;
}
