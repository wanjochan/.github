/**
 * CDP Concurrent Command Execution Module
 * Supports async/await pattern for parallel CDP commands
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include "cdp_concurrent.h"

/* External dependencies */
extern int ws_cmd_id;
extern int send_command_with_retry(const char* cmd);
extern int ws_recv_text(int sock, char *buffer, size_t max_len);
extern int ws_sock;
extern int verbose;

/* Build CDP command JSON */
static void build_command(char* out, size_t out_size, int id, const char* method, const char* params_json) {
    if (params_json && strlen(params_json) > 2) {
        snprintf(out, out_size, "{\"id\":%d,\"method\":\"%s\",\"params\":%s}",
                 id, method, params_json);
    } else {
        snprintf(out, out_size, "{\"id\":%d,\"method\":\"%s\"}", id, method);
    }
}

/* Internal promise structure */
typedef struct CDPPromise {
    int id;
    int completed;
    int success;
    char* result;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    struct timeval start_time;
    int timeout_ms;
} CDPPromise;

/* Promise pool */
#define MAX_PROMISES 128
static CDPPromise promises[MAX_PROMISES];
static int promise_count = 0;
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t receiver_thread;
static int receiver_running = 0;

/* Get or create promise for command ID */
static CDPPromise* get_promise(int id) {
    pthread_mutex_lock(&pool_mutex);

    // Find existing promise
    for (int i = 0; i < promise_count; i++) {
        if (promises[i].id == id) {
            pthread_mutex_unlock(&pool_mutex);
            return &promises[i];
        }
    }

    // Create new promise
    if (promise_count < MAX_PROMISES) {
        CDPPromise* p = &promises[promise_count++];
        p->id = id;
        p->completed = 0;
        p->success = 0;
        p->result = NULL;
        pthread_mutex_init(&p->mutex, NULL);
        pthread_cond_init(&p->cond, NULL);
        gettimeofday(&p->start_time, NULL);
        p->timeout_ms = 30000; // Default 30 seconds
        pthread_mutex_unlock(&pool_mutex);
        return p;
    }

    pthread_mutex_unlock(&pool_mutex);
    return NULL;
}

/* Background receiver thread */
static void* receiver_thread_func(void* arg) {
    char buffer[65536];

    while (receiver_running) {
        // Non-blocking receive with timeout
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);
        FD_SET(ws_sock, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms

        if (select(ws_sock + 1, &readfds, NULL, NULL, &tv) > 0) {
            int len = ws_recv_text(ws_sock, buffer, sizeof(buffer) - 1);
            if (len > 0) {
                buffer[len] = '\0';

                // Extract command ID from response
                const char* id_str = strstr(buffer, "\"id\":");
                if (id_str) {
                    int id = atoi(id_str + 5);
                    CDPPromise* promise = get_promise(id);

                    if (promise) {
                        pthread_mutex_lock(&promise->mutex);
                        promise->result = strdup(buffer);
                        promise->completed = 1;
                        promise->success = (strstr(buffer, "\"error\":") == NULL);
                        pthread_cond_signal(&promise->cond);
                        pthread_mutex_unlock(&promise->mutex);

                        if (verbose) {
                            printf("[Async] Received response for command %d\n", id);
                        }
                    }
                }

                // Also handle events (no "id" field)
                if (!id_str && strstr(buffer, "\"method\":")) {
                    // TODO: Call event handlers
                    if (verbose) {
                        printf("[Async] Event: %.100s...\n", buffer);
                    }
                }
            }
        }

        // Check for timeouts
        struct timeval now;
        gettimeofday(&now, NULL);

        pthread_mutex_lock(&pool_mutex);
        for (int i = 0; i < promise_count; i++) {
            CDPPromise* p = &promises[i];
            if (!p->completed) {
                long elapsed_ms = (now.tv_sec - p->start_time.tv_sec) * 1000 +
                                 (now.tv_usec - p->start_time.tv_usec) / 1000;
                if (elapsed_ms > p->timeout_ms) {
                    pthread_mutex_lock(&p->mutex);
                    p->completed = 1;
                    p->success = 0;
                    p->result = strdup("{\"error\":{\"message\":\"Command timeout\"}}");
                    pthread_cond_signal(&p->cond);
                    pthread_mutex_unlock(&p->mutex);
                }
            }
        }
        pthread_mutex_unlock(&pool_mutex);
    }

    return NULL;
}

