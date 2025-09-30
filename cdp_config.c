/**
 * CDP Config helpers
 */

#include "cdp_internal.h"
#include <stdio.h>

void cdp_config_apply_defaults(CDPContext *ctx) {
    if (!ctx) return;
    if (!ctx->config.server_host) ctx->config.server_host = "127.0.0.1";
    if (!ctx->config.chrome_host) ctx->config.chrome_host = "127.0.0.1";
    if (ctx->config.debug_port <= 0) ctx->config.debug_port = CHROME_DEFAULT_PORT;
    if (ctx->config.timeout_ms <= 0) ctx->config.timeout_ms = DEFAULT_TIMEOUT_MS;
}

void cdp_config_dump(const CDPContext *ctx) {
    if (!ctx) return;
    printf("Config:\n");
    printf("  Chrome Host: %s\n", ctx->config.chrome_host);
    printf("  Debug Port : %d\n", ctx->config.debug_port);
    printf("  Timeout(ms): %d\n", ctx->config.timeout_ms);
}

