/**
 * CDP Utility Functions Implementation
 * Common utilities for JSON, WebSocket, and string operations
 */

#include "cdp_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

/* JSON Utilities */

void json_escape_safe(char *dst, const char *src, size_t dst_size) {
    if (!dst || !src || dst_size == 0) return;
    
    size_t j = 0;
    size_t src_len = strlen(src);
    
    for (size_t i = 0; i < src_len && j < dst_size - 2; i++) {
        switch (src[i]) {
            case '"':
                if (j + 2 < dst_size) {
                    dst[j++] = '\\';
                    dst[j++] = '"';
                }
                break;
            case '\\':
                if (j + 2 < dst_size) {
                    dst[j++] = '\\';
                    dst[j++] = '\\';
                }
                break;
            case '\n':
                if (j + 2 < dst_size) {
                    dst[j++] = '\\';
                    dst[j++] = 'n';
                }
                break;
            case '\r':
                if (j + 2 < dst_size) {
                    dst[j++] = '\\';
                    dst[j++] = 'r';
                }
                break;
            case '\t':
                if (j + 2 < dst_size) {
                    dst[j++] = '\\';
                    dst[j++] = 't';
                }
                break;
            default:
                if (src[i] >= 32 && src[i] < 127) {
                    if (j < dst_size - 1) {
                        dst[j++] = src[i];
                    }
                }
                break;
        }
    }
    dst[j] = '\0';
}

char* json_get_string(const char *json, const char *key, char *value, size_t value_size) {
    if (!json || !key || !value || value_size == 0) return NULL;
    
    value[0] = '\0';
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    const char *key_pos = strstr(json, search_key);
    if (!key_pos) {
        // Try without space after colon
        snprintf(search_key, sizeof(search_key), "\"%s\":", key);
        key_pos = strstr(json, search_key);
        if (!key_pos) return NULL;
    }
    
    const char *value_start = key_pos + strlen(search_key);
    while (*value_start == ' ') value_start++;
    
    if (*value_start == '"') {
        value_start++;
        const char *value_end = strchr(value_start, '"');
        while (value_end && *(value_end - 1) == '\\') {
            value_end = strchr(value_end + 1, '"');
        }
        
        if (value_end) {
            size_t len = value_end - value_start;
            if (len < value_size) {
                strncpy(value, value_start, len);
                value[len] = '\0';
                return value;
            }
        }
    }
    
    return NULL;
}

int json_get_int(const char *json, const char *key) {
    if (!json || !key) return 0;
    
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    const char *key_pos = strstr(json, search_key);
    if (!key_pos) return 0;
    
    const char *value_start = key_pos + strlen(search_key);
    while (*value_start == ' ') value_start++;
    
    return atoi(value_start);
}

char* json_extract_value(const char *json, const char *path, char *value, size_t value_size) {
    // Simple JSON path extractor (e.g., "result.value")
    if (!json || !path || !value || value_size == 0) return NULL;
    
    char *path_copy = strdup(path);
    if (!path_copy) return NULL;
    
    const char *current = json;
    char *token = strtok(path_copy, ".");
    
    while (token) {
        char temp_value[4096];
        if (!json_get_string(current, token, temp_value, sizeof(temp_value))) {
            free(path_copy);
            return NULL;
        }
        current = temp_value;
        token = strtok(NULL, ".");
    }
    
    str_copy_safe(value, current, value_size);
    free(path_copy);
    return value;
}

/* WebSocket Utilities */

void ws_base64_encode(const unsigned char *input, int length, char *output, size_t output_size) {
    if (!input || !output || output_size == 0) return;
    
    const char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i = 0, j = 0;
    
    while (i < length && j < output_size - 4) {
        uint32_t octet_a = i < length ? input[i++] : 0;
        uint32_t octet_b = i < length ? input[i++] : 0;
        uint32_t octet_c = i < length ? input[i++] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        
        output[j++] = chars[(triple >> 18) & 0x3F];
        output[j++] = chars[(triple >> 12) & 0x3F];
        output[j++] = i > length + 1 ? '=' : chars[(triple >> 6) & 0x3F];
        output[j++] = i > length ? '=' : chars[triple & 0x3F];
    }
    output[j] = '\0';
}

