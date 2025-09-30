/**
 * cdp_quickjs.c - QuickJS Integration Implementation
 *
 * Secure JavaScript execution environment with prototype pollution protection
 */

#include "cdp_quickjs.h"
#include "quickjs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* Preset configurations */
const CDPQuickJSConfig CDP_QJS_CONFIG_PARANOID = {
    .allow_eval = 0,
    .allow_function_constructor = 0,
    .freeze_prototypes = 1,
    .block_prototype_access = 1,
    .max_memory_mb = 5,
    .max_stack_size = 256 * 1024,
    .max_operations = 1000000,
    .timeout_ms = 1000,
    .max_string_len = 10000,
    .enable_console = 1,
    .enable_json = 1,
    .enable_math = 1,
    .enable_date = 0,
    .enable_regexp = 0
};

const CDPQuickJSConfig CDP_QJS_CONFIG_BALANCED = {
    .allow_eval = 1,  /* But with safe_eval wrapper */
    .allow_function_constructor = 0,
    .freeze_prototypes = 1,
    .block_prototype_access = 1,
    .max_memory_mb = 10,
    .max_stack_size = 512 * 1024,
    .max_operations = 10000000,
    .timeout_ms = 5000,
    .max_string_len = 100000,
    .enable_console = 1,
    .enable_json = 1,
    .enable_math = 1,
    .enable_date = 1,
    .enable_regexp = 1
};

const CDPQuickJSConfig CDP_QJS_CONFIG_RELAXED = {
    .allow_eval = 1,
    .allow_function_constructor = 1,
    .freeze_prototypes = 0,
    .block_prototype_access = 0,
    .max_memory_mb = 50,
    .max_stack_size = 1024 * 1024,
    .max_operations = 100000000,
    .timeout_ms = 30000,
    .max_string_len = 1000000,
    .enable_console = 1,
    .enable_json = 1,
    .enable_math = 1,
    .enable_date = 1,
    .enable_regexp = 1
};

/* Statistics tracking */
static struct {
    uint64_t total_evals;
    uint64_t failed_evals;
    uint64_t security_violations;
    uint64_t timeouts;
    uint64_t memory_limit_hits;
} g_stats = {0};

/* Helper functions */
static int64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* Interrupt handler for timeout and operation limit */
static int interrupt_handler(JSRuntime *rt, void *opaque) {
    CDPQuickJSContext *ctx = (CDPQuickJSContext *)opaque;

    /* Increment operation count more aggressively */
    ctx->operation_count += 100;  /* Count in batches for better performance */

    /* Check operation limit */
    if (ctx->config.max_operations > 0 &&
        ctx->operation_count > ctx->config.max_operations) {
        snprintf(ctx->last_error, sizeof(ctx->last_error),
                "Operation limit exceeded (%llu operations)",
                (unsigned long long)ctx->operation_count);
        ctx->error_code = -1;
        return 1;  /* Interrupt execution */
    }

    /* Check timeout */
    if (ctx->config.timeout_ms > 0) {
        int64_t elapsed = get_time_ms() - ctx->start_time_ms;
        if (elapsed > ctx->config.timeout_ms) {
            snprintf(ctx->last_error, sizeof(ctx->last_error),
                    "Execution timeout (%lld ms)", (long long)elapsed);
            ctx->error_code = -2;
            g_stats.timeouts++;
            return 1;  /* Interrupt execution */
        }
    }

    return 0;  /* Continue execution */
}

