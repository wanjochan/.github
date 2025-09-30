/**
 * CDP Command Templates - Reusable wrappers
 */

#include "cdp_internal.h"
/* #include "cdp_bus.h" - merged into cdp_internal.h */
#include "cdp_commands.h"

int cdp_runtime_enable(void) {
    return cdp_send_cmd("Runtime.enable", NULL) >= 0 ? 0 : -1;
}

int cdp_runtime_eval(const char *expr, int returnByValue, int generatePreview,
                     char *out_json, size_t out_size, int timeout_ms) {
    if (!expr) return -1;
    char escaped[32768];  // Increased to 32KB to handle large JS injections
    json_escape_safe(escaped, expr, sizeof(escaped));
    char params[33000];   // Increased to accommodate larger escaped content
    snprintf(params, sizeof(params),
             QUOTE({"expression":"%s","returnByValue":%s,"generatePreview":%s}),
             escaped, returnByValue ? "true" : "false",
             generatePreview ? "true" : "false");
    return cdp_call_cmd("Runtime.evaluate", params, out_json, out_size, timeout_ms);
}

int cdp_page_navigate(const char *url, char *out_json, size_t out_size, int timeout_ms) {
    if (!url) return -1;
    char escaped[2048];
    json_escape_safe(escaped, url, sizeof(escaped));
    char params[2100];
    snprintf(params, sizeof(params), QUOTE({"url":"%s"}), escaped);
    return cdp_call_cmd("Page.navigate", params, out_json, out_size, timeout_ms);
}

int cdp_page_screenshot(char *out_json, size_t out_size, int timeout_ms) {
    return cdp_call_cmd("Page.captureScreenshot", QUOTE({}), out_json, out_size, timeout_ms);
}

int cdp_fetch_continue(const char *request_id) {
    if (!request_id) return -1;
    char params[256];
    snprintf(params, sizeof(params), QUOTE({"requestId":"%s"}), request_id);
    return cdp_send_cmd("Fetch.continueRequest", params) >= 0 ? 0 : -1;
}

int cdp_fetch_fulfill(const char *request_id,
                      int status_code,
                      const char *headers_json_array,
                      const char *body_b64,
                      char *out_json, size_t out_size, int timeout_ms) {
    if (!request_id) return -1;
    const char *headers = headers_json_array && *headers_json_array ? headers_json_array : "[]";
    const char *body = body_b64 ? body_b64 : "";
    char params[8192];
    snprintf(params, sizeof(params),
             QUOTE({"requestId":"%s","responseCode":%d,"responseHeaders":%s,"body":"%s"}),
             request_id, status_code, headers, body);
    return cdp_call_cmd("Fetch.fulfillRequest", params, out_json, out_size, timeout_ms);
}

int cdp_fetch_enable(const char *patterns_json_array) {
    const char *patterns = patterns_json_array && *patterns_json_array ? patterns_json_array :
        QUOTE([
            {"urlPattern":"cli://*"},
            {"urlPattern":"gui://*"},
            {"urlPattern":"cdp-internal.local/*"},
            {"urlPattern":"notify://*"},
            {"urlPattern":"file://*"}
        ]);
    char params[1024];
    snprintf(params, sizeof(params), QUOTE({"patterns":%s}), patterns);
    return cdp_send_cmd("Fetch.enable", params) >= 0 ? 0 : -1;
}

int cdp_fetch_disable(void) {
    return cdp_send_cmd("Fetch.disable", NULL) >= 0 ? 0 : -1;
}

int cdp_network_enable(void) {
    return cdp_send_cmd("Network.enable", NULL) >= 0 ? 0 : -1;
}

int cdp_network_disable(void) {
    return cdp_send_cmd("Network.disable", NULL) >= 0 ? 0 : -1;
}

int cdp_page_add_script_newdoc(const char *script_source,
                               char *out_json, size_t out_size, int timeout_ms) {
    if (!script_source) return -1;
    char escaped[8192];
    json_escape_safe(escaped, script_source, sizeof(escaped));
    char params[8300];
    snprintf(params, sizeof(params), QUOTE({"source":"%s"}), escaped);
    return cdp_call_cmd("Page.addScriptToEvaluateOnNewDocument", params, out_json, out_size, timeout_ms);
}

