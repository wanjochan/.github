/**
 * CDP Core Abstractions
 * Provides high-level abstractions for Chrome DevTools Protocol
 */

#ifndef CDP_CORE_H
#define CDP_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* Forward declarations */
typedef struct CDPContext CDPContext;
typedef struct CDPMessage CDPMessage;
typedef struct CDPApp CDPApp;
typedef struct WebSocketClient WebSocketClient;

/* Configuration structure */
typedef struct {
    /* Network settings */
    char* chrome_host;
    int debug_port;
    char* server_host;
    int server_port;
    
    /* Chrome settings */
    char* user_data_dir;
    char* chrome_binary;
    
    /* Runtime settings */
    int verbose;
    char* init_script;
    char* init_file;
    
    /* Event subscriptions */
    int dom_events;
    int network_events;
    int console_events;
    
    /* Limits and timeouts */
    int max_children;
    int connect_timeout_ms;
    int command_timeout_ms;
    int max_retries;
} CDPConfig;

/* Connection state */
typedef struct {
    int socket;
    int connected;
    time_t last_activity;
    int reconnect_count;
    char target_id[128];
} CDPConnection;

/* Runtime state */
typedef struct {
    int command_id;
    int runtime_ready;
    int page_ready;
} CDPRuntime;

/* Child process info */
typedef struct {
    pid_t pid;
    int command_id;
    int pipe_fd;
    time_t start_time;
} CDPProcess;

/* Process pool */
typedef struct {
    CDPProcess* processes;
    int count;
    int capacity;
} CDPProcessPool;

/* Main context - replaces all global variables */
struct CDPContext {
    CDPConfig config;
    CDPConnection connection;
    CDPRuntime runtime;
    CDPProcessPool process_pool;
    
    /* Internal state */
    void* ws_client;  /* WebSocketClient* */
    void* chrome_controller;
    void* event_dispatcher;
    void* command_queue;
    
    /* Callbacks */
    void (*on_connect)(CDPContext* ctx);
    void (*on_disconnect)(CDPContext* ctx);
    void (*on_error)(CDPContext* ctx, const char* error);
    void (*on_event)(CDPContext* ctx, const char* event);
};

/* CDP Protocol Message abstraction */
struct CDPMessage {
    int id;
    char* method;
    char* params_json;  /* JSON string of parameters */
    char* result_json;  /* JSON string of result */
    char* error_json;   /* JSON string of error */
    
    /* Internal */
    char* raw_json;     /* Full message JSON */
    int is_event;       /* True if this is an event, not response */
};

/* Message builder for type-safe protocol construction */
typedef struct {
    CDPMessage* message;
    CDPContext* context;
} CDPMessageBuilder;

/* Application lifecycle */
struct CDPApp {
    CDPContext* context;
    
    /* Lifecycle methods */
    int (*init)(CDPApp* app, int argc, char* argv[]);
    int (*configure)(CDPApp* app);
    int (*connect)(CDPApp* app);
    int (*run)(CDPApp* app);
    void (*cleanup)(CDPApp* app);
    
    /* State */
    int running;
    int exit_code;
};

/* Context management */
CDPContext* cdp_context_create(void);
void cdp_context_destroy(CDPContext* ctx);
int cdp_context_init(CDPContext* ctx, CDPConfig* config);
CDPConfig* cdp_config_from_args(int argc, char* argv[]);
CDPConfig* cdp_config_defaults(void);

/* Message construction */
CDPMessageBuilder* cdp_message_builder_new(CDPContext* ctx);
CDPMessageBuilder* cdp_message_method(CDPMessageBuilder* builder, const char* method);
CDPMessageBuilder* cdp_message_param(CDPMessageBuilder* builder, const char* key, const char* value);
CDPMessageBuilder* cdp_message_param_int(CDPMessageBuilder* builder, const char* key, int value);
CDPMessageBuilder* cdp_message_param_bool(CDPMessageBuilder* builder, const char* key, int value);
CDPMessage* cdp_message_build(CDPMessageBuilder* builder);
void cdp_message_destroy(CDPMessage* msg);

/* Protocol operations */
int cdp_send_message(CDPContext* ctx, CDPMessage* msg);
CDPMessage* cdp_receive_message(CDPContext* ctx, int timeout_ms);
CDPMessage* cdp_execute_command(CDPContext* ctx, const char* method, const char* params);

/* Application lifecycle */
CDPApp* cdp_app_create(void);
void cdp_app_destroy(CDPApp* app);
int cdp_app_run(CDPApp* app, int argc, char* argv[]);

/* Connection management */
int cdp_connect(CDPContext* ctx);
int cdp_disconnect(CDPContext* ctx);
int cdp_reconnect(CDPContext* ctx);
int cdp_is_connected(CDPContext* ctx);

/* Event handling */
typedef void (*CDPEventHandler)(CDPContext* ctx, CDPMessage* event, void* userdata);
int cdp_subscribe_event(CDPContext* ctx, const char* event_name, CDPEventHandler handler, void* userdata);
int cdp_unsubscribe_event(CDPContext* ctx, const char* event_name);
int cdp_enable_domain(CDPContext* ctx, const char* domain);

/* Process management */
int cdp_execute_system_command(CDPContext* ctx, const char* command);
int cdp_get_process_output(CDPContext* ctx, int command_id, char* buffer, size_t size);

/* Utility functions */
int cdp_get_next_id(CDPContext* ctx);
const char* cdp_get_error_string(int error_code);
void cdp_set_verbose(CDPContext* ctx, int verbose);

/* Error codes */
enum {
    CDP_OK = 0,
    CDP_ERROR_CONNECT = -1,
    CDP_ERROR_TIMEOUT = -2,
    CDP_ERROR_PROTOCOL = -3,
    CDP_ERROR_MEMORY = -4,
    CDP_ERROR_INVALID_ARG = -5,
    CDP_ERROR_NOT_CONNECTED = -6,
    CDP_ERROR_CHROME_NOT_FOUND = -7,
    CDP_ERROR_MAX_PROCESSES = -8
};

#endif /* CDP_CORE_H */