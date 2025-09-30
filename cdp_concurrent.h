/**
 * CDP Concurrent Command Execution Module
 * Async/await pattern for parallel CDP commands
 */

#ifndef CDP_CONCURRENT_H
#define CDP_CONCURRENT_H

#include <stddef.h>

/* Async handle type */
typedef int cdp_async_handle_t;
#define CDP_ASYNC_INVALID -1

/* Batch command structure */
typedef struct {
    const char* method;
    const char* params_json;
} CDPBatchCommand;

/* Initialize/cleanup concurrent module */
int cdp_concurrent_init(void);
void cdp_concurrent_cleanup(void);

/* Async command execution */
cdp_async_handle_t cdp_send_async(const char* method, const char* params_json);
int cdp_await(cdp_async_handle_t handle, char* out_response, size_t out_size, int timeout_ms);
int cdp_is_complete(cdp_async_handle_t handle);

/* Batch operations */
int cdp_await_all(cdp_async_handle_t* handles, int count, int timeout_ms);
int cdp_batch_send(const CDPBatchCommand* commands, int count, cdp_async_handle_t* out_handles);

/* Helper: Send and wait (backward compatible) */
int cdp_call_async(const char* method, const char* params_json,
                   char* out_response, size_t out_size, int timeout_ms);

/* Callback-based API (optional) */
typedef void (*cdp_async_callback_t)(int command_id, const char* response, void* user_data);
int cdp_send_async_callback(const char* method, const char* params_json,
                           cdp_async_callback_t callback, void* user_data);

#endif /* CDP_CONCURRENT_H */