int cdp_network_set_extra_headers(const char *headers_json_object) {
    if (!headers_json_object || !*headers_json_object) return -1;
    char params[8192];
    snprintf(params, sizeof(params), QUOTE({"headers":%s}), headers_json_object);
    return cdp_send_cmd("Network.setExtraHTTPHeaders", params) >= 0 ? 0 : -1;
}

int cdp_runtime_get_value(const char *expr, char *out_value, size_t out_size, int timeout_ms) {
    if (!expr || !out_value || out_size == 0) return -1;
    char resp[RESPONSE_BUFFER_SIZE];
    if (cdp_runtime_eval(expr, 1, 0, resp, sizeof(resp), timeout_ms) != 0) return -1;
    const char *value_start = strstr(resp, QUOTE("value":));
    if (!value_start) return -1;
    value_start += 8;
    const char *result_end = strstr(value_start, ",\"");
    if (!result_end) result_end = strstr(value_start, "}");
    if (!result_end) return -1;
    size_t len = result_end - value_start;
    if (len >= out_size) len = out_size - 1;
    strncpy(out_value, value_start, len);
    out_value[len] = '\0';
    return 0;
}

int cdp_page_enable(void) {
    return cdp_send_cmd("Page.enable", NULL) >= 0 ? 0 : -1;
}

int cdp_dom_enable(void) {
    return cdp_send_cmd("DOM.enable", NULL) >= 0 ? 0 : -1;
}

int cdp_runtime_get_properties(const char *objectId,
                               int ownProperties,
                               int accessorPropertiesOnly,
                               int generatePreview,
                               char *out_json, size_t out_size, int timeout_ms) {
    if (!objectId || !out_json) return -1;
    char params[1024];
    snprintf(params, sizeof(params),
             QUOTE({"objectId":"%s","ownProperties":%s,"accessorPropertiesOnly":%s,"generatePreview":%s}),
             objectId,
             ownProperties?"true":"false",
             accessorPropertiesOnly?"true":"false",
             generatePreview?"true":"false");
    return cdp_call_cmd("Runtime.getProperties", params, out_json, out_size, timeout_ms);
}

int cdp_dom_get_document(int depth, char *out_json, size_t out_size, int timeout_ms) {
    char params[64];
    snprintf(params, sizeof(params), QUOTE({"depth":%d}), depth);
    return cdp_call_cmd("DOM.getDocument", params, out_json, out_size, timeout_ms);
}

int cdp_dom_query_selector(int nodeId, const char *selector, char *out_json, size_t out_size, int timeout_ms) {
    if (!selector) return -1;
    char esc[2048]; json_escape_safe(esc, selector, sizeof(esc));
    char params[2200];
    snprintf(params, sizeof(params), QUOTE({"nodeId":%d,"selector":"%s"}), nodeId, esc);
    return cdp_call_cmd("DOM.querySelector", params, out_json, out_size, timeout_ms);
}

int cdp_dom_resolve_node(int nodeId, char *out_json, size_t out_size, int timeout_ms) {
    char params[128];
    snprintf(params, sizeof(params), QUOTE({"nodeId":%d}), nodeId);
    return cdp_call_cmd("DOM.resolveNode", params, out_json, out_size, timeout_ms);
}

int cdp_runtime_call_function_on(const char *objectId, const char *functionDeclaration,
                                 int returnByValue, char *out_json, size_t out_size, int timeout_ms) {
    if (!objectId || !functionDeclaration) return -1;
    char escFunc[4096]; json_escape_safe(escFunc, functionDeclaration, sizeof(escFunc));
    char params[4600];
    snprintf(params, sizeof(params),
             QUOTE({"objectId":"%s","functionDeclaration":"%s","returnByValue":%s}),
             objectId, escFunc, returnByValue?"true":"false");
    return cdp_call_cmd("Runtime.callFunctionOn", params, out_json, out_size, timeout_ms);
}

