/**
 * CDP Async Execution Module
 * Non-blocking command execution with callback support
 */

#include "cdp_internal.h"
#include <sys/select.h>
#include <pthread.h>
#include <errno.h>
/* #include "cdp_log.h" - merged into cdp_internal.h */

/* Command queue structures */
#define MAX_PENDING_COMMANDS 100
#define COMMAND_TIMEOUT_MS 30000

typedef enum {
    CMD_STATE_PENDING,
    CMD_STATE_SENT,
    CMD_STATE_WAITING,
    CMD_STATE_COMPLETED,
    CMD_STATE_FAILED,
    CMD_STATE_TIMEOUT
} CommandState;

typedef struct {
    int id;
    char command[MAX_CMD_SIZE];
    char response[RESPONSE_BUFFER_SIZE];
    CommandState state;
    time_t timestamp;
    void (*callback)(int id, const char *response, void *user_data);
    void *user_data;
    int timeout_ms;
} AsyncCommand;

typedef struct {
    AsyncCommand commands[MAX_PENDING_COMMANDS];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int running;
    pthread_t worker_thread;
} CommandQueue;

/* Global command queue */
static CommandQueue g_cmd_queue = {
    .head = 0,
    .tail = 0,
    .count = 0,
    .running = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER
};

/* External variables */
extern int ws_sock;
extern int ws_cmd_id;
extern int verbose;

/* Forward declarations */
static void* async_worker_thread(void *arg);
static int process_pending_commands(void);
static int wait_for_response(int cmd_id, char *buffer, size_t size, int timeout_ms);

/* Initialize async execution system */
int cdp_async_init(void) {
    pthread_mutex_lock(&g_cmd_queue.mutex);
    
    if (g_cmd_queue.running) {
        pthread_mutex_unlock(&g_cmd_queue.mutex);
        return 0; // Already running
    }
    
    g_cmd_queue.running = 1;
    g_cmd_queue.head = 0;
    g_cmd_queue.tail = 0;
    g_cmd_queue.count = 0;
    
    // Start worker thread
    if (pthread_create(&g_cmd_queue.worker_thread, NULL, async_worker_thread, NULL) != 0) {
        g_cmd_queue.running = 0;
        pthread_mutex_unlock(&g_cmd_queue.mutex);
        return cdp_error_push(CDP_ERR_COMMAND_FAILED, "Failed to create worker thread");
    }
    
    pthread_mutex_unlock(&g_cmd_queue.mutex);
    
    if (verbose) {
        printf("Async execution system initialized\n");
    }
    
    return 0;
}

/* Shutdown async execution system */
void cdp_async_shutdown(void) {
    pthread_mutex_lock(&g_cmd_queue.mutex);
    g_cmd_queue.running = 0;
    pthread_cond_signal(&g_cmd_queue.cond);
    pthread_mutex_unlock(&g_cmd_queue.mutex);
    
    // Wait for worker thread to finish
    pthread_join(g_cmd_queue.worker_thread, NULL);
    
    if (verbose) {
        printf("Async execution system shutdown\n");
    }
}

/* Queue a command for async execution */
int cdp_async_execute(const char *command, 
                      void (*callback)(int, const char*, void*),
                      void *user_data,
                      int timeout_ms) {
    if (!command) {
        return cdp_error_push(CDP_ERR_INVALID_ARGS, "Command is NULL");
    }
    
    pthread_mutex_lock(&g_cmd_queue.mutex);
    
    if (g_cmd_queue.count >= MAX_PENDING_COMMANDS) {
        pthread_mutex_unlock(&g_cmd_queue.mutex);
        return cdp_error_push(CDP_ERR_COMMAND_FAILED, "Command queue is full");
    }
    
    // Add command to queue
    AsyncCommand *cmd = &g_cmd_queue.commands[g_cmd_queue.tail];
    cmd->id = ws_cmd_id++;
    str_copy_safe(cmd->command, command, sizeof(cmd->command));
    cmd->state = CMD_STATE_PENDING;
    cmd->timestamp = time(NULL);
    cmd->callback = callback;
    cmd->user_data = user_data;
    cmd->timeout_ms = timeout_ms > 0 ? timeout_ms : COMMAND_TIMEOUT_MS;
    cmd->response[0] = '\0';
    
    g_cmd_queue.tail = (g_cmd_queue.tail + 1) % MAX_PENDING_COMMANDS;
    g_cmd_queue.count++;
    
    // Signal worker thread
    pthread_cond_signal(&g_cmd_queue.cond);
    
    int cmd_id = cmd->id;
    pthread_mutex_unlock(&g_cmd_queue.mutex);
    
    return cmd_id;
}

