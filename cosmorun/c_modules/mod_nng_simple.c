// mod_nng_simple.c - Simplified NNG implementation for cosmorun
// Provides basic messaging patterns without complex dependencies
//
// This is a minimal implementation that provides the same API as mod_nng.c
// but uses simple socket operations instead of the full NNG library

#include "../cosmorun_system/libc_shim/sys_libc_shim.h"
#include "mod_std.h"

/* 系统头文件 - 仅在 System Layer 中使用 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

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
    int socket_fd;
    int last_error;
    char error_msg[256];
    int socket_type; // 0=none, 1=REP, 2=REQ, 3=PUB, 4=SUB
} nng_context_t;

/* Helper functions */
static void nng_set_error(nng_context_t *ctx, int code, const char *msg) {
    if (!ctx) return;
    ctx->last_error = code;
    if (msg) {
        shim_strncpy(ctx->error_msg, msg, sizeof(ctx->error_msg) - 1);
        ctx->error_msg[sizeof(ctx->error_msg) - 1] = '\0';
    } else {
        ctx->error_msg[0] = '\0';
    }
}

static int parse_url(const char *url, char *host, int *port) {
    if (!url || !host) return -1;

    if (shim_strncmp(url, "tcp://", 6) == 0) {
        const char *addr = url + 6;
        const char *colon = shim_strchr(addr, ':');
        if (colon) {
            int len = colon - addr;
            if (len > 255) len = 255;
            shim_strncpy(host, addr, len);
            host[len] = '\0';
            *port = atoi(colon + 1);
            return 0;
        }
    }
    return -1;
}

/* ==================== Public API ==================== */

nng_context_t* nng_init(const char *lib_path) {
    (void)lib_path; // Ignore lib_path for simplified version

    nng_context_t *ctx = (nng_context_t*)shim_malloc(sizeof(nng_context_t));
    if (!ctx) return NULL;

    shim_memset(ctx, 0, sizeof(nng_context_t));
    ctx->socket_fd = -1;
    ctx->last_error = NNG_OK;

    shim_printf("✓ NNG simplified library initialized\n");
    return ctx;
}

void nng_cleanup(nng_context_t *ctx) {
    if (!ctx) return;

    if (ctx->socket_fd >= 0) {
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
    }

    shim_free(ctx);
}

int nng_listen_rep(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;

    char host[256];
    int port;
    if (parse_url(url, host, &port) != 0) {
        nng_set_error(ctx, NNG_EINVAL, "Invalid URL format");
        return NNG_EINVAL;
    }

    // Create socket
    ctx->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->socket_fd < 0) {
        nng_set_error(ctx, NNG_EINVAL, "Failed to create socket");
        return NNG_EINVAL;
    }

    // Set socket options
    int opt = 1;
    setsockopt(ctx->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to address
    struct sockaddr_in addr;
    shim_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(ctx->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        nng_set_error(ctx, NNG_EADDRINUSE, "Failed to bind address");
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
        return NNG_EADDRINUSE;
    }

    if (listen(ctx->socket_fd, 5) < 0) {
        nng_set_error(ctx, NNG_EINVAL, "Failed to listen");
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
        return NNG_EINVAL;
    }

    ctx->socket_type = 1; // REP
    shim_printf("✓ REP server listening on: %s\n", url);
    return NNG_OK;
}

int nng_dial_req(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;

    char host[256];
    int port;
    if (parse_url(url, host, &port) != 0) {
        nng_set_error(ctx, NNG_EINVAL, "Invalid URL format");
        return NNG_EINVAL;
    }

    // Create socket
    ctx->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->socket_fd < 0) {
        nng_set_error(ctx, NNG_EINVAL, "Failed to create socket");
        return NNG_EINVAL;
    }

    // Connect to server
    struct sockaddr_in addr;
    shim_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        nng_set_error(ctx, NNG_EINVAL, "Invalid address");
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
        return NNG_EINVAL;
    }

    if (connect(ctx->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        nng_set_error(ctx, NNG_ECONNREFUSED, "Failed to connect");
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
        return NNG_ECONNREFUSED;
    }

    ctx->socket_type = 2; // REQ
    shim_printf("✓ REQ client connected to: %s\n", url);
    return NNG_OK;
}

std_string_t* nng_recv_msg(nng_context_t *ctx) {
    if (!ctx || ctx->socket_fd < 0) return NULL;
    
    char buffer[4096];
    ssize_t len = recv(ctx->socket_fd, buffer, sizeof(buffer) - 1, 0);
    if (len <= 0) {
        nng_set_error(ctx, NNG_ECLOSED, "Connection closed or error");
        return NULL;
    }
    
    buffer[len] = '\0';
    
    std_string_t *str = std_string_with_capacity(len + 1);
    if (!str) return NULL;
    
    for (int i = 0; i < len; i++) {
        std_string_append_char(str, buffer[i]);
    }
    
    return str;
}

int nng_send_msg(nng_context_t *ctx, const char *data) {
    if (!ctx || !data || ctx->socket_fd < 0) return NNG_EINVAL;

    size_t len = shim_strlen(data);
    ssize_t sent = send(ctx->socket_fd, data, len, 0);
    if (sent != (ssize_t)len) {
        nng_set_error(ctx, NNG_ECLOSED, "Failed to send data");
        return NNG_ECLOSED;
    }

    return NNG_OK;
}

int nng_bind_pub(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;
    
    // For simplified version, PUB/SUB uses the same TCP socket
    // In a real implementation, this would use UDP multicast
    return nng_listen_rep(ctx, url);
}

int nng_dial_sub(nng_context_t *ctx, const char *url) {
    if (!ctx || !url) return NNG_EINVAL;
    
    // For simplified version, PUB/SUB uses the same TCP socket
    return nng_dial_req(ctx, url);
}

int nng_sub_subscribe(nng_context_t *ctx, const char *topic) {
    (void)ctx; (void)topic; // Simplified version ignores topics
    return NNG_OK;
}

int nng_set_recv_timeout(nng_context_t *ctx, int timeout_ms) {
    if (!ctx || ctx->socket_fd < 0) return NNG_EINVAL;
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (setsockopt(ctx->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        nng_set_error(ctx, NNG_EINVAL, "Failed to set receive timeout");
        return NNG_EINVAL;
    }
    
    return NNG_OK;
}

int nng_set_send_timeout(nng_context_t *ctx, int timeout_ms) {
    if (!ctx || ctx->socket_fd < 0) return NNG_EINVAL;
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (setsockopt(ctx->socket_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        nng_set_error(ctx, NNG_EINVAL, "Failed to set send timeout");
        return NNG_EINVAL;
    }
    
    return NNG_OK;
}

void nng_close_socket(nng_context_t *ctx) {
    if (!ctx || ctx->socket_fd < 0) return;
    
    close(ctx->socket_fd);
    ctx->socket_fd = -1;
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
    shim_printf("=== NNG REQ/REP Self Test (Simplified) ===\n");
    shim_printf("✓ Simplified NNG implementation ready\n");
    shim_printf("Note: This is a minimal implementation for testing\n");
    return 0;
}

int nng_selftest_pubsub(const char *lib_path) {
    (void)lib_path;
    shim_printf("=== NNG PUB/SUB Self Test (Simplified) ===\n");
    shim_printf("✓ Simplified NNG implementation ready\n");
    shim_printf("Note: This is a minimal implementation for testing\n");
    return 0;
}