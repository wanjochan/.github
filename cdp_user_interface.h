/**
 * CDP Command Templates - Reusable wrappers
 */
#ifndef CDP_COMMANDS_H
#define CDP_COMMANDS_H

#include <stddef.h>

/* Enhanced API function declaration */
int cdp_execute_enhanced_command(const char *command, char *output, size_t output_size);

int cdp_runtime_enable(void);
int cdp_runtime_eval(const char *expr, int returnByValue, int generatePreview,
                     char *out_json, size_t out_size, int timeout_ms);
int cdp_page_navigate(const char *url, char *out_json, size_t out_size, int timeout_ms);
int cdp_page_screenshot(char *out_json, size_t out_size, int timeout_ms);

/* Fetch domain helpers */
int cdp_fetch_continue(const char *request_id);
int cdp_fetch_fulfill(const char *request_id,
                      int status_code,
                      const char *headers_json_array,
                      const char *body_b64,
                      char *out_json, size_t out_size, int timeout_ms);

/* Enable/disable protocol/network domains */
int cdp_fetch_enable(const char *patterns_json_array);  /* NULL → default patterns */
int cdp_fetch_disable(void);
int cdp_network_enable(void);
int cdp_network_disable(void);

/* Page helpers */
int cdp_page_add_script_newdoc(const char *script_source,
                               char *out_json, size_t out_size, int timeout_ms);

/* Network extra headers */
int cdp_network_set_extra_headers(const char *headers_json_object);

/* Convenience: evaluate and extract result.value as string */
int cdp_runtime_get_value(const char *expr, char *out_value, size_t out_size, int timeout_ms);

/* Additional protocol templates (high-value common ops) */
int cdp_page_enable(void);
int cdp_dom_enable(void);

int cdp_runtime_get_properties(const char *objectId,
                               int ownProperties,
                               int accessorPropertiesOnly,
                               int generatePreview,
                               char *out_json, size_t out_size, int timeout_ms);

int cdp_dom_get_document(int depth, char *out_json, size_t out_size, int timeout_ms);
int cdp_dom_query_selector(int nodeId, const char *selector, char *out_json, size_t out_size, int timeout_ms);
int cdp_dom_resolve_node(int nodeId, char *out_json, size_t out_size, int timeout_ms);
int cdp_runtime_call_function_on(const char *objectId, const char *functionDeclaration,
                                 int returnByValue, char *out_json, size_t out_size, int timeout_ms);

/* Extended with arguments (args_json is array) */
int cdp_runtime_call_function_on_args(const char *objectId, const char *functionDeclaration,
                                      const char *args_json_array,
                                      int returnByValue, char *out_json, size_t out_size, int timeout_ms);

/* High-level DOM helpers */
int cdp_dom_select_objectId(const char *selector, char *out_objectId, size_t out_size, int timeout_ms);
int cdp_runtime_call_on_selector(const char *selector, const char *functionDeclaration,
                                 int returnByValue, char *out_json, size_t out_size, int timeout_ms);
int cdp_dom_click_selector(const char *selector, int timeout_ms);
int cdp_dom_set_value_selector(const char *selector, const char *value, int timeout_ms);
int cdp_dom_get_inner_text(const char *selector, char *out_text, size_t out_size, int timeout_ms);

/* Query selector all texts (helper) */
int cdp_page_qsa_texts(const char *selector, char *out_json_array, size_t out_size, int timeout_ms);
int cdp_dom_get_attributes_json(const char *selector, char *out_json, size_t out_size, int timeout_ms);

#endif /* CDP_COMMANDS_H */
