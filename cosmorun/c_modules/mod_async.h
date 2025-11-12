/*
 * mod_async.h - Async I/O Module with Event Loop
 *
 * Provides asynchronous I/O operations with an event loop based on:
 * - epoll on Linux
 * - kqueue on BSD/macOS
 * - select as fallback
 *
 * Features:
 * - Event loop with async callbacks
 * - Async file I/O (read, write, stat)
 * - Async network I/O (TCP connect, accept, read, write)
 * - Timer support (setTimeout, setInterval)
 * - Promise-style chaining
 * - Integration with mod_ds queues
 */

#ifndef MOD_ASYNC_H
#define MOD_ASYNC_H

/* CosmoRun v2: Use System Layer API for libc functions */
#include "../cosmorun_system/libc_shim/sys_libc_shim.h"

/* ==================== Forward Declarations ==================== */

typedef struct async_loop async_loop_t;
typedef struct async_handle async_handle_t;
typedef struct async_timer async_timer_t;

/* ==================== Callback Types ==================== */

/* Generic callback */
typedef void (*async_callback_t)(async_handle_t *handle, void *udata);

/* File I/O callback: data, length, status, user data */
typedef void (*async_read_cb)(char *data, int len, int status, void *udata);
typedef void (*async_write_cb)(int status, void *udata);
typedef void (*async_stat_cb)(struct stat *st, int status, void *udata);

/* Network I/O callback: fd, status, user data */
typedef void (*async_connect_cb)(int fd, int status, void *udata);
typedef void (*async_accept_cb)(int client_fd, void *udata);
typedef void (*async_socket_cb)(char *data, int len, int status, void *udata);

/* Timer callback */
typedef void (*async_timer_cb)(async_timer_t *timer, void *udata);

/* ==================== Event Loop ==================== */

/* Create a new event loop
 * @return: Event loop instance or NULL on failure
 */
async_loop_t* async_loop_new(void);

/* Run the event loop (blocks until stopped)
 * @param loop: Event loop instance
 * @return: 0 on clean exit, -1 on error
 */
int async_loop_run(async_loop_t *loop);

/* Stop the event loop (can be called from callback)
 * @param loop: Event loop instance
 */
void async_loop_stop(async_loop_t *loop);

/* Free the event loop and all resources
 * @param loop: Event loop instance
 */
void async_loop_free(async_loop_t *loop);

/* ==================== Async File I/O ==================== */

/* Async file read
 * @param loop: Event loop
 * @param path: File path to read
 * @param callback: Callback with data, length, status
 * @param udata: User data passed to callback
 * @return: Handle or NULL on error
 */
async_handle_t* async_read_file(async_loop_t *loop, const char *path,
                                  async_read_cb callback, void *udata);

/* Async file write
 * @param loop: Event loop
 * @param path: File path to write
 * @param data: Data buffer to write
 * @param len: Data length
 * @param callback: Callback with status
 * @param udata: User data passed to callback
 * @return: Handle or NULL on error
 */
async_handle_t* async_write_file(async_loop_t *loop, const char *path,
                                   const char *data, int len,
                                   async_write_cb callback, void *udata);

/* Async file stat
 * @param loop: Event loop
 * @param path: File path to stat
 * @param callback: Callback with stat structure and status
 * @param udata: User data passed to callback
 * @return: Handle or NULL on error
 */
async_handle_t* async_stat_file(async_loop_t *loop, const char *path,
                                  async_stat_cb callback, void *udata);

/* ==================== Async Network I/O ==================== */

/* Async TCP connect
 * @param loop: Event loop
 * @param host: Hostname or IP address
 * @param port: Port number
 * @param callback: Callback with connected fd or -1 on error
 * @param udata: User data passed to callback
 * @return: Handle or NULL on error
 */
async_handle_t* async_tcp_connect(async_loop_t *loop, const char *host,
                                    int port, async_connect_cb callback,
                                    void *udata);

/* Async TCP server (listen and accept)
 * @param loop: Event loop
 * @param port: Port to listen on
 * @param callback: Callback for each new client connection
 * @param udata: User data passed to callback
 * @return: Handle or NULL on error
 */
async_handle_t* async_tcp_server(async_loop_t *loop, int port,
                                   async_accept_cb callback, void *udata);

/* Async socket read
 * @param loop: Event loop
 * @param fd: Socket file descriptor
 * @param callback: Callback with data received
 * @param udata: User data passed to callback
 * @return: Handle or NULL on error
 */
async_handle_t* async_socket_read(async_loop_t *loop, int fd,
                                    async_socket_cb callback, void *udata);

/* Async socket write
 * @param loop: Event loop
 * @param fd: Socket file descriptor
 * @param data: Data to write
 * @param len: Data length
 * @param callback: Callback with status
 * @param udata: User data passed to callback
 * @return: Handle or NULL on error
 */
async_handle_t* async_socket_write(async_loop_t *loop, int fd,
                                     const char *data, int len,
                                     async_write_cb callback, void *udata);

/* ==================== Timers ==================== */

/* Create a one-shot timer (setTimeout equivalent)
 * @param loop: Event loop
 * @param ms: Milliseconds to wait
 * @param callback: Callback when timer fires
 * @param udata: User data passed to callback
 * @return: Timer handle or NULL on error
 */
async_timer_t* async_timeout(async_loop_t *loop, int ms,
                               async_timer_cb callback, void *udata);

/* Create a repeating timer (setInterval equivalent)
 * @param loop: Event loop
 * @param ms: Milliseconds between triggers
 * @param callback: Callback when timer fires
 * @param udata: User data passed to callback
 * @return: Timer handle or NULL on error
 */
async_timer_t* async_interval(async_loop_t *loop, int ms,
                                async_timer_cb callback, void *udata);

/* Cancel a timer
 * @param timer: Timer to cancel
 * @return: 0 on success, -1 if already cancelled/fired
 */
int async_timer_cancel(async_timer_t *timer);

/* ==================== Handle Management ==================== */

/* Close an async handle
 * @param handle: Handle to close
 */
void async_handle_close(async_handle_t *handle);

/* Check if handle is active
 * @param handle: Handle to check
 * @return: 1 if active, 0 otherwise
 */
int async_handle_is_active(async_handle_t *handle);

#endif /* MOD_ASYNC_H */
