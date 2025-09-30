/**
 * CDP JavaScript Module - Correct Two-Phase Solution
 * Phase 1: Execute expression directly
 * Phase 2: Convert to string if needed
 */

#include "cdp_internal.h"
#include "cdp_quickjs.h"  /* QuickJS for JSON processing */
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>

extern CDPContext g_ctx;
extern int verbose;
extern int ws_sock;
extern int ws_cmd_id;

#define min(a, b) ((a) < (b) ? (a) : (b))

// Function pointer for event handler
static int (*event_handler_callback)(const char* event_json) = NULL;

/* Set event handler callback for pipe mode */
void set_runtime_event_handler(int (*handler)(const char* event_json)) {
    event_handler_callback = handler;
}

/* Send command with retry on failure */
int send_command_with_retry(const char *command) {
    int attempts = 0;
    int max_attempts = 3;
    
    while (attempts < max_attempts) {
        if (ws_sock < 0) {
            if (reconnect_websocket_with_backoff() < 0) {
                return -1;
            }
        }
        
        int ret = ws_send_text(ws_sock, command);
        if (ret > 0) {
            return ret;
        }
        
        DEBUG_LOG("Send failed, reconnect attempt %d/%d", attempts + 1, max_attempts);
        close(ws_sock);
        ws_sock = -1;
        attempts++;
        
        if (attempts < max_attempts) {
            usleep(100000 * attempts);
        }
    }
    
    return -1;
}

/* Receive response with specific ID */
int receive_response_by_id(char *buffer, size_t max_len, int cmd_id, int max_messages) {
    // Switch from fixed message count to time-bounded wait using select().
    // We still track messages_checked for diagnostics, but do not hard-fail early
    // when Chrome emits many unrelated events.
    int messages_checked = 0;

    // Determine timeout window (cap between 1s and 10s for safety)
    int timeout_ms = g_ctx.config.timeout_ms > 0 ? g_ctx.config.timeout_ms : DEFAULT_TIMEOUT_MS;
    if (timeout_ms < 1000) timeout_ms = 1000;
    if (timeout_ms > 10000) timeout_ms = 10000;

    struct timeval start_tv;
    gettimeofday(&start_tv, NULL);

    while (1) {
        // Check elapsed time
        struct timeval now_tv;
        gettimeofday(&now_tv, NULL);
        long elapsed_ms = (now_tv.tv_sec - start_tv.tv_sec) * 1000L +
                          (now_tv.tv_usec - start_tv.tv_usec) / 1000L;
        if (elapsed_ms >= timeout_ms) {
            DEBUG_LOG("Timeout waiting for id:%d after ~%ld ms (processed %d messages)",
                      cmd_id, elapsed_ms, messages_checked);
            return -1;
        }

        // Wait up to 100ms for readability to avoid blocking indefinitely
        fd_set rfds; FD_ZERO(&rfds); FD_SET(ws_sock, &rfds);
        struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 100000; // 100ms
        int sel = select(ws_sock + 1, &rfds, NULL, NULL, &tv);
        if (sel < 0) {
            // Interrupted or error
            if (errno == EINTR) continue;
            DEBUG_LOG("select() error while waiting for response: %s", strerror(errno));
            return -1;
        }
        if (sel == 0) {
            // No data yet; loop again until timeout
            continue;
        }

        // Check response bus first
        extern int cdp_bus_try_get(int id, char *out, size_t out_size);
        if (cdp_bus_try_get(cmd_id, buffer, max_len)) {
            return strlen(buffer);
        }
        int len = ws_recv_text(ws_sock, buffer, max_len);
        if (len <= 0) {
            // Socket closed or error
            return len;
        }

        messages_checked++;

        char id_pattern[32];
        snprintf(id_pattern, sizeof(id_pattern), JKEY("id") "%d", cmd_id);

        if (strstr(buffer, id_pattern)) {
            return len;
        }

        // Process important events using callback if available
        if (event_handler_callback &&
            (strstr(buffer, "Runtime.bindingCalled") || strstr(buffer, "Runtime.consoleAPICalled") ||
             strstr(buffer, "Fetch.requestPaused") || strstr(buffer, "Runtime.executionContextCreated") ||
             strstr(buffer, "Runtime.executionContextDestroyed"))) {
            event_handler_callback(buffer);
        }

        // Stash non-matching id responses on the bus for future consumers
        extern void cdp_bus_store(const char *json);
        if (strstr(buffer, JKEY("id"))) {
            cdp_bus_store(buffer);
        }

        DEBUG_LOG("Processed message %d (looking for id:%d)", messages_checked, cmd_id);

        // As a last resort, if caller passed a small max_messages, don't prematurely abort.
        // We honor max_messages only if it's > 0 and we've exceeded it AND at least 500ms elapsed,
        // to avoid infinite loops with noisy streams in very slow environments.
        if (max_messages > 0 && messages_checked >= max_messages && elapsed_ms > 500) {
            DEBUG_LOG("Exceeded max_messages=%d without finding id:%d; continuing until timeout",
                      max_messages, cmd_id);
            // Do not return yet; continue until overall timeout_ms
        }
    }
}

