#ifndef MOD_CHILD_H
#define MOD_CHILD_H

/*
 * mod_child - Node.js-style child_process module for cosmorun
 *
 * Provides process spawning and management with stream integration:
 * - spawn(): Spawn process with stream I/O
 * - exec(): Execute and buffer output
 * - execSync(): Synchronous execution
 * - kill(): Send signals to child
 * - wait(): Wait for child to exit
 *
 * Integration:
 * - mod_stream: stdin/stdout/stderr as streams
 * - mod_events: 'exit', 'error', 'close' events
 * - mod_os: Process execution foundation
 *
 * Example:
 *   child_options_t opts = {0};
 *   opts.capture_stdout = 1;
 *   child_process_t *child = child_spawn("/bin/echo", args, &opts);
 *   child_wait(child);
 *   printf("Exit code: %d\n", child->exit_code);
 *   child_free(child);
 */

/* Forward declarations to avoid header conflicts */
#ifndef COSMO_LIBC_H
#include "src/cosmo_libc.h"
#endif

#include "mod_events.h"

/* ==================== Forward Declarations ==================== */

typedef struct child_process_t child_process_t;
typedef struct stream_t stream_t;  // Forward declaration for future stream integration

/* ==================== Callback Types ==================== */

/**
 * Callback for exec() - called when process completes
 * @param error Error message (NULL if no error)
 * @param stdout_data Captured stdout data
 * @param stdout_len Length of stdout data
 * @param stderr_data Captured stderr data
 * @param stderr_len Length of stderr data
 * @param userdata User-provided context
 */
typedef void (*child_exec_callback_t)(
    const char *error,
    const void *stdout_data,
    size_t stdout_len,
    const void *stderr_data,
    size_t stderr_len,
    void *userdata
);

/* ==================== Stdio Configuration ==================== */

typedef enum {
    CHILD_STDIO_PIPE = 0,       // Create pipe (default)
    CHILD_STDIO_INHERIT = 1,    // Inherit from parent
    CHILD_STDIO_IGNORE = 2,     // Redirect to /dev/null
    CHILD_STDIO_FD = 3          // Use specific file descriptor
} child_stdio_mode_t;

typedef struct {
    child_stdio_mode_t mode;
    int fd;  // Only used if mode == CHILD_STDIO_FD
} child_stdio_config_t;

/* ==================== Child Process Options ==================== */

typedef struct {
    // Working directory
    const char *cwd;

    // Environment variables (NULL-terminated array of "KEY=VALUE" strings)
    char **env;

    // Stdio configuration
    child_stdio_config_t stdin_cfg;
    child_stdio_config_t stdout_cfg;
    child_stdio_config_t stderr_cfg;

    // Convenience flags (overridden by stdio configs)
    int capture_stdout;  // Set stdout_cfg to PIPE
    int capture_stderr;  // Set stderr_cfg to PIPE
    int inherit_stdio;   // Set all to INHERIT

    // Shell execution
    int use_shell;       // Execute via /bin/sh -c

    // Detached process
    int detached;        // Don't wait for child
} child_options_t;

/* Initialize options with defaults */
#define CHILD_OPTIONS_INIT {0}

/* ==================== Child Process Structure ==================== */

struct child_process_t {
    // Process info
    pid_t pid;
    int exit_code;
    int running;
    int signaled;
    int signal_code;

    // Streams (NULL if not configured as pipe)
    stream_t *stdin_stream;
    stream_t *stdout_stream;
    stream_t *stderr_stream;

    // Internal file descriptors
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;

    // Event emitter
    event_emitter_t *emitter;

    // Error tracking
    char *error_message;

    // Options (copy)
    child_options_t options;

    // Internal state
    int killed;
    int exit_handled;
};

/* ==================== Core API ==================== */

/**
 * Spawn a child process
 * @param command Command path or name
 * @param args NULL-terminated argument array (args[0] should be command)
 * @param options Process options (NULL for defaults)
 * @return Child process instance or NULL on error
 */
child_process_t* child_spawn(const char *command, char **args, child_options_t *options);

/**
 * Execute command and buffer output (asynchronous)
 * @param command Command to execute (shell string if contains spaces)
 * @param callback Completion callback
 * @param userdata User data for callback
 * @return Child process instance or NULL on error
 */
child_process_t* child_exec(const char *command, child_exec_callback_t callback, void *userdata);

/**
 * Execute command synchronously
 * @param command Command to execute
 * @param stdout_buf Buffer for stdout (NULL to ignore)
 * @param stdout_size Buffer size
 * @param stderr_buf Buffer for stderr (NULL to ignore)
 * @param stderr_size Buffer size
 * @return Exit code, or -1 on error
 */
int child_exec_sync(
    const char *command,
    char *stdout_buf,
    size_t stdout_size,
    char *stderr_buf,
    size_t stderr_size
);

/**
 * Send signal to child process
 * @param child Child process instance
 * @param signal Signal number (e.g., SIGTERM, SIGKILL)
 * @return 0 on success, -1 on error
 */
int child_kill(child_process_t *child, int signal);

/**
 * Wait for child process to exit
 * @param child Child process instance
 * @param timeout_ms Timeout in milliseconds (0 = no wait, -1 = infinite)
 * @return Exit code on completion, -2 on timeout, -1 on error
 */
int child_wait(child_process_t *child, int timeout_ms);

/**
 * Check if child is still running (non-blocking)
 * @param child Child process instance
 * @return 1 if running, 0 if exited, -1 on error
 */
int child_is_running(child_process_t *child);

/**
 * Free child process resources
 * @param child Child process instance
 */
void child_free(child_process_t *child);

/* ==================== Event API ==================== */

/**
 * Register event listener on child process
 * Events:
 * - "exit": (int*) exit_code - Process exited normally
 * - "error": (char*) error_message - Error occurred
 * - "close": NULL - All stdio streams closed
 * - "spawn": NULL - Process spawned successfully
 *
 * @param child Child process instance
 * @param event Event name
 * @param listener Event listener callback
 * @param userdata User data for callback
 */
void child_on(child_process_t *child, const char *event, event_listener_fn listener, void *userdata);

/**
 * Remove event listener
 * @param child Child process instance
 * @param event Event name
 * @param listener Callback to remove
 */
void child_off(child_process_t *child, const char *event, event_listener_fn listener);

/* ==================== Utility Functions ==================== */

/**
 * Parse command string into argv array
 * Simple parser that splits on spaces (no quote handling)
 * @param command Command string
 * @param out_argv Output argv array (caller must free each element and array)
 * @return Argument count
 */
int child_parse_command(const char *command, char ***out_argv);

/**
 * Build environment array from key-value pairs
 * @param pairs Array of "KEY=VALUE" strings
 * @param count Number of pairs
 * @return NULL-terminated environment array (caller must free)
 */
char** child_build_env(const char **pairs, size_t count);

/**
 * Free argv array
 * @param argv Argument array
 * @param argc Argument count
 */
void child_free_argv(char **argv, int argc);

/**
 * Set default options
 * @param options Options structure to initialize
 */
void child_options_init(child_options_t *options);

#endif /* MOD_CHILD_H */
