/*
 * mod_co - Coroutine Module for cosmorun
 *
 * ARCHITECTURE: Host-Compiled Builtin API Wrapper
 *
 * This module provides a friendly API layer that wraps the host-compiled
 * builtin coroutine runtime. All actual coroutine logic (setjmp/longjmp,
 * stack-copying) happens in cosmo_co.c (cosmopolitan-compiled), while
 * this TCC-compiled code only makes API calls.
 *
 * WHY: TCC's stack frame layout is incompatible with system ABI, making
 * setjmp/longjmp fail in TCC functions. By keeping all coroutine logic
 * in the host, we completely avoid this issue.
 *
 * FILES:
 * - cosmo_co.c:74-290 - Host builtin coroutine runtime
 * - cosmo_tcc.c:102-109 - Builtin API declarations
 * - cosmo_tcc.c:1143-1149 - Symbol registration
 * - THIS FILE - TCC-layer friendly wrapper
 */

#include "src/cosmo_libc.h"

/* ============================================================================
 * Host Builtin Coroutine API (Exposed via tcc_add_symbol)
 * ========================================================================= */

typedef void (*co_builtin_func_t)(void* arg);

extern void* __co_builtin_create(co_builtin_func_t func, void* arg);
extern void* __co_builtin_resume_api(void* handle);
extern int __co_builtin_yield(void* value);
extern void __co_builtin_free(void* handle);
extern int __co_builtin_state(void* handle);
extern int __co_builtin_is_alive(void* handle);

/* Coroutine states (must match cosmo_co.c) */
typedef enum {
    CO_CREATED = 0,
    CO_RUNNING = 1,
    CO_SUSPENDED = 2,
    CO_TERMINATED = 3
} co_state_t;

/* ============================================================================
 * Friendly Wrapper API
 * ========================================================================= */

typedef struct co_t co_t;
typedef void* (*co_func_t)(void* arg);

/* Opaque coroutine handle */
struct co_t {
    void* handle;  /* Pointer to host __co_builtin_t */
};

/* Global state (for co_current() and co_yield_current()) */
static co_t* g_current_wrapper = NULL;

/* Forward declaration for error reporting */
static void co_set_error(const char* msg);

/* ============================================================================
 * Public API Implementation
 * ========================================================================= */

/**
 * Create a new coroutine
 * @param func - Coroutine function (will be called from host code)
 * @param arg - Argument to pass to func
 * @return New coroutine handle, or NULL on error
 */
co_t* co_new(co_func_t func, void* arg) {
    if (!func) {
        co_set_error("co_new: function pointer is NULL");
        return NULL;
    }

    void* handle = __co_builtin_create((co_builtin_func_t)func, arg);
    if (!handle) {
        co_set_error("co_new: failed to create builtin coroutine");
        return NULL;
    }

    co_t* co = (co_t*)malloc(sizeof(co_t));
    if (!co) {
        __co_builtin_free(handle);
        co_set_error("co_new: out of memory");
        return NULL;
    }

    co->handle = handle;
    co_set_error(NULL);  // Clear error on success
    return co;
}

/**
 * Start or resume a coroutine
 * @param co - Coroutine to resume
 * @return Value passed to co_yield_current(), or NULL on termination
 */
void* co_start(co_t* co) {
    if (!co || !co->handle) {
        co_set_error("co_start: coroutine handle is NULL");
        return NULL;
    }

    co_t* prev = g_current_wrapper;
    g_current_wrapper = co;

    void* ret = __co_builtin_resume_api(co->handle);

    g_current_wrapper = prev;
    co_set_error(NULL);  // Clear error on success
    return ret;
}

/**
 * Yield from current coroutine
 * @param value - Value to return to caller
 *
 * IMPORTANT: This can ONLY be called from within a coroutine function
 * (i.e., the function passed to co_new()). Calling from main() will fail.
 */
void co_yield_current(void* value) {
    __co_builtin_yield(value);
}

/**
 * Get current coroutine
 * @return Current coroutine, or NULL if called from main context
 */
co_t* co_current(void) {
    return g_current_wrapper;
}

/**
 * Free coroutine resources
 * @param co - Coroutine to free
 */
void co_free(co_t* co) {
    if (!co) return;

    if (co->handle) {
        __co_builtin_free(co->handle);
    }

    free(co);
}

/**
 * Get coroutine state
 * @param co - Coroutine
 * @return State (CO_CREATED, CO_RUNNING, CO_SUSPENDED, CO_TERMINATED)
 */
co_state_t co_state(co_t* co) {
    if (!co || !co->handle) return CO_TERMINATED;
    return (co_state_t)__co_builtin_state(co->handle);
}

/**
 * Check if coroutine is alive
 * @param co - Coroutine
 * @return 1 if alive (not terminated), 0 otherwise
 */
int co_is_alive(co_t* co) {
    if (!co || !co->handle) return 0;
    return __co_builtin_is_alive(co->handle);
}

/* Stack size management (not applicable for copy-stack, kept for API compatibility) */
static unsigned long g_default_stack_size = 65536;  // 64KB default (unused in copy-stack)

void co_set_default_stack_size(unsigned long bytes) {
    g_default_stack_size = bytes;  // Stored but not used in copy-stack implementation
}

unsigned long co_get_default_stack_size(void) {
    return g_default_stack_size;
}

/* Error reporting */
static const char* g_last_error = NULL;

static void co_set_error(const char* msg) {
    g_last_error = msg;
}

const char* co_last_error(void) {
    return g_last_error;
}

/* ============================================================================
 * Module API (for __import support)
 * ========================================================================= */

typedef struct {
    co_t* (*co_new)(co_func_t func, void* arg);
    void* (*co_start)(co_t* co);
    void (*co_yield_current)(void* value);
    co_t* (*co_current)(void);
    void (*co_free)(co_t* co);
    co_state_t (*co_state)(co_t* co);
    int (*co_is_alive)(co_t* co);
    void (*co_set_default_stack_size)(unsigned long bytes);
    unsigned long (*co_get_default_stack_size)(void);
    const char* (*co_last_error)(void);
} co_api;

co_api* co_module_init(void) {
    static co_api api = {
        .co_new = co_new,
        .co_start = co_start,
        .co_yield_current = co_yield_current,
        .co_current = co_current,
        .co_free = co_free,
        .co_state = co_state,
        .co_is_alive = co_is_alive,
        .co_set_default_stack_size = co_set_default_stack_size,
        .co_get_default_stack_size = co_get_default_stack_size,
        .co_last_error = co_last_error,
    };
    return &api;
}

/* ============================================================================
 * Usage Example
 * ========================================================================= */

/*
void my_coroutine(void* arg) {
    int max = (int)(long)arg;
    printf("Coroutine started, max=%d\n", max);

    for (int i = 1; i <= max; i++) {
        printf("  Count: %d\n", i);
        co_yield_current((void*)(long)i);
    }

    printf("Coroutine finished\n");
}

int main() {
    co_t* co = co_new(my_coroutine, (void*)3);

    while (co_is_alive(co)) {
        void* ret = co_start(co);
        printf("Returned: %p\n", ret);
    }

    co_free(co);
    return 0;
}
*/