/* Parse response and extract result info */
typedef struct {
    char type[32];
    char subtype[32];
    char className[64];
    char objectId[128];
    char value[MAX_RESULT_SIZE];
    int isAsync;
    int needsStringify;
} ResponseInfo;

static int parse_response(const char *buffer, ResponseInfo *info) {
    memset(info, 0, sizeof(ResponseInfo));

    // Debug: print what we received
    if (getenv("CDP_DEBUG_JSON")) {
        fprintf(stderr, "[DEBUG] Raw response: %.500s\n", buffer);
    }

    // Check for error
    if (strstr(buffer, QUOTE("error")) || strstr(buffer, QUOTE("exceptionDetails"))) {
        const char *msg = strstr(buffer, JKEYQ("message"));
        if (msg) {
            msg += 11;
            const char *end = strchr(msg, '"');
            if (end) {
                size_t len = min(end - msg, MAX_RESULT_SIZE - 1);
                strncpy(info->value, msg, len);
                info->value[len] = '\0';
            } else {
                strcpy(info->value, "Error in execution");
            }
        } else {
            strcpy(info->value, "Error in execution");
        }
        strcpy(info->type, "error");
        return -1;
    }
    
    // Find result object
    const char *result_obj = strstr(buffer, JKEY("result") "{");
    if (!result_obj) {
        strcpy(info->value, "No result");
        return -1;
    }
    
    // Use QuickJS to extract type, subtype, and className cleanly
    cdp_json_get_nested(buffer, "result.result.type", info->type, sizeof(info->type));
    cdp_json_get_nested(buffer, "result.result.subtype", info->subtype, sizeof(info->subtype));
    cdp_json_get_nested(buffer, "result.result.className", info->className, sizeof(info->className));
    
    // Check for Promise/async
    if (strcmp(info->className, "Promise") == 0 || strstr(info->subtype, "promise")) {
        info->isAsync = 1;
    }
    
    // Extract objectId if present
    cdp_json_get_nested(buffer, "result.result.objectId", info->objectId, sizeof(info->objectId));
    
    // Determine if we need to stringify
    if (strcmp(info->type, "object") == 0 ||
        strcmp(info->type, "function") == 0) {
        info->needsStringify = 1;
    }

    if (getenv("CDP_DEBUG_JSON")) {
        fprintf(stderr, "[DEBUG] Parsed type='%s', subtype='%s'\n", info->type, info->subtype);
    }
    
    // Initialize QuickJS JSON (required)
    static int json_initialized = 0;
    if (!json_initialized) {
        if (cdp_json_init() < 0) {
            fprintf(stderr, "Failed to initialize QuickJS JSON parser\n");
            return -1;
        }
        json_initialized = 1;
    }

    // Use QuickJS for clean JSON parsing of the entire response
    // The buffer contains the full CDP response, use nested path to extract value
    if (strcmp(info->type, "string") == 0) {
        // String values should be extracted as-is
        int ret = cdp_json_get_nested(buffer, "result.result.value", info->value, MAX_RESULT_SIZE);
        if (getenv("CDP_DEBUG_JSON")) {
            fprintf(stderr, "[DEBUG] String extraction: ret=%d, value='%s'\n", ret, info->value);
        }
        if (ret != 0) {
            // Failed to get value with QuickJS, fallback to manual extraction
            fprintf(stderr, "[DEBUG] QuickJS failed to extract string value, ret=%d\n", ret);
        }
    } else if (strcmp(info->type, "number") == 0) {
        // For numbers, we need to handle both integers and floats
        // QuickJS will parse them correctly from the nested path
        char num_str[64];
        // Get the value as a nested field - QuickJS will handle the number format
        if (cdp_json_get_nested(buffer, "result.result.value", num_str, sizeof(num_str)) == 0) {
            // The value is already extracted as a string representation of the number
            strcpy(info->value, num_str);
        }
    } else if (strcmp(info->type, "boolean") == 0) {
        // Booleans need special handling - get as string
        char bool_str[16];
        if (cdp_json_get_nested(buffer, "result.result.value", bool_str, sizeof(bool_str)) == 0) {
            strcpy(info->value, bool_str);
        }
    }

    if (strcmp(info->type, "undefined") == 0) {
        strcpy(info->value, "undefined");
    } else if (strcmp(info->type, "object") == 0 && strcmp(info->subtype, "null") == 0) {
        strcpy(info->value, "null");
    }
    
    return 0;
}

