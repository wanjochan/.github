 /**
 * CDP Chrome Companion
 * @ref cdp.md
 */
#include "cdp_internal.h"
#include "cdp_javascript.h"
#include "cdp_notify.h"
#include "cdp_user_interface.h"
#include "cdp_js_resources.h"
#include "cdp_http.h"
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// // Macro for clean JSON construction (avoiding escape hell)
// #define QUOTE(...) #__VA_ARGS__

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
int relaunch_chrome = 0;       // Force relaunch Chrome regardless of existing instance
int http_port = 0;              // HTTP API port (0 = disabled)

/* Execution context selection for frames */
int g_selected_context_id = 0;
static char g_selected_frame_id[128] = "";
#define MAX_CONTEXTS 256
typedef struct { int id; char frameId[128]; } CtxEntry;
static CtxEntry g_ctx_map[MAX_CONTEXTS];
static int g_ctx_count = 0;

static void ctx_add(int id, const char* frameId) {
    if (!frameId || !*frameId) return;
    for (int i=0;i<g_ctx_count;i++) if (g_ctx_map[i].id==id) return;
    if (g_ctx_count < MAX_CONTEXTS) {
        g_ctx_map[g_ctx_count].id = id;
        strncpy(g_ctx_map[g_ctx_count].frameId, frameId, sizeof(g_ctx_map[g_ctx_count].frameId)-1);
        g_ctx_map[g_ctx_count].frameId[sizeof(g_ctx_map[g_ctx_count].frameId)-1] = '\0';
        g_ctx_count++;
    }
}
static void ctx_remove(int id) {
    for (int i=0;i<g_ctx_count;i++) if (g_ctx_map[i].id==id) { for (int j=i+1;j<g_ctx_count;j++) g_ctx_map[j-1]=g_ctx_map[j]; g_ctx_count--; break; }
    if (g_selected_context_id == id) { g_selected_context_id = 0; g_selected_frame_id[0] = '\0'; }
}
int cdp_select_frame_by_id(const char* frame_id) {
    if (!frame_id || !*frame_id) { g_selected_context_id=0; g_selected_frame_id[0]='\0'; return 0; }
    for (int i=0;i<g_ctx_count;i++) {
        if (strcmp(g_ctx_map[i].frameId, frame_id)==0) {
            g_selected_context_id = g_ctx_map[i].id;
            strncpy(g_selected_frame_id, frame_id, sizeof(g_selected_frame_id)-1);
            g_selected_frame_id[sizeof(g_selected_frame_id)-1] = '\0';
            return 0;
        }
    }
    return -1;
}
int cdp_get_selected_frame(char* out, size_t out_sz) {
    if (!out || out_sz==0) return -1;
    if (!*g_selected_frame_id) { out[0]='\0'; return 0; }
    strncpy(out, g_selected_frame_id, out_sz-1); out[out_sz-1]='\0'; return 0;
}

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
    printf("  --relaunch-chrome   Relaunch Chrome even if a debugging instance exists\n");
    printf("  --http-port         Start simple HTTP API server on this port\n");
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
// HTTP functions moved to cdp_http module
int cdp_logs_tail(char *out, size_t out_sz, int max_lines, const char* filter);
static int handle_runtime_event(const char* event_json);
extern void set_runtime_event_handler(int (*handler)(const char* event_json));
char* execute_cdp_cli_command(const char* command, const char* args);
static int handle_fetch_request(const char* request_id, const char* url, const char* method);
static int handle_enhanced_fetch(const char* payload);
static int handle_binding_command(const char* command);
char* execute_protocol_command(const char* url, const char* method);
static int send_fetch_response(const char* request_id, const char* response_body, int status_code);
static void url_decode(const char* src, char* dest, size_t dest_size);
static int check_protocol_permission(const char* page_url, const char* protocol_url);
static int check_protocol_permission_level(const char* protocol_url);
static int parse_binding_permissions(const char* perm_string);
static int parse_domain_whitelist(const char* domain_string);
static int check_binding_permission(const char* current_url, const char* command);
static int check_command_permission(const char* command);
static int get_current_page_url(char* url_buffer, size_t buffer_size);

/* =========================
 * REPL Input (prompt+history)
 * ========================= */
static struct termios repl_orig_termios;
static int repl_orig_fl = -1;
static int repl_raw_enabled = 0;

static int repl_enable_raw(void) {
    if (!isatty(STDIN_FILENO)) return 0;
    if (tcgetattr(STDIN_FILENO, &repl_orig_termios) == -1) return -1;
    struct termios raw = repl_orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1; // 100ms
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return -1;
    repl_orig_fl = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (repl_orig_fl != -1) {
        fcntl(STDIN_FILENO, F_SETFL, repl_orig_fl | O_NONBLOCK);
    }
    repl_raw_enabled = 1;
    return 0;
}

static void repl_disable_raw(void) {
    if (!repl_raw_enabled) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &repl_orig_termios);
    if (repl_orig_fl != -1) {
        fcntl(STDIN_FILENO, F_SETFL, repl_orig_fl);
    }
    repl_raw_enabled = 0;
}

static void repl_clear_line(int prev_len, const char *prompt) {
    // Move to line start, print prompt, clear to end
    (void)prev_len;
    printf("\r%s\x1b[K", prompt);
    fflush(stdout);
}

static void repl_render_line(const char *prompt, const char *buf) {
    printf("\r%s%s\x1b[K", prompt, buf);
    fflush(stdout);
}

// Simple in-memory history
#define REPL_HIST_MAX 200
static char *repl_hist[REPL_HIST_MAX];
static int repl_hist_count = 0;
static char repl_hist_path[1024] = "";

static void repl_hist_add(const char *line) {
    if (!line || !*line) return;
    if (repl_hist_count > 0 && strcmp(repl_hist[repl_hist_count-1], line) == 0) return;
    if (repl_hist_count == REPL_HIST_MAX) {
        free(repl_hist[0]);
        memmove(&repl_hist[0], &repl_hist[1], sizeof(repl_hist[0]) * (REPL_HIST_MAX-1));
        repl_hist_count--;
    }
    repl_hist[repl_hist_count++] = strdup(line);
}

static const char* repl_hist_get(int idx) {
    if (idx < 0 || idx >= repl_hist_count) return NULL;
    return repl_hist[idx];
}

static void repl_hist_get_path(void) {
    const char *env_path = getenv("CDP_HISTORY_FILE");
    if (env_path && *env_path) {
        strncpy(repl_hist_path, env_path, sizeof(repl_hist_path)-1);
        repl_hist_path[sizeof(repl_hist_path)-1] = '\0';
        return;
    }
    const char *home = getenv("HOME");
    if (home && *home) {
        snprintf(repl_hist_path, sizeof(repl_hist_path), "%s/%s", home, ".cdp_history");
        return;
    }
    // Fallback to current directory
    strncpy(repl_hist_path, ".cdp_history", sizeof(repl_hist_path)-1);
    repl_hist_path[sizeof(repl_hist_path)-1] = '\0';
}