/* Worker thread for processing commands */
static void* async_worker_thread(void *arg) {
    (void)arg;
    
    while (1) {
        pthread_mutex_lock(&g_cmd_queue.mutex);
        
        // Wait for commands or shutdown signal
        while (g_cmd_queue.running && g_cmd_queue.count == 0) {
            pthread_cond_wait(&g_cmd_queue.cond, &g_cmd_queue.mutex);
        }
        
        if (!g_cmd_queue.running) {
            pthread_mutex_unlock(&g_cmd_queue.mutex);
            break;
        }
        
        pthread_mutex_unlock(&g_cmd_queue.mutex);
        
        // Process pending commands
        process_pending_commands();
        
        // Small delay to prevent busy waiting
        usleep(10000); // 10ms
    }
    
    return NULL;
}

/* Process all pending commands */
static int process_pending_commands(void) {
    fd_set read_fds, write_fds, error_fds;
    struct timeval tv;
    int max_fd = 0;
    int processed = 0;
    
    pthread_mutex_lock(&g_cmd_queue.mutex);
    
    if (g_cmd_queue.count == 0 || ws_sock < 0) {
        pthread_mutex_unlock(&g_cmd_queue.mutex);
        return 0;
    }
    
    // Process each command in queue
    for (int i = 0; i < MAX_PENDING_COMMANDS && processed < g_cmd_queue.count; i++) {
        int idx = (g_cmd_queue.head + i) % MAX_PENDING_COMMANDS;
        AsyncCommand *cmd = &g_cmd_queue.commands[idx];
        
        if (cmd->state == CMD_STATE_PENDING) {
            // Send command (with retry)
            if (send_command_with_retry(cmd->command) > 0) {
                cmd->state = CMD_STATE_SENT;
                cmd->timestamp = time(NULL);
                processed++;
            } else {
                cmd->state = CMD_STATE_FAILED;
                str_copy_safe(cmd->response, "Failed to send command", sizeof(cmd->response));
                cdp_log(CDP_LOG_ERR, "ASYNC", "Send failed for id=%d", cmd->id);
            }
        }
    }
    
    pthread_mutex_unlock(&g_cmd_queue.mutex);
    
    // Now check for responses using select
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&error_fds);
    
    if (ws_sock >= 0) {
        FD_SET(ws_sock, &read_fds);
        FD_SET(ws_sock, &error_fds);
        max_fd = ws_sock;
    }
    
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100ms timeout
    
    int ret = select(max_fd + 1, &read_fds, NULL, &error_fds, &tv);
    
    if (ret > 0 && FD_ISSET(ws_sock, &read_fds)) {
        // Read available responses and stash on the bus
        char buffer[RESPONSE_BUFFER_SIZE];
        int len = ws_recv_text(ws_sock, buffer, sizeof(buffer));
        if (len > 0) {
            extern void cdp_bus_store(const char*);
            cdp_bus_store(buffer);
        }
    }

    // Try to fulfill any SENT commands from the bus
    pthread_mutex_lock(&g_cmd_queue.mutex);
    for (int i = 0; i < MAX_PENDING_COMMANDS; i++) {
        int idx = (g_cmd_queue.head + i) % MAX_PENDING_COMMANDS;
        AsyncCommand *cmd = &g_cmd_queue.commands[idx];
        if (cmd->state == CMD_STATE_SENT) {
            extern int cdp_bus_try_get(int, char*, size_t);
            if (cdp_bus_try_get(cmd->id, cmd->response, sizeof(cmd->response))) {
                cmd->state = CMD_STATE_COMPLETED;
                if (cmd->callback) cmd->callback(cmd->id, cmd->response, cmd->user_data);
                if (idx == g_cmd_queue.head) {
                    g_cmd_queue.head = (g_cmd_queue.head + 1) % MAX_PENDING_COMMANDS;
                    g_cmd_queue.count--;
                }
            }
        }
    }
    pthread_mutex_unlock(&g_cmd_queue.mutex);
    
    // Check for timeouts
    time_t now = time(NULL);
    pthread_mutex_lock(&g_cmd_queue.mutex);
    
    for (int i = 0; i < MAX_PENDING_COMMANDS && i < g_cmd_queue.count; i++) {
        int idx = (g_cmd_queue.head + i) % MAX_PENDING_COMMANDS;
        AsyncCommand *cmd = &g_cmd_queue.commands[idx];
        
        if (cmd->state == CMD_STATE_SENT) {
            if ((now - cmd->timestamp) * 1000 > cmd->timeout_ms) {
                cmd->state = CMD_STATE_TIMEOUT;
                str_copy_safe(cmd->response, "Command timed out", sizeof(cmd->response));
                cdp_log(CDP_LOG_WARN, "ASYNC", "Command timeout id=%d after %dms", cmd->id, cmd->timeout_ms);
                
                if (cmd->callback) {
                    cmd->callback(cmd->id, cmd->response, cmd->user_data);
                }
            }
        }
    }
    
    pthread_mutex_unlock(&g_cmd_queue.mutex);
    
    return processed;
}