/* Forward declarations */
static char* execute_javascript_on_object(const char *objectId);

/* Wait for async result */
static char* wait_for_promise(const char *objectId) {
    static char result[MAX_RESULT_SIZE];
    result[0] = '\0';
    
    // Use Runtime.awaitPromise
    char *command = (char*)safe_malloc(512);
    if (!command) {
        str_copy_safe(result, "Error: Memory allocation failed", sizeof(result));
        return result;
    }
    
    int cmd_id = ws_cmd_id++;
    snprintf(command, 512,
             QUOTE({"id":%d,"method":"Runtime.awaitPromise","params":{"promiseObjectId":"%s","returnByValue":true}}),
             cmd_id, objectId);
    
    if (send_command_with_retry(command) < 0) {
        safe_free((void**)&command);
        str_copy_safe(result, "Error: Failed to send await command", sizeof(result));
        return result;
    }
    
    safe_free((void**)&command);
    
    // Wait for promise result
    char buffer[8192];
    int len = receive_response_by_id(buffer, sizeof(buffer), cmd_id, 10);
    
    if (len <= 0) {
        str_copy_safe(result, "Error: No response for promise", sizeof(result));
        return result;
    }
    
    // Parse the promise result
    ResponseInfo info;
    parse_response(buffer, &info);
    
    if (info.value[0]) {
        str_copy_safe(result, info.value, sizeof(result));
    } else if (info.needsStringify && info.objectId[0]) {
        // Need to stringify the resolved value
        return execute_javascript_on_object(info.objectId);
    } else {
        str_copy_safe(result, "Promise resolved", sizeof(result));
    }
    
    return result;
}

/* Execute JavaScript on object using JSON.stringify */
static char* execute_javascript_on_object(const char *objectId) {
    static char result[MAX_RESULT_SIZE];
    result[0] = '\0';
    
    char *command = (char*)safe_malloc(512);
    if (!command) {
        str_copy_safe(result, "Error: Memory allocation failed", sizeof(result));
        return result;
    }
    
    int cmd_id = ws_cmd_id++;
    snprintf(command, 512,
             QUOTE({"id":%d,"method":"Runtime.callFunctionOn","params":{"objectId":"%s","functionDeclaration":"function() { try { return JSON.stringify(this); } catch(e) { return String(this); } }","returnByValue":true}}),
             cmd_id, objectId);
    
    if (send_command_with_retry(command) < 0) {
        safe_free((void**)&command);
        str_copy_safe(result, "Error: Failed to send stringify command", sizeof(result));
        return result;
    }
    
    safe_free((void**)&command);
    
    char buffer[8192];
    int len = receive_response_by_id(buffer, sizeof(buffer), cmd_id, 10);
    
    if (len <= 0) {
        str_copy_safe(result, "[object]", sizeof(result));
        return result;
    }
    
    ResponseInfo info;
    parse_response(buffer, &info);
    
    if (info.value[0]) {
        str_copy_safe(result, info.value, sizeof(result));
    } else {
        str_copy_safe(result, "[object]", sizeof(result));
    }
    
    return result;
}

