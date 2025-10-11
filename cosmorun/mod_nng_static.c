// mod_nng_static.c - Static NNG implementation for cosmorun
// Links directly with libnng.a static library
//
// This version statically links NNG functions, providing the same API
// as the original mod_nng.c but without dynamic loading

#include "cosmo_libc.h"
#include "mod_std.h"

// NNG opaque types - simplified for cosmorun
typedef uint32_t nng_socket;
typedef int32_t nng_duration;

/* NNG return codes - use NNG's actual codes */
#define NNG_OK 0
#define NNG_EINVAL 1
#define NNG_ENOMEM 2
#define NNG_ECLOSED 3
#define NNG_ETIMEDOUT 5
#define NNG_ECONNREFUSED 6
#define NNG_EADDRINUSE 7

/* NNG context structure - manages library state and sockets */
typedef struct nng_context {
    nng_socket socket;
    int last_error;
    char error_msg[256];
    int socket_type; // 0=none, 1=REP, 2=REQ, 3=PUB, 4=SUB
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
    ctx->socket = NNG_SOCKET_INITIALIZER;
    ctx->last_error = NNG_OK;
    ctx->socket_type = 0;
    
    printf("✓ NNG static library initialized\n");
    return ctx;
}

void nng_cleanup(nng_context_t *ctx) {
    if (!ctx) return;
    
    if (ctx->socket_type != 0) {
        nng_close(ctx->socket);
        ctx->socket_type = 0;
    }
    
    free(ctx);
}

int nng_listen_rep(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;
    
    int rv = nng_rep0_open(&ctx->socket);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to open REP socket");
        return rv;
    }
    
    rv = nng_listen(ctx->socket, url, NULL, 0);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to listen");
        nng_close(ctx->socket);
        ctx->socket = NNG_SOCKET_INITIALIZER;
        return rv;
    }
    
    ctx->socket_type = 1; // REP
    printf("✓ REP server listening on: %s\n", url);
    return NNG_OK;
}

int nng_dial_req(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;
    
    int rv = nng_req0_open(&ctx->socket);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to open REQ socket");
        return rv;
    }
    
    rv = nng_dial(ctx->socket, url, NULL, 0);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to dial");
        nng_close(ctx->socket);
        ctx->socket = NNG_SOCKET_INITIALIZER;
        return rv;
    }
    
    ctx->socket_type = 2; // REQ
    printf("✓ REQ client connected to: %s\n", url);
    return NNG_OK;
}

std_string_t* nng_recv_msg(nng_context_t *ctx) {
    if (!ctx || ctx->socket_type == 0) return NULL;
    
    nng_msg *msg;
    int rv = nng_recvmsg(ctx->socket, &msg, 0);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to receive message");
        return NULL;
    }
    
    size_t size = nng_msg_len(msg);
    void *data = nng_msg_body(msg);
    
    std_string_t *str = std_string_with_capacity(size + 1);
    if (!str) {
        nng_msg_free(msg);
        return NULL;
    }
    
    for (size_t i = 0; i < size; i++) {
        std_string_append_char(str, ((char*)data)[i]);
    }
    
    nng_msg_free(msg);
    return str;
}

int nng_send_msg(nng_context_t *ctx, const char *data) {
    if (!ctx || !data || ctx->socket_type == 0) return NNG_EINVAL;
    
    size_t len = strlen(data);
    int rv = nng_send(ctx->socket, (void*)data, len, 0);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to send message");
        return rv;
    }
    
    return NNG_OK;
}

int nng_bind_pub(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;
    
    int rv = nng_pub0_open(&ctx->socket);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to open PUB socket");
        return rv;
    }
    
    rv = nng_listen(ctx->socket, url, NULL, 0);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to bind");
        nng_close(ctx->socket);
        ctx->socket = NNG_SOCKET_INITIALIZER;
        return rv;
    }
    
    ctx->socket_type = 3; // PUB
    printf("✓ PUB server bound to: %s\n", url);
    return NNG_OK;
}