/* Execute multiple JavaScript expressions in parallel */
int cdp_async_batch_execute(const char **expressions, int count,
                            void (*callback)(int, const char**, void*),
                            void *user_data) {
    if (!expressions || count <= 0) {
        return cdp_error_push(CDP_ERR_INVALID_ARGS, "Invalid expressions array");
    }
    
    int *cmd_ids = malloc(count * sizeof(int));
    if (!cmd_ids) {
        return cdp_error_push(CDP_ERR_MEMORY, "Failed to allocate command IDs");
    }
    
    // Queue all commands
    for (int i = 0; i < count; i++) {
        char command[MAX_CMD_SIZE];
        char escaped[MAX_CMD_SIZE];
        
        json_escape_safe(escaped, expressions[i], sizeof(escaped));
        snprintf(command, sizeof(command),
                QUOTE({"id":%d,"method":"Runtime.evaluate","params":{"expression":"%s","returnByValue":true}}),
                ws_cmd_id, escaped);
        
        cmd_ids[i] = cdp_async_execute(command, NULL, NULL, DEFAULT_TIMEOUT_MS);
        if (cmd_ids[i] < 0) {
            free(cmd_ids);
            return cmd_ids[i];
        }
    }
    
    // Wait for all to complete (simplified for now)
    // In a real implementation, this would be truly async
    char **results = malloc(count * sizeof(char*));
    if (results) {
        for (int i = 0; i < count; i++) {
            results[i] = malloc(RESPONSE_BUFFER_SIZE);
            if (results[i]) {
                // Would normally get from completed queue
                strcpy(results[i], "async result");
            }
        }
        
        if (callback) {
            callback(count, (const char**)results, user_data);
        }
        
        for (int i = 0; i < count; i++) {
            free(results[i]);
        }
        free(results);
    }
    
    free(cmd_ids);
    return 0;
}

/* Get async execution statistics */
void cdp_async_stats(int *pending, int *completed, int *failed) {
    pthread_mutex_lock(&g_cmd_queue.mutex);
    
    int p = 0, c = 0, f = 0;
    
    for (int i = 0; i < g_cmd_queue.count; i++) {
        int idx = (g_cmd_queue.head + i) % MAX_PENDING_COMMANDS;
        AsyncCommand *cmd = &g_cmd_queue.commands[idx];
        
        switch (cmd->state) {
            case CMD_STATE_PENDING:
            case CMD_STATE_SENT:
            case CMD_STATE_WAITING:
                p++;
                break;
            case CMD_STATE_COMPLETED:
                c++;
                break;
            case CMD_STATE_FAILED:
            case CMD_STATE_TIMEOUT:
                f++;
                break;
        }
    }
    
    if (pending) *pending = p;
    if (completed) *completed = c;
    if (failed) *failed = f;
    
    pthread_mutex_unlock(&g_cmd_queue.mutex);
}
