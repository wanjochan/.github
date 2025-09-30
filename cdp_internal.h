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
#include <stdarg.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "cdp_utils.h"

/* Helper macro to write JSON/JS snippets without escape hell */
#ifndef QUOTE
#define QUOTE(...) #__VA_ARGS__
#endif

/* Common min/MAX style helpers */
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

/* JSON key helpers for parser patterns */
#ifndef JKEY
#define JKEY(k) "\"" k "\":"
#endif
#ifndef JKEYQ
#define JKEYQ(k) "\"" k "\":\""
#endif
#ifndef JPAIR
#define JPAIR(k,v) "\"" k "\":\"" v "\""
#endif

/* Dynamic JSON format helpers for snprintf */
#ifndef JKEY_FMT
#define JKEY_FMT "\"%s\":"
#endif
#ifndef JPAIR_FMT
#define JPAIR_FMT "\"%s\":\"%s\""
#endif
#ifndef JNUM_FMT
#define JNUM_FMT "\"%s\":%d"
#endif
#ifndef JBOOL_FMT
#define JBOOL_FMT "\"%s\":%s"
#endif

/* Constants */
#define MAX_CHILDREN 32
#define MAX_CMD_SIZE 65536  // Increased to 64KB for large JS injections
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
/* cdp_get_shortcut removed - shortcuts now handled by JS Enhanced API */
void cdp_show_shortcuts(void);
void cdp_perf_init(void);
void cdp_perf_track(double time_ms);
void cdp_show_stats(void);
char* cdp_process_user_command(const char *input);
int cdp_inject_helpers(void);

/* =================================================================== */
/* MERGED MODULES DECLARATIONS (from consolidated modules)            */
/* =================================================================== */

/* Log module types and functions (from cdp_log.h) */
typedef enum {
    CDP_LOG_DEBUG = 0,
    CDP_LOG_INFO  = 1,
    CDP_LOG_WARN  = 2,
    CDP_LOG_ERR   = 3
} cdp_log_level_t;
void cdp_log(cdp_log_level_t level, const char *module, const char *fmt, ...);

/* Config module functions (from cdp_config.h) */
void cdp_config_apply_defaults(CDPContext *ctx);
void cdp_config_dump(const CDPContext *ctx);

/* Auth module functions (from cdp_auth.h) */
int cdp_authz_allow(const char *action, const char *target);

/* Bus module functions (from cdp_bus.h) */
typedef void (*cdp_bus_cb_t)(const char *json, void *user);
int cdp_send_cmd(const char *method, const char *params_json);
int cdp_call_cmd(const char *method, const char *params_json,
                 char *out_response, size_t out_size, int timeout_ms);
void cdp_bus_store(const char *json);
int cdp_bus_try_get(int id, char *out, size_t out_size);
int cdp_bus_register(int id, cdp_bus_cb_t cb, void *user);
int cdp_bus_unregister(int id);

/* CLI module constants and functions */
#define CDP_FILE_SUCCESS 0
int cdp_handle_cli_protocol(const char* url, char* response, size_t response_size);
int cdp_init_cli_module(void);
int cdp_cleanup_cli_module(void);
int cdp_validate_file_exists(const char* file_path);
const char* cdp_file_error_to_string(int error_code);
int cdp_start_download_monitor(const char* dir);

/* Connection module functions (from cdp_conn.h) */
void cdp_conn_init(void);
void cdp_conn_tick(void);

/* Process/Chrome module constants and types (from merged cdp_process.h) */
#define CDP_PROCESS_SUCCESS 0
#define CDP_MAX_CHROME_INSTANCES 32
#define CDP_MAX_PATH_LENGTH 512
#define CDP_MAX_FLAGS_LENGTH 1024
#define CDP_MAX_STATUS_LENGTH 32
#define CDP_MAX_ERROR_MESSAGE 256
#define CDP_DEFAULT_HEALTH_CHECK_INTERVAL 30
#define CDP_MAX_RESTART_ATTEMPTS 3
#define CDP_PROCESS_TIMEOUT_SEC 30

/* Error codes specific to process management */
typedef enum {
    CDP_PROCESS_ERR_INVALID_CONFIG = -1000,
    CDP_PROCESS_ERR_LAUNCH_FAILED = -1001,
    CDP_PROCESS_ERR_INSTANCE_NOT_FOUND = -1002,
    CDP_PROCESS_ERR_INSTANCE_LIMIT_REACHED = -1003,
    CDP_PROCESS_ERR_KILL_FAILED = -1004,
    CDP_PROCESS_ERR_HEALTH_CHECK_FAILED = -1005,
    CDP_PROCESS_ERR_CLEANUP_FAILED = -1006,
    CDP_PROCESS_ERR_PORT_CONFLICT = -1007,
    CDP_PROCESS_ERR_PERMISSION_DENIED = -1008,
    CDP_PROCESS_ERR_TIMEOUT = -1009,
    CDP_PROCESS_ERR_MEMORY = -1010
} cdp_process_error_t;