/* Main execution function - Two-phase approach */
char* execute_javascript(const char *expression) {
    static char result[MAX_RESULT_SIZE];
    result[0] = '\0';
    
    if (!expression || !*expression) {
        return result;
    }
    
    // Check if on browser endpoint
    if (strstr(g_ctx.conn.target_id, "browser/")) {
        str_copy_safe(result, "Browser endpoint doesn't support JavaScript", sizeof(result));
        return result;
    }
    
    // Ensure connection
    if (ws_sock < 0) {
        if (reconnect_websocket_with_backoff() < 0) {
            str_copy_safe(result, "Error: Not connected to Chrome", sizeof(result));
            return result;
        }
    }
    
    // PHASE 1: Execute expression directly
    char *escaped = (char*)safe_malloc(strlen(expression) * 2 + 1);
    if (!escaped) {
        str_copy_safe(result, "Error: Memory allocation failed", sizeof(result));
        return result;
    }
    
    json_escape_safe(escaped, expression, strlen(expression) * 2 + 1);
    
    char *command = (char*)safe_malloc(strlen(escaped) + 256);
    if (!command) {
        safe_free((void**)&escaped);
        str_copy_safe(result, "Error: Memory allocation failed", sizeof(result));
        return result;
    }
    
    int cmd_id = ws_cmd_id++;
    extern int g_selected_context_id; // from cdp.c
    if (g_selected_context_id > 0) {
        snprintf(command, strlen(escaped) + 320,
                 QUOTE({"id":%d,"method":"Runtime.evaluate","params":{"expression":"%s","returnByValue":false,"generatePreview":true,"contextId":%d}}),
                 cmd_id, escaped, g_selected_context_id);
    } else {
        snprintf(command, strlen(escaped) + 256,
                 QUOTE({"id":%d,"method":"Runtime.evaluate","params":{"expression":"%s","returnByValue":false,"generatePreview":true}}),
                 cmd_id, escaped);
    }
    
    safe_free((void**)&escaped);
    
    if (verbose) {
        DEBUG_LOG("Phase 1: Executing expression directly");
    }
    
    if (send_command_with_retry(command) < 0) {
        safe_free((void**)&command);
        str_copy_safe(result, "Error: Failed to send command", sizeof(result));
        return result;
    }
    
    safe_free((void**)&command);
    
    // Receive response
    char buffer[8192];
    int len = receive_response_by_id(buffer, sizeof(buffer), cmd_id, 10);
    
    if (len <= 0) {
        str_copy_safe(result, "Error: No response from Chrome", sizeof(result));
        return result;
    }
    
    // Parse response
    ResponseInfo info;
    if (parse_response(buffer, &info) < 0) {
        // Error occurred
        str_copy_safe(result, info.value, sizeof(result));
        return result;
    }
    
    // Check if we have a direct value
    if (info.value[0] && !info.needsStringify) {
        str_copy_safe(result, info.value, sizeof(result));
        return result;
    }
    
    // PHASE 2: Handle special cases
    if (info.isAsync) {
        if (verbose) {
            DEBUG_LOG("Phase 2: Waiting for Promise");
        }
        return wait_for_promise(info.objectId);
    }
    
    if (info.needsStringify && info.objectId[0]) {
        if (verbose) {
            DEBUG_LOG("Phase 2: Converting object to JSON string");
        }
        return execute_javascript_on_object(info.objectId);
    }
    
    // Default fallback
    if (info.value[0]) {
        str_copy_safe(result, info.value, sizeof(result));
    } else {
        str_copy_safe(result, "undefined", sizeof(result));
    }

    return result;
}

/* ========================================================================== */
/*                          JSON API for Other Modules                       */
/* ========================================================================== */