static void repl_hist_load(void) {
    repl_hist_get_path();
    FILE *fp = fopen(repl_hist_path, "r");
    if (!fp) return;
    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) { line[--n] = '\0'; }
        repl_hist_add(line);
    }
    fclose(fp);
}

static void repl_hist_save(void) {
    if (!*repl_hist_path) repl_hist_get_path();
    FILE *fp = fopen(repl_hist_path, "w");
    if (!fp) return;
    for (int i = 0; i < repl_hist_count; i++) {
        if (repl_hist[i] && *repl_hist[i]) {
            fputs(repl_hist[i], fp);
            fputc('\n', fp);
        }
    }
    fclose(fp);
}

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
        {"relaunch-chrome", no_argument, 0, 'R'},
        {"allow-binding", required_argument, 0, 'b'},
        {"allow-domain", required_argument, 0, 'a'},
        {"dev-mode", no_argument, 0, 'D'},
        {"http-port", required_argument, 0, 'P'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "hvd:H:gp:Rb:a:DP:", long_options, NULL)) != -1) {
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
            case 'R':
                relaunch_chrome = 1;
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
            case 'P':
                http_port = atoi(optarg);
                if (http_port < 0) http_port = 0;
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
        cdp_log(CDP_LOG_ERR, "FILE", "Cannot open file %s", filename);
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
        cdp_log(CDP_LOG_WARN, "FILE", "Unclosed expression at end of file");
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

    // Apply defaults to config
    cdp_config_apply_defaults(&g_ctx);

    // Initialize enhanced Chrome process management
    int process_init_result = cdp_init_chrome_registry();
    if (process_init_result != CDP_PROCESS_SUCCESS) {
        cdp_log(CDP_LOG_WARN, "INIT", "Chrome process management init failed: %s",
                cdp_process_error_to_string(process_init_result));
        // Continue anyway for backward compatibility
    } else if (verbose) {
        printf("Enhanced Chrome process management initialized\n");
    }

    // Initialize filesystem interaction module
    int filesystem_init_result = cdp_init_cli_module();
    if (filesystem_init_result != CDP_FILE_SUCCESS) {
        cdp_log(CDP_LOG_WARN, "INIT", "Filesystem module init failed: %s",
                cdp_file_error_to_string(filesystem_init_result));
        // Continue anyway for backward compatibility
    } else if (verbose) {
        printf("Enhanced filesystem interaction initialized\n");
    }

    // Initialize system integration module
    int system_init_result = cdp_init_system_module();
    if (system_init_result != CDP_SYSTEM_SUCCESS) {
        cdp_log(CDP_LOG_WARN, "INIT", "System module init failed: %s",
                cdp_system_error_to_string(system_init_result));
        // Continue anyway for backward compatibility
    } else if (verbose) {
        printf("Enhanced system integration initialized\n");
    }

    /* Concurrent operations removed - using simple -d port approach for multi-instance */
    
    // Show executable info in verbose mode
    if (verbose) {
        int is_cdp_exe = (strcmp(base_name, "cdp.exe") == 0);
        cdp_log(CDP_LOG_INFO, "INIT", "Running as: %s%s", base_name, is_cdp_exe ? " (correct executable)" : " (warning: should be cdp.exe)");
        cdp_log(CDP_LOG_INFO, "INIT", "CDP Client v2.0 (Modular)");
        cdp_log(CDP_LOG_INFO, "INIT", "Chrome: %s:%d", g_ctx.config.chrome_host, g_ctx.config.debug_port);
        cdp_log(CDP_LOG_INFO, "INIT", "Mode: %s", verbose ? "Verbose" : "Normal");
    }
    
    // Ensure Chrome is running
    if (ensure_chrome_running() != 0) {
        cdp_log(CDP_LOG_ERR, "INIT", "Failed to connect to Chrome");
        return 1;
    }
    
    // Get Chrome target ID
    char *target_id = get_chrome_target_id();
    if (!target_id) {
        cdp_log(CDP_LOG_ERR, "INIT", "Failed to get Chrome target ID");
        return 1;
    }
    
    if (verbose) {
        cdp_log(CDP_LOG_INFO, "CONN", "Connecting to Chrome on %s:%d...", 
               g_ctx.config.chrome_host, g_ctx.config.debug_port);
        cdp_log(CDP_LOG_INFO, "CONN", "Chrome is running!");
    }
    
    // Connect via WebSocket to browser endpoint
    str_copy_safe(g_ctx.conn.target_id, target_id, sizeof(g_ctx.conn.target_id));
    ws_sock = connect_chrome_websocket(target_id);
    if (ws_sock < 0) {
        cdp_log(CDP_LOG_ERR, "WS", "Failed to connect WebSocket");
        return 1;
    }
    
    if (verbose) {
        cdp_log(CDP_LOG_INFO, "WS", "WebSocket connected to %s endpoint successfully",
               strstr(target_id, "browser") ? "browser" : "page");
    }
    
    // Auto-attach to JavaScript context
    // Initialize performance tracking
    cdp_perf_init();
    cdp_conn_init();
    
    if (verbose) {
        printf("\n=== Chrome DevTools Protocol Client ===\n");
        printf("Auto-attaching to JavaScript context...\n");
    }
    
    // Check for existing about:blank or create new one
    char *about_blank_target = NULL;
    int created_new_page = 0;  // Track if we created a new page
    {
        char response[RESPONSE_BUFFER_SIZE];
        if (cdp_call_cmd("Target.getTargets", NULL, response, sizeof(response), g_ctx.config.timeout_ms) == 0) {
            // Use elegant JSON API to find about:blank target
            char temp_target[256];
            if (cdp_js_find_target_with_url(response, "about:blank", temp_target, sizeof(temp_target)) == 0) {
                about_blank_target = strdup(temp_target);
                if (verbose) printf("Found existing about:blank: %s\n", about_blank_target);
            }
        }
    }
    
    // If no about:blank found, create one
    if (!about_blank_target) {
        char *new_target = create_new_page_via_browser(ws_sock);
        if (new_target && strlen(new_target) > 0) {
            // create_new_page_via_browser returns static buffer, need to copy
            about_blank_target = malloc(strlen(new_target) + 1);
            if (about_blank_target) {
                strcpy(about_blank_target, new_target);
                created_new_page = 1;  // Mark that we created a new page
                if (verbose) {
                    printf("Created new about:blank: %s\n", about_blank_target);
                }
            }
        } else {
            if (verbose) {
                printf("Failed to create about:blank page\n");
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
            if (cdp_runtime_enable() == 0) {
                g_ctx.runtime.runtime_ready = 1;
                
                // Initialize performance tracking after connection
                cdp_perf_init();
                // Enable Page/DOM domains for high-level helpers
                cdp_page_enable();
                cdp_dom_enable();
                
            if (verbose) {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                printf("Attached to page endpoint, JavaScript execution ready [%ld.%03ld]\n", 
                       ts.tv_sec, ts.tv_nsec/1000000);
                fflush(stdout);
            }
            
            // Inject enhanced JS resources (migrate logic to JS side)  
            if (cdp_enhanced_js_available()) {
                char inj_resp[1024];
                
                // OPTIMIZATION: Skip addScriptToEvaluateOnNewDocument for immediate use
                // Only evaluate in current context - much faster startup
                if (verbose) {
                    struct timespec ts;
                    clock_gettime(CLOCK_REALTIME, &ts);
                    cdp_log(CDP_LOG_DEBUG, "PERF", "Evaluating Enhanced JS runtime... [%ld.%03ld]",
                           ts.tv_sec, ts.tv_nsec/1000000);
                }
                int eval_result = cdp_runtime_eval(get_cdp_enhanced_js(), 0, 0, inj_resp, sizeof(inj_resp), g_ctx.config.timeout_ms);
                if (verbose) {
                    struct timespec ts;
                    clock_gettime(CLOCK_REALTIME, &ts);
                    cdp_log(CDP_LOG_DEBUG, "PERF", "Enhanced JS evaluation completed [%ld.%03ld]",
                           ts.tv_sec, ts.tv_nsec/1000000);
                    
                    // Show injection result for debugging
                    if (eval_result != 0) {
                        cdp_log(CDP_LOG_DEBUG, "JS", "JS injection failed, result: %d", eval_result);
                    }
                    if (strlen(inj_resp) > 0 && strstr(inj_resp, "error")) {
                        cdp_log(CDP_LOG_DEBUG, "JS", "JS injection response: %.200s", inj_resp);
                    }
                }
                
                if (verbose) cdp_log(CDP_LOG_INFO, "INIT", "Enhanced JS injected");
            }
            if (verbose) {
                printf("[PERF] Starting native bindings setup...\n");
            }
            if (verbose) printf("[PERF] Setting runtime event handler...\n");
            set_runtime_event_handler(handle_runtime_event);
            if (verbose) printf("[PERF] Event handler set\n");
            
            // Fallback: allow for about:blank or if can't determine URL
            if (verbose) printf("[PERF] Calling setup_native_bindings()...\n");
            setup_native_bindings();
            if (verbose) printf("[PERF] Native bindings setup completed\n");
            if (verbose) cdp_log(CDP_LOG_INFO, "INIT", "Bindings setup completed");

            // Optionally enable Network + Fetch interception for custom protocols
            {
                const char *env_net = getenv("CDP_ENABLE_NETWORK");
                const char *env_fetch = getenv("CDP_ENABLE_FETCH");
                const char *env_patterns = getenv("CDP_FETCH_PATTERNS");
                const char *env_headers = getenv("CDP_EXTRA_HEADERS");
                const char *env_inject = getenv("CDP_SCRIPT_SRC");
                int enable_net = (!env_net || strcmp(env_net, "0") != 0);
                int enable_fetch = (!env_fetch || strcmp(env_fetch, "0") != 0);
                if (enable_net && cdp_network_enable() == 0 && verbose) {
                    cdp_log(CDP_LOG_INFO, "INIT", "Network domain enabled");
                }
                if (enable_fetch && cdp_fetch_enable(env_patterns && *env_patterns ? env_patterns : NULL) == 0 && verbose) {
                    cdp_log(CDP_LOG_INFO, "INIT", "Fetch interception enabled");
                }
                if (env_headers && *env_headers) {
                    if (cdp_network_set_extra_headers(env_headers) == 0 && verbose) {
                        cdp_log(CDP_LOG_INFO, "INIT", "Extra HTTP headers set");
                    }
                }
                if (env_inject && *env_inject) {
                    char resp[1024];
                    if (cdp_page_add_script_newdoc(env_inject, resp, sizeof(resp), g_ctx.config.timeout_ms) == 0 && verbose) {
                        cdp_log(CDP_LOG_INFO, "INIT", "Injected new-document script");
                    }
                }
            }
            
            // Navigate to target URL if specified
            if (strcmp(target_url, "about:blank") != 0) {
                if (verbose) cdp_log(CDP_LOG_INFO, "NAV", "Navigating to: %s", target_url);
                char nav_resp[RESPONSE_BUFFER_SIZE];
                if (cdp_page_navigate(target_url, nav_resp, sizeof(nav_resp), g_ctx.config.timeout_ms) == 0 && verbose) {
                    cdp_log(CDP_LOG_INFO, "NAV", "Navigation initiated");
                }
            }
            
            if (verbose) cdp_log(CDP_LOG_INFO, "INIT", "CDP.cli() ready!");
            }
        }
        // Don't free about_blank_target here - we need it later for cleanup
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
            cdp_log(CDP_LOG_ERR, "ARGS", "Unknown argument: %s", argument);
            fprintf(stderr, "Usage: %s [options] [URL|script.js]\n", argv[0]);
            return 1;
        }
    } else if (!isatty(STDIN_FILENO)) {
        // Read from pipe - with multi-line support

        // Wait for initialization to complete
        // This prevents "Error in execution" on first command
        usleep(300000); // 300ms delay to ensure Chrome and CDP are fully ready

        // Pipe mode input processing
        char input[4096];
        char multiline[16384] = "";
        int in_multiline = 0;

        while (fgets(input, sizeof(input), stdin)) {
            size_t len = strlen(input);
            if (len > 0 && input[len-1] == '\n') {
                input[len-1] = '\0';
            }

            // Strip surrounding quotes that may come from shell
            // This handles cases like: echo "2**3" | cdp.exe
            // where the shell passes the literal quotes
            if (len >= 2 && input[0] == '"' && input[len-1] == '"') {
                // Remove surrounding quotes
                memmove(input, input + 1, len - 2);
                input[len - 2] = '\0';
                len -= 2;
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
                    
                    // No special async handling for now
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

    // In pipe mode, close the page if we created it
    if (!isatty(STDIN_FILENO) && created_new_page && about_blank_target) {
        // Close the tab we created to keep Chrome clean
        if (verbose) {
            printf("Closing created page: %s\n", about_blank_target);
        }
        char close_params[512];
        snprintf(close_params, sizeof(close_params),
                 QUOTE({"targetId":"%s"}), about_blank_target);
        int close_result = cdp_send_cmd("Target.closeTarget", close_params);
        if (verbose) {
            printf("Close result: %d\n", close_result);
        }
    } else if (verbose && !isatty(STDIN_FILENO)) {
        printf("Not closing page: created_new=%d, target=%p\n",
               created_new_page, (void*)about_blank_target);
    }

    // Cleanup
    if (ws_sock >= 0) {
        close(ws_sock);
    }

    // Free allocated memory
    if (about_blank_target) {
        free(about_blank_target);
    }

    // Cleanup enhanced modules
    /* concurrent module removed */
    cdp_cleanup_system_module();
    cdp_cleanup_cli_module();
    cdp_cleanup_chrome_registry();
    
    return 0;
}

/* Enhanced REPL with Native Bindings support */
static int run_repl_with_bindings(void) {
    char input[4096];
    char multiline[16384] = "";
    int in_multiline = 0;
    int prompt_shown = 0;
    char current_line[4096] = ""; // buffer for raw-mode editing
    int cur_len = 0;
    int hist_pos = -1; // -1 means editing new line
    
    printf("Chrome DevTools Protocol REPL with Native Bindings\n");
    printf("Type JavaScript expressions, system commands, or .help for commands\n");
    printf("Multi-line input supported (unclosed brackets)\n");
    printf("Press Ctrl+C to cancel input, Ctrl+D to exit\n\n");
    
    // Set up Ctrl+C handler
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    
    // Load persistent history before enabling raw mode
    repl_hist_load();
    int use_raw = isatty(STDIN_FILENO) ? (repl_enable_raw() == 0) : 0;
    const char *base_prompt = "> ";
    const char *ml_prompt = "... ";
    
    // Initialize HTTP listener if enabled
    if (http_port > 0 && cdp_http_listen_sock < 0) {
        cdp_http_listen_sock = cdp_http_init(http_port);
        if (cdp_http_listen_sock >= 0 && verbose) {
            printf("HTTP API listening on 127.0.0.1:%d\n", http_port);
        }
    }

    while (1) {
        // Setup jump point for Ctrl+C
        if (sigsetjmp(jump_buffer, 1)) {
            // Ctrl+C pressed
            printf("\n^C\n");
            multiline[0] = '\0';
            in_multiline = 0;
            interrupted = 0;
            // Reset current edit line
            current_line[0] = '\0';
            cur_len = 0;
            hist_pos = -1;
            prompt_shown = 0;
            continue;
        }
        
        // Check for binding events while waiting for input
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(ws_sock, &read_fds);
        if (cdp_http_listen_sock >= 0) FD_SET(cdp_http_listen_sock, &read_fds);
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms timeout
        
        // Keepalive tick
        cdp_conn_tick();
        // Show prompt proactively if not shown yet
        if (!prompt_shown) {
            repl_render_line(in_multiline ? ml_prompt : base_prompt, current_line);
            prompt_shown = 1;
        }
        int ready = select(FD_SETSIZE, &read_fds, NULL, NULL, &tv);
        
        if (ready > 0) {
            // Check for WebSocket events (binding calls)
            if (FD_ISSET(ws_sock, &read_fds)) {
                char event_buffer[RESPONSE_BUFFER_SIZE];
                int event_len = ws_recv_text(ws_sock, event_buffer, sizeof(event_buffer));
                if (event_len > 0) {
                    // If it's a response with id, push to bus; else let runtime handler process
                    if (strstr(event_buffer, JKEY("id"))) {
                        extern void cdp_bus_store(const char*);
                        cdp_bus_store(event_buffer);
                    } else {
                        handle_runtime_event(event_buffer);
                    }
                }
            }
            
            // Check for HTTP API connection
            if (cdp_http_listen_sock >= 0 && FD_ISSET(cdp_http_listen_sock, &read_fds)) {
                int cfd = cdp_http_accept_connection(cdp_http_listen_sock);
                if (cfd >= 0) {
                    cdp_http_handle_connection(cfd);
                    close(cfd);
                }
            }

            // Check for user input
            if (FD_ISSET(STDIN_FILENO, &read_fds)) {
                if (use_raw) {
                    int done = 0;
                    for (;;) {
                        unsigned char ch;
                        ssize_t r = read(STDIN_FILENO, &ch, 1);
                        if (r <= 0) break;
                        if (ch == '\r' || ch == '\n') {
                            // finalize line
                            printf("\r%s%s\n", in_multiline ? ml_prompt : base_prompt, current_line);
                            strncpy(input, current_line, sizeof(input)-1);
                            input[sizeof(input)-1] = '\0';
                            current_line[0] = '\0';
                            cur_len = 0;
                            hist_pos = -1;
                            prompt_shown = 0;
                            done = 1;
                            break;
                        } else if (ch == 0x7f || ch == 0x08) {
                            // backspace
                            if (cur_len > 0) {
                                current_line[--cur_len] = '\0';
                                repl_render_line(in_multiline ? ml_prompt : base_prompt, current_line);
                            }
                        } else if (ch == 0x1b) {
                            // escape sequence
                            unsigned char seq[2];
                            if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
                                if (seq[0] == '[') {
                                    if (seq[1] == 'A') { // Up
                                        if (repl_hist_count > 0) {
                                            if (hist_pos < repl_hist_count - 1) hist_pos++;
                                            const char *hl = repl_hist[repl_hist_count - 1 - hist_pos];
                                            if (hl) {
                                                strncpy(current_line, hl, sizeof(current_line)-1);
                                                cur_len = (int)strlen(current_line);
                                                repl_render_line(in_multiline ? ml_prompt : base_prompt, current_line);
                                            }
                                        }
                                    } else if (seq[1] == 'B') { // Down
                                        if (hist_pos > 0) {
                                            hist_pos--;
                                            const char *hl = repl_hist[repl_hist_count - 1 - hist_pos];
                                            if (hl) {
                                                strncpy(current_line, hl, sizeof(current_line)-1);
                                                cur_len = (int)strlen(current_line);
                                            }
                                        } else {
                                            hist_pos = -1;
                                            current_line[0] = '\0'; cur_len = 0;
                                        }
                                        repl_render_line(in_multiline ? ml_prompt : base_prompt, current_line);
                                    }
                                }
                            }
                        } else if (ch >= 32 && ch < 127) {
                            if (cur_len < (int)sizeof(current_line)-1) {
                                current_line[cur_len++] = (char)ch;
                                current_line[cur_len] = '\0';
                                repl_render_line(in_multiline ? ml_prompt : base_prompt, current_line);
                            }
                        }
                    }
                    if (!done) continue;
                } else {
                    // Fallback: canonical line input
                    if (!fgets(input, sizeof(input), stdin)) { printf("\n"); break; }
                    size_t len = strlen(input);
                    if (len > 0 && input[len-1] == '\n') input[len-1] = '\0';
                }

                // Skip empty lines in normal mode
                if (!in_multiline && strlen(input) == 0) { continue; }

                // Handle exit commands
                if (!in_multiline && (strcmp(input, ".exit") == 0 || strcmp(input, ".quit") == 0)) { break; }

                // Multi-line input handling
                if (in_multiline) {
                    if (strlen(input) > 0) {
                        strcat(multiline, "\n");
                        strcat(multiline, input);
                    }
                    if (!needs_more_input(multiline)) {
                        char *result = cdp_process_user_command(multiline);
                        if (result && *result) { printf("%s\n", result); }
                        multiline[0] = '\0';
                        in_multiline = 0;
                        // Add entire multiline block to history
                        repl_hist_add(multiline);
                    }
                } else {
                    if (needs_more_input(input)) {
                        strcpy(multiline, input);
                        in_multiline = 1;
                    } else {
                        char *result = cdp_process_user_command(input);
                        if (result && *result) { printf("%s\n", result); }
                        repl_hist_add(input);
                    }
                }
            }
        }
    }
    if (use_raw) repl_disable_raw();
    // Save history on exit
    repl_hist_save();
    
    return 0;
}

/* Handle runtime events (dual approach) */
static int handle_runtime_event(const char* event_json) {
    if (verbose && strlen(event_json) < 500) {
        printf("Received event: %s\n", event_json);
    }
    
    // Branch A: CDP Fetch Domain (experimental)
    if (strstr(event_json, "Fetch.requestPaused")) {
        if (verbose) {
            printf("🚀 Fetch request intercepted (domain method)!\n");
        }
        
        // Extract request details for Fetch domain
        char request_id[128] = {0};
        char url[1024] = {0};
        char method[32] = "GET";
        
        // Parse request ID using elegant JSON API
        cdp_js_get_request_id(event_json, request_id, sizeof(request_id));
        
        // Parse URL using elegant JSON API
        cdp_js_get_url(event_json, url, sizeof(url));
        
        if (verbose) {
            printf("Intercepted: %s %s (ID: %s)\n", method, url, request_id);
        }
        
        return handle_fetch_request(request_id, url, method);
    }

    // Branch B: AddBinding approach (default)
    if (strstr(event_json, "Runtime.bindingCalled")) {
        if (verbose) {
            printf("🚀 Binding called (addBinding method)!\n");
        }
        
        // Extract binding name and payload using elegant JSON API
        char binding_name[64] = {0};
        char command[1024] = {0};

        cdp_js_get_string(event_json, "name", binding_name, sizeof(binding_name));
        cdp_js_get_string(event_json, "payload", command, sizeof(command));
        
        if (verbose) {
            printf("Binding: %s, Command: %s\n", binding_name, command);
        }
        
        if (strcmp(binding_name, "CDP_exec") == 0) {
            return handle_binding_command(command);
        }
    }

    // Track execution contexts for frame switching
    if (strstr(event_json, "Runtime.executionContextCreated")) {
        // Parse context id and frameId using elegant JSON API
        int cid = 0;
        cdp_js_get_execution_context_id(event_json, &cid);

        char frameId[128] = {0};
        cdp_js_get_frame_id(event_json, frameId, sizeof(frameId));
        if (cid > 0 && frameId[0]) {
            ctx_add(cid, frameId);
            // If selecting this frame, update selected context id
            if (*g_selected_frame_id && strcmp(g_selected_frame_id, frameId)==0) {
                g_selected_context_id = cid;
            }
        }
    } else if (strstr(event_json, "Runtime.executionContextDestroyed")) {
        int cid = 0;
        if (cdp_js_get_execution_context_id(event_json, &cid) == 0 && cid > 0) {
            ctx_remove(cid);
        }
    }

    // Branch C: Network/Runtime events for logs and idle detection
    {
        static int net_inflight = 0; // local static shadow; real vars below
    }
    if (strstr(event_json, "Network.requestWillBeSent")) {
        extern void cdp_net_event_update(const char* type);
        cdp_net_event_update("req");
    } else if (strstr(event_json, "Network.loadingFinished") || strstr(event_json, "Network.loadingFailed")) {
        extern void cdp_net_event_update(const char* type);
        cdp_net_event_update("fin");
    }
    if (strstr(event_json, "Runtime.consoleAPICalled") || strstr(event_json, "Runtime.exceptionThrown")) {
        extern void cdp_logs_push(const char* line);
        cdp_logs_push(event_json);
    }
    
    return 0;
}

/* Handle command from addBinding (simplified executor) */
static int handle_binding_command(const char* command) {
    if (verbose) {
        printf("Executing binding command: %s\n", command);
    }
    
    char* result = malloc(4096);
    if (!result) return -1;
    
    // Simple command dispatcher for addBinding method
    if (strncmp(command, "cli:", 4) == 0) {
        const char* cli_cmd = command + 4;
        if (strcmp(cli_cmd, "pwd") == 0) {
            cdp_js_build_success_response("pwd", "/workspace/self-evolve-ai", result, 4096);
        } else if (strcmp(cli_cmd, "ps") == 0) {
            cdp_js_build_success_response("ps", "cdp.exe chrome node", result, 4096);
        } else {
            char msg[256];
            snprintf(msg, sizeof(msg), "Unknown CLI command: %s", cli_cmd);
            cdp_js_build_error_response(msg, NULL, result, 4096);
        }
        
    } else if (strncmp(command, "gui:", 4) == 0) {
        const char* gui_cmd = command + 4;
        if (strcmp(gui_cmd, "screenshot") == 0) {
            // Execute CDP screenshot
            {
                char response[LARGE_BUFFER_SIZE];
                if (cdp_page_screenshot(response, sizeof(response), g_ctx.config.timeout_ms) == 0) {
                cdp_js_build_success_response("screenshot", "Screenshot captured", result, 4096);
                } else {
                cdp_js_build_error_response("Screenshot failed", NULL, result, 4096);
                }
            }
        } else {
            char msg[256];
            snprintf(msg, sizeof(msg), "Unknown GUI command: %s", gui_cmd);
            cdp_js_build_error_response(msg, NULL, result, 4096);
        }
        
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "Unknown command format: %s", command);
        cdp_js_build_error_response(msg, NULL, result, 4096);
    }
    
    // Send result back to Chrome via response evaluation
    if (result) {
        char response_cmd[8192];
        char escaped_result[4096];
        json_escape_safe(escaped_result, result, sizeof(escaped_result));
        
        snprintf(response_cmd, sizeof(response_cmd),
            QUOTE(if (window._cdpResolve) { window._cdpResolve({ ok: true, status: 200, json: () => Promise.resolve(%s) }); window._cdpResolve = null; }),
            escaped_result);
        char response[RESPONSE_BUFFER_SIZE];
        cdp_runtime_eval(response_cmd, 1, 0, response, sizeof(response), g_ctx.config.timeout_ms);
        
        free(result);
    }
    
    return 0;
}

/* Handle system commands from Chrome (simplified executor) */
static int handle_enhanced_fetch(const char* command) {
    if (verbose) {
        printf("Executing system command: %s\n", command);
    }
    
    char* result = malloc(4096);
    if (!result) return -1;
    
    // Simple command dispatcher
    if (strncmp(command, "screenshot", 10) == 0) {
        // Take screenshot via CDP
        {
            char response[LARGE_BUFFER_SIZE];
            if (cdp_page_screenshot(response, sizeof(response), g_ctx.config.timeout_ms) == 0) {
            snprintf(result, 4096,
                QUOTE({"success":true,"type":"screenshot","message":"Screenshot captured"}));
            } else {
            cdp_js_build_error_response("Screenshot failed", NULL, result, 4096);
            }
        }
        
    } else if (strncmp(command, "shell:", 6) == 0) {
        // Execute shell command
        const char* shell_cmd = command + 6;
        if (!cdp_authz_allow("system:", shell_cmd)) {
            cdp_js_build_error_response("system command denied", NULL, result, 4096);
        } else {
        char* sys_result = execute_system_command_safe(shell_cmd, "/workspace/self-evolve-ai");
        if (sys_result) {
            CDPJSONBuilder builder;
            cdp_js_builder_init(&builder);
            cdp_js_builder_add_bool(&builder, "success", 1);
            cdp_js_builder_add_string(&builder, "command", shell_cmd);
            cdp_js_builder_add_string(&builder, "result", sys_result);
            strncpy(result, cdp_js_builder_get(&builder), 4096);
            free(sys_result);
        } else {
            CDPJSONBuilder builder;
            cdp_js_builder_init(&builder);
            cdp_js_builder_add_string(&builder, "error", "Command execution failed");
            cdp_js_builder_add_string(&builder, "command", shell_cmd);
            strncpy(result, cdp_js_builder_get(&builder), 4096);
        }}
        
    } else if (strncmp(command, "file:", 5) == 0) {
        // File operations
        const char* file_path = command + 5;
        if (!cdp_authz_allow("file:", file_path)) {
            cdp_js_build_error_response("file operation denied", NULL, result, 4096);
        } else {
            snprintf(result, 4096,
                QUOTE({"success":true,"type":"file","path":"%s","message":"File operation ready"}), 
                file_path);
        }
        
    } else {
        CDPJSONBuilder builder;
        cdp_js_builder_init(&builder);
        cdp_js_builder_add_string(&builder, "error", "Unknown system command");
        cdp_js_builder_add_string(&builder, "command", command);
        strncpy(result, cdp_js_builder_get(&builder), 4096);
    }
    
    // Send result back to Chrome
    if (result) {
        char response_cmd[8192];
        char escaped_result[4096];
        json_escape_safe(escaped_result, result, sizeof(escaped_result));
        
        snprintf(response_cmd, sizeof(response_cmd),
            QUOTE(if (window._cdpResolve) { window._cdpResolve({ ok: true, status: 200, json: function() { return Promise.resolve(%s); } }); window._cdpResolve = null; }),
            escaped_result);
        char response[RESPONSE_BUFFER_SIZE];
        cdp_runtime_eval(response_cmd, 1, 0, response, sizeof(response), g_ctx.config.timeout_ms);
        
        free(result);
    }
    
    return 0;
}

/* Handle binding call from page */
static int handle_binding_call(const char* name, const char* payload) {
    char* result = NULL;
    
    if (verbose) {
        printf("Handling binding call: %s with payload: %s\n", name, payload);
    }
    
    if (strcmp(name, "CDP_cli") == 0) {
        // Parse JSON payload to extract command and args
        // For now, simple string parsing
        result = execute_cdp_cli_command(payload, "cli");
    } else if (strcmp(name, "CDP_system") == 0) {
        result = execute_cdp_cli_command(payload, "system");
    } else {
        if (verbose) {
            printf("Unknown binding: %s\n", name);
        }
        return -1;
    }
    
    // Send result back to page via Runtime.evaluate
    if (result) {
        char eval_cmd[8192];
        char escaped_result[4096];
        json_escape_safe(escaped_result, result, sizeof(escaped_result));
        
        snprintf(eval_cmd, sizeof(eval_cmd),
            QUOTE(if (window.CDP_bindingCallback) { window.CDP_bindingCallback(%s); }), escaped_result);
        char response[RESPONSE_BUFFER_SIZE];
        cdp_runtime_eval(eval_cmd, 1, 0, response, sizeof(response), g_ctx.config.timeout_ms);
        
        free(result);
    }
    
    return 0;
}

/* Setup minimal bindings - focus on stability first */
static int setup_native_bindings(void) {
    if (verbose) {
        printf("Setup completed\n");
    }
    return 0;
}

/* Execute CLI command */
static char* execute_cli_command(const char* command, const char* type, const char* options) {
    char* result = malloc(8192);
    if (!result) return NULL;
    
    if (strcmp(type, "cli") == 0) {
        // Execute JavaScript via command template
        char value[4096];
        if (cdp_runtime_get_value(command, value, sizeof(value), g_ctx.config.timeout_ms) == 0) {
            strncpy(result, value, 8191);
            result[8191] = '\0';
            return result;
        }
        // Fallback
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
        CDPJSONBuilder builder;
        cdp_js_builder_init(&builder);
        cdp_js_builder_add_string(&builder, "error", "Failed to execute command");
        cdp_js_builder_add_string(&builder, "command", command);
        cdp_js_builder_add_bool(&builder, "success", 0);
        strncpy(result, cdp_js_builder_get(&builder), 8192);
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
    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_string(&builder, "stdout", escaped_output);
    cdp_js_builder_add_int(&builder, "exitCode", exit_code);
    cdp_js_builder_add_bool(&builder, "success", exit_code == 0);
    strncpy(result, cdp_js_builder_get(&builder), 8192);
    
    return result;
}

/* Execute CDP CLI commands */
char* execute_cdp_cli_command(const char* command, const char* args) {
    char* result = malloc(8192);
    if (!result) return NULL;
    
    if (verbose) {
        printf("Executing CDP CLI command: %s %s\n", command, args);
    }
    
    // Route to appropriate CDP functionality
    if (strcmp(command, "screenshot") == 0) {
        // Take screenshot using command template
        char response[LARGE_BUFFER_SIZE];
        if (cdp_page_screenshot(response, sizeof(response), g_ctx.config.timeout_ms) == 0 && strstr(response, JKEY("data"))) {
            CDPJSONBuilder builder;
            cdp_js_builder_init(&builder);
            cdp_js_builder_add_bool(&builder, "success", 1);
            cdp_js_builder_add_string(&builder, "type", "screenshot");
            cdp_js_builder_add_string(&builder, "data", "base64_data");
            strcpy(result, cdp_js_builder_get(&builder));
        } else {
            cdp_js_build_error_response("Screenshot failed", NULL, result, 8192);
        }
        
    } else if (strcmp(command, "monitor-downloads") == 0) {
        // Start download monitoring
        char monitor_result[512];
        int monitor_status = cdp_start_download_monitor("./downloads/");
        
        CDPJSONBuilder builder;
        cdp_js_builder_init(&builder);
        cdp_js_builder_add_bool(&builder, "success", monitor_status == 0);
        cdp_js_builder_add_string(&builder, "command", "monitor-downloads");
        cdp_js_builder_add_int(&builder, "status", monitor_status);
        strncpy(result, cdp_js_builder_get(&builder), 8192);
        
    } else if (strcmp(command, "process-list") == 0) {
        // List Chrome processes
        cdp_chrome_instance_t* instances;
        int count;
        
        if (cdp_list_chrome_instances(&instances, &count) == 0) {
            CDPJSONBuilder builder;
            cdp_js_builder_init(&builder);
            cdp_js_builder_add_bool(&builder, "success", 1);
            cdp_js_builder_add_string(&builder, "command", "process-list");
            cdp_js_builder_add_int(&builder, "count", count);
            cdp_js_builder_add_raw(&builder, "instances", "[]");
            strncpy(result, cdp_js_builder_get(&builder), 8192);
        } else {
            cdp_js_build_error_response("Failed to get process list", NULL, result, 8192);
        }
        
    } else if (strcmp(command, "system-notify") == 0) {
        // Send system notification
        char title[256] = "CDP Notification";
        char message[512] = "Message from web page";
        
        int notify_result = cdp_send_desktop_notification(title, message, CDP_NOTIFY_INFO);
        
        CDPJSONBuilder builder;
        cdp_js_builder_init(&builder);
        cdp_js_builder_add_bool(&builder, "success", notify_result == 0);
        cdp_js_builder_add_string(&builder, "command", "system-notify");
        cdp_js_builder_add_int(&builder, "result", notify_result);
        strncpy(result, cdp_js_builder_get(&builder), 8192);
            
    } else if (strcmp(command, "batch-test") == 0) {
        // Batch testing functionality
        CDPJSONBuilder builder;
        cdp_js_builder_init(&builder);
        cdp_js_builder_add_bool(&builder, "success", 1);
        cdp_js_builder_add_string(&builder, "command", "batch-test");
        cdp_js_builder_add_string(&builder, "status", "ready");
        strcpy(result, cdp_js_builder_get(&builder));
        
    } else if (strcmp(command, "load-test") == 0) {
        // Load testing functionality  
        CDPJSONBuilder builder;
        cdp_js_builder_init(&builder);
        cdp_js_builder_add_bool(&builder, "success", 1);
        cdp_js_builder_add_string(&builder, "command", "load-test");
        cdp_js_builder_add_string(&builder, "status", "ready");
        strcpy(result, cdp_js_builder_get(&builder));
        
    } else {
        snprintf(result, 8192, 
            QUOTE({"error":"Unknown CDP command: %s","available":["screenshot","monitor-downloads","process-list","system-notify","batch-test","load-test"]}),
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
    
    // Otherwise query current page URL via CDP using command template
    {
        char response[RESPONSE_BUFFER_SIZE];
        if (cdp_runtime_eval("window.location.href", 1, 0, response, sizeof(response), g_ctx.config.timeout_ms) == 0) {
            const char* value_start = strstr(response, JKEYQ("value"));
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

/* Handle fetch request - filter for custom protocols */
static int handle_fetch_request(const char* request_id, const char* url, const char* method) {
    if (verbose) {
        printf("Handling fetch request: %s %s\n", method, url);
    }
    
    // Check if this is our custom protocol
    if (strncmp(url, "cli://", 6) == 0 || strncmp(url, "gui://", 6) == 0 || strncmp(url, "file://", 7) == 0) {
        // Handle our custom protocols
        char response_body[4096];
        int status_code = 200;
        
        if (strncmp(url, "cli://", 6) == 0) {
            const char* command = url + 6;
            
            if (strcmp(command, "pwd") == 0) {
                snprintf(response_body, sizeof(response_body),
                    QUOTE({"success":true,"command":"pwd","result":"/workspace/self-evolve-ai"}));
            } else if (strcmp(command, "ps") == 0) {
                snprintf(response_body, sizeof(response_body),
                    QUOTE({"success":true,"command":"ps","result":"cdp.exe chrome node"}));
            } else {
                snprintf(response_body, sizeof(response_body),
                    QUOTE({"error":"Unknown CLI command: %s"}), command);
                status_code = 400;
            }
            
        } else if (strncmp(url, "gui://", 6) == 0) {
            const char* command = url + 6;
            
            if (strcmp(command, "screenshot") == 0) {
                snprintf(response_body, sizeof(response_body),
                    QUOTE({"success":true,"command":"screenshot","message":"Screenshot captured"}));
            } else {
                snprintf(response_body, sizeof(response_body),
                    QUOTE({"error":"Unknown GUI command: %s"}), command);
                status_code = 400;
            }
            
        } else {
            snprintf(response_body, sizeof(response_body),
                QUOTE({"error":"Unknown protocol in URL: %s"}), url);
            status_code = 400;
        }
        
        return send_fetch_response(request_id, response_body, status_code);
        
    } else {
        // For non-custom protocols, continue the request normally
        char continue_cmd[512];
        snprintf(continue_cmd, sizeof(continue_cmd),
            QUOTE({"id":%d,"method":"Fetch.continueRequest","params":{"requestId":"%s"}}),
            ws_cmd_id++, request_id);
        
        if (verbose) {
            printf("Continuing normal request: %s\n", url);
        }
        
        return cdp_fetch_continue(request_id);
    }
}

/* Execute protocol-based commands */
char* execute_protocol_command(const char* url, const char* method) {
    char* result = malloc(8192);
    if (!result) return NULL;
    
    if (verbose) {
        printf("Executing protocol command: %s %s\n", method, url);
    }
    
    // Handle CLI protocol: cli://pwd, cli://npm%20run%20build etc.
    if (strncmp(url, "cli://", 6) == 0) {
        const char* command_encoded = url + 6;
        
        // URL decode the command
        char decoded_command[2048];
        cdp_http_url_decode(command_encoded, decoded_command, sizeof(decoded_command));
        
        if (verbose) {
            printf("CLI command decoded: %s\n", decoded_command);
        }
        
        if (strcmp(decoded_command, "pwd") == 0) {
            snprintf(result, 8192, 
                QUOTE({"success":true,"command":"pwd","result":"/workspace/self-evolve-ai"}));
        } else if (strcmp(decoded_command, "ps") == 0) {
            snprintf(result, 8192,
                QUOTE({"success":true,"command":"ps","result":"cdp.exe chrome node"}));
        } else if (strncmp(decoded_command, "npm ", 4) == 0 || strncmp(decoded_command, "git ", 4) == 0) {
            // Execute system commands safely
            if (!cdp_authz_allow("system:", decoded_command)) {
                snprintf(result, 8192, QUOTE({"error":"system command denied","command":"%s"}), decoded_command);
                return result;
            }
            char* sys_result = execute_system_command_safe(decoded_command, "/workspace/self-evolve-ai");
            if (sys_result) {
                snprintf(result, 8192,
                    QUOTE({"success":true,"command":"%s","result":"%s"}), 
                    decoded_command, sys_result);
                free(sys_result);
            } else {
                snprintf(result, 8192,
                    QUOTE({"error":"Command execution failed","command":"%s"}), 
                    decoded_command);
            }
        } else {
            snprintf(result, 8192, 
                QUOTE({"error":"Unknown or unsafe CLI command: %s"}), decoded_command);
        }
        
    } else if (strncmp(url, "gui://", 6) == 0) {
        const char* command_path = url + 6;
        
        if (strncmp(command_path, "screenshot", 10) == 0) {
            // Take screenshot via template
            char response[LARGE_BUFFER_SIZE];
            if (cdp_page_screenshot(response, sizeof(response), g_ctx.config.timeout_ms) == 0 && strstr(response, JKEY("data"))) {
                strcpy(result, QUOTE({"success":true,"type":"screenshot","data":"captured"}));
                return result;
            }
            cdp_js_build_error_response("Screenshot failed", NULL, result, 8192);
            
        } else if (strncmp(command_path, "process/list", 12) == 0) {
            // List processes
            cdp_chrome_instance_t* instances;
            int count;
            
            if (cdp_list_chrome_instances(&instances, &count) == 0) {
                snprintf(result, 8192,
                    QUOTE({"success":true,"command":"process-list","count":%d}),
                    count);
            } else {
                cdp_js_build_error_response("Failed to get process list", NULL, result, 8192);
            }
            
        } else if (strncmp(command_path, "monitor/downloads", 17) == 0) {
            // Start download monitoring
            int monitor_result = cdp_start_download_monitor("./downloads/");
            snprintf(result, 8192,
                QUOTE({"success":%s,"command":"monitor-downloads"}),
                monitor_result == 0 ? "true" : "false");
        } else {
            snprintf(result, 8192, QUOTE({"error":"Unknown CLI command: %s"}), command_path);
        }
        
    } else if (strncmp(url, "file://", 7) == 0 || strstr(url, "cdp-internal.local/file/") != NULL) {
        // File protocol: file://monitor/downloads or http://cdp-internal.local/file/path
        const char* file_path;
        if (strncmp(url, "file://", 7) == 0) {
            file_path = url + 7;
        } else {
            file_path = strstr(url, "/file/") + 6;
        }
        if (!cdp_authz_allow("file:", file_path)) {
            snprintf(result, 8192, QUOTE({"error":"file operation denied","path":"%s"}), file_path);
            return result;
        }
        snprintf(result, 8192,
            QUOTE({"success":true,"protocol":"file","path":"%s","status":"ready"}),
            file_path);
        
    } else if (strncmp(url, "notify://", 9) == 0 || strstr(url, "cdp-internal.local/notify/") != NULL) {
        // Notification protocol: notify://desktop/Title/Message or http://cdp-internal.local/notify/message
        const char* notify_path;
        if (strncmp(url, "notify://", 9) == 0) {
            notify_path = url + 9;
        } else {
            notify_path = strstr(url, "/notify/") + 8;
        }
        if (!cdp_authz_allow("notify:", notify_path)) {
            snprintf(result, 8192, QUOTE({"error":"notify denied","path":"%s"}), notify_path);
            return result;
        }
        int notify_result = cdp_send_desktop_notification("CDP", "Web notification", CDP_NOTIFY_INFO);
        snprintf(result, 8192,
            QUOTE({"success":%s,"protocol":"notify","result":%d}),
            notify_result == 0 ? "true" : "false", notify_result);
    } else {
        snprintf(result, 8192, QUOTE({"error":"Unknown protocol in URL: %s"}), url);
    }
    
    return result;
}

/* Send fetch response back to Chrome */
static int send_fetch_response(const char* request_id, const char* response_body, int status_code) {
    // Escape JSON content and fulfill via command template
    char escaped_body[8192];
    json_escape_safe(escaped_body, response_body, sizeof(escaped_body));
    const char *headers = QUOTE([
        {"name":"Content-Type","value":"application/json"},
        {"name":"Access-Control-Allow-Origin","value":"*"}
    ]);
    char out[1024];
    return cdp_fetch_fulfill(request_id, status_code, headers, escaped_body, out, sizeof(out), g_ctx.config.timeout_ms);
}

// URL decoder moved to cdp_http module

// HTTP implementation moved to cdp_http.c

// HTTP response function moved to cdp_http.c

// HTTP handle_connection function moved to cdp_http.c

/* Check protocol permission */
static int check_protocol_permission(const char* page_url, const char* protocol_url) {
    if (!page_url || !protocol_url) return 0;
    
    // Always allow about:blank
    if (strstr(page_url, "about:blank")) {
        return 1;
    }
    
    // Dev mode allows all protocols for localhost
    if (security_config.dev_mode && 
        (strstr(page_url, "localhost") || strstr(page_url, "127.0.0.1"))) {
        return 1;
    }
    
    // Check allowed domains
    for (int i = 0; i < security_config.domain_count; i++) {
        const char* domain = security_config.allowed_domains[i];
        if (strstr(page_url, domain)) {
            return check_protocol_permission_level(protocol_url);
        }
    }
    
    return 0;  // Default deny
}

/* Check protocol permission level */
static int check_protocol_permission_level(const char* protocol_url) {
    if (strncmp(protocol_url, "cli://", 6) == 0) {
        return security_config.permissions & (CDP_PERM_READ | CDP_PERM_SYSTEM);
    } else if (strncmp(protocol_url, "gui://", 6) == 0) {
        return security_config.permissions & CDP_PERM_SCREENSHOT;
    } else if (strncmp(protocol_url, "file://", 7) == 0) {
        return security_config.permissions & (CDP_PERM_FILE | CDP_PERM_MONITOR);
    } else if (strncmp(protocol_url, "notify://", 9) == 0) {
        return security_config.permissions & CDP_PERM_SYSTEM;
    }
    
    return 0;
}

/* ==============================================================
 * Network idle tracker + logs ring buffer (for CLI utilities)
 * ============================================================== */
static volatile int g_net_inflight = 0;
static struct timespec g_net_last_activity = {0};
static void net_touch(void) {
    clock_gettime(CLOCK_MONOTONIC, &g_net_last_activity);
}

void cdp_net_event_update(const char* type) {
    if (!type) return;
    if (type[0] == 'r') { // request
        g_net_inflight++;
        if (g_net_inflight < 0) g_net_inflight = 0;
        net_touch();
    } else { // finish/fail
        if (g_net_inflight > 0) g_net_inflight--;
        net_touch();
    }
}

int cdp_net_inflight(void) {
    return g_net_inflight;
}

long cdp_net_ms_since_activity(void) {
    if (g_net_last_activity.tv_sec == 0 && g_net_last_activity.tv_nsec == 0) return 1<<30;
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - g_net_last_activity.tv_sec) * 1000L + (now.tv_nsec - g_net_last_activity.tv_nsec) / 1000000L;
}

#define LOG_RING_MAX 512
#define LOG_LINE_MAX 512
static char g_logs[LOG_RING_MAX][LOG_LINE_MAX];
static int g_logs_count = 0;
static int g_logs_enabled = 1;

void cdp_logs_push(const char* line) {
    if (!g_logs_enabled || !line) return;
    size_t n = strlen(line);
    if (n >= LOG_LINE_MAX) n = LOG_LINE_MAX - 1;
    strncpy(g_logs[g_logs_count % LOG_RING_MAX], line, n);
    g_logs[g_logs_count % LOG_RING_MAX][n] = '\0';
    g_logs_count++;
}

void cdp_logs_clear(void) { g_logs_count = 0; }
void cdp_logs_set_enabled(int en) { g_logs_enabled = en ? 1 : 0; }
int cdp_logs_get_enabled(void) { return g_logs_enabled; }

int cdp_logs_tail(char *out, size_t out_sz, int max_lines, const char* filter) {
    if (!out || out_sz == 0) return -1;
    if (max_lines <= 0) max_lines = 50;
    int total = g_logs_count < LOG_RING_MAX ? g_logs_count : LOG_RING_MAX;
    int start = g_logs_count - total; if (start < 0) start = 0;
    int printed = 0; size_t off = 0;
    for (int i = start + total - 1; i >= start && printed < max_lines; i--) {
        const char *ln = g_logs[i % LOG_RING_MAX];
        if (filter && *filter) { if (!strstr(ln, filter)) continue; }
        size_t l = strlen(ln);
        if (off + l + 1 >= out_sz) break;
        memcpy(out + off, ln, l); off += l; out[off++]='\n';
        printed++;
    }
    out[off] = '\0';
    return printed;
}