typedef enum {
    CDP_CHROME_STATUS_UNKNOWN = 0,
    CDP_CHROME_STATUS_STARTING,
    CDP_CHROME_STATUS_RUNNING,
    CDP_CHROME_STATUS_STOPPING,
    CDP_CHROME_STATUS_STOPPED,
    CDP_CHROME_STATUS_CRASHED,
    CDP_CHROME_STATUS_FAILED
} cdp_chrome_status_t;

/* Chrome configuration structure */
typedef struct {
    char profile_dir[CDP_MAX_PATH_LENGTH];
    int debug_port;
    int headless;
    int disable_gpu;
    int disable_web_security;
    char additional_flags[1024];
    int window_width;
    int window_height;
    char user_data_dir[CDP_MAX_PATH_LENGTH];
    char chrome_binary[CDP_MAX_PATH_LENGTH];
    int incognito;
    int no_sandbox;
    int disable_dev_shm_usage;
    int memory_limit_mb;
    int timeout_sec;
    int auto_restart;
    int max_restart_attempts;
    time_t created_time;
    char user_agent[512];
    char proxy_server[CDP_MAX_PATH_LENGTH];
    char extra_flags[CDP_MAX_FLAGS_LENGTH];
} cdp_chrome_config_t;

/* Chrome instance structure */
typedef struct {
    /* Instance identification */
    int instance_id;
    pid_t pid;
    int debug_port;
    
    /* Paths and configuration */
    char profile_path[CDP_MAX_PATH_LENGTH];
    char user_data_dir[CDP_MAX_PATH_LENGTH];
    cdp_chrome_config_t config;
    
    /* Status and timing */
    cdp_chrome_status_t status;
    time_t start_time;
    time_t last_health_check;
    time_t last_activity;
    
    /* Health monitoring */
    int health_check_failures;
    int restart_count;
    int auto_restart_enabled;
    
    /* Resource usage */
    double cpu_usage;
    size_t memory_usage_mb;
    
    /* Error tracking */
    char last_error[256];  /* CDP_MAX_ERROR_MESSAGE */
    int error_count;
    
    /* Thread safety */
    pthread_mutex_t mutex;
    
    /* Internal state */
    int websocket_connected;
    time_t last_websocket_activity;
} cdp_chrome_instance_t;

/* Chrome instance registry */
typedef struct {
    cdp_chrome_instance_t instances[CDP_MAX_CHROME_INSTANCES];
    int instance_count;
    int next_instance_id;
    int next_debug_port;
    pthread_mutex_t registry_mutex;
    
    /* Global settings */
    int health_check_interval;
    int auto_cleanup_enabled;
    char temp_dir[CDP_MAX_PATH_LENGTH];
} cdp_chrome_registry_t;

/* Health check callback function type */
typedef void (*cdp_health_callback_t)(cdp_chrome_instance_t* instance, void* user_data);

int cdp_init_chrome_registry(void);
int cdp_cleanup_chrome_registry(void);
int cdp_list_chrome_instances(cdp_chrome_instance_t** instances, int* count);
const char* cdp_process_error_to_string(int error_code);
int cdp_create_chrome_config(cdp_chrome_config_t* config);
int cdp_validate_chrome_config(const cdp_chrome_config_t* config);
int cdp_launch_chrome_instance(const cdp_chrome_config_t* config, cdp_chrome_instance_t* instance);
int cdp_kill_chrome_instance(int instance_id, int force);
int cdp_get_instance_status(int instance_id, cdp_chrome_instance_t* status);
int cdp_find_instance_by_pid(pid_t pid, cdp_chrome_instance_t** instance);
int cdp_find_instance_by_port(int debug_port, cdp_chrome_instance_t** instance);
int cdp_monitor_chrome_health(int instance_id, int check_interval_sec);
int cdp_set_health_check_callback(cdp_health_callback_t callback, void* user_data);
int cdp_auto_restart_on_crash(int instance_id, int max_restart_attempts);
int cdp_check_instance_health(int instance_id);
int cdp_get_instance_metrics(int instance_id, double* cpu_usage, size_t* memory_mb);
int cdp_is_chrome_running(pid_t pid);
int cdp_chrome_stop(void);

#endif /* CDP_INTERNAL_H */