#include "cdp_javascript.h"

/* Helper: Find JSON body in HTTP response */
static const char* find_json_body(const char *response) {
    // If it starts with '{' or '[', it's already JSON
    const char *p = response;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (*p == '{' || *p == '[') return p;

    // Look for end of HTTP headers
    const char *json_start = strstr(response, "\r\n\r\n");
    if (json_start) return json_start + 4;

    json_start = strstr(response, "\n\n");
    if (json_start) return json_start + 2;

    // Fallback: look for first '{' or '['
    json_start = strchr(response, '{');
    if (!json_start) json_start = strchr(response, '[');
    return json_start;
}

/* Initialize JavaScript engine */
int cdp_js_init(void) {
    return cdp_json_init();
}

/* Cleanup JavaScript engine */
void cdp_js_cleanup(void) {
    cdp_json_cleanup();
}

/* Extract string value from JSON */
int cdp_js_get_string(const char *json_or_response, const char *field, char *out, size_t out_size) {
    if (!json_or_response || !field || !out) return -1;

    const char *json = find_json_body(json_or_response);
    if (!json) return -1;

    return cdp_json_get_string(json, field, out, out_size);
}

/* Extract integer value from JSON */
int cdp_js_get_int(const char *json_or_response, const char *field, int *out) {
    if (!json_or_response || !field || !out) return -1;

    const char *json = find_json_body(json_or_response);
    if (!json) return -1;

    return cdp_json_get_int(json, field, out);
}

/* Extract boolean value from JSON */
int cdp_js_get_bool(const char *json_or_response, const char *field, int *out) {
    if (!json_or_response || !field || !out) return -1;

    const char *json = find_json_body(json_or_response);
    if (!json) return -1;

    return cdp_json_get_bool(json, field, out);
}

/* Extract nested field from JSON */
int cdp_js_get_nested(const char *json_or_response, const char *path, char *out, size_t out_size) {
    if (!json_or_response || !path || !out) return -1;

    const char *json = find_json_body(json_or_response);
    if (!json) return -1;

    return cdp_json_get_nested(json, path, out, out_size);
}

/* Pretty print JSON */
int cdp_js_beautify(const char *json, char *out, size_t out_size) {
    if (!json || !out) return -1;

    const char *json_body = find_json_body(json);
    if (!json_body) return -1;

    return cdp_json_beautify(json_body, out, out_size);
}

/* CDP-specific: Extract target ID */
int cdp_js_get_target_id(const char *response, char *target_id, size_t size) {
    return cdp_js_get_string(response, "targetId", target_id, size);
}

/* CDP-specific: Extract node ID */
int cdp_js_get_node_id(const char *response, int *node_id) {
    return cdp_js_get_int(response, "nodeId", node_id);
}

/* CDP-specific: Extract object ID */
int cdp_js_get_object_id(const char *response, char *object_id, size_t size) {
    return cdp_js_get_string(response, "objectId", object_id, size);
}

/* CDP-specific: Extract WebSocket URL and return just the target part */
int cdp_js_get_websocket_url(const char *response, char *url, size_t size) {
    char full_url[512];
    if (cdp_js_get_string(response, "webSocketDebuggerUrl", full_url, sizeof(full_url)) != 0) {
        return -1;
    }

    // Extract the target ID part from the URL (e.g., "browser/xxx" from "ws://localhost/devtools/browser/xxx")
    const char *devtools = strstr(full_url, "/devtools/");
    if (!devtools) {
        // Return full URL if pattern not found
        strncpy(url, full_url, size - 1);
        url[size - 1] = '\0';
        return 0;
    }

    devtools += 10;  // Skip "/devtools/"
    strncpy(url, devtools, size - 1);
    url[size - 1] = '\0';
    return 0;
}

/* CDP-specific: Extract request ID */
int cdp_js_get_request_id(const char *response, char *request_id, size_t size) {
    return cdp_js_get_string(response, "requestId", request_id, size);
}

/* CDP-specific: Extract URL */
int cdp_js_get_url(const char *response, char *url, size_t size) {
    return cdp_js_get_string(response, "url", url, size);
}

