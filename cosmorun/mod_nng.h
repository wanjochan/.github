#ifndef MOD_NNG_H
#define MOD_NNG_H

/*
 * mod_nng - NNG (nanomsg-next-gen) runtime module for cosmorun
 *
 * Provides lightweight messaging patterns for API server and event systems:
 * - REQ/REP (Request/Response) - ideal for API servers
 * - PUB/SUB (Publish/Subscribe) - ideal for event broadcasting
 * - PUSH/PULL (Pipeline) - ideal for load balancing
 *
 * Uses dynamic loading (dlopen) to avoid static dependencies
 */

#include "cosmo_libc.h"
#include "mod_std.h"

/* NNG opaque types (actual implementation hidden in libnng.so) */
typedef uint32_t nng_socket;
typedef int32_t nng_duration;  /* milliseconds, -1 for infinite */

/* NNG return codes */
#define NNG_OK 0
#define NNG_EINVAL 1
#define NNG_ENOMEM 2
#define NNG_ECLOSED 3
#define NNG_ETIMEDOUT 5
#define NNG_ECONNREFUSED 6
#define NNG_EADDRINUSE 7

/* NNG socket options */
#define NNG_OPT_RECVTIMEO "recv-timeout"
#define NNG_OPT_SENDTIMEO "send-timeout"
#define NNG_OPT_RECVBUF "recv-buffer"
#define NNG_OPT_SENDBUF "send-buffer"

/* NNG duration constants */
#define NNG_DURATION_INFINITE -1
#define NNG_DURATION_DEFAULT -2
#define NNG_DURATION_ZERO 0

/* NNG context structure - manages library state and sockets */
typedef struct nng_context nng_context_t;

/* ==================== Initialization & Cleanup ==================== */

/**
 * Initialize NNG context and load library
 * @param lib_path Optional library path (NULL for auto-detection)
 * @return Pointer to allocated context, or NULL on failure
 */
nng_context_t* nng_init(const char *lib_path);

/**
 * Cleanup NNG context and free resources
 * @param ctx NNG context to cleanup
 */
void nng_cleanup(nng_context_t *ctx);

/* ==================== REQ/REP Pattern (API Server) ==================== */

/**
 * Create REP (reply) socket and listen on URL
 * Ideal for API server endpoints
 * @param ctx NNG context
 * @param url Bind URL (e.g., "tcp://0.0.0.0:8080", "ipc:///tmp/api.sock")
 * @return 0 on success, NNG error code on failure
 */
int nng_listen_rep(nng_context_t *ctx, const char *url);

/**
 * Create REQ (request) socket and connect to URL
 * Ideal for API clients
 * @param ctx NNG context
 * @param url Connect URL (e.g., "tcp://127.0.0.1:8080")
 * @return 0 on success, NNG error code on failure
 */
int nng_dial_req(nng_context_t *ctx, const char *url);

/**
 * Receive message (blocking or with timeout)
 * Used by REP server to receive requests
 * @param ctx NNG context
 * @return String containing message data, or NULL on error (caller must free)
 */
std_string_t* nng_recv_msg(nng_context_t *ctx);

/**
 * Send message (blocking or with timeout)
 * Used by REP server to send responses, or REQ client to send requests
 * @param ctx NNG context
 * @param data Message data (null-terminated string)
 * @return 0 on success, NNG error code on failure
 */
int nng_send_msg(nng_context_t *ctx, const char *data);

/* ==================== PUB/SUB Pattern (Event System) ==================== */

/**
 * Create PUB (publisher) socket and bind to URL
 * Ideal for event broadcasting
 * @param ctx NNG context
 * @param url Bind URL (e.g., "tcp://0.0.0.0:9090")
 * @return 0 on success, NNG error code on failure
 */
int nng_bind_pub(nng_context_t *ctx, const char *url);

/**
 * Create SUB (subscriber) socket and connect to URL
 * Ideal for event consumers
 * @param ctx NNG context
 * @param url Connect URL (e.g., "tcp://127.0.0.1:9090")
 * @return 0 on success, NNG error code on failure
 */
int nng_dial_sub(nng_context_t *ctx, const char *url);

/**
 * Subscribe to topic (SUB socket only)
 * Empty string subscribes to all messages
 * @param ctx NNG context
 * @param topic Topic prefix to subscribe (NULL or "" for all)
 * @return 0 on success, NNG error code on failure
 */
int nng_sub_subscribe(nng_context_t *ctx, const char *topic);

/* ==================== Socket Options ==================== */

/**
 * Set receive timeout
 * @param ctx NNG context
 * @param timeout_ms Timeout in milliseconds (NNG_DURATION_INFINITE for no timeout)
 * @return 0 on success, NNG error code on failure
 */
int nng_set_recv_timeout(nng_context_t *ctx, int timeout_ms);

/**
 * Set send timeout
 * @param ctx NNG context
 * @param timeout_ms Timeout in milliseconds (NNG_DURATION_INFINITE for no timeout)
 * @return 0 on success, NNG error code on failure
 */
int nng_set_send_timeout(nng_context_t *ctx, int timeout_ms);

/* ==================== Socket Management ==================== */

/**
 * Close current socket
 * @param ctx NNG context
 */
void nng_close_socket(nng_context_t *ctx);

/**
 * Get last error message
 * @param ctx NNG context
 * @return Error string (do not free)
 */
const char* nng_get_error(nng_context_t *ctx);

/* ==================== Testing & Examples ==================== */

/**
 * Run self-test demonstrating REQ/REP pattern
 * Creates server and client, exchanges messages
 * @param lib_path Optional library path (NULL for auto-detection)
 * @return 0 on success, -1 on failure
 */
int nng_selftest_reqrep(const char *lib_path);

/**
 * Run self-test demonstrating PUB/SUB pattern
 * Creates publisher and subscriber, sends events
 * @param lib_path Optional library path (NULL for auto-detection)
 * @return 0 on success, -1 on failure
 */
int nng_selftest_pubsub(const char *lib_path);

#endif /* MOD_NNG_H */
