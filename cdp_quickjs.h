/**
 * cdp_quickjs.h - QuickJS Integration for CDP
 *
 * Provides a secure JavaScript execution environment for CDP operations
 * with built-in protections against prototype pollution and other attacks.
 */

#ifndef CDP_QUICKJS_H
#define CDP_QUICKJS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct JSRuntime JSRuntime;
typedef struct JSContext JSContext;
typedef struct JSValue JSValue;

/* Security configuration for QuickJS context */
typedef struct {
    /* Security settings */
    int allow_eval;                 /* Allow eval() function */
    int allow_function_constructor; /* Allow new Function() */
    int freeze_prototypes;          /* Freeze built-in prototypes */
    int block_prototype_access;     /* Block __proto__ access */

    /* Resource limits */
    size_t max_memory_mb;           /* Maximum memory in MB */
    size_t max_stack_size;          /* Maximum stack size */
    uint64_t max_operations;        /* Maximum operations before interrupt */
    int timeout_ms;                 /* Execution timeout in milliseconds */
    size_t max_string_len;          /* Maximum string length */

    /* Feature flags */
    int enable_console;             /* Enable console.log */
    int enable_json;                /* Enable JSON.parse/stringify */
    int enable_math;                /* Enable Math functions */
    int enable_date;                /* Enable Date object */
    int enable_regexp;              /* Enable RegExp */
} CDPQuickJSConfig;

/* Secure JavaScript context */
typedef struct {
    JSRuntime *runtime;
    JSContext *context;
    CDPQuickJSConfig config;

    /* Security state */
    int eval_depth;                 /* Current eval recursion depth */
    int prototype_locked;           /* Prototypes are frozen */
    uint64_t operation_count;       /* Current operation count */

    /* Timing */
    int64_t start_time_ms;          /* Execution start time */

    /* Error handling */
    char last_error[1024];          /* Last error message */
    int error_code;                 /* Last error code */
} CDPQuickJSContext;

/* Preset security configurations */
extern const CDPQuickJSConfig CDP_QJS_CONFIG_PARANOID;  /* Maximum security */
extern const CDPQuickJSConfig CDP_QJS_CONFIG_BALANCED;  /* Balanced security/features */
extern const CDPQuickJSConfig CDP_QJS_CONFIG_RELAXED;   /* More permissive */

/* Context management */
CDPQuickJSContext* cdp_qjs_create_context(const CDPQuickJSConfig *config);
void cdp_qjs_destroy_context(CDPQuickJSContext *ctx);
int cdp_qjs_reset_context(CDPQuickJSContext *ctx);

/* Code execution */
int cdp_qjs_eval(CDPQuickJSContext *ctx, const char *code, char *result, size_t result_size);
int cdp_qjs_eval_json(CDPQuickJSContext *ctx, const char *code, char *json_result, size_t result_size);
int cdp_qjs_call_function(CDPQuickJSContext *ctx, const char *func_name, const char *args_json, char *result, size_t result_size);

/* Global object management */
int cdp_qjs_set_global(CDPQuickJSContext *ctx, const char *name, const char *value);
int cdp_qjs_get_global(CDPQuickJSContext *ctx, const char *name, char *value, size_t value_size);
int cdp_qjs_delete_global(CDPQuickJSContext *ctx, const char *name);

/* Security utilities */
int cdp_qjs_is_safe_code(const char *code);
int cdp_qjs_sanitize_code(const char *input, char *output, size_t output_size);
const char* cdp_qjs_get_last_error(CDPQuickJSContext *ctx);

/* CDP-specific utilities */
int cdp_qjs_eval_selector(CDPQuickJSContext *ctx, const char *selector, char *result, size_t result_size);
int cdp_qjs_transform_response(CDPQuickJSContext *ctx, const char *response, const char *transform_code, char *result, size_t result_size);

/* Statistics and debugging */
typedef struct {
    uint64_t total_evals;
    uint64_t failed_evals;
    uint64_t security_violations;
    uint64_t timeouts;
    uint64_t memory_limit_hits;
    size_t current_memory_usage;
    uint64_t total_operations;
} CDPQuickJSStats;

int cdp_qjs_get_stats(CDPQuickJSContext *ctx, CDPQuickJSStats *stats);
void cdp_qjs_dump_context(CDPQuickJSContext *ctx);

/* JSON processing utilities (lightweight, no external deps) */
int cdp_json_init(void);
void cdp_json_cleanup(void);
int cdp_json_get_string(const char *json, const char *field, char *out, size_t out_size);
int cdp_json_get_int(const char *json, const char *field, int *out);
int cdp_json_get_bool(const char *json, const char *field, int *out);
int cdp_json_get_nested(const char *json, const char *path, char *out, size_t out_size);
int cdp_json_beautify(const char *json, char *out, size_t out_size);
int cdp_json_find_target_with_url(const char *json, const char *url, char *target_id, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* CDP_QUICKJS_H */