/**
 * CDP Utility Functions Header
 * Common utilities for JSON, WebSocket, and string operations
 */

#ifndef CDP_UTILS_H
#define CDP_UTILS_H

#include <stddef.h>
#include <stdint.h>

/* JSON Utilities */
void json_escape_safe(char *dst, const char *src, size_t dst_size);
char* json_get_string(const char *json, const char *key, char *value, size_t value_size);
int json_get_int(const char *json, const char *key);
char* json_extract_value(const char *json, const char *path, char *value, size_t value_size);

/* WebSocket Utilities */
void ws_base64_encode(const unsigned char *input, int length, char *output, size_t output_size);
int ws_send_text_frame(int sock, const char *text, size_t text_len);
int ws_recv_text_frame(int sock, char *buffer, size_t max_len, int timeout_ms);
int ws_send_pong(int sock);
int ws_parse_frame_header(const uint8_t *header, uint8_t *opcode, uint64_t *payload_len);

/* String Utilities */
size_t str_copy_safe(char *dst, const char *src, size_t dst_size);
size_t str_append_safe(char *dst, const char *src, size_t dst_size);
int str_trim(char *str);
char* str_find_token(const char *str, const char *token);
int str_replace_char(char *str, char old_char, char new_char);

/* Memory Utilities */
void* safe_malloc(size_t size);
void* safe_realloc(void *ptr, size_t size);
void safe_free(void **ptr);
#define SAFE_FREE(ptr) safe_free((void**)&(ptr))

/* System Utilities */
double get_time_ms(void);
int set_nonblocking(int fd);
int calculate_backoff_delay(int attempt, int base_delay_ms, double backoff_factor, int max_delay_ms);
void wait_with_backoff(int attempt, int base_delay_ms, double backoff_factor, int max_delay_ms);

/* CDP Message Utilities */
typedef struct {
    int id;
    char method[128];
    char params[4096];
    int params_added;
} CDPMessage;

CDPMessage* cdp_message_new(const char *method);
void cdp_message_add_param(CDPMessage* msg, const char* key, const char* value);
void cdp_message_add_param_int(CDPMessage* msg, const char* key, int value);
void cdp_message_add_param_bool(CDPMessage* msg, const char* key, int value);
char* cdp_message_build(CDPMessage* msg);
void cdp_message_free(CDPMessage* msg);

/* JSON Command Parsing */
int parse_json_command(const char *json, int *id, char *cmd, size_t cmd_size);

#endif /* CDP_UTILS_H */