/* CDP-specific: Extract frame ID */
int cdp_js_get_frame_id(const char *response, char *frame_id, size_t size) {
    return cdp_js_get_string(response, "frameId", frame_id, size);
}

/* CDP-specific: Extract execution context ID */
int cdp_js_get_execution_context_id(const char *response, int *context_id) {
    // Try both "id" and "executionContextId"
    if (cdp_js_get_int(response, "executionContextId", context_id) == 0) {
        return 0;
    }
    return cdp_js_get_int(response, "id", context_id);
}

/* Check if response contains error */
int cdp_js_has_error(const char *response) {
    if (!response) return 0;

    const char *json = find_json_body(response);
    if (!json) return 0;

    // Check if "error" field exists
    char error_msg[256];
    if (cdp_js_get_string(response, "error", error_msg, sizeof(error_msg)) == 0) {
        return 1;  // Has error
    }

    // Also check for "error" object with message
    if (cdp_js_get_nested(response, "error.message", error_msg, sizeof(error_msg)) == 0) {
        return 1;  // Has error
    }

    return 0;  // No error
}

/* Find target with specific URL */
int cdp_js_find_target_with_url(const char *response, const char *search_url, char *target_id, size_t size) {
    if (!response || !search_url || !target_id) return -1;

    const char *json = find_json_body(response);
    if (!json) return -1;

    // Use the cdp_quickjs function directly
    return cdp_json_find_target_with_url(json, search_url, target_id, size);
}

/* ========================================================================== */
/*                        JSON Builder Implementation                        */
/* ========================================================================== */

/* Initialize JSON builder */
void cdp_js_builder_init(CDPJSONBuilder *builder) {
    if (!builder) return;
    builder->buffer[0] = '{';
    builder->buffer[1] = '\0';
    builder->pos = 1;
}

/* Add string field to JSON */
void cdp_js_builder_add_string(CDPJSONBuilder *builder, const char *key, const char *value) {
    if (!builder || !key || !value) return;

    // Add comma if not first field
    if (builder->pos > 1) {
        builder->buffer[builder->pos++] = ',';
    }

    // Escape the value
    char escaped[2048];
    int j = 0;
    for (int i = 0; value[i] && j < sizeof(escaped) - 2; i++) {
        if (value[i] == '"') {
            escaped[j++] = '\\';
            escaped[j++] = '"';
        } else if (value[i] == '\\') {
            escaped[j++] = '\\';
            escaped[j++] = '\\';
        } else if (value[i] == '\n') {
            escaped[j++] = '\\';
            escaped[j++] = 'n';
        } else {
            escaped[j++] = value[i];
        }
    }
    escaped[j] = '\0';

    builder->pos += snprintf(builder->buffer + builder->pos,
                            sizeof(builder->buffer) - builder->pos - 2,
                            "\"%s\":\"%s\"", key, escaped);
}

/* Add integer field to JSON */
void cdp_js_builder_add_int(CDPJSONBuilder *builder, const char *key, int value) {
    if (!builder || !key) return;

    if (builder->pos > 1) {
        builder->buffer[builder->pos++] = ',';
    }

    builder->pos += snprintf(builder->buffer + builder->pos,
                            sizeof(builder->buffer) - builder->pos - 2,
                            "\"%s\":%d", key, value);
}

/* Add boolean field to JSON */
void cdp_js_builder_add_bool(CDPJSONBuilder *builder, const char *key, int value) {
    if (!builder || !key) return;

    if (builder->pos > 1) {
        builder->buffer[builder->pos++] = ',';
    }

    builder->pos += snprintf(builder->buffer + builder->pos,
                            sizeof(builder->buffer) - builder->pos - 2,
                            "\"%s\":%s", key, value ? "true" : "false");
}

/* Add raw JSON field */
void cdp_js_builder_add_raw(CDPJSONBuilder *builder, const char *key, const char *raw_json) {
    if (!builder || !key || !raw_json) return;

    if (builder->pos > 1) {
        builder->buffer[builder->pos++] = ',';
    }

    builder->pos += snprintf(builder->buffer + builder->pos,
                            sizeof(builder->buffer) - builder->pos - 2,
                            "\"%s\":%s", key, raw_json);
}