/* Initialize concurrent module */
int cdp_concurrent_init(void) {
    if (receiver_running) return 0; // Already initialized

    receiver_running = 1;
    if (pthread_create(&receiver_thread, NULL, receiver_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create receiver thread\n");
        receiver_running = 0;
        return -1;
    }

    if (verbose) {
        printf("CDP concurrent module initialized\n");
    }

    return 0;
}

/* Cleanup concurrent module */
void cdp_concurrent_cleanup(void) {
    if (!receiver_running) return;

    receiver_running = 0;
    pthread_join(receiver_thread, NULL);

    // Free all pending promises
    pthread_mutex_lock(&pool_mutex);
    for (int i = 0; i < promise_count; i++) {
        if (promises[i].result) {
            free(promises[i].result);
        }
        pthread_mutex_destroy(&promises[i].mutex);
        pthread_cond_destroy(&promises[i].cond);
    }
    promise_count = 0;
    pthread_mutex_unlock(&pool_mutex);

    if (verbose) {
        printf("CDP concurrent module cleaned up\n");
    }
}

/* Send command asynchronously */
cdp_async_handle_t cdp_send_async(const char* method, const char* params_json) {
    int id = ws_cmd_id++;
    char cmd[8192];

    build_command(cmd, sizeof(cmd), id, method, params_json);

    // Pre-create promise before sending
    CDPPromise* promise = get_promise(id);
    if (!promise) {
        return CDP_ASYNC_INVALID;
    }

    if (send_command_with_retry(cmd) < 0) {
        return CDP_ASYNC_INVALID;
    }

    if (verbose) {
        printf("[Async] Sent command %d: %s\n", id, method);
    }

    return (cdp_async_handle_t)id;
}

/* Wait for async command with timeout */
int cdp_await(cdp_async_handle_t handle, char* out_response, size_t out_size, int timeout_ms) {
    if (handle == CDP_ASYNC_INVALID) return -1;

    int id = (int)handle;
    CDPPromise* promise = get_promise(id);
    if (!promise) return -1;

    // Update timeout if specified
    if (timeout_ms > 0) {
        promise->timeout_ms = timeout_ms;
    }

    // Wait for completion
    pthread_mutex_lock(&promise->mutex);

    while (!promise->completed) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += (promise->timeout_ms / 1000);
        ts.tv_nsec += (promise->timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }

        if (pthread_cond_timedwait(&promise->cond, &promise->mutex, &ts) != 0) {
            // Timeout
            promise->completed = 1;
            promise->success = 0;
            promise->result = strdup("{\"error\":{\"message\":\"Await timeout\"}}");
        }
    }

    // Copy result
    int success = promise->success;
    if (out_response && out_size > 0) {
        if (promise->result) {
            strncpy(out_response, promise->result, out_size - 1);
            out_response[out_size - 1] = '\0';
        } else {
            out_response[0] = '\0';
        }
    }

    pthread_mutex_unlock(&promise->mutex);

    if (verbose) {
        printf("[Async] Awaited command %d: %s\n", id, success ? "success" : "failed");
    }

    return success ? 0 : -1;
}

/* Check if async command is complete (non-blocking) */
int cdp_is_complete(cdp_async_handle_t handle) {
    if (handle == CDP_ASYNC_INVALID) return 1;

    int id = (int)handle;
    CDPPromise* promise = get_promise(id);
    if (!promise) return 1;

    pthread_mutex_lock(&promise->mutex);
    int complete = promise->completed;
    pthread_mutex_unlock(&promise->mutex);

    return complete;
}

/* Wait for multiple async commands */
int cdp_await_all(cdp_async_handle_t* handles, int count, int timeout_ms) {
    if (!handles || count <= 0) return -1;

    struct timeval start;
    gettimeofday(&start, NULL);

    int all_success = 1;

    for (int i = 0; i < count; i++) {
        // Calculate remaining timeout
        struct timeval now;
        gettimeofday(&now, NULL);
        long elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 +
                         (now.tv_usec - start.tv_usec) / 1000;
        int remaining_ms = timeout_ms - elapsed_ms;

        if (remaining_ms <= 0) {
            all_success = 0;
            break;
        }

        if (cdp_await(handles[i], NULL, 0, remaining_ms) != 0) {
            all_success = 0;
        }
    }

    return all_success ? 0 : -1;
}

/* Send batch of commands in parallel */
int cdp_batch_send(const CDPBatchCommand* commands, int count,
                   cdp_async_handle_t* out_handles) {
    if (!commands || count <= 0 || !out_handles) return -1;

    // Initialize if needed
    if (!receiver_running) {
        if (cdp_concurrent_init() != 0) return -1;
    }

    // Send all commands
    for (int i = 0; i < count; i++) {
        out_handles[i] = cdp_send_async(commands[i].method, commands[i].params_json);
        if (out_handles[i] == CDP_ASYNC_INVALID) {
            // Cancel previous commands on failure
            for (int j = 0; j < i; j++) {
                // TODO: Implement cancel
            }
            return -1;
        }
    }

    if (verbose) {
        printf("[Async] Sent batch of %d commands\n", count);
    }

    return 0;
}

/* Helper: Send command and wait (backward compatible) */
int cdp_call_async(const char* method, const char* params_json,
                   char* out_response, size_t out_size, int timeout_ms) {
    cdp_async_handle_t handle = cdp_send_async(method, params_json);
    return cdp_await(handle, out_response, out_size, timeout_ms);
}