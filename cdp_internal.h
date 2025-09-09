/**
 * CDP Internal Header - Shared structures and declarations
 * For internal use by CDP modules only
 */

#ifndef CDP_INTERNAL_H
#define CDP_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "cdp_utils.h"

/* Constants */
#define MAX_CHILDREN 32
#define MAX_CMD_SIZE 8192
#define MAX_RESULT_SIZE 4096
#define INITIAL_BUFFER_SIZE 4096
#define MAX_BUFFER_SIZE (10 * 1024 * 1024)

/* Network Constants */
#define CHROME_DEFAULT_PORT 9222
#define DEFAULT_TIMEOUT_MS 5000
#define MAX_RECONNECT_ATTEMPTS 5
#define RECONNECT_BASE_DELAY_MS 1000
#define RECONNECT_MAX_DELAY_MS 30000

/* WebSocket Constants */
#define WS_FRAME_HEADER_SIZE 10
#define WS_MAX_PAYLOAD_SIZE 65536
#define WS_PING_INTERVAL_MS 30000

/* Buffer Sizes */
#define SMALL_BUFFER_SIZE 256
#define MEDIUM_BUFFER_SIZE 1024
#define LARGE_BUFFER_SIZE 4096
#define RESPONSE_BUFFER_SIZE 8192

/* Debug logging macro */
#define DEBUG_LOG(...) do { if (g_ctx.config.debug_mode) { fprintf(stderr, "[DEBUG] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); } } while(0)

/* Error codes */
typedef enum {
    CDP_SUCCESS = 0,
    CDP_ERR_CONNECTION_FAILED,
    CDP_ERR_WEBSOCKET_FAILED,
    CDP_ERR_CHROME_NOT_FOUND,
    CDP_ERR_TIMEOUT,
    CDP_ERR_INVALID_RESPONSE,
    CDP_ERR_COMMAND_FAILED,
    CDP_ERR_MEMORY,
    CDP_ERR_INVALID_ARGS
} CDPError;

/* Error handling structure */
typedef struct {
    CDPError code;
    char message[256];
    char details[512];
} CDPErrorInfo;

/* CDP Context - replaces all global variables */
typedef struct {
    /* Configuration */
    struct {
        int debug_port;
        int server_port;
        char *user_data_dir;
        char *server_host;
        char *chrome_host;
        int verbose;
        char *init_script;
        char *init_file;
        char *script_file;
        int debug_mode;
        int timeout_ms;
        int dom_events;
        int network_events;
        int console_events;
        /* Track if pointers were dynamically allocated */
        int server_host_allocated;
        int chrome_host_allocated;
        int user_data_dir_allocated;
    } config;
    
    /* Connection state */
    struct {
        int ws_sock;
        int server_sock;
        int connected;
        time_t last_activity;
        char target_id[128];
        int reconnect_attempts;
        int max_reconnect_attempts;
        int reconnect_delay_ms;
    } conn;
    
    /* Runtime state */
    struct {
        int command_id;
        int runtime_ready;
        int page_ready;
    } runtime;
    
    /* Child process tracking */
    struct {
        pid_t pid;
        int id;
        int pipe_fd;
    } children[MAX_CHILDREN];
    int num_children;
} CDPContext;

/* Command routing */
typedef struct {
    const char *prefix;
    char* (*handler)(const char *cmd);
    const char *description;
} CommandRoute;

/* WebSocket client abstraction */
typedef struct {
    int socket;
    int connected;
    char last_error[256];
} WebSocketClient;

/* Retry configuration */
typedef struct {
    int max_attempts;
    int base_delay_ms;
    double backoff_factor;
    int max_delay_ms;
} RetryConfig;

/* Global context - extern declaration for all modules */
extern CDPContext g_ctx;
extern CDPErrorInfo g_last_error;
extern int verbose;
extern int debug_mode;
extern int ws_sock;
extern int ws_cmd_id;

/* Module function declarations */

/* WebSocket module (cdp_websocket.c) */
int ws_send_text(int sock, const char *text);
int ws_recv_text(int sock, char *buffer, size_t max_len);
int connect_chrome_websocket(const char *target_id);
int reconnect_websocket_with_backoff(void);
int check_ws_health(void);

/* Chrome module (cdp_chrome.c) */
char* get_chrome_target_id(void);
char* create_new_page_via_browser(int browser_sock);
char* find_chrome_executable(void);
void launch_chrome(void);
int ensure_chrome_running(void);

/* Command module (cdp_commands.c) */
char* handle_help_command(const char *cmd);
char* handle_system_command(const char *cmd);
char* handle_cdp_command(const char *cmd);
int validate_system_command(const char *cmd);
void execute_async(int cmd_id, const char *command);
void poll_system_commands(void);

/* JavaScript module (cdp_javascript.c) */
char* execute_javascript(const char *expression);
char* parse_javascript_result(const char *buffer, char *result, size_t result_size);
int send_command_with_retry(const char *command);
int receive_response_by_id(char *buffer, size_t max_len, int cmd_id, int max_messages);

/* REPL module (cdp_repl.c) */
int app_run_repl(void);
int process_repl_input(char *input);
int handle_special_commands(const char *input);

/* Error handling - Enhanced API */
#define cdp_error_push(code, fmt, ...) \
    cdp_error_push_ex(code, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

int cdp_error_push_ex(CDPError code, const char *file, int line, const char *fmt, ...);
CDPError cdp_error_last_code(void);
const char* cdp_error_last_message(void);
const char* cdp_error_stack_trace(void);
void cdp_error_clear(void);
CDPError cdp_error_pop(void);
int cdp_has_error(void);
void cdp_error_stats(int *total_errors, int *stack_depth);
int cdp_check_result(int result, CDPError error_code, const char *operation);
void* cdp_check_ptr(void *ptr, const char *what);

/* Legacy compatibility */
#define cdp_set_error(code, msg, details) cdp_error_push(code, "%s: %s", msg, details)
#define cdp_get_last_error() cdp_error_last_code()
#define cdp_get_error_message() cdp_error_last_message()
#define cdp_clear_error() cdp_error_clear()

/* Utility functions */
void sigchld_handler(int sig);
void print_usage(const char *prog_name);

/* User features module (cdp_user_features.c) */
int cdp_execute_script_file(const char *filename);
int cdp_execute_script_files(char **filenames, int count);
char* cdp_beautify_output(const char *result);
const char* cdp_get_shortcut(const char *command);
void cdp_show_shortcuts(void);
void cdp_perf_init(void);
void cdp_perf_track(double time_ms);
void cdp_show_stats(void);
char* cdp_process_user_command(const char *input);
int cdp_inject_helpers(void);

#endif /* CDP_INTERNAL_H */