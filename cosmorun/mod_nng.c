// mod_nng.c - NNG (nanomsg-next-gen) runtime module for cosmorun
// Provides reusable NNG messaging patterns via __import()
//
// NNG is a lightweight messaging library supporting:
// - REQ/REP: Request/Response for API servers
// - PUB/SUB: Publish/Subscribe for event systems
// - PUSH/PULL: Pipeline for load balancing
//
// Uses dynamic loading (dlopen) to avoid static dependencies

#define NULL (void*)0

#ifndef RTLD_LAZY
#define RTLD_LAZY 1
#endif
#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 256
#endif

#include "mod_nng.h"

/* ==================== NNG Function Pointer Types ==================== */

// Socket management
typedef int (*nng_rep0_open_t)(nng_socket *);
typedef int (*nng_req0_open_t)(nng_socket *);
typedef int (*nng_pub0_open_t)(nng_socket *);
typedef int (*nng_sub0_open_t)(nng_socket *);
typedef int (*nng_close_t)(nng_socket);

// Network operations
typedef int (*nng_listen_t)(nng_socket, const char *, void *, int);
typedef int (*nng_dial_t)(nng_socket, const char *, void *, int);

// Message operations
typedef int (*nng_send_t)(nng_socket, void *, size_t, int);
typedef int (*nng_recv_t)(nng_socket, void *, size_t *, int);

// Socket options
typedef int (*nng_socket_set_ms_t)(nng_socket, const char *, nng_duration);
typedef int (*nng_socket_set_t)(nng_socket, const char *, const void *, size_t);

// Error handling
typedef const char* (*nng_strerror_t)(int);

// SUB-specific operations
typedef int (*nng_sub0_subscribe_t)(nng_socket, const void *, size_t);

/* ==================== NNG Context Structure ==================== */

struct nng_context {
    void *lib_handle;
    nng_socket socket;
    int last_error;
    char error_msg[256];

    // Function pointers
    nng_rep0_open_t rep0_open;
    nng_req0_open_t req0_open;
    nng_pub0_open_t pub0_open;
    nng_sub0_open_t sub0_open;
    nng_close_t close;
    nng_listen_t listen;
    nng_dial_t dial;
    nng_send_t send;
    nng_recv_t recv;
    nng_socket_set_ms_t socket_set_ms;
    nng_socket_set_t socket_set;
    nng_strerror_t strerror;
    nng_sub0_subscribe_t sub0_subscribe;
};

/* ==================== Helper Functions ==================== */

static int nng_preferred_dlopen_flags(void) {
#if defined(_WIN32) || defined(_WIN64)
    return RTLD_LAZY;
#else
    return RTLD_LAZY | RTLD_GLOBAL;
#endif
}

static void *nng_try_dlopen(const char *path) {
    if (!path || !*path) {
        return NULL;
    }

    int flags = nng_preferred_dlopen_flags();
    void *handle = dlopen(path, flags);
    if (handle) {
        return handle;
    }

    return dlopen(path, 0);
}

static void *nng_dlopen_auto(const char *requested_path) {
    static const char *const candidates[] = {
#if defined(_WIN32) || defined(_WIN64)
        "lib/libnng.dll",
        "lib/nng.dll",
        "./nng.dll",
        "nng.dll",
        "./libnng.dll",
        "libnng.dll",
#elif defined(__APPLE__)
        "lib/libnng.dylib",
        "./libnng.dylib",
        "libnng.dylib",
#else
        "lib/libnng.so",
        "./libnng.so",
        "libnng.so",
        "/usr/lib/libnng.so",
        "/usr/local/lib/libnng.so",
#endif
        NULL
    };

    if (requested_path && *requested_path) {
        void *handle = nng_try_dlopen(requested_path);
        if (handle) {
            return handle;
        }
    }

    for (const char *const *cand = candidates; *cand; ++cand) {
        if (requested_path && *requested_path && strcmp(requested_path, *cand) == 0) {
            continue;
        }
        void *handle = nng_try_dlopen(*cand);
        if (handle) {
            return handle;
        }
    }
    return NULL;
}

static void nng_set_error(nng_context_t *ctx, int code, const char *msg) {
    if (!ctx) return;

    ctx->last_error = code;
    if (msg) {
        size_t i = 0;
        while (msg[i] && i < 255) {
            ctx->error_msg[i] = msg[i];
            i++;
        }
        ctx->error_msg[i] = '\0';
    } else {
        ctx->error_msg[0] = '\0';
    }
}

