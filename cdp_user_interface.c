/**
 * CDP User Interface Module
 * 
 * This module combines CDP command templates and user features into a unified interface.
 * It provides both low-level CDP command wrappers and high-level user-friendly features.
 * 
 * Structure:
 * - Basic CDP Commands 
 * - Enhanced User Features 
 * - Performance Tracking
 * - Output Beautification
 */

#include "cdp_internal.h"
#include "cdp_user_interface.h"
#include "cdp_javascript.h"
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* ================================================================ */
/* CLI Enhancements: refs, waits, cookies/storage, logs (C-side)    */
/* ================================================================ */

/* --- Element reference table ----------------------------------- */
typedef struct {
    char name[64];
    char selector[1024];
} RefEntry;

#define MAX_REFS 256
static RefEntry g_refs[MAX_REFS];
static int g_ref_count = 0;
static int g_ref_seq = 1;

static int ref_find_index(const char *name) {
    for (int i = 0; i < g_ref_count; i++) {
        if (strcmp(g_refs[i].name, name) == 0) return i;
    }
    return -1;
}

static const char* ref_add(const char *selector, const char *opt_name) {
    if (!selector || !*selector) return NULL;
    if (g_ref_count >= MAX_REFS) return NULL;
    RefEntry *e = &g_refs[g_ref_count];
    if (opt_name && *opt_name) {
        snprintf(e->name, sizeof(e->name), "%s", opt_name[0] == '@' ? opt_name + 1 : opt_name);
    } else {
        snprintf(e->name, sizeof(e->name), "e%d", g_ref_seq++);
    }
    snprintf(e->selector, sizeof(e->selector), "%s", selector);
    g_ref_count++;
    static char token[72];
    snprintf(token, sizeof(token), "@%s", e->name);
    return token;
}

static int ref_remove(const char *name_or_token) {
    if (!name_or_token || !*name_or_token) return -1;
    const char *name = name_or_token[0] == '@' ? name_or_token + 1 : name_or_token;
    int idx = ref_find_index(name);
    if (idx < 0) return -1;
    for (int i = idx + 1; i < g_ref_count; i++) g_refs[i-1] = g_refs[i];
    g_ref_count--;
    return 0;
}

static const char* ref_get_selector(const char *name_or_token) {
    if (!name_or_token || !*name_or_token) return NULL;
    const char *name = name_or_token[0] == '@' ? name_or_token + 1 : name_or_token;
    int idx = ref_find_index(name);
    return idx >= 0 ? g_refs[idx].selector : NULL;
}

static void refs_list(char *out, size_t out_sz) {
    size_t off = 0; off += snprintf(out+off, out_sz-off, "=== Refs (%d) ===\n", g_ref_count);
    for (int i = 0; i < g_ref_count && off < out_sz; i++) {
        off += snprintf(out+off, out_sz-off, "@%s -> %s\n", g_refs[i].name, g_refs[i].selector);
    }
}

/* --- Expand @tokens in command --------------------------------- */
static void expand_ref_tokens(const char *in, char *out, size_t out_sz) {
    size_t oi = 0;
    for (size_t i = 0; in[i] && oi < out_sz-1; ) {
        if (in[i] == '@') {
            size_t j = i + 1; char name[64]; size_t nj=0;
            while (in[j] && !isspace((unsigned char)in[j]) && in[j] != ',' && nj < sizeof(name)-1) {
                name[nj++] = in[j++];
            }
            name[nj] = '\0';
            if (nj > 0) {
                char token[72]; snprintf(token, sizeof(token), "@%s", name);
                const char *sel = ref_get_selector(token);
                if (sel) {
                    size_t sl = strlen(sel);
                    if (sl > out_sz - 1 - oi) sl = out_sz - 1 - oi;
                    memcpy(out + oi, sel, sl); oi += sl; i = j; continue;
                }
            }
        }
        out[oi++] = in[i++];
    }
    out[oi] = '\0';
}

/* --- Timeout / wait helpers ------------------------------------ */
static int g_user_timeout_ms = 5000;

static int wait_ms_loop(long timeout_ms, int interval_ms, int (*check)(void*), void *ud) {
    if (timeout_ms <= 0) timeout_ms = g_user_timeout_ms;
    if (interval_ms <= 0) interval_ms = 100;
    struct timespec start, now; clock_gettime(CLOCK_MONOTONIC, &start);
    while (1) {
        if (check && check(ud)) return 0;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000L + (now.tv_nsec - start.tv_nsec) / 1000000L;
        if (elapsed >= timeout_ms) return -1;
        usleep(interval_ms * 1000);
    }
}

/* Forward decls from cdp.c for network idle and logs */
extern int cdp_net_inflight(void);
extern long cdp_net_ms_since_activity(void);
extern void cdp_logs_set_enabled(int en);
extern int cdp_logs_get_enabled(void);
extern void cdp_logs_clear(void);
extern int cdp_logs_tail(char *out, size_t out_sz, int max_lines, const char* filter);
extern int cdp_select_frame_by_id(const char* frame_id);
extern int cdp_get_selected_frame(char* out, size_t out_sz);

/* --- Cookies/Storage helpers (basic JS-side via evaluate) ------ */
static int js_eval_bool(const char *expr) {
    char code[4096]; snprintf(code, sizeof(code), "!!(%s)", expr);
    char *res = execute_javascript(code);
    return (res && (strcmp(res, "true") == 0 || strcmp(res, "1") == 0));
}

/* Document readiness + JS eval retry helpers */
static int wait_document_ready(int timeout_ms) {
    if (timeout_ms <= 0) timeout_ms = g_user_timeout_ms;
    struct timespec start, now; clock_gettime(CLOCK_MONOTONIC, &start);
    while (1) {
        char *rs = execute_javascript("document && document.readyState");
        if (rs && (strcmp(rs, "interactive")==0 || strcmp(rs, "complete")==0 || strcmp(rs, "loading")==0)) {
            return 0;
        }
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000L + (now.tv_nsec - start.tv_nsec) / 1000000L;
        if (elapsed >= timeout_ms) return -1;
        usleep(50 * 1000);
    }
}