int ws_send_text_frame(int sock, const char *text, size_t text_len) {
    if (sock < 0 || !text) return -1;
    
    if (text_len == 0) text_len = strlen(text);
    
    uint8_t *frame = malloc(10 + text_len);
    if (!frame) return -1;
    
    int frame_size = 0;
    
    // FIN=1, Opcode=1 (text)
    frame[0] = 0x81;
    
    // Mask=1, payload length
    if (text_len < 126) {
        frame[1] = 0x80 | text_len;
        frame_size = 2;
    } else if (text_len < 65536) {
        frame[1] = 0x80 | 126;
        frame[2] = (text_len >> 8) & 0xFF;
        frame[3] = text_len & 0xFF;
        frame_size = 4;
    } else {
        free(frame);
        return -1;
    }
    
    // Masking key
    uint8_t mask[4];
    for (int i = 0; i < 4; i++) {
        mask[i] = rand() & 0xFF;
        frame[frame_size + i] = mask[i];
    }
    frame_size += 4;
    
    // Masked payload
    for (size_t i = 0; i < text_len; i++) {
        frame[frame_size + i] = text[i] ^ mask[i % 4];
    }
    frame_size += text_len;
    
    int ret = send(sock, frame, frame_size, 0);
    free(frame);
    return ret;
}

int ws_recv_text_frame(int sock, char *buffer, size_t max_len, int timeout_ms) {
    if (sock < 0 || !buffer || max_len == 0) return -1;
    
    // Set timeout
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    
    uint8_t header[2];
    int ret = recv(sock, header, 2, 0);
    if (ret != 2) return -1;
    
    uint8_t opcode = header[0] & 0x0F;
    uint8_t mask = (header[1] >> 7) & 1;
    uint64_t payload_len = header[1] & 0x7F;
    
    // Extended payload length
    if (payload_len == 126) {
        uint8_t ext[2];
        if (recv(sock, ext, 2, 0) != 2) return -1;
        payload_len = (ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        if (recv(sock, ext, 8, 0) != 8) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | ext[i];
        }
    }
    
    if (payload_len >= max_len) return -1;
    
    // Masking key
    uint8_t mask_key[4] = {0};
    if (mask) {
        if (recv(sock, mask_key, 4, 0) != 4) return -1;
    }
    
    // Payload
    size_t total_read = 0;
    while (total_read < payload_len) {
        ssize_t bytes = recv(sock, buffer + total_read, payload_len - total_read, 0);
        if (bytes <= 0) return -1;
        total_read += bytes;
    }
    
    // Unmask
    if (mask) {
        for (uint64_t i = 0; i < payload_len; i++) {
            buffer[i] ^= mask_key[i % 4];
        }
    }
    
    buffer[payload_len] = '\0';
    
    // Handle ping
    if (opcode == 0x9) {
        ws_send_pong(sock);
        return 0;
    }
    
    return payload_len;
}

int ws_send_pong(int sock) {
    uint8_t pong[2] = {0x8A, 0x00};
    return send(sock, pong, 2, 0);
}

int ws_parse_frame_header(const uint8_t *header, uint8_t *opcode, uint64_t *payload_len) {
    if (!header || !opcode || !payload_len) return -1;
    
    *opcode = header[0] & 0x0F;
    *payload_len = header[1] & 0x7F;
    
    if (*payload_len == 126) return 2;  // Need 2 more bytes
    if (*payload_len == 127) return 8;  // Need 8 more bytes
    return 0;  // No extended length needed
}

/* String Utilities */

size_t str_copy_safe(char *dst, const char *src, size_t dst_size) {
    if (!dst || !src || dst_size == 0) return 0;
    
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dst_size - 1) ? src_len : dst_size - 1;
    
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
    
    return copy_len;
}

size_t str_append_safe(char *dst, const char *src, size_t dst_size) {
    if (!dst || !src || dst_size == 0) return 0;
    
    size_t dst_len = strlen(dst);
    if (dst_len >= dst_size - 1) return dst_len;
    
    size_t available = dst_size - dst_len - 1;
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < available) ? src_len : available;
    
    memcpy(dst + dst_len, src, copy_len);
    dst[dst_len + copy_len] = '\0';
    
    return dst_len + copy_len;
}

int str_trim(char *str) {
    if (!str) return 0;
    
    // Trim leading whitespace
    char *start = str;
    while (*start && isspace(*start)) start++;
    
    // Trim trailing whitespace
    char *end = str + strlen(str) - 1;
    while (end > start && isspace(*end)) end--;
    
    // Move trimmed string
    size_t len = end - start + 1;
    if (start != str) {
        memmove(str, start, len);
    }
    str[len] = '\0';
    
    return len;
}

char* str_find_token(const char *str, const char *token) {
    if (!str || !token) return NULL;
    return strstr(str, token);
}

int str_replace_char(char *str, char old_char, char new_char) {
    if (!str) return 0;
    
    int count = 0;
    for (char *p = str; *p; p++) {
        if (*p == old_char) {
            *p = new_char;
            count++;
        }
    }
    return count;
}

/* Memory Utilities */

void* safe_malloc(size_t size) {
    if (size == 0) return NULL;
    
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Memory allocation failed for %zu bytes\n", size);
    }
    return ptr;
}

