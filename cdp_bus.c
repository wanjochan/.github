/**
 * CDP Message Bus - Unified send/call API
 */

#include "cdp_internal.h"
#include "cdp_bus.h"
#include <pthread.h>

extern int ws_sock;
extern int ws_cmd_id;

/* Simple in-memory response registry */
typedef struct { int id; char *json; } BusEntry;
static BusEntry g_bus[64];
static int g_bus_count = 0;
static pthread_mutex_t g_bus_mtx = PTHREAD_MUTEX_INITIALIZER;
typedef struct { int id; cdp_bus_cb_t cb; void *user; } BusCb;
static BusCb g_cbs[128];
static int g_cb_count = 0;
static pthread_mutex_t g_cb_mtx = PTHREAD_MUTEX_INITIALIZER;

static int extract_id(const char *json) {
    if (!json) return -1;
    const char *p = strstr(json, "\"id\":");
    if (!p) return -1;
    return atoi(p + 5);
}

void cdp_bus_store(const char *json) {
    if (!json) return;
    int id = extract_id(json);
    if (id <= 0) return;
    pthread_mutex_lock(&g_cb_mtx);
    // If callback exists, dispatch immediately
    for (int i=0;i<g_cb_count;i++) {
        if (g_cbs[i].id == id && g_cbs[i].cb) {
            g_cbs[i].cb(json, g_cbs[i].user);
            // remove entry
            for (int j=i+1;j<g_cb_count;j++) g_cbs[j-1]=g_cbs[j];
            g_cb_count--;
            pthread_mutex_unlock(&g_cb_mtx);
            return;
        }
    }
    pthread_mutex_unlock(&g_cb_mtx);
    // Replace existing
    pthread_mutex_lock(&g_bus_mtx);
    for (int i=0;i<g_bus_count;i++) {
        if (g_bus[i].id == id) { free(g_bus[i].json); g_bus[i].json = strdup(json); return; }
    }
    if (g_bus_count < (int)(sizeof(g_bus)/sizeof(g_bus[0]))) {
        g_bus[g_bus_count].id = id;
        g_bus[g_bus_count].json = strdup(json);
        g_bus_count++;
    } else {
        // overwrite oldest
        free(g_bus[0].json);
        for (int i=1;i<g_bus_count;i++) g_bus[i-1]=g_bus[i];
        g_bus[g_bus_count-1].id = id;
        g_bus[g_bus_count-1].json = strdup(json);
    }
    pthread_mutex_unlock(&g_bus_mtx);
}

int cdp_bus_try_get(int id, char *out, size_t out_size) {
    if (id <= 0 || !out || out_size == 0) return 0;
    pthread_mutex_lock(&g_bus_mtx);
    for (int i=0;i<g_bus_count;i++) {
        if (g_bus[i].id == id) {
            strncpy(out, g_bus[i].json, out_size-1);
            out[out_size-1] = '\0';
            free(g_bus[i].json);
            for (int j=i+1;j<g_bus_count;j++) g_bus[j-1]=g_bus[j];
            g_bus_count--;
            pthread_mutex_unlock(&g_bus_mtx);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_bus_mtx);
    return 0;
}

int cdp_bus_register(int id, cdp_bus_cb_t cb, void *user) {
    if (id <= 0 || !cb) return -1;
    pthread_mutex_lock(&g_cb_mtx);
    if (g_cb_count < (int)(sizeof(g_cbs)/sizeof(g_cbs[0]))) {
        g_cbs[g_cb_count].id = id;
        g_cbs[g_cb_count].cb = cb;
        g_cbs[g_cb_count].user = user;
        g_cb_count++;
        pthread_mutex_unlock(&g_cb_mtx);
        return 0;
    }
    pthread_mutex_unlock(&g_cb_mtx);
    return -1;
}

int cdp_bus_unregister(int id) {
    pthread_mutex_lock(&g_cb_mtx);
    for (int i=0;i<g_cb_count;i++) {
        if (g_cbs[i].id == id) {
            for (int j=i+1;j<g_cb_count;j++) g_cbs[j-1]=g_cbs[j];
            g_cb_count--;
            pthread_mutex_unlock(&g_cb_mtx);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_cb_mtx);
    return -1;
}

/* Build minimal command JSON using QUOTE and optional params */
static int build_command(char *buf, size_t sz, int id, const char *method, const char *params_json) {
    if (!buf || !method) return -1;
    if (params_json && *params_json) {
        return snprintf(buf, sz, QUOTE({"id":%d,"method":"%s","params":%s}), id, method, params_json);
    } else {
        return snprintf(buf, sz, QUOTE({"id":%d,"method":"%s"}), id, method);
    }
}

int cdp_send_cmd(const char *method, const char *params_json) {
    int id = ws_cmd_id++;
    char cmd[MAX_CMD_SIZE];
    build_command(cmd, sizeof(cmd), id, method, params_json);
    return send_command_with_retry(cmd);
}

int cdp_call_cmd(const char *method, const char *params_json,
                 char *out_response, size_t out_size, int timeout_ms) {
    if (!out_response || out_size == 0) return -1;
    int id = ws_cmd_id++;
    char cmd[MAX_CMD_SIZE];
    build_command(cmd, sizeof(cmd), id, method, params_json);
    if (send_command_with_retry(cmd) < 0) return -1;

    /* Temporary: reuse existing receive_response_by_id with bounded tries based on time */
    /* timeout_ms is honored inside receive_response_by_id via g_ctx.config.timeout_ms; set temporarily */
    int saved = g_ctx.config.timeout_ms;
    if (timeout_ms > 0) g_ctx.config.timeout_ms = timeout_ms;
    // Try bus first
    if (cdp_bus_try_get(id, out_response, out_size)) return 0;
    int len = receive_response_by_id(out_response, out_size, id, 10);
    g_ctx.config.timeout_ms = saved;
    return len > 0 ? 0 : -1;
}