/* Safe eval implementation */
static JSValue safe_eval(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv) {
    CDPQuickJSContext *qjs_ctx = (CDPQuickJSContext *)JS_GetContextOpaque(ctx);

    /* Check recursion depth */
    if (qjs_ctx->eval_depth >= 3) {
        g_stats.security_violations++;
        return JS_ThrowRangeError(ctx, "eval recursion limit exceeded");
    }

    if (argc < 1) return JS_UNDEFINED;

    const char *code = JS_ToCString(ctx, argv[0]);
    if (!code) return JS_EXCEPTION;

    /* Basic security check */
    if (strstr(code, "__proto__") ||
        strstr(code, "constructor.prototype") ||
        strstr(code, "Object.prototype") ||
        strstr(code, "Array.prototype") ||
        strstr(code, "Function.prototype")) {
        JS_FreeCString(ctx, code);
        g_stats.security_violations++;
        return JS_ThrowSyntaxError(ctx, "Potentially unsafe code detected");
    }

    qjs_ctx->eval_depth++;
    JSValue result = JS_Eval(ctx, code, strlen(code), "<eval>", JS_EVAL_TYPE_GLOBAL);
    qjs_ctx->eval_depth--;

    JS_FreeCString(ctx, code);
    return result;
}

/* Console.log implementation */
static JSValue console_log(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
    for (int i = 0; i < argc; i++) {
        if (i > 0) printf(" ");
        const char *str = JS_ToCString(ctx, argv[i]);
        if (str) {
            printf("%s", str);
            JS_FreeCString(ctx, str);
        } else {
            printf("[object]");
        }
    }
    printf("\n");
    fflush(stdout);
    return JS_UNDEFINED;
}

/* Property getter hook to prevent prototype access */
static JSValue property_getter_hook(JSContext *ctx, JSPropertyDescriptor *desc,
                                    JSValueConst obj, JSAtom prop) {
    CDPQuickJSContext *qjs_ctx = (CDPQuickJSContext *)JS_GetContextOpaque(ctx);

    if (!qjs_ctx->config.block_prototype_access) {
        return JS_UNDEFINED;
    }

    const char *prop_str = JS_AtomToCString(ctx, prop);
    if (!prop_str) return JS_UNDEFINED;

    /* Block dangerous property access */
    if (strcmp(prop_str, "__proto__") == 0 ||
        strcmp(prop_str, "__lookupGetter__") == 0 ||
        strcmp(prop_str, "__lookupSetter__") == 0 ||
        strcmp(prop_str, "__defineGetter__") == 0 ||
        strcmp(prop_str, "__defineSetter__") == 0) {
        JS_FreeCString(ctx, prop_str);
        g_stats.security_violations++;
        return JS_ThrowTypeError(ctx, "Access to %s is forbidden", prop_str);
    }

    JS_FreeCString(ctx, prop_str);
    return JS_UNDEFINED;
}