static char* exec_js_with_retry(const char *expr, int timeout_ms) {
    if (timeout_ms <= 0) timeout_ms = g_user_timeout_ms;
    struct timespec start, now; clock_gettime(CLOCK_MONOTONIC, &start);
    char *res = NULL;
    while (1) {
        res = execute_javascript(expr);
        if (res && strcmp(res, "Error in execution") != 0) return res;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000L + (now.tv_nsec - start.tv_nsec) / 1000000L;
        if (elapsed >= timeout_ms) return res; // last result (possibly error)
        usleep(50 * 1000);
    }
}


/* ========================================================================== */
/* PERFORMANCE TRACKING                                                       */
/* ========================================================================== */

/* Performance statistics */
typedef struct {
    int total_commands;
    double total_time_ms;
    double min_time_ms;
    double max_time_ms;
    time_t session_start;
} PerfStats;

static PerfStats perf_stats = {
    .total_commands = 0,
    .total_time_ms = 0,
    .min_time_ms = 999999,
    .max_time_ms = 0,
    .session_start = 0
};

/* Initialize performance tracking */
void cdp_perf_init(void) {
    perf_stats.session_start = time(NULL);
    perf_stats.total_commands = 0;
    perf_stats.total_time_ms = 0;
    perf_stats.min_time_ms = 999999;
    perf_stats.max_time_ms = 0;
}

/* Track command execution time */
void cdp_perf_track(double time_ms) {
    perf_stats.total_commands++;
    perf_stats.total_time_ms += time_ms;
    
    if (time_ms < perf_stats.min_time_ms) {
        perf_stats.min_time_ms = time_ms;
    }
    if (time_ms > perf_stats.max_time_ms) {
        perf_stats.max_time_ms = time_ms;
    }
}

/* Show performance statistics */
void cdp_show_stats(void) {
    if (perf_stats.total_commands == 0) {
        cdp_log(CDP_LOG_INFO, "STATS", "No commands executed yet.");
        return;
    }
    
    time_t session_time = time(NULL) - perf_stats.session_start;
    double avg_time = perf_stats.total_time_ms / perf_stats.total_commands;
    
    cdp_log(CDP_LOG_INFO, "STATS", "\n=== Session Statistics ===");
    cdp_log(CDP_LOG_INFO, "STATS", "Session duration:  %ld seconds", session_time);
    cdp_log(CDP_LOG_INFO, "STATS", "Commands executed: %d", perf_stats.total_commands);
    cdp_log(CDP_LOG_INFO, "STATS", "Average time:      %.2f ms", avg_time);
    cdp_log(CDP_LOG_INFO, "STATS", "Min time:          %.2f ms", perf_stats.min_time_ms);
    cdp_log(CDP_LOG_INFO, "STATS", "Max time:          %.2f ms", perf_stats.max_time_ms);
    cdp_log(CDP_LOG_INFO, "STATS", "Total time:        %.2f ms", perf_stats.total_time_ms);
    
    if (session_time > 0) {
        cdp_log(CDP_LOG_INFO, "STATS", "Commands/second:   %.2f", 
               (double)perf_stats.total_commands / session_time);
    }
}

/* ========================================================================== */
/* BASIC CDP COMMANDS 
/* ========================================================================== */

int cdp_runtime_enable(void) {
    return cdp_send_cmd("Runtime.enable", NULL) >= 0 ? 0 : -1;
}

int cdp_runtime_eval(const char *expr, int returnByValue, int generatePreview,
                     char *out_json, size_t out_size, int timeout_ms) {
    if (!expr) return -1;
    char escaped[32768];  // Increased to 32KB to handle large JS injections
    json_escape_safe(escaped, expr, sizeof(escaped));

    // Check if expression contains async/await keywords
    //int has_async = 1; //(strstr(expr, "async") != NULL || strstr(expr, "await") != NULL);
    char params[33000];   // Increased to accommodate larger escaped content

    // if (has_async) {
        // For async code, add awaitPromise parameter to wait for completion
        snprintf(params, sizeof(params),
                 QUOTE({"expression":"%s","returnByValue":%s,"generatePreview":%s,"awaitPromise":true}),
                 escaped, returnByValue ? "true" : "false",
                 generatePreview ? "true" : "false");
    // } else {
    //     // For regular code, keep original behavior
    //     snprintf(params, sizeof(params),
    //              QUOTE({"expression":"%s","returnByValue":%s,"generatePreview":%s}),
    //              escaped, returnByValue ? "true" : "false",
    //              generatePreview ? "true" : "false");
    // }

    return cdp_call_cmd("Runtime.evaluate", params, out_json, out_size, timeout_ms);
}

int cdp_page_navigate(const char *url, char *out_json, size_t out_size, int timeout_ms) {
    if (!url) return -1;
    char params[2100];
    cdp_js_build_navigate(url, params, sizeof(params));
    return cdp_call_cmd("Page.navigate", params, out_json, out_size, timeout_ms);
}

int cdp_page_screenshot(char *out_json, size_t out_size, int timeout_ms) {
    char params[64];
    cdp_js_build_screenshot(params, sizeof(params));
    return cdp_call_cmd("Page.captureScreenshot", params, out_json, out_size, timeout_ms);
}

int cdp_fetch_continue(const char *request_id) {
    if (!request_id) return -1;
    char params[256];
    cdp_js_build_fetch_continue(request_id, params, sizeof(params));
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
    cdp_js_build_fetch_fulfill(request_id, status_code, headers, body, params, sizeof(params));
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
    cdp_js_build_fetch_patterns(patterns, params, sizeof(params));
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
    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_string(&builder, "source", escaped);
    strncpy(params, cdp_js_builder_get(&builder), sizeof(params));
    return cdp_call_cmd("Page.addScriptToEvaluateOnNewDocument", params, out_json, out_size, timeout_ms);
}

