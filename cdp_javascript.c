/**
 * CDP JavaScript Module
 * Handles JavaScript execution and result parsing
 */

#include "cdp_internal.h"
#include <sys/select.h>
#include <errno.h>

/* External global variables */
extern CDPContext g_ctx;
extern int verbose;
extern int ws_sock;
extern int ws_cmd_id;

/* Helper macros */
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
            return ret;  // Success
        }
        
        // Failed, try to reconnect
        DEBUG_LOG("Send failed, attempting reconnect (attempt %d/%d)", attempts + 1, max_attempts);
        close(ws_sock);
        ws_sock = -1;
        attempts++;
        
        if (attempts < max_attempts) {
            usleep(100000 * attempts);  // Progressive delay
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
        
        // Check if this is the response we're looking for
        char id_pattern[32];
        snprintf(id_pattern, sizeof(id_pattern), "\"id\":%d", cmd_id);
        
        if (strstr(buffer, id_pattern)) {
            return len;  // Found our response
        }
        
        // Not our response, continue looking
        DEBUG_LOG("Skipping message %d (looking for id:%d)", messages_checked, cmd_id);
    }
    
    DEBUG_LOG("Response with id:%d not found after %d messages", cmd_id, max_messages);
    return -1;
}

/* Parse JavaScript result from CDP response */
char* parse_javascript_result(const char *buffer, char *result, size_t result_size) {
    if (!buffer || !result || result_size == 0) {
        return NULL;
    }
    
    result[0] = '\0';
    
    // Check for error first
    if (strstr(buffer, "\"error\"")) {
        const char *error_msg = strstr(buffer, "\"message\":\"");
        if (error_msg) {
            error_msg += 11;
            const char *end = strchr(error_msg, '"');
            if (end) {
                size_t len = end - error_msg;
                if (len < result_size - 1) {
                    strncpy(result, error_msg, min(len, result_size - 1));
                    result[min(len, result_size - 1)] = '\0';
                    return result;
                }
            }
        }
        str_copy_safe(result, "Error: Command failed", result_size);
        return result;
    }
    
    // Look for exception details
    const char *exception = strstr(buffer, "\"exceptionDetails\"");
    if (exception) {
        const char *text = strstr(exception, "\"text\":\"");
        if (text) {
            text += 8;
            const char *end = strchr(text, '"');
            if (end) {
                size_t len = end - text;
                if (len < result_size - 7) {
                    str_copy_safe(result, "Error: ", result_size);
                    size_t prefix_len = strlen(result);
                    if (prefix_len < result_size - 1) {
                        size_t safe_len = len < (result_size - prefix_len - 1) ? len : (result_size - prefix_len - 1);
                        strncat(result, text, safe_len);
                    }
                    return result;
                }
            }
        }
        
        // Try to get exception description
        const char *desc = strstr(exception, "\"description\":\"");
        if (desc) {
            desc += 15;
            const char *end = strchr(desc, '"');
            if (end) {
                size_t len = end - desc;
                if (len < result_size - 1) {
                    size_t safe_len = len < (result_size - 1) ? len : (result_size - 1);
                    strncpy(result, desc, safe_len);
                    result[safe_len] = '\0';
                    return result;
                }
            }
        }
        
        str_copy_safe(result, "Error: JavaScript exception", result_size);
        return result;
    }
    
    // Parse successful result
    const char *result_start = strstr(buffer, "\"result\":{");
    if (!result_start) {
        return NULL;
    }
    
    // Check result type
    const char *type = strstr(result_start, "\"type\":\"");
    if (!type) {
        return NULL;
    }
    type += 8;
    
    // Handle different types
    if (strncmp(type, "undefined", 9) == 0) {
        str_copy_safe(result, "undefined", result_size);
    } else if (strncmp(type, "string", 6) == 0) {
        const char *value = strstr(result_start, "\"value\":\"");
        if (value) {
            value += 9;
            const char *end = value;
            int escape = 0;
            
            // Find the end of the string, handling escapes
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
                if (len < result_size - 1) {
                    // Copy and handle escape sequences
                    size_t j = 0;
                    for (size_t i = 0; i < len && j < result_size - 1; i++) {
                        if (value[i] == '\\' && i + 1 < len) {
                            switch (value[i + 1]) {
                                case 'n': result[j++] = '\n'; i++; break;
                                case 't': result[j++] = '\t'; i++; break;
                                case 'r': result[j++] = '\r'; i++; break;
                                case '\\': result[j++] = '\\'; i++; break;
                                case '"': result[j++] = '"'; i++; break;
                                default: result[j++] = value[i]; break;
                            }
                        } else {
                            result[j++] = value[i];
                        }
                    }
                    result[j] = '\0';
                }
            }
        }
    } else if (strncmp(type, "number", 6) == 0) {
        const char *value = strstr(result_start, "\"value\":");
        if (value) {
            value += 8;
            char *end;
            double num = strtod(value, &end);
            
            // Check if it's an integer or float
            if (num == (long long)num) {
                snprintf(result, result_size, "%lld", (long long)num);
            } else {
                snprintf(result, result_size, "%g", num);
            }
        }
    } else if (strncmp(type, "boolean", 7) == 0) {
        const char *value = strstr(result_start, "\"value\":");
        if (value) {
            value += 8;
            if (strncmp(value, "true", 4) == 0) {
                str_copy_safe(result, "true", result_size);
            } else {
                str_copy_safe(result, "false", result_size);
            }
        }
    } else if (strncmp(type, "object", 6) == 0) {
        // Check for null
        const char *subtype = strstr(result_start, "\"subtype\":\"");
        if (subtype && strncmp(subtype + 11, "null", 4) == 0) {
            str_copy_safe(result, "null", result_size);
        } else {
            // For arrays and objects, try to get the description
            const char *desc = strstr(result_start, "\"description\":\"");
            if (desc) {
                desc += 15;
                const char *end = strchr(desc, '"');
                if (end) {
                    size_t len = end - desc;
                    if (len < result_size - 1) {
                        strncpy(result, desc, len);
                        result[len] = '\0';
                    }
                }
            } else {
                // Try to get className
                const char *className = strstr(result_start, "\"className\":\"");
                if (className) {
                    className += 13;
                    const char *end = strchr(className, '"');
                    if (end) {
                        size_t len = end - className;
                        if (len < result_size - 10) {
                            strcpy(result, "[object ");
                            strncat(result, className, len);
                            strcat(result, "]");
                        }
                    }
                } else {
                    str_copy_safe(result, "[object Object]", result_size);
                }
            }
        }
    } else if (strncmp(type, "function", 8) == 0) {
        const char *desc = strstr(result_start, "\"description\":\"");
        if (desc) {
            desc += 15;
            const char *end = strchr(desc, '"');
            if (end) {
                size_t len = end - desc;
                if (len < result_size - 1) {
                    size_t safe_len = len < (result_size - 1) ? len : (result_size - 1);
                    strncpy(result, desc, safe_len);
                    result[safe_len] = '\0';
                }
            }
        } else {
            str_copy_safe(result, "[Function]", result_size);
        }
    } else {
        // Unknown type, try to get any value
        const char *value = strstr(result_start, "\"value\":");
        if (value) {
            value += 8;
            if (*value == '"') {
                value++;
                const char *end = strchr(value, '"');
                if (end) {
                    size_t len = end - value;
                    if (len < result_size - 1) {
                        strncpy(result, value, len);
                        result[len] = '\0';
                    }
                }
            } else {
                // Non-string value
                char *end;
                while (*value == ' ') value++;
                end = (char*)value;
                while (*end && *end != ',' && *end != '}') end++;
                size_t len = end - value;
                if (len < result_size - 1) {
                    strncpy(result, value, len);
                    result[len] = '\0';
                }
            }
        }
    }
    
    return result[0] ? result : NULL;
}