/* ==================== Public API: Initialization ==================== */

nng_context_t* nng_init(const char *lib_path) {
    void *handle = nng_dlopen_auto(lib_path ? lib_path : "");
    if (!handle) {
        char *err = dlerror();
        printf("✗ Failed to load NNG library: %s\n", err ? err : "unknown error");
        return NULL;
    }

    nng_context_t *ctx = (nng_context_t*)malloc(sizeof(nng_context_t));
    if (!ctx) {
        dlclose(handle);
        return NULL;
    }

    memset(ctx, 0, sizeof(nng_context_t));
    ctx->lib_handle = handle;
    ctx->socket = (nng_socket){0};
    ctx->last_error = NNG_OK;

    // Load function pointers
    ctx->rep0_open = (nng_rep0_open_t)dlsym(handle, "nng_rep0_open");
    ctx->req0_open = (nng_req0_open_t)dlsym(handle, "nng_req0_open");
    ctx->pub0_open = (nng_pub0_open_t)dlsym(handle, "nng_pub0_open");
    ctx->sub0_open = (nng_sub0_open_t)dlsym(handle, "nng_sub0_open");
    ctx->close = (nng_close_t)dlsym(handle, "nng_close");
    ctx->listen = (nng_listen_t)dlsym(handle, "nng_listen");
    ctx->dial = (nng_dial_t)dlsym(handle, "nng_dial");
    ctx->send = (nng_send_t)dlsym(handle, "nng_send");
    ctx->recv = (nng_recv_t)dlsym(handle, "nng_recv");
    ctx->socket_set_ms = (nng_socket_set_ms_t)dlsym(handle, "nng_socket_set_ms");
    ctx->socket_set = (nng_socket_set_t)dlsym(handle, "nng_socket_set");
    ctx->strerror = (nng_strerror_t)dlsym(handle, "nng_strerror");
    ctx->sub0_subscribe = (nng_sub0_subscribe_t)dlsym(handle, "nng_socket_set");

    // Check essential functions
    if (!ctx->rep0_open || !ctx->req0_open || !ctx->close ||
        !ctx->listen || !ctx->dial || !ctx->send || !ctx->recv) {
        printf("✗ Failed to resolve essential NNG functions\n");
        free(ctx);
        dlclose(handle);
        return NULL;
    }

    printf("✓ NNG library loaded successfully\n");
    return ctx;
}

void nng_cleanup(nng_context_t *ctx) {
    if (!ctx) return;

    // Close socket if open
    if (ctx->socket.id != 0 && ctx->close) {
        ctx->close(ctx->socket);
    }

    // Close library
    if (ctx->lib_handle) {
        dlclose(ctx->lib_handle);
        ctx->lib_handle = NULL;
    }

    free(ctx);
}

/* ==================== REQ/REP Pattern ==================== */

int nng_listen_rep(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) {
        return NNG_EINVAL;
    }

    // Close existing socket if any
    if (ctx->socket.id != 0 && ctx->close) {
        ctx->close(ctx->socket);
        ctx->socket = (nng_socket){0};
    }

    // Open REP socket
    int rv = ctx->rep0_open(&ctx->socket);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to open REP socket");
        return rv;
    }

    // Listen on URL
    rv = ctx->listen(ctx->socket, url, NULL, 0);
    if (rv != NNG_OK) {
        if (ctx->strerror) {
            nng_set_error(ctx, rv, ctx->strerror(rv));
        } else {
            nng_set_error(ctx, rv, "Failed to listen");
        }
        ctx->close(ctx->socket);
        ctx->socket = (nng_socket){0};
        return rv;
    }

    printf("✓ REP server listening on: %s\n", url);
    return NNG_OK;
}

int nng_dial_req(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) {
        return NNG_EINVAL;
    }

    // Close existing socket if any
    if (ctx->socket.id != 0 && ctx->close) {
        ctx->close(ctx->socket);
        ctx->socket = (nng_socket){0};
    }

    // Open REQ socket
    int rv = ctx->req0_open(&ctx->socket);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to open REQ socket");
        return rv;
    }

    // Connect to URL
    rv = ctx->dial(ctx->socket, url, NULL, 0);
    if (rv != NNG_OK) {
        if (ctx->strerror) {
            nng_set_error(ctx, rv, ctx->strerror(rv));
        } else {
            nng_set_error(ctx, rv, "Failed to dial");
        }
        ctx->close(ctx->socket);
        ctx->socket = (nng_socket){0};
        return rv;
    }

    printf("✓ REQ client connected to: %s\n", url);
    return NNG_OK;
}