void* safe_realloc(void *ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        fprintf(stderr, "Memory reallocation failed for %zu bytes\n", size);
    }
    return new_ptr;
}

void safe_free(void **ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

/* System Utilities */

double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int calculate_backoff_delay(int attempt, int base_delay_ms, double backoff_factor, int max_delay_ms) {
    if (attempt <= 0) return base_delay_ms;
    
    int delay = base_delay_ms;
    for (int i = 0; i < attempt; i++) {
        delay = (int)(delay * backoff_factor);
        if (delay > max_delay_ms) {
            delay = max_delay_ms;
            break;
        }
    }
    return delay;
}

void wait_with_backoff(int attempt, int base_delay_ms, double backoff_factor, int max_delay_ms) {
    int delay_ms = calculate_backoff_delay(attempt, base_delay_ms, backoff_factor, max_delay_ms);
    usleep(delay_ms * 1000);
}

/* CDP Message Utilities */

CDPMessage* cdp_message_new(const char *method) {
    CDPMessage* msg = (CDPMessage*)safe_malloc(sizeof(CDPMessage));
    if (!msg) return NULL;
    
    static int message_id = 1;
    msg->id = message_id++;
    str_copy_safe(msg->method, method, sizeof(msg->method));
    msg->params[0] = '\0';
    msg->params_added = 0;
    return msg;
}

void cdp_message_add_param(CDPMessage* msg, const char* key, const char* value) {
    if (!msg || !key || !value) return;
    
    char escaped[1024];
    json_escape_safe(escaped, value, sizeof(escaped));
    
    if (msg->params_added > 0) {
        str_append_safe(msg->params, ",", sizeof(msg->params));
    }
    
    char param[1280];
    snprintf(param, sizeof(param), "\"%s\":\"%s\"", key, escaped);
    str_append_safe(msg->params, param, sizeof(msg->params));
    msg->params_added++;
}

void cdp_message_add_param_int(CDPMessage* msg, const char* key, int value) {
    if (!msg || !key) return;
    
    if (msg->params_added > 0) {
        str_append_safe(msg->params, ",", sizeof(msg->params));
    }
    
    char param[256];
    snprintf(param, sizeof(param), "\"%s\":%d", key, value);
    str_append_safe(msg->params, param, sizeof(msg->params));
    msg->params_added++;
}

void cdp_message_add_param_bool(CDPMessage* msg, const char* key, int value) {
    if (!msg || !key) return;
    
    if (msg->params_added > 0) {
        str_append_safe(msg->params, ",", sizeof(msg->params));
    }
    
    char param[256];
    snprintf(param, sizeof(param), "\"%s\":%s", key, value ? "true" : "false");
    str_append_safe(msg->params, param, sizeof(msg->params));
    msg->params_added++;
}

char* cdp_message_build(CDPMessage* msg) {
    if (!msg) return NULL;
    
    static char buffer[8192];
    if (msg->params_added > 0) {
        snprintf(buffer, sizeof(buffer), "{\"id\":%d,\"method\":\"%s\",\"params\":{%s}}", 
                 msg->id, msg->method, msg->params);
    } else {
        snprintf(buffer, sizeof(buffer), "{\"id\":%d,\"method\":\"%s\"}", 
                 msg->id, msg->method);
    }
    return buffer;
}

void cdp_message_free(CDPMessage* msg) {
    if (msg) {
        safe_free((void**)&msg);
    }
}

/* JSON Command Parsing */

int parse_json_command(const char *json, int *id, char *cmd, size_t cmd_size) {
    *id = 0;
    cmd[0] = '\0';
    
    // Simple JSON parser
    const char *id_ptr = strstr(json, "\"id\"");
    const char *cmd_ptr = strstr(json, "\"cmd\"");
    
    if (id_ptr && cmd_ptr) {
        // Parse ID
        id_ptr = strchr(id_ptr, ':');
        if (id_ptr) {
            *id = atoi(id_ptr + 1);
        }
        
        // Parse command
        cmd_ptr = strchr(cmd_ptr, ':');
        if (cmd_ptr) {
            cmd_ptr = strchr(cmd_ptr, '\"');
            if (cmd_ptr) {
                cmd_ptr++;
                const char *end = strchr(cmd_ptr, '\"');
                if (end) {
                    size_t len = end - cmd_ptr;
                    if (len < cmd_size) {
                        strncpy(cmd, cmd_ptr, len);
                        cmd[len] = '\0';
                        return 1;
                    }
                }
            }
        }
    }
    
    // Fallback: treat entire input as command if not JSON
    if (strlen(json) > 0 && json[0] != '{') {
        strncpy(cmd, json, cmd_size - 1);
        cmd[cmd_size - 1] = '\0';
        *id = rand() % 10000;
        return 1;
    }
    
    return 0;
}