/* Execute JavaScript in Chrome via WebSocket */
char* execute_javascript(const char *expression) {
    static char result[MAX_RESULT_SIZE];
    result[0] = '\0';
    
    if (!expression || !*expression) {
        return result;
    }
    
    // Check if we're on browser endpoint (doesn't support Runtime.evaluate)
    if (strstr(g_ctx.conn.target_id, "browser/")) {
        str_copy_safe(result, "Browser endpoint doesn't support JavaScript execution. Use CDP commands like Target.createTarget instead.", sizeof(result));
        return result;
    }
    
    // Ensure WebSocket is connected
    if (ws_sock < 0) {
        if (reconnect_websocket_with_backoff() < 0) {
            str_copy_safe(result, "Error: Not connected to Chrome", sizeof(result));
            return result;
        }
    }
    
    // Escape the JavaScript expression for JSON
    char *escaped_expr = (char*)safe_malloc(strlen(expression) * 2 + 1);
    if (!escaped_expr) {
        str_copy_safe(result, "Error: Memory allocation failed", sizeof(result));
        return result;
    }
    json_escape_safe(escaped_expr, expression, strlen(expression) * 2 + 1);
    
    // Build Runtime.evaluate command
    char *command = (char*)safe_malloc(strlen(escaped_expr) + 256);
    if (!command) {
        safe_free((void**)&escaped_expr);
        str_copy_safe(result, "Error: Memory allocation failed", sizeof(result));
        return result;
    }
    
    int cmd_id = ws_cmd_id++;
    snprintf(command, strlen(escaped_expr) + 256,
             "{\"id\":%d,\"method\":\"Runtime.evaluate\",\"params\":{\"expression\":\"%s\",\"returnByValue\":true}}",
             cmd_id, escaped_expr);
    
    safe_free((void**)&escaped_expr);
    
    // Send command
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
    
    // Parse the result
    if (!parse_javascript_result(buffer, result, sizeof(result))) {
        // If parsing failed, check for raw error message
        if (strstr(buffer, "\"error\"")) {
            str_copy_safe(result, "Error: Command failed", sizeof(result));
        } else {
            result[0] = '\0';  // Empty result for undefined
        }
    }
    
    return result;
}