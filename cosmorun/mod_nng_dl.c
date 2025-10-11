// mod_nng_dl.c - Dynamic NNG implementation for cosmorun
// Uses dlopen to load libnng.so at runtime
//
// This version dynamically loads NNG functions, providing the same API
// as the original mod_nng.c with full dynamic loading support

#include "cosmo_libc.h"
#include "mod_std.h"

/* NNG opaque types (actual implementation hidden in libnng.so) */
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

/* NNG socket options */
#define NNG_OPT_RECVTIMEO "recv-timeout"
#define NNG_OPT_SENDTIMEO "send-timeout"
#define NNG_OPT_SUB_SUBSCRIBE "sub:subscribe"

/* NNG duration constants */
#define NNG_DURATION_INFINITE -1
#define NNG_DURATION_DEFAULT -2
#define NNG_DURATION_ZERO 0

/* NNG context structure - manages library state and sockets */
typedef struct nng_context {
    void *lib_handle;
    nng_socket socket;
    int last_error;
    char error_msg[256];
    int socket_type; // 0=none, 1=REP, 2=REQ, 3=PUB, 4=SUB
    
    // Function pointers
    int (*rep0_open)(nng_socket *);
    int (*req0_open)(nng_socket *);
    int (*pub0_open)(nng_socket *);
    int (*sub0_open)(nng_socket *);
    int (*close)(nng_socket);
    int (*listen)(nng_socket, const char *, void *, int);
    int (*dial)(nng_socket, const char *, void *, int);
    int (*send)(nng_socket, void *, size_t, int);
    int (*recvmsg)(nng_socket, void **, int);
    int (*socket_set_ms)(nng_socket, const char *, nng_duration);
    int (*socket_set)(nng_socket, const char *, const void *, size_t);
    const char* (*strerror)(int);
    size_t (*msg_len)(void *);
    void* (*msg_body)(void *);
    void (*msg_free)(void *);
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

static int nng_preferred_dlopen_flags(void) {
#if defined(_WIN32) || defined(_WIN64)
    return RTLD_LAZY;
#else
    return RTLD_LAZY | RTLD_GLOBAL;
#endif
}

static void *nng_try_dlopen(const char *path) {
    if (!path || !*path) {
        printf("Debug: Empty path provided to nng_try_dlopen\n");
        return NULL;
    }

    printf("Debug: Attempting to dlopen: %s\n", path);
    
    int flags = nng_preferred_dlopen_flags();
    void *handle = dlopen(path, flags);
    if (handle) {
        printf("Debug: Successfully loaded: %s\n", path);
        return handle;
    }
    
    handle = dlopen(path, 0);
    if (handle) {
        printf("Debug: Successfully loaded with flags=0: %s\n", path);
        return handle;
    }
    return NULL;
}

static void *nng_dlopen_auto(const char *requested_path) {
    if (requested_path && *requested_path) {
        return nng_try_dlopen(requested_path);
    }

    // Determine library name based on OS and architecture
    const char *lib_name = NULL;
#if defined(__APPLE__)
  #if defined(__aarch64__) || defined(__arm64__)
    lib_name = "lib/nng-arm-64.dylib";
  #else
    lib_name = "lib/nng-x86-64.dylib";
  #endif
#elif defined(__linux__)
  #if defined(__aarch64__) || defined(__arm64__)
    lib_name = "lib/nng-arm-64.so";
  #else
    lib_name = "lib/nng-x86-64.so";
  #endif
#elif defined(_WIN32)
  #if defined(__aarch64__) || defined(__arm64__)
    lib_name = "lib/nng-arm-64.dll";
  #else
    lib_name = "lib/nng-x86-64.dll";
  #endif
#else
    printf("✗ Unsupported platform for NNG library loading\n");
    return NULL;
#endif

    printf("Debug: Loading NNG library: %s\n", lib_name);
    return nng_try_dlopen(lib_name);
}

/* ==================== Public API ==================== */

nng_context_t* nng_init(const char *lib_path) {
    void *handle = nng_dlopen_auto(lib_path ? lib_path : "");
    if (!handle) {
        printf("✗ Failed to load NNG library\n");
        return NULL;
    }
    
    nng_context_t *ctx = (nng_context_t*)malloc(sizeof(nng_context_t));
    if (!ctx) {
        dlclose(handle);
        return NULL;
    }
    
    memset(ctx, 0, sizeof(nng_context_t));
    ctx->lib_handle = handle;
    ctx->socket = 0;
    ctx->last_error = NNG_OK;
    ctx->socket_type = 0;
    
    // Load function pointers
    ctx->rep0_open = (int (*)(nng_socket *))dlsym(handle, "nng_rep0_open");
    ctx->req0_open = (int (*)(nng_socket *))dlsym(handle, "nng_req0_open");
    ctx->pub0_open = (int (*)(nng_socket *))dlsym(handle, "nng_pub0_open");
    ctx->sub0_open = (int (*)(nng_socket *))dlsym(handle, "nng_sub0_open");
    ctx->close = (int (*)(nng_socket))dlsym(handle, "nng_close");
    ctx->listen = (int (*)(nng_socket, const char *, void *, int))dlsym(handle, "nng_listen");
    ctx->dial = (int (*)(nng_socket, const char *, void *, int))dlsym(handle, "nng_dial");
    ctx->send = (int (*)(nng_socket, void *, size_t, int))dlsym(handle, "nng_send");
    ctx->recvmsg = (int (*)(nng_socket, void **, int))dlsym(handle, "nng_recvmsg");
    ctx->socket_set_ms = (int (*)(nng_socket, const char *, nng_duration))dlsym(handle, "nng_socket_set_ms");
    ctx->socket_set = (int (*)(nng_socket, const char *, const void *, size_t))dlsym(handle, "nng_socket_set");
    ctx->strerror = (const char* (*)(int))dlsym(handle, "nng_strerror");
    ctx->msg_len = (size_t (*)(void *))dlsym(handle, "nng_msg_len");
    ctx->msg_body = (void* (*)(void *))dlsym(handle, "nng_msg_body");
    ctx->msg_free = (void (*)(void *))dlsym(handle, "nng_msg_free");
    
    // Check essential functions
    if (!ctx->rep0_open || !ctx->req0_open || !ctx->close ||
        !ctx->listen || !ctx->dial || !ctx->send || !ctx->recvmsg) {
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
    if (ctx->socket != 0 && ctx->close) {
        ctx->close(ctx->socket);
    }
    
    // Close library
    if (ctx->lib_handle) {
        dlclose(ctx->lib_handle);
        ctx->lib_handle = NULL;
    }
    
    free(ctx);
}

int nng_listen_rep(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;
    
    // Close existing socket if any
    if (ctx->socket != 0 && ctx->close) {
        ctx->close(ctx->socket);
        ctx->socket = 0;
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
        ctx->socket = 0;
        return rv;
    }
    
    ctx->socket_type = 1; // REP
    printf("✓ REP server listening on: %s\n", url);
    return NNG_OK;
}

int nng_dial_req(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;
    
    // Close existing socket if any
    if (ctx->socket != 0 && ctx->close) {
        ctx->close(ctx->socket);
        ctx->socket = 0;
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
        ctx->socket = 0;
        return rv;
    }
    
    ctx->socket_type = 2; // REQ
    printf("✓ REQ client connected to: %s\n", url);
    return NNG_OK;
}

std_string_t* nng_recv_msg(nng_context_t *ctx) {
    if (!ctx || ctx->socket == 0) return NULL;
    
    void *msg;
    int rv = ctx->recvmsg(ctx->socket, &msg, 0);
    if (rv != NNG_OK) {
        if (ctx->strerror) {
            nng_set_error(ctx, rv, ctx->strerror(rv));
        } else {
            nng_set_error(ctx, rv, "Receive failed");
        }
        return NULL;
    }
    
    size_t size = ctx->msg_len(msg);
    void *data = ctx->msg_body(msg);
    
    std_string_t *str = std_string_with_capacity(size + 1);
    if (!str) {
        ctx->msg_free(msg);
        return NULL;
    }
    
    for (size_t i = 0; i < size; i++) {
        std_string_append_char(str, ((char*)data)[i]);
    }
    
    ctx->msg_free(msg);
    return str;
}

int nng_send_msg(nng_context_t *ctx, const char *data) {
    if (!ctx || !data || ctx->socket == 0) return NNG_EINVAL;
    
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

int nng_bind_pub(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;
    
    // Close existing socket if any
    if (ctx->socket != 0 && ctx->close) {
        ctx->close(ctx->socket);
        ctx->socket = 0;
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
        ctx->socket = 0;
        return rv;
    }
    
    ctx->socket_type = 3; // PUB
    printf("✓ PUB server bound to: %s\n", url);
    return NNG_OK;
}

int nng_dial_sub(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;
    
    // Close existing socket if any
    if (ctx->socket != 0 && ctx->close) {
        ctx->close(ctx->socket);
        ctx->socket = 0;
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
        ctx->socket = 0;
        return rv;
    }
    
    ctx->socket_type = 4; // SUB
    printf("✓ SUB client connected to: %s\n", url);
    return NNG_OK;
}

int nng_sub_subscribe(nng_context_t *ctx, const char *topic) {
    if (!ctx || ctx->socket_type != 4) return NNG_EINVAL;
    
    if (!ctx->socket_set) {
        nng_set_error(ctx, NNG_EINVAL, "socket_set function not available");
        return NNG_EINVAL;
    }
    
    const char *t = topic ? topic : "";
    size_t len = strlen(t);
    
    int rv = ctx->socket_set(ctx->socket, NNG_OPT_SUB_SUBSCRIBE, t, len);
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

int nng_set_recv_timeout(nng_context_t *ctx, int timeout_ms) {
    if (!ctx || ctx->socket == 0) return NNG_EINVAL;
    
    if (!ctx->socket_set_ms) {
        nng_set_error(ctx, NNG_EINVAL, "socket_set_ms function not available");
        return NNG_EINVAL;
    }
    
    nng_duration timeout = timeout_ms;
    int rv = ctx->socket_set_ms(ctx->socket, NNG_OPT_RECVTIMEO, timeout);
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
    if (!ctx || ctx->socket == 0) return NNG_EINVAL;
    
    if (!ctx->socket_set_ms) {
        nng_set_error(ctx, NNG_EINVAL, "socket_set_ms function not available");
        return NNG_EINVAL;
    }
    
    nng_duration timeout = timeout_ms;
    int rv = ctx->socket_set_ms(ctx->socket, NNG_OPT_SENDTIMEO, timeout);
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

void nng_close_socket(nng_context_t *ctx) {
    if (!ctx || ctx->socket == 0) return;
    
    if (ctx->close) {
        ctx->close(ctx->socket);
        ctx->socket = 0;
    }
}

const char* nng_get_error(nng_context_t *ctx) {
    if (!ctx) return "Invalid context";
    
    if (ctx->error_msg[0] != '\0') {
        return ctx->error_msg;
    }
    
    if (ctx->strerror) {
        return ctx->strerror(ctx->last_error);
    }
    
    return "Unknown error";
}

int nng_selftest_reqrep(const char *lib_path) {
    printf("=== NNG REQ/REP Self Test (Dynamic) ===\n");
    
    nng_context_t *ctx = nng_init(lib_path);
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
    printf("=== NNG PUB/SUB Self Test (Dynamic) ===\n");
    
    nng_context_t *ctx = nng_init(lib_path);
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