int cdp_runtime_call_function_on_args(const char *objectId, const char *functionDeclaration,
                                      const char *args_json_array,
                                      int returnByValue, char *out_json, size_t out_size, int timeout_ms) {
    if (!objectId || !functionDeclaration) return -1;
    char escFunc[4096]; json_escape_safe(escFunc, functionDeclaration, sizeof(escFunc));
    const char *args = (args_json_array && *args_json_array) ? args_json_array : "[]";
    char params[5000];
    snprintf(params, sizeof(params),
             QUOTE({"objectId":"%s","functionDeclaration":"%s","arguments":%s,"returnByValue":%s}),
             objectId, escFunc, args, returnByValue?"true":"false");
    return cdp_call_cmd("Runtime.callFunctionOn", params, out_json, out_size, timeout_ms);
}

/* --- High-level DOM helpers --- */
int cdp_dom_select_objectId(const char *selector, char *out_objectId, size_t out_size, int timeout_ms) {
    if (!selector || !out_objectId || out_size == 0) return -1;
    char doc[2048];
    if (cdp_dom_get_document(1, doc, sizeof(doc), timeout_ms) != 0) return -1;
    const char *nid = strstr(doc, QUOTE("nodeId":)); if (!nid) return -1;
    int nodeId = atoi(nid + 9);
    char js[4096];
    if (cdp_dom_query_selector(nodeId, selector, js, sizeof(js), timeout_ms) != 0) return -1;
    const char *selNode = strstr(js, QUOTE("nodeId":)); if (!selNode) return -1;
    int selId = atoi(selNode + 9);
    char resolved[4096];
    if (cdp_dom_resolve_node(selId, resolved, sizeof(resolved), timeout_ms) != 0) return -1;
    const char *obj = strstr(resolved, JKEYQ("objectId")); if (!obj) return -1;
    obj += 12; const char *end = strchr(obj, '"'); if (!end) return -1;
    size_t len = end - obj; if (len >= out_size) len = out_size - 1;
    strncpy(out_objectId, obj, len); out_objectId[len] = '\0';
    return 0;
}

int cdp_runtime_call_on_selector(const char *selector, const char *functionDeclaration,
                                 int returnByValue, char *out_json, size_t out_size, int timeout_ms) {
    char objectId[256];
    if (cdp_dom_select_objectId(selector, objectId, sizeof(objectId), timeout_ms) != 0) return -1;
    return cdp_runtime_call_function_on(objectId, functionDeclaration, returnByValue, out_json, out_size, timeout_ms);
}

/* SIMPLIFIED DOM helpers - Use JS Enhanced API instead of complex multi-step operations
 * These lightweight wrappers replace 50+ lines of complex CDP orchestration with simple API calls
 */
int cdp_dom_click_selector(const char *selector, int timeout_ms) {
    char cmd[512]; snprintf(cmd, sizeof(cmd), ".click %s", selector); char out[256];
    return cdp_execute_enhanced_command(cmd, out, sizeof(out));
}

int cdp_dom_set_value_selector(const char *selector, const char *value, int timeout_ms) {
    if (!value) return -1; char cmd[1024]; snprintf(cmd, sizeof(cmd), ".set %s %s", selector, value); char out[256];
    return cdp_execute_enhanced_command(cmd, out, sizeof(out));
}

int cdp_dom_get_inner_text(const char *selector, char *out_text, size_t out_size, int timeout_ms) {
    char cmd[512]; snprintf(cmd, sizeof(cmd), ".text %s", selector); char out[1024];
    if (cdp_execute_enhanced_command(cmd, out, sizeof(out)) != 0) return -1;
    const char *data = strstr(out, QUOTE("data":)); if (!data) return -1; data += 7;
    const char *end = strstr(data, QUOTE(",")); if (!end) end = strstr(data, "}");
    if (!end) return -1; size_t len = end - data - 2; if (len >= out_size) len = out_size - 1;
    strncpy(out_text, data + 1, len); out_text[len] = '\0'; return 0;
}

int cdp_page_qsa_texts(const char *selector, char *out_json_array, size_t out_size, int timeout_ms) {
    char cmd[512]; snprintf(cmd, sizeof(cmd), ".texts %s", selector); 
    return cdp_execute_enhanced_command(cmd, out_json_array, out_size);
}

int cdp_dom_get_attributes_json(const char *selector, char *out_json, size_t out_size, int timeout_ms) {
    char cmd[512]; snprintf(cmd, sizeof(cmd), "fastDOM.attrs('%s')", selector);
    return cdp_execute_enhanced_command(cmd, out_json, out_size);
}