int cdp_network_set_extra_headers(const char *headers_json_object) {
    if (!headers_json_object || !*headers_json_object) return -1;
    char params[8192];
    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_raw(&builder, "headers", headers_json_object);
    strncpy(params, cdp_js_builder_get(&builder), sizeof(params));
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
    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_string(&builder, "objectId", objectId);
    cdp_js_builder_add_bool(&builder, "ownProperties", ownProperties);
    cdp_js_builder_add_bool(&builder, "accessorPropertiesOnly", accessorPropertiesOnly);
    cdp_js_builder_add_bool(&builder, "generatePreview", generatePreview);
    strncpy(params, cdp_js_builder_get(&builder), sizeof(params));
    return cdp_call_cmd("Runtime.getProperties", params, out_json, out_size, timeout_ms);
}

int cdp_dom_get_document(int depth, char *out_json, size_t out_size, int timeout_ms) {
    char params[64];
    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_int(&builder, "depth", depth);
    strncpy(params, cdp_js_builder_get(&builder), sizeof(params));
    return cdp_call_cmd("DOM.getDocument", params, out_json, out_size, timeout_ms);
}

int cdp_dom_query_selector(int nodeId, const char *selector, char *out_json, size_t out_size, int timeout_ms) {
    if (!selector) return -1;
    char esc[2048]; json_escape_safe(esc, selector, sizeof(esc));
    char params[2200];
    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_int(&builder, "nodeId", nodeId);
    cdp_js_builder_add_string(&builder, "selector", esc);
    strncpy(params, cdp_js_builder_get(&builder), sizeof(params));
    return cdp_call_cmd("DOM.querySelector", params, out_json, out_size, timeout_ms);
}

int cdp_dom_resolve_node(int nodeId, char *out_json, size_t out_size, int timeout_ms) {
    char params[128];
    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_int(&builder, "nodeId", nodeId);
    strncpy(params, cdp_js_builder_get(&builder), sizeof(params));
    return cdp_call_cmd("DOM.resolveNode", params, out_json, out_size, timeout_ms);
}

int cdp_runtime_call_function_on(const char *objectId, const char *functionDeclaration,
                                 int returnByValue, char *out_json, size_t out_size, int timeout_ms) {
    if (!objectId || !functionDeclaration) return -1;
    char escFunc[4096]; json_escape_safe(escFunc, functionDeclaration, sizeof(escFunc));
    char params[4600];
    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_string(&builder, "objectId", objectId);
    cdp_js_builder_add_string(&builder, "functionDeclaration", escFunc);
    cdp_js_builder_add_bool(&builder, "returnByValue", returnByValue);
    strncpy(params, cdp_js_builder_get(&builder), sizeof(params));
    return cdp_call_cmd("Runtime.callFunctionOn", params, out_json, out_size, timeout_ms);
}

int cdp_runtime_call_function_on_args(const char *objectId, const char *functionDeclaration,
                                      const char *args_json_array,
                                      int returnByValue, char *out_json, size_t out_size, int timeout_ms) {
    if (!objectId || !functionDeclaration) return -1;
    char escFunc[4096]; json_escape_safe(escFunc, functionDeclaration, sizeof(escFunc));
    const char *args = (args_json_array && *args_json_array) ? args_json_array : "[]";
    char params[5000];
    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_string(&builder, "objectId", objectId);
    cdp_js_builder_add_string(&builder, "functionDeclaration", escFunc);
    cdp_js_builder_add_raw(&builder, "arguments", args);
    cdp_js_builder_add_bool(&builder, "returnByValue", returnByValue);
    strncpy(params, cdp_js_builder_get(&builder), sizeof(params));
    return cdp_call_cmd("Runtime.callFunctionOn", params, out_json, out_size, timeout_ms);
}

/* --- High-level DOM helpers --- */
int cdp_dom_select_objectId(const char *selector, char *out_objectId, size_t out_size, int timeout_ms) {
    if (!selector || !out_objectId || out_size == 0) return -1;
    char doc[2048];
    if (cdp_dom_get_document(1, doc, sizeof(doc), timeout_ms) != 0) return -1;
    int nodeId;
    if (cdp_js_get_node_id(doc, &nodeId) != 0) return -1;

    char js[4096];
    if (cdp_dom_query_selector(nodeId, selector, js, sizeof(js), timeout_ms) != 0) return -1;

    int selId;
    if (cdp_js_get_node_id(js, &selId) != 0) return -1;

    char resolved[4096];
    if (cdp_dom_resolve_node(selId, resolved, sizeof(resolved), timeout_ms) != 0) return -1;

    if (cdp_js_get_object_id(resolved, out_objectId, out_size) != 0) return -1;
    return 0;
}

int cdp_runtime_call_on_selector(const char *selector, const char *functionDeclaration,
                                 int returnByValue, char *out_json, size_t out_size, int timeout_ms) {
    char objectId[256];
    if (cdp_dom_select_objectId(selector, objectId, sizeof(objectId), timeout_ms) != 0) return -1;
    return cdp_runtime_call_function_on(objectId, functionDeclaration, returnByValue, out_json, out_size, timeout_ms);
}

/* ========================================================================== */
/* ENHANCED USER FEATURES 
/* ========================================================================== */

/* SIMPLIFIED: Use JS Enhanced API for command processing */
/* Simple JSON field extractor for compatibility unwrapping */
static int extract_json_string_field(const char *json, const char *key,
                                     char *out, size_t out_size) {
    if (!json || !key || !out || out_size == 0) return 0;
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern); if (!p) return 0; p += strlen(pattern);
    while (*p == ' '||*p=='\t'||*p=='\n'||*p=='\r') p++; if (*p != ':') return 0; p++;
    while (*p == ' '||*p=='\t'||*p=='\n'||*p=='\r') p++;
    size_t j=0;
    if (*p == '"') { // string
        p++;
        int esc=0; while (*p && j<out_size-1) { char c=*p++;
            if (esc) { switch(c){case 'n':out[j++]='\n';break;case 'r':out[j++]='\r';break;case 't':out[j++]='\t';break;case '\\':out[j++]='\\';break;case '"':out[j++]='"';break;case 'u': if (p[0]&&p[1]&&p[2]&&p[3]) p+=4; break; default: out[j++]=c;} esc=0; continue; }
            if (c=='\\'){esc=1;continue;} if (c=='"') break; out[j++]=c; }
        out[j]='\0'; return 1;
    }
    return 0;
}

