/**
 * CDP JavaScript Module - Correct Two-Phase Solution
 * Phase 1: Execute expression directly
 * Phase 2: Convert to string if needed
 */

#include "cdp_internal.h"
#include <sys/select.h>
#include <errno.h>

extern CDPContext g_ctx;
extern int verbose;
extern int ws_sock;
extern int ws_cmd_id;

#define min(a, b) ((a) < (b) ? (a) : (b))

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
    int messages_checked = 0;
    
    while (messages_checked < max_messages) {
        int len = ws_recv_text(ws_sock, buffer, max_len);
        if (len <= 0) {
            return len;
        }
        
        messages_checked++;
        
        char id_pattern[32];
        snprintf(id_pattern, sizeof(id_pattern), "\"id\":%d", cmd_id);
        
        if (strstr(buffer, id_pattern)) {
            return len;
        }
        
        DEBUG_LOG("Skipping message %d (looking for id:%d)", messages_checked, cmd_id);
    }
    
    DEBUG_LOG("Response with id:%d not found after %d messages", cmd_id, max_messages);
    return -1;
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
    
    // Check for error
    if (strstr(buffer, "\"error\"") || strstr(buffer, "\"exceptionDetails\"")) {
        const char *msg = strstr(buffer, "\"message\":\"");
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
    const char *result_obj = strstr(buffer, "\"result\":{");
    if (!result_obj) {
        strcpy(info->value, "No result");
        return -1;
    }
    
    // Extract type
    const char *type_str = strstr(result_obj, "\"type\":\"");
    if (type_str) {
        type_str += 8;
        const char *end = strchr(type_str, '"');
        if (end) {
            size_t len = min(end - type_str, sizeof(info->type) - 1);
            strncpy(info->type, type_str, len);
            info->type[len] = '\0';
        }
    }
    
    // Extract subtype if present
    const char *subtype_str = strstr(result_obj, "\"subtype\":\"");
    if (subtype_str) {
        subtype_str += 11;
        const char *end = strchr(subtype_str, '"');
        if (end) {
            size_t len = min(end - subtype_str, sizeof(info->subtype) - 1);
            strncpy(info->subtype, subtype_str, len);
            info->subtype[len] = '\0';
        }
    }
    
    // Extract className if present
    const char *class_str = strstr(result_obj, "\"className\":\"");
    if (class_str) {
        class_str += 13;
        const char *end = strchr(class_str, '"');
        if (end) {
            size_t len = min(end - class_str, sizeof(info->className) - 1);
            strncpy(info->className, class_str, len);
            info->className[len] = '\0';
        }
    }
    
    // Check for Promise/async
    if (strcmp(info->className, "Promise") == 0 || strstr(info->subtype, "promise")) {
        info->isAsync = 1;
    }
    
    // Extract objectId if present
    const char *obj_id = strstr(result_obj, "\"objectId\":\"");
    if (obj_id) {
        obj_id += 12;
        const char *end = strchr(obj_id, '"');
        if (end) {
            size_t len = min(end - obj_id, sizeof(info->objectId) - 1);
            strncpy(info->objectId, obj_id, len);
            info->objectId[len] = '\0';
        }
    }
    
    // Determine if we need to stringify
    if (strcmp(info->type, "object") == 0 || 
        strcmp(info->type, "function") == 0) {
        info->needsStringify = 1;
    }
    
    // Extract value for primitive types
    if (strcmp(info->type, "string") == 0) {
        const char *value = strstr(result_obj, "\"value\":\"");
        if (value) {
            value += 9;
            const char *end = value;
            int escape = 0;
            
            while (*end) {
                if (*end == '\\' && !escape) {
                    escape = 1;
                } else if (*end == '"' && !escape) {
                    break;
                } else {
                    escape = 0;
                }
                end++;
            }
            
            if (*end == '"') {
                size_t len = end - value;
                if (len < MAX_RESULT_SIZE) {
                    size_t j = 0;
                    for (size_t i = 0; i < len && j < MAX_RESULT_SIZE - 1; i++) {
                        if (value[i] == '\\' && i + 1 < len) {
                            switch (value[i + 1]) {
                                case 'n': info->value[j++] = '\n'; i++; break;
                                case 't': info->value[j++] = '\t'; i++; break;
                                case 'r': info->value[j++] = '\r'; i++; break;
                                case '\\': info->value[j++] = '\\'; i++; break;
                                case '"': info->value[j++] = '"'; i++; break;
                                case 'u': // Unicode
                                    if (i + 5 < len) {
                                        i += 5;
                                    }
                                    break;
                                default: info->value[j++] = value[i]; break;
                            }
                        } else {
                            info->value[j++] = value[i];
                        }
                    }
                    info->value[j] = '\0';
                }
            }
        }
    } else if (strcmp(info->type, "number") == 0) {
        const char *value = strstr(result_obj, "\"value\":");
        if (value) {
            value += 8;
            char *end;
            double num = strtod(value, &end);
            if (end != value) {
                if (num == (long long)num) {
                    snprintf(info->value, MAX_RESULT_SIZE, "%lld", (long long)num);
                } else {
                    snprintf(info->value, MAX_RESULT_SIZE, "%g", num);
                }
            }
        }
    } else if (strcmp(info->type, "boolean") == 0) {
        const char *value = strstr(result_obj, "\"value\":");
        if (value) {
            value += 8;
            if (strncmp(value, "true", 4) == 0) {
                strcpy(info->value, "true");
            } else {
                strcpy(info->value, "false");
            }
        }
    } else if (strcmp(info->type, "undefined") == 0) {
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
             "{\"id\":%d,\"method\":\"Runtime.awaitPromise\","
             "\"params\":{\"promiseObjectId\":\"%s\",\"returnByValue\":true}}",
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
             "{\"id\":%d,\"method\":\"Runtime.callFunctionOn\","
             "\"params\":{\"objectId\":\"%s\","
             "\"functionDeclaration\":\"function() { try { return JSON.stringify(this); } catch(e) { return String(this); } }\","
             "\"returnByValue\":true}}",
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
    snprintf(command, strlen(escaped) + 256,
             "{\"id\":%d,\"method\":\"Runtime.evaluate\","
             "\"params\":{\"expression\":\"%s\",\"returnByValue\":false,\"generatePreview\":true}}",
             cmd_id, escaped);
    
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