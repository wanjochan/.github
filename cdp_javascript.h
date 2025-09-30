/**
 * cdp_javascript.h - JavaScript and JSON Processing Module
 *
 * Provides high-level JSON manipulation and JavaScript execution
 * Built on top of QuickJS for elegant JSON handling
 */

#ifndef CDP_JAVASCRIPT_H
#define CDP_JAVASCRIPT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize JavaScript engine (called automatically when needed) */
int cdp_js_init(void);
void cdp_js_cleanup(void);

/* JSON Parsing Functions - Clean API for other modules */

/**
 * Extract string value from JSON
 * Handles HTTP responses by automatically finding JSON body after headers
 */
int cdp_js_get_string(const char *json_or_response, const char *field, char *out, size_t out_size);

/**
 * Extract integer value from JSON
 */
int cdp_js_get_int(const char *json_or_response, const char *field, int *out);

/**
 * Extract boolean value from JSON
 */
int cdp_js_get_bool(const char *json_or_response, const char *field, int *out);

/**
 * Extract nested field from JSON (e.g., "result.result.type")
 */
int cdp_js_get_nested(const char *json_or_response, const char *path, char *out, size_t out_size);

/**
 * Pretty print JSON
 */
int cdp_js_beautify(const char *json, char *out, size_t out_size);

/**
 * Parse and extract specific CDP response fields
 */
int cdp_js_get_target_id(const char *response, char *target_id, size_t size);
int cdp_js_get_node_id(const char *response, int *node_id);
int cdp_js_get_object_id(const char *response, char *object_id, size_t size);
int cdp_js_get_websocket_url(const char *response, char *url, size_t size);

/**
 * Additional helper functions for CDP
 */
int cdp_js_get_request_id(const char *response, char *request_id, size_t size);
int cdp_js_get_url(const char *response, char *url, size_t size);
int cdp_js_get_frame_id(const char *response, char *frame_id, size_t size);
int cdp_js_get_execution_context_id(const char *response, int *context_id);
int cdp_js_has_error(const char *response);
int cdp_js_find_target_with_url(const char *response, const char *url, char *target_id, size_t size);

/**
 * JSON Builder Functions - Create properly formatted JSON without manual string manipulation
 */
typedef struct {
    char buffer[8192];
    size_t pos;
} CDPJSONBuilder;

void cdp_js_builder_init(CDPJSONBuilder *builder);
void cdp_js_builder_add_string(CDPJSONBuilder *builder, const char *key, const char *value);
void cdp_js_builder_add_int(CDPJSONBuilder *builder, const char *key, int value);
void cdp_js_builder_add_bool(CDPJSONBuilder *builder, const char *key, int value);
void cdp_js_builder_add_raw(CDPJSONBuilder *builder, const char *key, const char *raw_json);
const char* cdp_js_builder_get(CDPJSONBuilder *builder);

/**
 * High-level CDP Command Builders
 */
int cdp_js_build_evaluate(const char *expression, int context_id, char *out, size_t size);
int cdp_js_build_call_function(const char *object_id, const char *func, const char *args, char *out, size_t size);
int cdp_js_build_navigate(const char *url, char *out, size_t size);
int cdp_js_build_screenshot(char *out, size_t size);
int cdp_js_build_fetch_continue(const char *request_id, char *out, size_t size);
int cdp_js_build_fetch_fulfill(const char *request_id, int status, const char *headers, const char *body, char *out, size_t size);
int cdp_js_build_fetch_patterns(const char *patterns_array, char *out, size_t size);
int cdp_js_build_mouse_event(const char *type, int x, int y, const char *button, char *out, size_t size);

/**
 * JavaScript Code Templates - Common JS patterns
 */
int cdp_js_eval_object_keys(const char *object_name, char *result, size_t size);
int cdp_js_eval_storage_get(const char *storage_type, const char *key, char *result, size_t size);
int cdp_js_eval_storage_set(const char *storage_type, const char *key, const char *value, char *result, size_t size);
int cdp_js_eval_safe_json_stringify(const char *expression, char *result, size_t size);

/**
 * Response Builder Functions - Create API responses
 */
int cdp_js_build_success_response(const char *command, const char *result, char *out, size_t size);
int cdp_js_build_error_response(const char *error_msg, const char *details, char *out, size_t size);
int cdp_js_build_simple_response(const char *key, const char *value, char *out, size_t size);

/* JavaScript Execution (existing functions) */
/* Note: parse_response is internal to cdp_javascript.c */
int evaluate_expression(const char *expr, char *result, size_t result_size);
int evaluate_on_object(const char *object_id, const char *expr, char *result, size_t result_size);
int evaluate_function_on_object(const char *object_id, const char *func_decl, const char *args_json, char *result, size_t result_size);

#ifdef __cplusplus
}
#endif

#endif /* CDP_JAVASCRIPT_H */