/* Execute enhanced command by returning whatever string JS formats for display */
int cdp_execute_enhanced_command(const char *command, char *output, size_t output_size) {
    if (!command || !output || output_size == 0) return -1;
    char escaped_cmd[1024];
    json_escape_safe(escaped_cmd, command, sizeof(escaped_cmd));
    char js_call[2048];
    snprintf(js_call, sizeof(js_call),
             QUOTE(window.CDP_Enhanced ? CDP_Enhanced.exec("%s") : "[CDP_Enhanced not loaded]"),
             escaped_cmd);
    char *res = execute_javascript(js_call);
    if (!res) return -1;
    // Compatibility: older JS versions returned JSON-wrapped strings {ok,data,err}
    // Try to unwrap here; if not matched, fall back to raw string
    if (strstr(res, "\"ok\"") && (strstr(res, "\"data\"") || strstr(res, "\"err\"")) && res[0] == '{') {
        // Attempt to extract data first, then err
        char buf[8192];
        if (extract_json_string_field(res, "data", buf, sizeof(buf)) ||
            extract_json_string_field(res, "err", buf, sizeof(buf))) {
            str_copy_safe(output, buf, output_size);
            return 0;
        }
        // fallthrough if extraction failed
    }
    str_copy_safe(output, res, output_size);
    return 0;
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
    char cmd[512]; snprintf(cmd, sizeof(cmd), ".text %s", selector); char out[2048];
    if (cdp_execute_enhanced_command(cmd, out, sizeof(out)) != 0) return -1;
    /* After extraction, 'out' already contains the data string */
    str_copy_safe(out_text, out, out_size);
    return 0;
}

int cdp_page_qsa_texts(const char *selector, char *out_json_array, size_t out_size, int timeout_ms) {
    char cmd[512]; snprintf(cmd, sizeof(cmd), ".texts %s", selector);
    return cdp_execute_enhanced_command(cmd, out_json_array, out_size);
}

int cdp_dom_get_attributes_json(const char *selector, char *out_json, size_t out_size, int timeout_ms) {
    /* Use direct JS enhanced API to fetch all attributes as object */
    char cmd[512]; snprintf(cmd, sizeof(cmd), "fastDOM.attrs('%s')", selector);
    return cdp_execute_enhanced_command(cmd, out_json, out_size);
}

/* Beautify JavaScript output */
char* cdp_beautify_output(const char *result) {
    if (!result) return NULL;
    
    // GENERIC SOLUTION: Don't try to re-parse ANY results
    // The wrapper function in Chrome already handles all formatting
    // Just return the result as-is
    return strdup(result);
}

/* Execute JavaScript from file */
int cdp_execute_script_file(const char *filename) {
    struct stat st;
    if (stat(filename, &st) != 0) {
        return cdp_error_push(CDP_ERR_INVALID_ARGS, 
                             "Script file not found: %s", filename);
    }
    
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return cdp_error_push(CDP_ERR_INVALID_ARGS,
                             "Cannot open script file: %s", filename);
    }
    
    if (verbose) {
        cdp_log(CDP_LOG_INFO, "SCRIPT", "Executing script: %s (%ld bytes)", filename, st.st_size);
    }
    
    // Read entire file for multi-line support
    char *script = malloc(st.st_size + 1);
    if (!script) {
        fclose(fp);
        return cdp_error_push(CDP_ERR_MEMORY, "Out of memory");
    }
    
    size_t read_size = fread(script, 1, st.st_size, fp);
    script[read_size] = '\0';
    fclose(fp);
    
    // Execute as single block for proper scope
    char *result = execute_javascript(script);
    
    if (result) {
        // Beautify output if it's an object
        char *beautified = cdp_beautify_output(result);
        cdp_log(CDP_LOG_INFO, "SCRIPT", "%s", beautified ? beautified : result);
        if (beautified != result) {
            free(beautified);
        }
    }
    
    free(script);
    return 0;
}

/* SIMPLIFIED: Get shortcuts from JS Enhanced API */
void cdp_show_shortcuts(void) {
    char help_output[4096];
    
    // Use JS Enhanced API to get help information
    int result = cdp_execute_enhanced_command("dispatcher.help()", help_output, sizeof(help_output));
    
    if (result >= 0) {
        cdp_log(CDP_LOG_INFO, "HELP", "\n=== Available Shortcuts (from Enhanced API) ===");
        cdp_log(CDP_LOG_INFO, "HELP", "%s", help_output);
        cdp_log(CDP_LOG_INFO, "HELP", "\nCLI Add-ons:");
        cdp_log(CDP_LOG_INFO, "HELP", "  .ref <selector> [name]     - 保存引用, 返回@name");
        cdp_log(CDP_LOG_INFO, "HELP", "  .refs                       - 列出引用");
        cdp_log(CDP_LOG_INFO, "HELP", "  .unref <@name|name>         - 删除引用");
        cdp_log(CDP_LOG_INFO, "HELP", "  .timeout get|set <ms>       - 获取/设置超时");
        cdp_log(CDP_LOG_INFO, "HELP", "  .wait <sel> [visible|hidden|exists|count>=N] [timeout=ms] [interval=ms]");
        cdp_log(CDP_LOG_INFO, "HELP", "  .wait_network_idle [ms]     - 等待网络空闲");
        cdp_log(CDP_LOG_INFO, "HELP", "  .until <js-expr>            - 直到表达式为真");
        cdp_log(CDP_LOG_INFO, "HELP", "  .cookie list|get|set|del    - Cookies 操作");
        cdp_log(CDP_LOG_INFO, "HELP", "  .local/.session keys|get|set|del - Web Storage 操作");
        cdp_log(CDP_LOG_INFO, "HELP", "  .logs on|off|clear|tail [N] - 控制/查看日志");
        cdp_log(CDP_LOG_INFO, "HELP", "  .frames list|switch <frameId>|current - Frame 管理");
        cdp_log(CDP_LOG_INFO, "HELP", "  .windows list|switch <id>  - 目标/窗口 列表/激活(受限)");
        cdp_log(CDP_LOG_INFO, "HELP", "  .mouse move x y | .mouse click x y [left|right|middle] - 鼠标动作");
        cdp_log(CDP_LOG_INFO, "HELP", "  .keys <text>               - 输入文本");
    } else {
        // Fallback for basic help
        cdp_log(CDP_LOG_INFO, "HELP", "\n=== Enhanced API Help ===");
        cdp_log(CDP_LOG_INFO, "HELP", "DOM: .click, .set, .text, .html, .exists, .count, .visible");
        cdp_log(CDP_LOG_INFO, "HELP", "Batch: .texts, .attrs"); 
        cdp_log(CDP_LOG_INFO, "HELP", "Page: .url, .title, .time, .ua, .screen, .viewport");
        cdp_log(CDP_LOG_INFO, "HELP", "Action: .clear, .reload, .back, .forward");
        cdp_log(CDP_LOG_INFO, "HELP", "Use: CDP_Enhanced.exec('command') or direct JavaScript");
    }
}