std_string_t* nng_recv_msg(nng_context_t *ctx) {
    if (!ctx || ctx->socket.id == 0) {
        return NULL;
    }

    // Allocate receive buffer (16KB should be enough for most messages)
    char *buf = (char*)malloc(16384);
    if (!buf) {
        nng_set_error(ctx, NNG_ENOMEM, "Failed to allocate receive buffer");
        return NULL;
    }

    size_t size = 16384;
    int rv = ctx->recv(ctx->socket, buf, &size, 0);
    if (rv != NNG_OK) {
        if (ctx->strerror) {
            nng_set_error(ctx, rv, ctx->strerror(rv));
        } else {
            nng_set_error(ctx, rv, "Receive failed");
        }
        free(buf);
        return NULL;
    }

    // Create string from received data
    std_string_t *str = std_string_with_capacity(size + 1);
    if (!str) {
        free(buf);
        return NULL;
    }

    for (size_t i = 0; i < size; i++) {
        std_string_append_char(str, buf[i]);
    }

    free(buf);
    return str;
}

int nng_send_msg(nng_context_t *ctx, const char *data) {
    if (!ctx || !data || ctx->socket.id == 0) {
        return NNG_EINVAL;
    }

    size_t len = strlen(data);
    int rv = ctx->send(ctx->socket, (void*)data, len, 0);
    if (rv != NNG_OK) {
        if (ctx->strerror) {
            nng_set_error(ctx, rv, ctx->strerror(rv));
        } else {
            nng_set_error(ctx, rv, "Send failed");
        }
        return rv;
    }

    return NNG_OK;
}

/* ==================== PUB/SUB Pattern ==================== */

int nng_bind_pub(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) {
        return NNG_EINVAL;
    }

    // Close existing socket if any
    if (ctx->socket.id != 0 && ctx->close) {
        ctx->close(ctx->socket);
        ctx->socket = (nng_socket){0};
    }

    // Open PUB socket
    int rv = ctx->pub0_open(&ctx->socket);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to open PUB socket");
        return rv;
    }

    // Bind to URL
    rv = ctx->listen(ctx->socket, url, NULL, 0);
    if (rv != NNG_OK) {
        if (ctx->strerror) {
            nng_set_error(ctx, rv, ctx->strerror(rv));
        } else {
            nng_set_error(ctx, rv, "Failed to bind");
        }
        ctx->close(ctx->socket);
        ctx->socket = (nng_socket){0};
        return rv;
    }

    printf("✓ PUB server bound to: %s\n", url);
    return NNG_OK;
}

int nng_dial_sub(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) {
        return NNG_EINVAL;
    }

    // Close existing socket if any
    if (ctx->socket.id != 0 && ctx->close) {
        ctx->close(ctx->socket);
        ctx->socket = (nng_socket){0};
    }

    // Open SUB socket
    int rv = ctx->sub0_open(&ctx->socket);
    if (rv != NNG_OK) {
        nng_set_error(ctx, rv, "Failed to open SUB socket");
        return rv;
    }

    // Connect to URL
    rv = ctx->dial(ctx->socket, url, NULL, 0);
    if (rv != NNG_OK) {
        if (ctx->strerror) {
            nng_set_error(ctx, rv, ctx->strerror(rv));
        } else {
            nng_set_error(ctx, rv, "Failed to dial");
        }
        ctx->close(ctx->socket);
        ctx->socket = (nng_socket){0};
        return rv;
    }

    printf("✓ SUB client connected to: %s\n", url);
    return NNG_OK;
}

int nng_sub_subscribe(nng_context_t *ctx, const char *topic) {
    if (!ctx || ctx->socket.id == 0) {
        return NNG_EINVAL;
    }

    if (!ctx->socket_set) {
        nng_set_error(ctx, NNG_EINVAL, "socket_set function not available");
        return NNG_EINVAL;
    }

    const char *t = topic ? topic : "";
    size_t len = strlen(t);

    // NNG_OPT_SUB_SUBSCRIBE option
    int rv = ctx->socket_set(ctx->socket, "sub:subscribe", t, len);
    if (rv != NNG_OK) {
        if (ctx->strerror) {
            nng_set_error(ctx, rv, ctx->strerror(rv));
        } else {
            nng_set_error(ctx, rv, "Subscribe failed");
        }
        return rv;
    }

    printf("✓ Subscribed to topic: %s\n", len > 0 ? t : "<all>");
    return NNG_OK;
}