int nng_dial_sub(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;
    
    int rv = nng_sub0_open(&ctx->socket);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to open SUB socket");
        return rv;
    }
    
    rv = nng_dial(ctx->socket, url, NULL, 0);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to dial");
        nng_close(ctx->socket);
        ctx->socket = NNG_SOCKET_INITIALIZER;
        return rv;
    }
    
    ctx->socket_type = 4; // SUB
    printf("✓ SUB client connected to: %s\n", url);
    return NNG_OK;
}

int nng_sub_subscribe(nng_context_t *ctx, const char *topic) {
    if (!ctx || ctx->socket_type != 4) return NNG_EINVAL;
    
    const char *t = topic ? topic : "";
    size_t len = strlen(t);
    
    int rv = nng_socket_set(ctx->socket, NNG_OPT_SUB_SUBSCRIBE, t, len);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to subscribe");
        return rv;
    }
    
    printf("✓ Subscribed to topic: %s\n", len > 0 ? t : "<all>");
    return NNG_OK;
}

int nng_set_recv_timeout(nng_context_t *ctx, int timeout_ms) {
    if (!ctx || ctx->socket_type == 0) return NNG_EINVAL;
    
    nng_duration timeout = timeout_ms;
    int rv = nng_socket_set_ms(ctx->socket, NNG_OPT_RECVTIMEO, timeout);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to set receive timeout");
        return rv;
    }
    
    return NNG_OK;
}

int nng_set_send_timeout(nng_context_t *ctx, int timeout_ms) {
    if (!ctx || ctx->socket_type == 0) return NNG_EINVAL;
    
    nng_duration timeout = timeout_ms;
    int rv = nng_socket_set_ms(ctx->socket, NNG_OPT_SENDTIMEO, timeout);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to set send timeout");
        return rv;
    }
    
    return NNG_OK;
}

void nng_close_socket(nng_context_t *ctx) {
    if (!ctx || ctx->socket_type == 0) return;
    
    nng_close(ctx->socket);
    ctx->socket = NNG_SOCKET_INITIALIZER;
    ctx->socket_type = 0;
}

const char* nng_get_error(nng_context_t *ctx) {
    if (!ctx) return "Invalid context";
    
    if (ctx->error_msg[0] != '\0') {
        return ctx->error_msg;
    }
    
    return nng_strerror(ctx->last_error);
}

int nng_selftest_reqrep(const char *lib_path) {
    (void)lib_path;
    printf("=== NNG REQ/REP Self Test (Static) ===\n");
    
    nng_context_t *ctx = nng_init(NULL);
    if (!ctx) {
        printf("✗ Failed to initialize context\n");
        return -1;
    }
    
    int rv = nng_listen_rep(ctx, "ipc:///tmp/nng_test_reqrep");
    if (rv != NNG_OK) {
        printf("✗ Failed to create REP server: %s\n", nng_get_error(ctx));
        nng_cleanup(ctx);
        return -1;
    }
    
    printf("✓ REP server created successfully\n");
    nng_cleanup(ctx);
    return 0;
}

int nng_selftest_pubsub(const char *lib_path) {
    (void)lib_path;
    printf("=== NNG PUB/SUB Self Test (Static) ===\n");
    
    nng_context_t *ctx = nng_init(NULL);
    if (!ctx) {
        printf("✗ Failed to initialize context\n");
        return -1;
    }
    
    int rv = nng_bind_pub(ctx, "ipc:///tmp/nng_test_pubsub");
    if (rv != NNG_OK) {
        printf("✗ Failed to create PUB server: %s\n", nng_get_error(ctx));
        nng_cleanup(ctx);
        return -1;
    }
    
    printf("✓ PUB server created successfully\n");
    nng_cleanup(ctx);
    return 0;
}