/* Process user command with enhancements */
char* cdp_process_user_command(const char *input) {
    static char last_result[8192];
    
    // Safety check
    if (!input || !*input) {
        return NULL;
    }
    
    // Try JS Enhanced API first (faster and more feature-rich)
    if (input[0] == '.') {
        // Built-in meta commands handled here before JS
        if (strncmp(input, ".ref ", 5) == 0) {
            const char *args = input + 5;
            // split selector and optional name
            char sel[1024]; char name[128] = {0};
            const char *sp = strrchr(args, ' ');
            if (sp && (sp > args)) {
                // treat last token as name only if starts with '@' or only one word and selector contains spaces
                if (sp[1] == '@' || strchr(args, ' ') != NULL) {
                    strncpy(sel, args, (size_t)(sp - args)); sel[sp - args] = '\0';
                    strncpy(name, sp + 1, sizeof(name)-1);
                } else {
                    snprintf(sel, sizeof(sel), "%s", args);
                }
            } else {
                snprintf(sel, sizeof(sel), "%s", args);
            }
            const char *token = ref_add(sel, name[0]?name:NULL);
            if (token) snprintf(last_result, sizeof(last_result), "%s -> %s", token, sel);
            else snprintf(last_result, sizeof(last_result), "ref failed");
            return last_result;
        }
        if (strcmp(input, ".refs") == 0) {
            refs_list(last_result, sizeof(last_result));
            return last_result;
        }
        if (strncmp(input, ".unref ", 7) == 0) {
            const char *name = input + 7;
            int rc = ref_remove(name);
            snprintf(last_result, sizeof(last_result), rc==0?"removed %s":"not found: %s", name);
            return last_result;
        }
        if (strncmp(input, ".timeout", 8) == 0) {
            const char *args = input + 8; while (*args==' ') args++;
            if (strncmp(args, "get", 3) == 0 || *args=='\0') {
                snprintf(last_result, sizeof(last_result), "%d", g_user_timeout_ms);
            } else if (strncmp(args, "set", 3) == 0) {
                int ms = atoi(args+3);
                if (ms > 0) { g_user_timeout_ms = ms; g_ctx.config.timeout_ms = ms; snprintf(last_result, sizeof(last_result), "ok:%d", ms); }
                else snprintf(last_result, sizeof(last_result), "invalid timeout");
            } else {
                int ms = atoi(args);
                if (ms > 0) { g_user_timeout_ms = ms; g_ctx.config.timeout_ms = ms; snprintf(last_result, sizeof(last_result), "ok:%d", ms); }
                else snprintf(last_result, sizeof(last_result), "usage: .timeout get|set <ms> or .timeout <ms>");
            }
            return last_result;
        }
        if (strncmp(input, ".wait_network_idle", 18) == 0) {
            // parse idle_ms and timeout
            int idle_ms = 500; int timeout = g_user_timeout_ms; int interval = 100;
            const char *p = input + 18; while (*p==' ') p++;
            if (*p) { idle_ms = atoi(p); if (idle_ms<=0) idle_ms=500; }
            // predicate: no inflight and ms_since_activity >= idle_ms
            // loop
            struct timespec start, now; clock_gettime(CLOCK_MONOTONIC, &start);
            int rc = -1;
            while (1) {
                if (cdp_net_inflight()==0 && cdp_net_ms_since_activity() >= idle_ms) { rc = 0; break; }
                clock_gettime(CLOCK_MONOTONIC, &now);
                long elapsed = (now.tv_sec - start.tv_sec) * 1000L + (now.tv_nsec - start.tv_nsec) / 1000000L;
                if (elapsed >= timeout) { rc = -1; break; }
                usleep(interval * 1000);
            }
            snprintf(last_result, sizeof(last_result), rc==0?"idle":"timeout");
            return last_result;
        }
        if (strncmp(input, ".wait ", 6) == 0) {
            char cmd[1024];
            const char *args = input + 6; while (*args==' ') args++;
            // parse selector and condition
            // expected: <selector> [exists|visible|hidden|count>=N] [timeout=ms] [interval=ms]
            char selector[512]; selector[0]='\0';
            char cond[128] = "exists"; int timeout=g_user_timeout_ms; int interval=100; int countN=-1;
            // naive parse: selector until first option keyword
            const char *kw = strstr(args, " timeout=");
            const char *kw2 = strstr(args, " interval=");
            size_t endSel = strlen(args);
            if (kw && (size_t)(kw-args) < endSel) endSel = (size_t)(kw-args);
            if (kw2 && (size_t)(kw2-args) < endSel) endSel = (size_t)(kw2-args);
            strncpy(selector, args, endSel < sizeof(selector)-1 ? endSel : sizeof(selector)-1);
            selector[endSel < sizeof(selector)-1 ? endSel : sizeof(selector)-1] = '\0';
            // extract cond from selector tail if present
            char *sp = strrchr(selector, ' ');
            if (sp && sp > selector) {
                if (strstr(sp+1, "exists")||strstr(sp+1, "visible")||strstr(sp+1, "hidden")||strstr(sp+1, "count>=")) {
                    snprintf(cond, sizeof(cond), "%s", sp+1);
                    *sp='\0';
                }
            }
            // parse options
            const char *opt = args + endSel; 
            while (*opt) {
                if (strncmp(opt, " timeout=", 9)==0) { timeout = atoi(opt+9); }
                if (strncmp(opt, " interval=", 10)==0) { interval = atoi(opt+10); }
                opt++;
            }
            // expand @refs in selector
            char sel_exp[1024]; expand_ref_tokens(selector, sel_exp, sizeof(sel_exp));
            // build probe command
            if (strncmp(cond, "visible", 7)==0) snprintf(cmd, sizeof(cmd), ".visible %s", sel_exp);
            else if (strncmp(cond, "hidden", 6)==0) snprintf(cmd, sizeof(cmd), ".visible %s", sel_exp);
            else if (strncmp(cond, "count>=", 7)==0) { countN = atoi(cond+7); snprintf(cmd, sizeof(cmd), ".count %s", sel_exp); }
            else snprintf(cmd, sizeof(cmd), ".exists %s", sel_exp);

            int want_hidden = strncmp(cond, "hidden", 6)==0; int is_count = (countN>=0); int wantN = countN;
            struct timespec start2, now2; clock_gettime(CLOCK_MONOTONIC, &start2);
            int rc = -1;
            while (1) {
                char outv[256]; if (cdp_execute_enhanced_command(cmd, outv, sizeof(outv))!=0) {}
                int ok = 0;
                if (is_count) { int v = atoi(outv); ok = (v >= wantN); }
                else { int truth = (strcmp(outv, "true")==0 || strcmp(outv, "1")==0); ok = want_hidden ? !truth : truth; }
                if (ok) { rc = 0; break; }
                clock_gettime(CLOCK_MONOTONIC, &now2);
                long elapsed = (now2.tv_sec - start2.tv_sec) * 1000L + (now2.tv_nsec - start2.tv_nsec) / 1000000L;
                if (elapsed >= timeout) { rc = -1; break; }
                usleep(interval * 1000);
            }
            snprintf(last_result, sizeof(last_result), rc==0?"ok":"timeout");
            return last_result;
        }
        if (strncmp(input, ".until ", 7) == 0) {
            const char *expr = input + 7; while (*expr==' ') expr++;
            int timeout=g_user_timeout_ms, interval=100;
            // allow suffix options like timeout= interval=
            // simple: ignore parsing; use default
            struct timespec start3, now3; clock_gettime(CLOCK_MONOTONIC, &start3);
            int rc = -1;
            while (1) {
                char code[4096]; snprintf(code, sizeof(code), "!!(%s)", expr);
                char *res = execute_javascript(code);
                int truth = res && (strcmp(res, "true")==0 || strcmp(res, "1")==0);
                if (truth) { rc = 0; break; }
                clock_gettime(CLOCK_MONOTONIC, &now3);
                long elapsed = (now3.tv_sec - start3.tv_sec) * 1000L + (now3.tv_nsec - start3.tv_nsec) / 1000000L;
                if (elapsed >= timeout) { rc = -1; break; }
                usleep(interval * 1000);
            }
            snprintf(last_result, sizeof(last_result), rc==0?"ok":"timeout");
            return last_result;
        }

        if (strncmp(input, ".logs", 5) == 0) {
            const char *args = input + 5; while (*args==' ') args++;
            if (strncmp(args, "on", 2) == 0) { cdp_logs_set_enabled(1); snprintf(last_result, sizeof(last_result), "logs:on"); }
            else if (strncmp(args, "off", 3) == 0) { cdp_logs_set_enabled(0); snprintf(last_result, sizeof(last_result), "logs:off"); }
            else if (strncmp(args, "clear", 5) == 0) { cdp_logs_clear(); snprintf(last_result, sizeof(last_result), "logs:cleared"); }
            else if (strncmp(args, "tail", 4) == 0) {
                int n = 50; const char *filter = NULL; const char *p = args + 4; while (*p==' ') p++;
                if (*p) { n = atoi(p); if (n<=0) n=50; }
                cdp_logs_tail(last_result, sizeof(last_result), n, filter);
            } else {
                snprintf(last_result, sizeof(last_result), "logs:%s", cdp_logs_get_enabled()?"on":"off");
            }
            return last_result;
        }

        if (strncmp(input, ".cookie", 7) == 0) {
            const char *args = input + 7; while (*args==' ') args++;
            wait_document_ready(g_user_timeout_ms);
            if (strncmp(args, "list", 4)==0) {
                char *res = exec_js_with_retry("document.cookie", g_user_timeout_ms);
                snprintf(last_result, sizeof(last_result), "%s", res?res:"" );
            } else if (strncmp(args, "get ", 4)==0) {
                const char *name = args + 4;
                char js[1024]; snprintf(js, sizeof(js),
                    "(function(n){const m=document.cookie.split(';').map(s=>s.trim().split('=')).find(x=>x[0]==n);return m?decodeURIComponent(m[1]||''):''})(%s)",
                    name);
                char *res = exec_js_with_retry(js, g_user_timeout_ms);
                snprintf(last_result, sizeof(last_result), "%s", res?res:"");
            } else if (strncmp(args, "set ", 4)==0) {
                const char *kv = args + 4; const char *sp = strchr(kv, ' ');
                if (sp) {
                    char k[256]; char v[512];
                    strncpy(k, kv, sp-kv); k[sp-kv]='\0'; snprintf(v, sizeof(v), "%s", sp+1);
                    char js[1024]; snprintf(js, sizeof(js),
                        "(function(k,v){document.cookie=k+'='+encodeURIComponent(v)+'; path=/'; return 1})(%s,%s)", k, v);
                    char *res = exec_js_with_retry(js, g_user_timeout_ms); (void)res;
                    snprintf(last_result, sizeof(last_result), "ok");
                } else snprintf(last_result, sizeof(last_result), "usage: .cookie set <k> <v>");
            } else if (strncmp(args, "del ", 4)==0) {
                const char *name = args + 4;
                char js[1024]; snprintf(js, sizeof(js),
                    "(function(n){document.cookie=n+'=; expires=Thu, 01 Jan 1970 00:00:00 GMT; path=/'; return 1})(%s)", name);
                char *res = exec_js_with_retry(js, g_user_timeout_ms); (void)res;
                snprintf(last_result, sizeof(last_result), "ok");
            } else {
                snprintf(last_result, sizeof(last_result), "usage: .cookie list|get <name>|set <k> <v>|del <name>");
            }
            return last_result;
        }

        if (strncmp(input, ".local ", 7) == 0 || strncmp(input, ".session ", 9) == 0) {
            int isLocal = input[1]=='l';
            const char *args = input + (isLocal?7:9); while (*args==' ') args++;
            const char *store = isLocal?"localStorage":"sessionStorage";
            wait_document_ready(g_user_timeout_ms);
            if (strncmp(args, "keys", 4)==0) {
                char js[512]; snprintf(js, sizeof(js), "(function(){try{return Object.keys(%s)}catch(e){return []}})()", store);
                char *res = exec_js_with_retry(js, g_user_timeout_ms); snprintf(last_result, sizeof(last_result), "%s", res?res:"");
            } else if (strncmp(args, "get ", 4)==0) {
                const char *k = args+4; char js[1024]; snprintf(js, sizeof(js), "(function(){try{return %s.getItem(%s)||''}catch(e){return ''}})()", store, k);
                char *res = exec_js_with_retry(js, g_user_timeout_ms); snprintf(last_result, sizeof(last_result), "%s", res?res:"");
            } else if (strncmp(args, "set ", 4)==0) {
                const char *kv = args+4; const char *sp = strchr(kv, ' ');
                if (sp) { char k[256]; char v[512]; strncpy(k, kv, sp-kv); k[sp-kv]='\0'; snprintf(v,sizeof(v),"%s", sp+1);
                    char js[1024]; snprintf(js,sizeof(js), "(function(){try{%s.setItem(%s,%s);return 1}catch(e){return 0}})()", store, k, v);
                    char *res = exec_js_with_retry(js, g_user_timeout_ms); (void)res; snprintf(last_result,sizeof(last_result),"ok");
                } else snprintf(last_result,sizeof(last_result),"usage: .%s set <k> <v>", isLocal?"local":"session");
            } else if (strncmp(args, "del ", 4)==0) {
                const char *k = args+4; char js[512]; snprintf(js, sizeof(js), "(function(){try{%s.removeItem(%s);return 1}catch(e){return 0}})()", store, k);
                char *res = exec_js_with_retry(js, g_user_timeout_ms); (void)res; snprintf(last_result, sizeof(last_result), "ok");
            } else {
                snprintf(last_result, sizeof(last_result), "usage: .%s keys|get <k>|set <k> <v>|del <k>", isLocal?"local":"session");
            }
            return last_result;
        }

        // frames management
        if (strncmp(input, ".frames", 7) == 0) {
            const char *args = input + 7; while (*args==' ') args++;
            if (strncmp(args, "list", 4)==0 || *args=='\0') {
                char resp[32768];
                cdp_page_enable();
                int ok = 0;
                for (int i=0;i<10;i++) {
                    ok = (cdp_call_cmd("Page.getFrameTree", QUOTE({}), resp, sizeof(resp), g_ctx.config.timeout_ms)==0);
                    if (ok) break;
                    usleep(100*1000);
                }
                if (ok) {
                    size_t off=0; last_result[0]='\0';
                    off += snprintf(last_result+off, sizeof(last_result)-off, "=== Frames ===\n");
                    const char *iter = resp;
                    while ((iter = strstr(iter, JKEYQ("frameId")))) {
                        iter += 11; char id[128]={0}; const char *end=strchr(iter,'"'); if (!end) break; size_t n=end-iter; if (n>120) n=120; strncpy(id, iter, n); id[n]='\0';
                        const char *u=strstr(end, JKEYQ("url")); char url[256]={0}; if (u){u+=7; const char *ue=strchr(u,'"'); if (ue){size_t m=ue-u; if (m>240) m=240; strncpy(url,u,m); url[m]='\0';}}
                        off += snprintf(last_result+off, sizeof(last_result)-off, "%s  %s\n", id, url);
                        iter = end;
                    }
                    return last_result;
                }
                snprintf(last_result, sizeof(last_result), "err: getFrameTree");
                return last_result;
            } else if (strncmp(args, "switch ", 7)==0) {
                const char *fid = args+7;
                int rc = cdp_select_frame_by_id(fid);
                if (rc==0) snprintf(last_result, sizeof(last_result), "frame:%s", fid);
                else snprintf(last_result, sizeof(last_result), "frame not found: %s", fid);
                return last_result;
            } else if (strncmp(args, "current", 7)==0) {
                char fid[128]; cdp_get_selected_frame(fid, sizeof(fid));
                snprintf(last_result, sizeof(last_result), "frame:%s", *fid?fid:"main");
                return last_result;
            } else {
                snprintf(last_result, sizeof(last_result), "usage: .frames list|switch <frameId>|current");
                return last_result;
            }
        }

        // windows/targets list
        if (strncmp(input, ".windows", 8) == 0) {
            const char *args = input + 8; while (*args==' ') args++;
            if (strncmp(args, "list", 4)==0 || *args=='\0') {
                char resp[65536];
                if (cdp_call_cmd("Target.getTargets", QUOTE({}), resp, sizeof(resp), g_ctx.config.timeout_ms)==0) {
                    size_t off=0; off+=snprintf(last_result+off,sizeof(last_result)-off,"=== Targets ===\n");
                    const char *iter = resp; int count=0;
                    while ((iter = strstr(iter, JKEYQ("targetId")))) {
                        iter += 12; char id[128]={0}; const char *e=strchr(iter,'"'); if (!e) break; size_t n=e-iter; if (n>120) n=120; strncpy(id,iter,n); id[n]='\0';
                        const char *t = strstr(e, JKEYQ("type")); char typ[32]={0}; if (t){t+=8; const char *te=strchr(t,'"'); if (te){size_t m=te-t; if (m>30) m=30; strncpy(typ,t,m); typ[m]='\0';}}
                        const char *u = strstr(e, JKEYQ("url")); char url[256]={0}; if (u){u+=7; const char *ue=strchr(u,'"'); if (ue){size_t m=ue-u; if (m>240) m=240; strncpy(url,u,m); url[m]='\0';}}
                        off += snprintf(last_result+off, sizeof(last_result)-off, "%s  [%s]  %s\n", id, typ, url);
                        count++; iter = e;
                    }
                    if (count==0) snprintf(last_result,sizeof(last_result),"no targets or not permitted");
                    return last_result;
                }
                snprintf(last_result,sizeof(last_result),"Target.getTargets not available");
                return last_result;
            } else if (strncmp(args, "switch ", 7)==0) {
                const char *tid = args+7; while (*tid==' ') tid++;
                char params[256]; snprintf(params, sizeof(params), QUOTE({"targetId":"%s"}), tid);
                int rc = cdp_send_cmd("Target.activateTarget", params) >= 0 ? 0 : -1;
                snprintf(last_result, sizeof(last_result), rc==0?"activated %s":"activate failed: %s", tid, tid);
                return last_result;
            }
            snprintf(last_result,sizeof(last_result),"usage: .windows list|switch <targetId>");
            return last_result;
        }

        // mouse actions
        if (strncmp(input, ".mouse ", 7) == 0) {
            const char *args = input+7; while (*args==' ') args++;
            if (strncmp(args, "move ", 5)==0 || strncmp(args, "click ", 6)==0) {
                int is_click = (args[0]=='c');
                const char *p = args + (is_click?6:5);
                int x = atoi(p);
                const char *sp = strchr(p, ' '); if (!sp) { snprintf(last_result,sizeof(last_result),"usage: .mouse %s x y [left|right|middle]", is_click?"click":"move"); return last_result; }
                int y = atoi(sp+1);
                const char *btn = is_click? strrchr(p,' '): NULL; const char *button = "left";
                if (is_click && btn && btn > sp) { button = btn+1; }
                char params[256];
                if (!is_click) {
                    cdp_js_build_mouse_event("mouseMoved", x, y, NULL, params, sizeof(params));
                    cdp_send_cmd("Input.dispatchMouseEvent", params);
                    snprintf(last_result,sizeof(last_result),"ok"); return last_result;
                } else {
                    cdp_js_build_mouse_event("mousePressed", x, y, button, params, sizeof(params));
                    cdp_send_cmd("Input.dispatchMouseEvent", params);
                    cdp_js_build_mouse_event("mouseReleased", x, y, button, params, sizeof(params));
                    cdp_send_cmd("Input.dispatchMouseEvent", params);
                    snprintf(last_result,sizeof(last_result),"ok"); return last_result;
                }
            }
            snprintf(last_result,sizeof(last_result),"usage: .mouse move x y | .mouse click x y [left|right|middle]");
            return last_result;
        }

        // key input
        if (strncmp(input, ".keys ", 6) == 0) {
            const char *txt = input+6; while (*txt==' ') txt++;
            char params[1024];
            snprintf(params, sizeof(params), QUOTE({"text":%s}), txt);
            cdp_send_cmd("Input.insertText", params);
            snprintf(last_result,sizeof(last_result),"ok"); return last_result;
        }

        // Expand @refs before sending to JS
        char expanded[4096]; expand_ref_tokens(input, expanded, sizeof(expanded));
        char js_result[8192];
        int js_rc = cdp_execute_enhanced_command(expanded, js_result, sizeof(js_result));
        if (js_rc >= 0) {
            // Successfully handled by JS Enhanced API
            strncpy(last_result, js_result, sizeof(last_result)-1);
            last_result[sizeof(last_result)-1] = '\0';
            return last_result;
        }
        // Fall back to C implementation if JS fails
    }
    
    // Special commands
    if (strcmp(input, ".help") == 0) {
        cdp_show_shortcuts();
        return NULL;
    }
    if (strcmp(input, ".stats") == 0) {
        cdp_show_stats();
        return NULL;
    }
    
    // Enhanced performance tracking with timestamps
    struct timespec start_time, js_start, js_end, beautify_start, beautify_end;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    if (verbose) {
        cdp_log(CDP_LOG_DEBUG, "PERF", "Command start: %s", input);
    }
    
    // Execute JavaScript with detailed timing
    clock_gettime(CLOCK_MONOTONIC, &js_start);
    char *result = execute_javascript(input);
    clock_gettime(CLOCK_MONOTONIC, &js_end);
    
    double js_time_ms = (js_end.tv_sec - js_start.tv_sec) * 1000.0 + 
                       (js_end.tv_nsec - js_start.tv_nsec) / 1000000.0;
    
    if (verbose) {
        cdp_log(CDP_LOG_DEBUG, "PERF", "JS execution: %.3f ms", js_time_ms);
    }
    
    // Track beautification time
    clock_gettime(CLOCK_MONOTONIC, &beautify_start);
    
    // Beautify output with timing
    if (result && *result) {
        char *beautified = cdp_beautify_output(result);
        clock_gettime(CLOCK_MONOTONIC, &beautify_end);
        
        double beautify_time_ms = (beautify_end.tv_sec - beautify_start.tv_sec) * 1000.0 + 
                                 (beautify_end.tv_nsec - beautify_start.tv_nsec) / 1000000.0;
        
        if (beautified) {
            str_copy_safe(last_result, beautified, sizeof(last_result));
            if (beautified != result && beautified != last_result) {
                free(beautified);
            }
            
            // Total execution time
            struct timespec end_time;
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            double total_time_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + 
                                  (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
            
            // Track legacy performance
            cdp_perf_track(total_time_ms);
            
            if (verbose) {
                cdp_log(CDP_LOG_DEBUG, "PERF", "Beautification: %.3f ms", beautify_time_ms);
                cdp_log(CDP_LOG_DEBUG, "PERF", "Total execution: %.3f ms", total_time_ms);
            }
            
            return last_result;
        }
    }
    
    // Final timing for non-beautified results
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double total_time_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + 
                          (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
    
    cdp_perf_track(total_time_ms);
    
    if (verbose) {
        cdp_log(CDP_LOG_DEBUG, "PERF", "Total execution: %.3f ms (no beautification)", total_time_ms);
    }
    
    return result;
}

/* DISABLED: Helper injection causes crashes on Windows */
int cdp_inject_helpers(void) {
    // DO NOT INJECT HELPERS - CAUSES CRASHES
    return 0;
}
