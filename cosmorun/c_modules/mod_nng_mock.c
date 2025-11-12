// mod_nng_mock.c - Mock NNG implementation for cosmorun testing
// Provides the same API as mod_nng.c but uses simple in-memory operations
// This is for testing purposes only

#include <stdint.h>
#include "../cosmorun_system/libc_shim/sys_libc_shim.h"
#include "mod_std.h"

/* NNG opaque types */
typedef uint32_t nng_socket;
typedef int32_t nng_duration;

/* NNG return codes */
#define NNG_OK 0
#define NNG_EINVAL 1
#define NNG_ENOMEM 2
#define NNG_ECLOSED 3
#define NNG_ETIMEDOUT 5
#define NNG_ECONNREFUSED 6
#define NNG_EADDRINUSE 7

/* NNG context structure - mock implementation */
typedef struct nng_context {
    int last_error;
    char error_msg[256];
    int socket_type; // 0=none, 1=REP, 2=REQ, 3=PUB, 4=SUB
    char *url;
    std_string_t *message_buffer;
} nng_context_t;

/* Helper functions */
static void nng_set_error(nng_context_t *ctx, int code, const char *msg) {
    if (!ctx) return;
    ctx->last_error = code;
    if (msg) {
        // 使用 System Layer shim_strncpy 替代标准库 strncpy
        shim_strncpy(ctx->error_msg, msg, sizeof(ctx->error_msg) - 1);
        ctx->error_msg[sizeof(ctx->error_msg) - 1] = '\0';
    } else {
        ctx->error_msg[0] = '\0';
    }
}

/* ==================== Public API ==================== */

nng_context_t* nng_init(const char *lib_path) {
    (void)lib_path; // Ignore lib_path for mock version

    // 使用 System Layer shim_malloc 替代标准库 malloc
    nng_context_t *ctx = (nng_context_t*)shim_malloc(sizeof(nng_context_t));
    if (!ctx) return NULL;

    // 使用 System Layer shim_memset 替代标准库 memset
    shim_memset(ctx, 0, sizeof(nng_context_t));
    ctx->last_error = NNG_OK;
    ctx->socket_type = 0;
    ctx->url = NULL;
    ctx->message_buffer = std_string_new("");

    // 使用 System Layer shim_printf 替代标准库 printf
    shim_printf("✓ NNG mock library initialized\n");
    return ctx;
}

void nng_cleanup(nng_context_t *ctx) {
    if (!ctx) return;

    if (ctx->url) {
        // 使用 System Layer shim_free 替代标准库 free
        shim_free(ctx->url);
        ctx->url = NULL;
    }

    if (ctx->message_buffer) {
        std_string_free(ctx->message_buffer);
        ctx->message_buffer = NULL;
    }

    // 使用 System Layer shim_free 替代标准库 free
    shim_free(ctx);
}

int nng_listen_rep(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;

    if (ctx->url) {
        // 使用 System Layer shim_free 替代标准库 free
        shim_free(ctx->url);
    }
    // 使用 System Layer shim_strdup 替代标准库 strdup
    ctx->url = shim_strdup(url);
    ctx->socket_type = 1; // REP

    // 使用 System Layer shim_printf 替代标准库 printf
    shim_printf("✓ REP server listening on: %s (mock)\n", url);
    return NNG_OK;
}

int nng_dial_req(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;

    if (ctx->url) {
        // 使用 System Layer shim_free 替代标准库 free
        shim_free(ctx->url);
    }
    // 使用 System Layer shim_strdup 替代标准库 strdup
    ctx->url = shim_strdup(url);
    ctx->socket_type = 2; // REQ

    // 使用 System Layer shim_printf 替代标准库 printf
    shim_printf("✓ REQ client connected to: %s (mock)\n", url);
    return NNG_OK;
}

std_string_t* nng_recv_msg(nng_context_t *ctx) {
    if (!ctx || ctx->socket_type == 0) return NULL;
    
    // Mock implementation: return a test message
    std_string_t *str = std_string_new("{\"cmd\":\"ping\"}");
    return str;
}

int nng_send_msg(nng_context_t *ctx, const char *data) {
    if (!ctx || !data || ctx->socket_type == 0) return NNG_EINVAL;

    // Mock implementation: just print the message
    // 使用 System Layer shim_printf 替代标准库 printf
    shim_printf("Mock send: %s\n", data);
    return NNG_OK;
}

int nng_bind_pub(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;

    if (ctx->url) {
        // 使用 System Layer shim_free 替代标准库 free
        shim_free(ctx->url);
    }
    // 使用 System Layer shim_strdup 替代标准库 strdup
    ctx->url = shim_strdup(url);
    ctx->socket_type = 3; // PUB

    // 使用 System Layer shim_printf 替代标准库 printf
    shim_printf("✓ PUB server bound to: %s (mock)\n", url);
    return NNG_OK;
}

int nng_dial_sub(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;

    if (ctx->url) {
        // 使用 System Layer shim_free 替代标准库 free
        shim_free(ctx->url);
    }
    // 使用 System Layer shim_strdup 替代标准库 strdup
    ctx->url = shim_strdup(url);
    ctx->socket_type = 4; // SUB

    // 使用 System Layer shim_printf 替代标准库 printf
    shim_printf("✓ SUB client connected to: %s (mock)\n", url);
    return NNG_OK;
}

int nng_sub_subscribe(nng_context_t *ctx, const char *topic) {
    (void)ctx; (void)topic; // Mock version ignores topics
    return NNG_OK;
}

int nng_set_recv_timeout(nng_context_t *ctx, int timeout_ms) {
    (void)ctx; (void)timeout_ms; // Mock version ignores timeouts
    return NNG_OK;
}

int nng_set_send_timeout(nng_context_t *ctx, int timeout_ms) {
    (void)ctx; (void)timeout_ms; // Mock version ignores timeouts
    return NNG_OK;
}

void nng_close_socket(nng_context_t *ctx) {
    if (!ctx) return;
    ctx->socket_type = 0;
}

const char* nng_get_error(nng_context_t *ctx) {
    if (!ctx) return "Invalid context";
    
    if (ctx->error_msg[0] != '\0') {
        return ctx->error_msg;
    }
    
    return "Unknown error";
}

int nng_selftest_reqrep(const char *lib_path) {
    (void)lib_path;
    // 使用 System Layer shim_printf 替代标准库 printf
    shim_printf("=== NNG REQ/REP Self Test (Mock) ===\n");
    shim_printf("✓ Mock NNG implementation ready\n");
    shim_printf("Note: This is a mock implementation for testing\n");
    return 0;
}

int nng_selftest_pubsub(const char *lib_path) {
    (void)lib_path;
    // 使用 System Layer shim_printf 替代标准库 printf
    shim_printf("=== NNG PUB/SUB Self Test (Mock) ===\n");
    shim_printf("✓ Mock NNG implementation ready\n");
    shim_printf("Note: This is a mock implementation for testing\n");
    return 0;
}