/* Get the built JSON */
const char* cdp_js_builder_get(CDPJSONBuilder *builder) {
    if (!builder) return "{}";
    builder->buffer[builder->pos] = '}';
    builder->buffer[builder->pos + 1] = '\0';
    return builder->buffer;
}

/* ========================================================================== */
/*                    High-level CDP Command Builders                        */
/* ========================================================================== */

/* Build Runtime.evaluate params */
int cdp_js_build_evaluate(const char *expression, int context_id, char *out, size_t size) {
    if (!expression || !out) return -1;

    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_string(&builder, "expression", expression);
    cdp_js_builder_add_bool(&builder, "returnByValue", 0);
    cdp_js_builder_add_bool(&builder, "generatePreview", 1);

    if (context_id > 0) {
        cdp_js_builder_add_int(&builder, "contextId", context_id);
    }

    strncpy(out, cdp_js_builder_get(&builder), size - 1);
    out[size - 1] = '\0';
    return 0;
}

/* Build Runtime.callFunctionOn params */
int cdp_js_build_call_function(const char *object_id, const char *func, const char *args, char *out, size_t size) {
    if (!object_id || !func || !out) return -1;

    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_string(&builder, "objectId", object_id);
    cdp_js_builder_add_string(&builder, "functionDeclaration", func);

    if (args && strlen(args) > 0) {
        cdp_js_builder_add_raw(&builder, "arguments", args);
    }

    cdp_js_builder_add_bool(&builder, "returnByValue", 1);

    strncpy(out, cdp_js_builder_get(&builder), size - 1);
    out[size - 1] = '\0';
    return 0;
}

/* Build Page.navigate params */
int cdp_js_build_navigate(const char *url, char *out, size_t size) {
    if (!url || !out) return -1;

    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_string(&builder, "url", url);

    strncpy(out, cdp_js_builder_get(&builder), size - 1);
    out[size - 1] = '\0';
    return 0;
}

/* Build Page.captureScreenshot params */
int cdp_js_build_screenshot(char *out, size_t size) {
    if (!out) return -1;

    strncpy(out, "{}", size - 1);
    out[size - 1] = '\0';
    return 0;
}

/* Build Fetch.continueRequest params */
int cdp_js_build_fetch_continue(const char *request_id, char *out, size_t size) {
    if (!request_id || !out) return -1;

    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_string(&builder, "requestId", request_id);

    strncpy(out, cdp_js_builder_get(&builder), size - 1);
    out[size - 1] = '\0';
    return 0;
}

/* Build Fetch.fulfillRequest params */
int cdp_js_build_fetch_fulfill(const char *request_id, int status, const char *headers, const char *body, char *out, size_t size) {
    if (!request_id || !out) return -1;

    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_string(&builder, "requestId", request_id);
    cdp_js_builder_add_int(&builder, "responseCode", status);

    if (headers && strlen(headers) > 0) {
        cdp_js_builder_add_raw(&builder, "responseHeaders", headers);
    } else {
        cdp_js_builder_add_raw(&builder, "responseHeaders", "[]");
    }

    if (body && strlen(body) > 0) {
        cdp_js_builder_add_string(&builder, "body", body);
    }

    strncpy(out, cdp_js_builder_get(&builder), size - 1);
    out[size - 1] = '\0';
    return 0;
}

/* Build Fetch.enable params */
int cdp_js_build_fetch_patterns(const char *patterns_array, char *out, size_t size) {
    if (!out) return -1;

    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);

    if (patterns_array && strlen(patterns_array) > 0) {
        cdp_js_builder_add_raw(&builder, "patterns", patterns_array);
    }

    strncpy(out, cdp_js_builder_get(&builder), size - 1);
    out[size - 1] = '\0';
    return 0;
}

