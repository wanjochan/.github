 /**
 * Chrome DevTools Protocol Client with JavaScript REPL
 * 
 * FEATURES:
 * - Auto-connects to Chrome debugging port (9222)
 * - Auto-attaches to about:blank page for JavaScript execution
 * - Reuses existing about:blank or creates new one
 * - Direct JavaScript expression evaluation
 * - Browser endpoint for management (Target, Browser, SystemInfo)
 * - Page endpoint for JavaScript execution (Runtime.evaluate)
 * 
 * BUILD(using cosmopolitan for win/lnx/osx and x86/arm):
 * 	./cdp_build.sh
 * 
 * USAGE:
 *   ./cdp.exe              # Auto-attach to JavaScript context
 *   ./cdp.exe -v           # Verbose mode
 *   ./cdp.exe -d 9222      # Custom Chrome debug port
 * 
 * EXAMPLES:
 *   2**3                   # => 8
 *   Math.random()          # => 0.123456...
 *   'Hello ' + 'World'     # => Hello World
 *   [1,2,3].map(x => x*2)  # => [2,4,6]
 *   new Date()             # => Current date
 * 
 * COMMANDS:
 *   .attach <targetId>     # Switch to page endpoint
 *   .detach                # Return to browser endpoint
 *   .quit/.exit            # Exit program
 *   CDP.<domain>.<method>  # Execute CDP command
 * 
 * DATE: 2025-09-08
 */
/**
 * CDP Main Module - Minimal entry point
 * Uses modular architecture
 */

#include "cdp_internal.h"
#include <getopt.h>
#include <unistd.h>

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

/* Print usage information */
void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("\nOptions:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -v, --verbose       Enable verbose output\n");
    printf("  -d, --debug-port    Chrome debug port (default: 9222)\n");
    printf("  -H, --host          Chrome host (default: 127.0.0.1)\n");
    printf("\nExamples:\n");
    printf("  %s                  # Start REPL\n", prog_name);
    printf("  echo '2+3' | %s     # Evaluate expression\n", prog_name);
    printf("  %s -v               # Verbose mode\n", prog_name);
}

/* Parse command line arguments */
static int parse_args(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"debug-port", required_argument, 0, 'd'},
        {"host", required_argument, 0, 'H'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "hvd:H:", long_options, NULL)) != -1) {
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
            default:
                print_usage(argv[0]);
                return -1;
        }
    }
    
    return 0;
}

/* Simple REPL */
static int run_repl(void) {
    char input[4096];
    
    printf("Chrome DevTools Protocol REPL\n");
    printf("Type JavaScript expressions or .help for commands\n");
    printf("Press Ctrl+D to exit\n\n");
    
    while (1) {
        printf("> ");
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
        
        // Skip empty lines
        if (strlen(input) == 0) {
            continue;
        }
        
        // Handle special commands
        if (strcmp(input, ".help") == 0) {
            printf("Commands:\n");
            printf("  .help    Show this help\n");
            printf("  .exit    Exit REPL\n");
            printf("  .quit    Exit REPL\n");
            printf("\nOr type any JavaScript expression\n");
            continue;
        }
        
        if (strcmp(input, ".exit") == 0 || strcmp(input, ".quit") == 0) {
            break;
        }
        
        // Process with user features (shortcuts, beautification, etc.)
        char *result = cdp_process_user_command(input);
        if (result && *result) {
            printf("%s\n", result);
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
        about_blank_target = create_new_page_via_browser(ws_sock);
        if (about_blank_target && verbose) {
            printf("Created new about:blank: %s\n", about_blank_target);
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
                
                // Only inject helpers if we have a valid WebSocket connection
                if (ws_sock > 0 && g_ctx.runtime.runtime_ready) {
                    // Small delay to ensure Runtime is fully ready
                    // Cosmopolitan provides usleep for all platforms
                    usleep(100000);  // 100ms delay
                    
                    if (cdp_inject_helpers() == 0 && verbose) {
                        printf("Helper functions injected: $(), $$(), $x(), sleep(), copy()\n");
                    }
                }
                
                if (verbose) {
                    printf("Attached to page endpoint, JavaScript execution ready\n");
                    printf("Type .help for shortcuts, .stats for statistics\n\n");
                }
            }
        }
        if (about_blank_target) {
            free(about_blank_target);
            about_blank_target = NULL;
        }
    }
    
    // Check if input is from pipe
    if (!isatty(STDIN_FILENO)) {
        // Read from pipe
        char input[4096];
        while (fgets(input, sizeof(input), stdin)) {
            size_t len = strlen(input);
            if (len > 0 && input[len-1] == '\n') {
                input[len-1] = '\0';
            }
            
            if (strlen(input) > 0) {
                char *result = cdp_process_user_command(input);
                if (result && *result) {
                    printf("%s\n", result);
                }
            }
        }
    } else {
        // Interactive REPL
        run_repl();
    }
    
    // Cleanup
    if (ws_sock >= 0) {
        close(ws_sock);
    }
    
    return 0;
}