/* Setup security hooks and freeze prototypes */
static void setup_security(CDPQuickJSContext *ctx) {
    JSValue global = JS_GetGlobalObject(ctx->context);

    /* Remove or replace dangerous globals */
    if (!ctx->config.allow_eval) {
        JS_SetPropertyStr(ctx->context, global, "eval", JS_UNDEFINED);
    } else {
        /* Replace with safe eval */
        JS_SetPropertyStr(ctx->context, global, "eval",
                         JS_NewCFunction(ctx->context, safe_eval, "eval", 1));
    }

    if (!ctx->config.allow_function_constructor) {
        JS_SetPropertyStr(ctx->context, global, "Function", JS_UNDEFINED);
        JS_SetPropertyStr(ctx->context, global, "AsyncFunction", JS_UNDEFINED);
        JS_SetPropertyStr(ctx->context, global, "GeneratorFunction", JS_UNDEFINED);
        JS_SetPropertyStr(ctx->context, global, "AsyncGeneratorFunction", JS_UNDEFINED);
    }

    /* Remove dangerous features */
    JS_SetPropertyStr(ctx->context, global, "WebAssembly", JS_UNDEFINED);
    JS_SetPropertyStr(ctx->context, global, "Atomics", JS_UNDEFINED);
    JS_SetPropertyStr(ctx->context, global, "SharedArrayBuffer", JS_UNDEFINED);

    /* Setup console if enabled */
    if (ctx->config.enable_console) {
        JSValue console = JS_NewObject(ctx->context);
        JS_SetPropertyStr(ctx->context, console, "log",
                         JS_NewCFunction(ctx->context, console_log, "log", 1));
        JS_SetPropertyStr(ctx->context, global, "console", console);
    }

    /* Conditionally disable features */
    if (!ctx->config.enable_date) {
        JS_SetPropertyStr(ctx->context, global, "Date", JS_UNDEFINED);
    }

    if (!ctx->config.enable_regexp) {
        JS_SetPropertyStr(ctx->context, global, "RegExp", JS_UNDEFINED);
    }

    /* Freeze prototypes if configured */
    if (ctx->config.freeze_prototypes) {
        const char *freeze_code =
            "(() => {"
            "  'use strict';"
            "  const prototypes = ["
            "    Object.prototype,"
            "    Array.prototype,"
            "    Function.prototype,"
            "    String.prototype,"
            "    Number.prototype,"
            "    Boolean.prototype,"
            "    Error.prototype"
            "  ];"
            "  for (const proto of prototypes) {"
            "    if (proto && typeof proto === 'object') {"
            "      Object.freeze(proto);"
            "      Object.seal(proto);"
            "      Object.preventExtensions(proto);"
            "    }"
            "  }"
            "  Object.freeze(Object);"
            "  Object.freeze(Array);"
            "  Object.freeze(Function);"
            "})();";

        JSValue result = JS_Eval(ctx->context, freeze_code, strlen(freeze_code),
                                "<freeze>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(ctx->context, result);
        ctx->prototype_locked = 1;
    }

    /* Block __proto__ setter if configured */
    if (ctx->config.block_prototype_access) {
        const char *block_proto =
            "delete Object.prototype.__proto__;"
            "delete Object.prototype.__lookupGetter__;"
            "delete Object.prototype.__lookupSetter__;"
            "delete Object.prototype.__defineGetter__;"
            "delete Object.prototype.__defineSetter__;";

        JSValue result = JS_Eval(ctx->context, block_proto, strlen(block_proto),
                                "<block_proto>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(ctx->context, result);
    }

    JS_FreeValue(ctx->context, global);
}

/* Create a new secure context */
CDPQuickJSContext* cdp_qjs_create_context(const CDPQuickJSConfig *config) {
    CDPQuickJSContext *ctx = calloc(1, sizeof(CDPQuickJSContext));
    if (!ctx) return NULL;

    /* Copy configuration */
    if (config) {
        ctx->config = *config;
    } else {
        ctx->config = CDP_QJS_CONFIG_BALANCED;
    }

    /* Create runtime */
    ctx->runtime = JS_NewRuntime();
    if (!ctx->runtime) {
        free(ctx);
        return NULL;
    }

    /* Set memory limit */
    if (ctx->config.max_memory_mb > 0) {
        JS_SetMemoryLimit(ctx->runtime, ctx->config.max_memory_mb * 1024 * 1024);
    }

    /* Set interrupt handler */
    JS_SetInterruptHandler(ctx->runtime, interrupt_handler, ctx);

    /* Create context */
    ctx->context = JS_NewContext(ctx->runtime);
    if (!ctx->context) {
        JS_FreeRuntime(ctx->runtime);
        free(ctx);
        return NULL;
    }

    /* Store context pointer for callbacks */
    JS_SetContextOpaque(ctx->context, ctx);

    /* Setup security */
    setup_security(ctx);

    return ctx;
}

/* Destroy context */
void cdp_qjs_destroy_context(CDPQuickJSContext *ctx) {
    if (!ctx) return;

    if (ctx->context) {
        JS_FreeContext(ctx->context);
    }

    if (ctx->runtime) {
        JS_FreeRuntime(ctx->runtime);
    }

    free(ctx);
}

/* Reset context state */
int cdp_qjs_reset_context(CDPQuickJSContext *ctx) {
    if (!ctx) return -1;

    /* Reset counters */
    ctx->operation_count = 0;
    ctx->eval_depth = 0;
    ctx->start_time_ms = 0;
    ctx->error_code = 0;
    ctx->last_error[0] = '\0';

    /* Run garbage collection */
    JS_RunGC(ctx->runtime);

    return 0;
}

/* Evaluate JavaScript code */
int cdp_qjs_eval(CDPQuickJSContext *ctx, const char *code, char *result, size_t result_size) {
    if (!ctx || !code || !result) return -1;

    /* Reset state */
    ctx->operation_count = 0;
    ctx->start_time_ms = get_time_ms();
    ctx->error_code = 0;

    g_stats.total_evals++;

    /* Evaluate code */
    JSValue val = JS_Eval(ctx->context, code, strlen(code),
                         "<input>", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(val)) {
        JSValue exception = JS_GetException(ctx->context);
        const char *err = JS_ToCString(ctx->context, exception);

        snprintf(result, result_size, "Error: %s", err ? err : "unknown error");
        snprintf(ctx->last_error, sizeof(ctx->last_error), "%s", err ? err : "unknown error");

        if (err) JS_FreeCString(ctx->context, err);
        JS_FreeValue(ctx->context, exception);
        JS_FreeValue(ctx->context, val);

        g_stats.failed_evals++;
        ctx->error_code = -1;
        return -1;
    }

    /* Convert result to string */
    const char *str = JS_ToCString(ctx->context, val);
    if (str) {
        strncpy(result, str, result_size - 1);
        result[result_size - 1] = '\0';
        JS_FreeCString(ctx->context, str);
    } else {
        /* Try JSON stringify for objects */
        JSValue json = JS_JSONStringify(ctx->context, val, JS_UNDEFINED, JS_UNDEFINED);
        if (!JS_IsException(json)) {
            const char *json_str = JS_ToCString(ctx->context, json);
            if (json_str) {
                strncpy(result, json_str, result_size - 1);
                result[result_size - 1] = '\0';
                JS_FreeCString(ctx->context, json_str);
            } else {
                strncpy(result, "[object]", result_size - 1);
                result[result_size - 1] = '\0';
            }
        } else {
            strncpy(result, "undefined", result_size - 1);
            result[result_size - 1] = '\0';
        }
        JS_FreeValue(ctx->context, json);
    }

    JS_FreeValue(ctx->context, val);
    return 0;
}

/* Evaluate and return JSON */
int cdp_qjs_eval_json(CDPQuickJSContext *ctx, const char *code, char *json_result, size_t result_size) {
    if (!ctx || !code || !json_result) return -1;

    /* Reset state */
    ctx->operation_count = 0;
    ctx->start_time_ms = get_time_ms();

    /* Evaluate code */
    JSValue val = JS_Eval(ctx->context, code, strlen(code),
                         "<input>", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(val)) {
        JSValue exception = JS_GetException(ctx->context);
        const char *err = JS_ToCString(ctx->context, exception);

        snprintf(json_result, result_size, "{\"error\":\"%s\"}",
                err ? err : "unknown error");

        if (err) JS_FreeCString(ctx->context, err);
        JS_FreeValue(ctx->context, exception);
        JS_FreeValue(ctx->context, val);
        return -1;
    }

    /* Convert to JSON */
    JSValue json = JS_JSONStringify(ctx->context, val, JS_UNDEFINED, JS_UNDEFINED);

    if (JS_IsException(json)) {
        strncpy(json_result, "{\"error\":\"Failed to stringify result\"}", result_size - 1);
        JS_FreeValue(ctx->context, val);
        JS_FreeValue(ctx->context, json);
        return -1;
    }

    const char *json_str = JS_ToCString(ctx->context, json);
    if (json_str) {
        strncpy(json_result, json_str, result_size - 1);
        json_result[result_size - 1] = '\0';
        JS_FreeCString(ctx->context, json_str);
    } else {
        strncpy(json_result, "null", result_size - 1);
    }

    JS_FreeValue(ctx->context, val);
    JS_FreeValue(ctx->context, json);
    return 0;
}

/* Check if code is safe */
int cdp_qjs_is_safe_code(const char *code) {
    if (!code) return 0;

    /* List of dangerous patterns */
    const char *dangerous[] = {
        "__proto__",
        "constructor.prototype",
        "Object.prototype",
        "Array.prototype",
        "Function.prototype",
        "eval(",
        "Function(",
        "setTimeout",
        "setInterval",
        "setImmediate",
        "WebAssembly",
        "import(",
        "require(",
        "process.",
        "global.",
        "window.",
        "document.",
        NULL
    };

    for (int i = 0; dangerous[i]; i++) {
        if (strstr(code, dangerous[i])) {
            return 0;  /* Not safe */
        }
    }

    return 1;  /* Appears safe */
}

/* Get last error message */
const char* cdp_qjs_get_last_error(CDPQuickJSContext *ctx) {
    if (!ctx) return "Invalid context";
    return ctx->last_error[0] ? ctx->last_error : "No error";
}

/* Get statistics */
int cdp_qjs_get_stats(CDPQuickJSContext *ctx, CDPQuickJSStats *stats) {
    if (!ctx || !stats) return -1;

    stats->total_evals = g_stats.total_evals;
    stats->failed_evals = g_stats.failed_evals;
    stats->security_violations = g_stats.security_violations;
    stats->timeouts = g_stats.timeouts;
    stats->memory_limit_hits = g_stats.memory_limit_hits;
    stats->current_memory_usage = 0;  /* TODO: Get from JS_GetMemoryUsage */
    stats->total_operations = ctx->operation_count;

    return 0;
}

/* CDP-specific: Evaluate CSS selector */
int cdp_qjs_eval_selector(CDPQuickJSContext *ctx, const char *selector, char *result, size_t result_size) {
    if (!ctx || !selector || !result) return -1;

    /* Build JavaScript code to evaluate selector */
    char code[4096];
    snprintf(code, sizeof(code),
            "(() => {"
            "  try {"
            "    const elements = document.querySelectorAll('%s');"
            "    return {"
            "      count: elements.length,"
            "      first: elements[0] ? {"
            "        tag: elements[0].tagName,"
            "        id: elements[0].id,"
            "        className: elements[0].className,"
            "        text: elements[0].textContent.substring(0, 100)"
            "      } : null"
            "    };"
            "  } catch(e) {"
            "    return {error: e.message};"
            "  }"
            "})()", selector);

    return cdp_qjs_eval_json(ctx, code, result, result_size);
}

/* Transform CDP response with JS code */
int cdp_qjs_transform_response(CDPQuickJSContext *ctx, const char *response,
                               const char *transform_code, char *result, size_t result_size) {
    if (!ctx || !response || !transform_code || !result) return -1;

    /* Set response as global variable */
    char *setup_code = malloc(strlen(response) + 1024);
    if (!setup_code) return -1;

    sprintf(setup_code, "const response = %s; %s", response, transform_code);

    int ret = cdp_qjs_eval(ctx, setup_code, result, result_size);

    free(setup_code);
    return ret;
}

/* ============================================================ */
/*                     JSON Processing Functions               */
/* ============================================================ */

/* Global JSON context - shared for all JSON operations */
static CDPQuickJSContext *g_json_ctx = NULL;

/* Initialize JSON processing context */
int cdp_json_init(void) {
    if (g_json_ctx) return 0;  /* Already initialized */

    g_json_ctx = cdp_qjs_create_context(&CDP_QJS_CONFIG_BALANCED);
    if (!g_json_ctx) return -1;

    /* Add helper functions */
    const char *helpers =
        "function extractValue(json, path) {"
        "  try {"
        "    const obj = typeof json === 'string' ? JSON.parse(json) : json;"
        "    const parts = path.split('.');"
        "    let result = obj;"
        "    for (const part of parts) {"
        "      if (result === null || result === undefined) return null;"
        "      result = result[part];"
        "    }"
        "    return result;"
        "  } catch(e) { return null; }"
        "}";

    char result[256];
    return cdp_qjs_eval(g_json_ctx, helpers, result, sizeof(result));
}

/* Cleanup JSON processing context */
void cdp_json_cleanup(void) {
    if (g_json_ctx) {
        cdp_qjs_destroy_context(g_json_ctx);
        g_json_ctx = NULL;
    }
}

/* Parse JSON and extract string value */
int cdp_json_get_string(const char *json, const char *field, char *out, size_t out_size) {
    if (!g_json_ctx && cdp_json_init() < 0) return -1;
    if (!json || !field || !out) return -1;

    // Escape the JSON string for safe embedding in JavaScript
    char escaped_json[8192];
    int j = 0;
    for (int i = 0; json[i] && j < sizeof(escaped_json) - 2; i++) {
        if (json[i] == '\\') {
            escaped_json[j++] = '\\';
            escaped_json[j++] = '\\';
        } else if (json[i] == '`') {
            escaped_json[j++] = '\\';
            escaped_json[j++] = '`';
        } else {
            escaped_json[j++] = json[i];
        }
    }
    escaped_json[j] = '\0';

    char code[16384];
    snprintf(code, sizeof(code),
        "(() => {"
        "  try {"
        "    const obj = JSON.parse(`%s`);"
        "    const value = extractValue(obj, '%s');"
        "    return value === null || value === undefined ? '' : String(value);"
        "  } catch(e) { return ''; }"
        "})()", escaped_json, field);

    return cdp_qjs_eval(g_json_ctx, code, out, out_size);
}

/* Parse JSON and extract integer value */
int cdp_json_get_int(const char *json, const char *field, int *out) {
    char value[64];
    if (cdp_json_get_string(json, field, value, sizeof(value)) < 0) return -1;
    if (strlen(value) == 0) return -1;
    *out = atoi(value);
    return 0;
}

/* Parse JSON and extract boolean value */
int cdp_json_get_bool(const char *json, const char *field, int *out) {
    char value[32];
    if (cdp_json_get_string(json, field, value, sizeof(value)) < 0) return -1;
    *out = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
    return 0;
}

/* Extract nested value using dot notation */
int cdp_json_get_nested(const char *json, const char *path, char *out, size_t out_size) {
    return cdp_json_get_string(json, path, out, out_size);
}

/* Pretty print JSON */
int cdp_json_beautify(const char *json, char *out, size_t out_size) {
    if (!g_json_ctx && cdp_json_init() < 0) return -1;

    char code[8192];
    snprintf(code, sizeof(code),
        "(() => {"
        "  try {"
        "    const obj = JSON.parse(`%s`);"
        "    return JSON.stringify(obj, null, 2);"
        "  } catch(e) { return 'Invalid JSON'; }"
        "})()", json);

    return cdp_qjs_eval(g_json_ctx, code, out, out_size);
}

/* Find target with specific URL in Target.getTargets response */
int cdp_json_find_target_with_url(const char *json, const char *url, char *target_id, size_t size) {
    if (!json || !url || !target_id) return -1;
    if (!g_json_ctx && cdp_json_init() < 0) return -1;

    // Escape the JSON for safe embedding
    char escaped_json[16384];
    int j = 0;
    for (int i = 0; json[i] && j < sizeof(escaped_json) - 2; i++) {
        if (json[i] == '\\') {
            escaped_json[j++] = '\\';
            escaped_json[j++] = '\\';
        } else if (json[i] == '`') {
            escaped_json[j++] = '\\';
            escaped_json[j++] = '`';
        } else if (json[i] == '\n') {
            escaped_json[j++] = '\\';
            escaped_json[j++] = 'n';
        } else if (json[i] == '\r') {
            escaped_json[j++] = '\\';
            escaped_json[j++] = 'r';
        } else {
            escaped_json[j++] = json[i];
        }
    }
    escaped_json[j] = '\0';

    char code[20000];
    snprintf(code, sizeof(code),
        "(function() {\n"
        "  try {\n"
        "    const data = JSON.parse(`%s`);\n"
        "    const targets = Array.isArray(data) ? data : (data.targetInfos || []);\n"
        "    const target = targets.find(t => t.url === '%s');\n"
        "    return target ? target.targetId : '';\n"
        "  } catch(e) { return ''; }\n"
        "})()", escaped_json, url);

    char result[256];
    int ret = cdp_qjs_eval(g_json_ctx, code, result, sizeof(result));

    if (ret != 0 || strlen(result) == 0) {
        return -1;
    }

    strncpy(target_id, result, size - 1);
    target_id[size - 1] = '\0';
    return 0;
}