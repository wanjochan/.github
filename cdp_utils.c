/**
 * CDP Utility Functions Implementation
 * Common utilities for JSON, WebSocket, and string operations
 */

#include "cdp_utils.h"
#include "cdp_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/utsname.h>
/* #include "cdp_log.h" - merged into cdp_internal.h */

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
    snprintf(search_key, sizeof(search_key), JKEY_FMT, key);
    
    const char *key_pos = strstr(json, search_key);
    if (!key_pos) {
        // Try without space after colon
        snprintf(search_key, sizeof(search_key), JKEY_FMT, key);
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
    snprintf(search_key, sizeof(search_key), JKEY_FMT, key);
    
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

/* ========================================================================== */
/* CDP CLI MODULE                                            */
/* ========================================================================== */

/* Simple CLI protocol handler */
int cdp_handle_cli_protocol(const char* url, char* response, size_t response_size) {
    if (!url || !response || response_size == 0) {
        return -1;
    }
    
    /* Extract command from cli://command?params */
    if (strncmp(url, "cli://", 6) != 0) {
        snprintf(response, response_size, QUOTE({"error": "Invalid CLI protocol URL"}));
        return -1;
    }
    
    const char* cmd_start = url + 6;
    const char* params_start = strchr(cmd_start, '?');
    
    char command[1024];
    if (params_start) {
        size_t cmd_len = params_start - cmd_start;
        if (cmd_len >= sizeof(command)) cmd_len = sizeof(command) - 1;
        strncpy(command, cmd_start, cmd_len);
        command[cmd_len] = '\0';
    } else {
        strncpy(command, cmd_start, sizeof(command) - 1);
        command[sizeof(command) - 1] = '\0';
    }
    
    /* Basic security: whitelist common safe commands */
    if (strncmp(command, "ls", 2) == 0 || 
        strncmp(command, "pwd", 3) == 0 ||
        strncmp(command, "echo", 4) == 0 ||
        strncmp(command, "date", 4) == 0 ||
        strncmp(command, "whoami", 6) == 0) {
        
        /* Execute command safely */
        FILE* pipe = popen(command, "r");
        if (!pipe) {
            snprintf(response, response_size, QUOTE({"error": "Command execution failed"}));
            return -1;
        }
        
        char output[4096] = {0};
        fread(output, 1, sizeof(output) - 1, pipe);
        int exit_code = pclose(pipe);
        
        /* Remove trailing newline */
        size_t len = strlen(output);
        if (len > 0 && output[len-1] == '\n') {
            output[len-1] = '\0';
        }
        
        snprintf(response, response_size, QUOTE({"ok": true, "output": "%s", "exit_code": %d}), 
                output, WEXITSTATUS(exit_code));
        return 0;
    }
    
    snprintf(response, response_size, QUOTE({"error": "Command not allowed"}));
    return -1;
}

/* Initialize CLI protocol service */
int cdp_init_cli_module(void) {
    /* No complex initialization needed - just protocol handling */
    return 0;
}

/* Cleanup CLI protocol service */
int cdp_cleanup_cli_module(void) {
    /* No cleanup needed for simple protocol handler */
    return 0;
}

/* File exists check - minimal filesystem function */
int cdp_validate_file_exists(const char* file_path) {
    if (!file_path) return 0;
    return access(file_path, F_OK) == 0 ? 1 : 0;
}

/* Compatibility stubs for removed filesystem functions */
const char* cdp_file_error_to_string(int error_code) {
    return error_code == 0 ? "Success" : "Error";
}

int cdp_start_download_monitor(const char* dir) {
    /* Simplified: just return success - no complex monitoring needed */
    return 0;
}

/* ========================================================================== */
/* CDP CONFIG MODULE (from cdp_config.c)                                     */
/* ========================================================================== */

void cdp_config_apply_defaults(CDPContext *ctx) {
    if (!ctx) return;
    if (!ctx->config.server_host) ctx->config.server_host = "127.0.0.1";
    if (!ctx->config.chrome_host) ctx->config.chrome_host = "127.0.0.1";
    if (ctx->config.debug_port <= 0) ctx->config.debug_port = CHROME_DEFAULT_PORT;
    if (ctx->config.timeout_ms <= 0) ctx->config.timeout_ms = DEFAULT_TIMEOUT_MS;
}

void cdp_config_dump(const CDPContext *ctx) {
    if (!ctx) return;
    cdp_log(CDP_LOG_INFO, "CONFIG", "Config:");
    cdp_log(CDP_LOG_INFO, "CONFIG", "  Chrome Host: %s", ctx->config.chrome_host);
    cdp_log(CDP_LOG_INFO, "CONFIG", "  Debug Port : %d", ctx->config.debug_port);
    cdp_log(CDP_LOG_INFO, "CONFIG", "  Timeout(ms): %d", ctx->config.timeout_ms);
}

/* ========================================================================== */
/* CDP CONNECTION MODULE (from cdp_conn.c)                                   */
/* ========================================================================== */

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

/* ========================================================================== */
/* CDP LOG MODULE (from cdp_log.c)                                           */
/* ========================================================================== */

void cdp_log(cdp_log_level_t level, const char *module, const char *fmt, ...) {
    if (!verbose && level == CDP_LOG_DEBUG) return;
    const char *lvl = level==CDP_LOG_DEBUG?"DEBUG": level==CDP_LOG_INFO?"INFO": level==CDP_LOG_WARN?"WARN":"ERR";
    fprintf(stderr, "[%s]%s%s ", lvl, module?"[":"", module?module:"");
    if (module) fputc(']', stderr);
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
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
        cdp_log(CDP_LOG_ERR, "UTIL", "malloc failed for %zu bytes", size);
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
        cdp_log(CDP_LOG_ERR, "UTIL", "realloc failed for %zu bytes", size);
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
    snprintf(param, sizeof(param), JPAIR_FMT, key, escaped);
    str_append_safe(msg->params, param, sizeof(msg->params));
    msg->params_added++;
}

void cdp_message_add_param_int(CDPMessage* msg, const char* key, int value) {
    if (!msg || !key) return;
    
    if (msg->params_added > 0) {
        str_append_safe(msg->params, ",", sizeof(msg->params));
    }
    
    char param[256];
    snprintf(param, sizeof(param), JNUM_FMT, key, value);
    str_append_safe(msg->params, param, sizeof(msg->params));
    msg->params_added++;
}

void cdp_message_add_param_bool(CDPMessage* msg, const char* key, int value) {
    if (!msg || !key) return;
    
    if (msg->params_added > 0) {
        str_append_safe(msg->params, ",", sizeof(msg->params));
    }
    
    char param[256];
    snprintf(param, sizeof(param), JBOOL_FMT, key, value ? "true" : "false");
    str_append_safe(msg->params, param, sizeof(msg->params));
    msg->params_added++;
}

char* cdp_message_build(CDPMessage* msg) {
    if (!msg) return NULL;
    
    static char buffer[8192];
    if (msg->params_added > 0) {
        snprintf(buffer, sizeof(buffer), QUOTE({"id":%d,"method":"%s","params":{%s}}), 
                 msg->id, msg->method, msg->params);
    } else {
        snprintf(buffer, sizeof(buffer), QUOTE({"id":%d,"method":"%s"}), 
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
    const char *id_ptr = strstr(json, QUOTE("id"));
    const char *cmd_ptr = strstr(json, QUOTE("cmd"));
    
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

/* Detect operating system at runtime */
const char* cdp_detect_os(void) {
    static char os_name[32];
    struct utsname sys_info;

    if (uname(&sys_info) == 0) {
        str_copy_safe(os_name, sys_info.sysname, sizeof(os_name));
        for (int i = 0; os_name[i]; i++) {
            os_name[i] = tolower(os_name[i]);
        }

        if (strstr(os_name, "darwin") || strstr(os_name, "mac")) {
            return "darwin";
        }
        if (strstr(os_name, "linux")) {
            return "linux";
        }
        if (strstr(os_name, "freebsd") || strstr(os_name, "openbsd") ||
            strstr(os_name, "netbsd")) {
            return os_name;
        }
        if (strstr(os_name, "win") || strstr(os_name, "mingw") ||
            strstr(os_name, "cygwin") || strstr(os_name, "msys")) {
            return "windows";
        }
        if (os_name[0]) {
            return os_name;
        }
    }

    if (getenv("WINDIR") || getenv("SYSTEMROOT") || getenv("OS")) {
        const char *os_env = getenv("OS");
        if (os_env) {
            char lowered[64];
            str_copy_safe(lowered, os_env, sizeof(lowered));
            for (int i = 0; lowered[i]; i++) {
                lowered[i] = tolower(lowered[i]);
            }
            if (strstr(lowered, "windows")) {
                return "windows";
            }
        }
        if (getenv("WINDIR") || getenv("SYSTEMROOT")) {
            return "windows";
        }
    }

    if (access("C:\\Windows", F_OK) == 0 || access("C:\\", F_OK) == 0) {
        return "windows";
    }

    if (getenv("HOME") && access("/System", F_OK) == 0) {
        return "darwin";
    }
    if (access("/proc", F_OK) == 0) {
        return "linux";
    }

    return "unknown";
}

/* ========================================================================== */
/* CDP AUTHORIZATION MODULE (from cdp_auth.c)                                */
/* ========================================================================== */

static int env_enabled(const char *name) {
    const char *v = getenv(name);
    return (v && (*v=='1' || strcasecmp(v, "true")==0 || strcasecmp(v, "yes")==0));
}

int cdp_authz_allow(const char *action, const char *target) {
    (void)target;
    if (!action) return 0;
    /* Simple policy gates by env flags */
    if (strncmp(action, "system:", 7) == 0 || strncmp(action, "shell:", 6) == 0) {
        return env_enabled("CDP_ALLOW_SYSTEM");
    }
    if (strncmp(action, "file:", 5) == 0) {
        return env_enabled("CDP_ALLOW_FILE");
    }
    if (strncmp(action, "notify:", 7) == 0) {
        return env_enabled("CDP_ALLOW_NOTIFY") || env_enabled("CDP_ALLOW_SYSTEM");
    }
    /* Allow others by default; future: integrate domain whitelist */
    return 1;
}

/* ========================================================================== */
/* CDP MESSAGE BUS MODULE (from cdp_bus.c)                                   */
/* ========================================================================== */

/* External globals needed for bus operations */
extern int ws_sock;
extern int ws_cmd_id;
extern CDPContext g_ctx;

/* Forward declarations for functions defined elsewhere */
extern int send_command_with_retry(const char* cmd);
extern int receive_response_by_id(char* response, size_t response_size, int command_id, int max_attempts);

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
