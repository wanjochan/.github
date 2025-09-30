/**
 * Simple connection keepalive using periodic lightweight CDP call
 */
#include "cdp_internal.h"
#include "cdp_bus.h"
#include <time.h>

static time_t g_last_ping = 0;
static int g_interval = 30; // seconds

void cdp_conn_init(void) {
    g_last_ping = time(NULL);
}

void cdp_conn_tick(void) {
    time_t now = time(NULL);
    if (ws_sock < 0) return;
    if (now - g_last_ping < g_interval) return;
    char buf[1024];
    // Use a cheap call; ignore result
    (void)cdp_call_cmd("Target.getTargets", NULL, buf, sizeof(buf), 1000);
    g_last_ping = now;
}