/* ==================== Socket Options ==================== */

int nng_set_recv_timeout(nng_context_t *ctx, int timeout_ms) {
    if (!ctx || ctx->socket.id == 0) {
        return NNG_EINVAL;
    }

    if (!ctx->socket_set_ms) {
        nng_set_error(ctx, NNG_EINVAL, "socket_set_ms function not available");
        return NNG_EINVAL;
    }

    int rv = ctx->socket_set_ms(ctx->socket, NNG_OPT_RECVTIMEO, (nng_duration)timeout_ms);
    if (rv != NNG_OK) {
        if (ctx->strerror) {
            nng_set_error(ctx, rv, ctx->strerror(rv));
        } else {
            nng_set_error(ctx, rv, "Failed to set receive timeout");
        }
        return rv;
    }

    return NNG_OK;
}

int nng_set_send_timeout(nng_context_t *ctx, int timeout_ms) {
    if (!ctx || ctx->socket.id == 0) {
        return NNG_EINVAL;
    }

    if (!ctx->socket_set_ms) {
        nng_set_error(ctx, NNG_EINVAL, "socket_set_ms function not available");
        return NNG_EINVAL;
    }

    int rv = ctx->socket_set_ms(ctx->socket, NNG_OPT_SENDTIMEO, (nng_duration)timeout_ms);
    if (rv != NNG_OK) {
        if (ctx->strerror) {
            nng_set_error(ctx, rv, ctx->strerror(rv));
        } else {
            nng_set_error(ctx, rv, "Failed to set send timeout");
        }
        return rv;
    }

    return NNG_OK;
}

/* ==================== Socket Management ==================== */

void nng_close_socket(nng_context_t *ctx) {
    if (!ctx || ctx->socket.id == 0) {
        return;
    }

    if (ctx->close) {
        ctx->close(ctx->socket);
        ctx->socket = (nng_socket){0};
    }
}

const char* nng_get_error(nng_context_t *ctx) {
    if (!ctx) {
        return "Invalid context";
    }

    if (ctx->error_msg[0] != '\0') {
        return ctx->error_msg;
    }

    if (ctx->strerror) {
        return ctx->strerror(ctx->last_error);
    }

    return "Unknown error";
}

/* ==================== Self Tests ==================== */

int nng_selftest_reqrep(const char *lib_path) {
    printf("=== NNG REQ/REP Self Test ===\n");

    // This is a simplified test - in practice, you'd need to fork/pthread
    // to run server and client simultaneously

    nng_context_t *server_ctx = nng_init(lib_path);
    if (!server_ctx) {
        printf("✗ Failed to initialize server context\n");
        return -1;
    }

    // Create REP server
    int rv = nng_listen_rep(server_ctx, "ipc:///tmp/nng_test_reqrep");
    if (rv != NNG_OK) {
        printf("✗ Failed to create REP server: %s\n", nng_get_error(server_ctx));
        nng_cleanup(server_ctx);
        return -1;
    }

    printf("✓ REP server created\n");
    printf("Note: Full REQ/REP test requires concurrent client (fork/pthread)\n");
    printf("      In production, use separate processes or threads\n");

    nng_cleanup(server_ctx);
    return 0;
}

int nng_selftest_pubsub(const char *lib_path) {
    printf("=== NNG PUB/SUB Self Test ===\n");

    nng_context_t *pub_ctx = nng_init(lib_path);
    if (!pub_ctx) {
        printf("✗ Failed to initialize publisher context\n");
        return -1;
    }

    // Create PUB server
    int rv = nng_bind_pub(pub_ctx, "ipc:///tmp/nng_test_pubsub");
    if (rv != NNG_OK) {
        printf("✗ Failed to create PUB server: %s\n", nng_get_error(pub_ctx));
        nng_cleanup(pub_ctx);
        return -1;
    }

    printf("✓ PUB server created\n");
    printf("Note: Full PUB/SUB test requires concurrent subscriber\n");
    printf("      In production, use separate processes or threads\n");

    nng_cleanup(pub_ctx);
    return 0;
}