/* Build Input.dispatchMouseEvent params */
int cdp_js_build_mouse_event(const char *type, int x, int y, const char *button, char *out, size_t size) {
    if (!type || !out) return -1;

    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_string(&builder, "type", type);
    cdp_js_builder_add_int(&builder, "x", x);
    cdp_js_builder_add_int(&builder, "y", y);

    if (button && strlen(button) > 0) {
        cdp_js_builder_add_string(&builder, "button", button);
        cdp_js_builder_add_int(&builder, "clickCount", 1);
    }

    strncpy(out, cdp_js_builder_get(&builder), size - 1);
    out[size - 1] = '\0';
    return 0;
}

/* ========================================================================== */
/*                     JavaScript Code Templates                             */
/* ========================================================================== */

/* Evaluate Object.keys() */
int cdp_js_eval_object_keys(const char *object_name, char *result, size_t size) {
    if (!object_name || !result) return -1;

    char expr[512];
    snprintf(expr, sizeof(expr),
             "(function(){try{return Object.keys(%s)}catch(e){return []}})()",
             object_name);

    char *res = execute_javascript(expr);
    if (res) {
        strncpy(result, res, size - 1);
        result[size - 1] = '\0';
        return 0;
    }
    return -1;
}

/* Get value from localStorage/sessionStorage */
int cdp_js_eval_storage_get(const char *storage_type, const char *key, char *result, size_t size) {
    if (!storage_type || !key || !result) return -1;

    char expr[1024];
    snprintf(expr, sizeof(expr),
             "(function(){try{return %s.getItem('%s')||''}catch(e){return ''}})()",
             storage_type, key);

    char *res = execute_javascript(expr);
    if (res) {
        strncpy(result, res, size - 1);
        result[size - 1] = '\0';
        return 0;
    }
    return -1;
}

/* Set value in localStorage/sessionStorage */
int cdp_js_eval_storage_set(const char *storage_type, const char *key, const char *value, char *result, size_t size) {
    if (!storage_type || !key || !value || !result) return -1;

    char expr[2048];
    snprintf(expr, sizeof(expr),
             "(function(){try{%s.setItem('%s','%s');return 1}catch(e){return 0}})()",
             storage_type, key, value);

    char *res = execute_javascript(expr);
    if (res) {
        strncpy(result, res, size - 1);
        result[size - 1] = '\0';
        return 0;
    }
    return -1;
}

/* Safe JSON.stringify with error handling */
int cdp_js_eval_safe_json_stringify(const char *expression, char *result, size_t size) {
    if (!expression || !result) return -1;

    char expr[2048];
    snprintf(expr, sizeof(expr),
             "(function(){try{return JSON.stringify(%s)}catch(e){return String(%s)}})()",
             expression, expression);

    char *res = execute_javascript(expr);
    if (res) {
        strncpy(result, res, size - 1);
        result[size - 1] = '\0';
        return 0;
    }
    return -1;
}

/* ========================================================================== */
/*                        Response Builder Functions                         */
/* ========================================================================== */

/* Build success response */
int cdp_js_build_success_response(const char *command, const char *result, char *out, size_t size) {
    if (!out) return -1;

    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_bool(&builder, "success", 1);

    if (command && strlen(command) > 0) {
        cdp_js_builder_add_string(&builder, "command", command);
    }

    if (result && strlen(result) > 0) {
        cdp_js_builder_add_string(&builder, "result", result);
    }

    strncpy(out, cdp_js_builder_get(&builder), size - 1);
    out[size - 1] = '\0';
    return 0;
}

/* Build error response */
int cdp_js_build_error_response(const char *error_msg, const char *details, char *out, size_t size) {
    if (!error_msg || !out) return -1;

    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_string(&builder, "error", error_msg);

    if (details && strlen(details) > 0) {
        cdp_js_builder_add_string(&builder, "details", details);
    }

    strncpy(out, cdp_js_builder_get(&builder), size - 1);
    out[size - 1] = '\0';
    return 0;
}

/* Build simple key-value response */
int cdp_js_build_simple_response(const char *key, const char *value, char *out, size_t size) {
    if (!key || !value || !out) return -1;

    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_string(&builder, key, value);

    strncpy(out, cdp_js_builder_get(&builder), size - 1);
    out[size - 1] = '\0';
    return 0;
}
