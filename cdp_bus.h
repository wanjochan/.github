/**
 * CDP Message Bus - Unified send/call API
 */
#ifndef CDP_BUS_H
#define CDP_BUS_H

#include <stddef.h>

/* Fire-and-forget CDP command */
int cdp_send_cmd(const char *method, const char *params_json);

/* Send command and wait for matching id response */
int cdp_call_cmd(const char *method, const char *params_json,
                 char *out_response, size_t out_size, int timeout_ms);

/* Centralized response bus (id -> json) */
void cdp_bus_store(const char *json);
int  cdp_bus_try_get(int id, char *out, size_t out_size);

/* Optional callback registry for async consumers */
typedef void (*cdp_bus_cb_t)(const char *json, void *user);
int  cdp_bus_register(int id, cdp_bus_cb_t cb, void *user);
int  cdp_bus_unregister(int id);

#endif /* CDP_BUS_H */
