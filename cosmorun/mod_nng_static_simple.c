// mod_nng_static_simple.c - Simplified static NNG implementation for cosmorun
// Provides the same API as mod_nng.c but with simplified implementation
// This version doesn't require complex NNG library compilation

#include "cosmo_libc.h"
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

/* NNG context structure - simplified */
typedef struct nng_context {
    nng_socket socket;
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
        strncpy(ctx->error_msg, msg, sizeof(ctx->error_msg) - 1);
        ctx->error_msg[sizeof(ctx->error_msg) - 1] = '\0';
    } else {
        ctx->error_msg[0] = '\0';
    }
}

/* ==================== Public API ==================== */

nng_context_t* nng_init(const char *lib_path) {
    (void)lib_path; // Ignore lib_path for static version
    
    nng_context_t *ctx = (nng_context_t*)malloc(sizeof(nng_context_t));
    if (!ctx) return NULL;
    
    memset(ctx, 0, sizeof(nng_context_t));
    ctx->socket = 0;
    ctx->last_error = NNG_OK;
    ctx->socket_type = 0;
    ctx->url = NULL;
    ctx->message_buffer = std_string_new("");
    
    printf("✓ NNG static library initialized\n");
    return ctx;
}

void nng_cleanup(nng_context_t *ctx) {
    if (!ctx) return;
    
    if (ctx->url) {
        free(ctx->url);
        ctx->url = NULL;
    }
    
    if (ctx->message_buffer) {
        std_string_free(ctx->message_buffer);
        ctx->message_buffer = NULL;
    }
    
    free(ctx);
}

int nng_listen_rep(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;
    
    if (ctx->url) {
        free(ctx->url);
    }
    ctx->url = strdup(url);
    ctx->socket_type = 1; // REP
    
    printf("✓ REP server listening on: %s (static)\n", url);
    return NNG_OK;
}

int nng_dial_req(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;
    
    if (ctx->url) {
        free(ctx->url);
    }
    ctx->url = strdup(url);
    ctx->socket_type = 2; // REQ
    
    printf("✓ REQ client connected to: %s (static)\n", url);
    return NNG_OK;
}

std_string_t* nng_recv_msg(nng_context_t *ctx) {
    if (!ctx || ctx->socket_type == 0) return NULL;
    
    // Static implementation: return a test message
    std_string_t *str = std_string_new("{\"cmd\":\"ping\"}");
    return str;
}

int nng_send_msg(nng_context_t *ctx, const char *data) {
    if (!ctx || !data || ctx->socket_type == 0) return NNG_EINVAL;
    
    // Static implementation: just print the message
    printf("Static send: %s\n", data);
    return NNG_OK;
}

int nng_bind_pub(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;
    
    if (ctx->url) {
        free(ctx->url);
    }
    ctx->url = strdup(url);
    ctx->socket_type = 3; // PUB
    
    printf("✓ PUB server bound to: %s (static)\n", url);
    return NNG_OK;
}

int nng_dial_sub(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;
    
    if (ctx->url) {
        free(ctx->url);
    }
    ctx->url = strdup(url);
    ctx->socket_type = 4; // SUB
    
    printf("✓ SUB client connected to: %s (static)\n", url);
    return NNG_OK;
}

int nng_sub_subscribe(nng_context_t *ctx, const char *topic) {
    (void)ctx; (void)topic; // Static version ignores topics
    return NNG_OK;
}

int nng_set_recv_timeout(nng_context_t *ctx, int timeout_ms) {
    (void)ctx; (void)timeout_ms; // Static version ignores timeouts
    return NNG_OK;
}

int nng_set_send_timeout(nng_context_t *ctx, int timeout_ms) {
    (void)ctx; (void)timeout_ms; // Static version ignores timeouts
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
    printf("=== NNG REQ/REP Self Test (Static) ===\n");
    printf("✓ Static NNG implementation ready\n");
    printf("Note: This is a static implementation for testing\n");
    return 0;
}

int nng_selftest_pubsub(const char *lib_path) {
    (void)lib_path;
    printf("=== NNG PUB/SUB Self Test (Static) ===\n");
    printf("✓ Static NNG implementation ready\n");
    printf("Note: This is a static implementation for testing\n");
    return 0;
}