// cosmo_co.c - Built-in coroutine support for cosmorun
// Compiled with system compiler (gcc/clang) into cosmorun.exe
// Exposed to TCC-compiled modules via tcc_add_symbol

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

// ARM64 coroutine context (matches libcorpc layout)
typedef struct {
    uint64_t regs[15];  // x30/LR, SP, x0, x1, x19-x28, x29/FP
} __co_ctx_t;

// Assembly functions (implemented in __co_ctx_arm64.S or __co_ctx_x64.S)
// These are declared here but defined in architecture-specific .S files
extern void __co_ctx_init(__co_ctx_t* ctx, void (*entry_func)(void), void* stack_top);
extern void __co_ctx_swap(__co_ctx_t* curr, __co_ctx_t* next);

// Stack allocation structure with guard pages (for Linux/future use)
// NOTE: Current copy-stack implementation doesn't use external stacks
// This is kept for future stackful coroutine implementations on Linux
typedef struct {
    void*  map_base;      // Base address of mmap'd region
    size_t map_len;       // Total length including guard pages
    uint8_t* usable_base; // Start of usable stack area
    size_t usable_len;    // Length of usable stack area
    size_t guard_len;     // Length of each guard page region
} __co_stack_t;

// Allocate stack with guard pages (returns 0 on success, -1 on error)
// WARNING: Do NOT use on macOS for syscalls - macOS only allows syscalls on pthread stacks
int __co_stack_alloc_ex(size_t usable_bytes, size_t guard_bytes, __co_stack_t* out) {
    if (!out) return -1;

    long ps = sysconf(_SC_PAGESIZE);
    if (ps <= 0) ps = 4096;  // Fallback to common page size

    // Round up to page boundaries
    size_t g = guard_bytes ? ((guard_bytes + ps - 1) / ps * ps) : (size_t)ps;
    size_t u = (usable_bytes + ps - 1) / ps * ps;
    size_t total = g + u + g;  // Guard + Usable + Guard

    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    // Do NOT use MAP_JIT - that's for executable code, not stacks

    // Allocate entire region as PROT_NONE (no access)
    void* p = mmap(NULL, total, PROT_NONE, flags, -1, 0);
    if (p == MAP_FAILED) return -1;

    // Enable R/W access for usable region only
    if (mprotect((uint8_t*)p + g, u, PROT_READ | PROT_WRITE) != 0) {
        munmap(p, total);
        return -1;
    }

    out->map_base = p;
    out->map_len = total;
    out->usable_base = (uint8_t*)p + g;
    out->usable_len = u;
    out->guard_len = g;
    return 0;
}

// Free stack allocated with __co_stack_alloc_ex
void __co_stack_free_ex(__co_stack_t* st) {
    if (!st || !st->map_base) return;
    munmap(st->map_base, st->map_len);
    memset(st, 0, sizeof(*st));
}

// Legacy simple allocator (kept for compatibility, deprecated)
void* __co_stack_alloc(size_t size) {
    __co_stack_t st;
    if (__co_stack_alloc_ex(size, 0, &st) != 0) {
        return NULL;
    }
    // WARNING: Caller cannot properly free this! Use __co_stack_alloc_ex instead
    return st.usable_base;
}

// Legacy simple free (cannot work correctly without size - deprecated)
void __co_stack_free(void* stack, size_t size) {
    if (!stack || size == 0) return;
    munmap(stack, size);
}

// Simple alloca implementation for stack-copying coroutines
// This adjusts SP and returns the allocated space
void* __co_alloca(size_t size) {
    // Use compiler builtin
    return __builtin_alloca(size);
}

// Read current stack pointer (for stack-copying coroutines)
// IMPORTANT: Using frame address instead of SP to avoid TCC inline asm issues
void* __co_read_sp(void) {
    return __builtin_frame_address(0);
}

// Builtin setjmp/longjmp wrappers (to bypass TCC issues)
// These are compiled with system compiler and exposed to TCC-compiled code
int __co_setjmp(jmp_buf env) {
    return setjmp(env);
}

void __co_longjmp(jmp_buf env, int val) {
    longjmp(env, val);
}

// ============================================================================
// Pure Builtin Coroutine Implementation (System-Compiled)
// ============================================================================
// This implementation runs entirely in system-compiled code to bypass TCC
// setjmp/longjmp incompatibility. TCC code only calls through function pointers.

#include <pthread.h>

#if defined(__x86_64__)
  #define CO_REDZONE 128u
#else
  #define CO_REDZONE 0u
#endif

typedef void* (*co_builtin_func_t)(void* arg);

typedef enum {
    CO_BUILTIN_CREATED,
    CO_BUILTIN_RUNNING,
    CO_BUILTIN_SUSPENDED,
    CO_BUILTIN_TERMINATED
} co_builtin_state_t;

// Resume policy: BARRIER (fast, caller must isolate) vs SHIELD (safe, protects caller)
typedef enum {
    CO_POLICY_BARRIER = 0,  // No caller protection (faster, but caller's locals may be corrupted)
    CO_POLICY_SHIELD = 1    // Protect caller's stack (default, safe for general use)
} co_policy_t;

typedef struct __co_builtin_t {
    co_builtin_func_t entry;
    void* arg;
    void* return_value;
    co_builtin_state_t state;

    jmp_buf jb_self;    // Coroutine's continuation point
    jmp_buf jb_caller;  // Caller's return point

    // Coroutine stack snapshot
    uint8_t* stack_top;
    size_t stack_len;
    uint8_t* stack_buf;
    size_t stack_cap;

    // Host (caller) stack shadow for SHIELD policy
    uint8_t* host_top;
    size_t host_len;
    uint8_t* host_shadow;
    size_t host_cap;

    pthread_t owner;    // Thread that owns this coroutine (for safety)
} __co_builtin_t;

// Thread-local storage for current coroutine (thread-safe)
static _Thread_local __co_builtin_t* g_builtin_current = NULL;

// Thread-local SHIELD state (for protecting caller stack during resume)
// This avoids multi-coroutine conflicts by storing SHIELD state per-thread
typedef struct {
    uint8_t* shadow_buf;
    size_t shadow_cap;
    uint8_t* saved_top;
    size_t saved_len;
    int active;  // 1 if SHIELD is active for current resume
} __co_shield_state_t;

static _Thread_local __co_shield_state_t g_shield = {NULL, 0, NULL, 0, 0};

// Check if coroutine is owned by current thread (prevents cross-thread access)
static inline int __co_check_owner(__co_builtin_t* co) {
    return pthread_equal(co->owner, pthread_self());
}

// Get pthread stack bounds (cross-platform)
static void __co_get_stack_bounds(uint8_t** low, uint8_t** top) {
    pthread_attr_t attr;
    if (pthread_getattr_np(pthread_self(), &attr) != 0) {
        *low = *top = NULL;
        return;
    }

    void* stackaddr;
    size_t stacksize;
    if (pthread_attr_getstack(&attr, &stackaddr, &stacksize) != 0) {
        pthread_attr_destroy(&attr);
        *low = *top = NULL;
        return;
    }

    *low = (uint8_t*)stackaddr;
    *top = (uint8_t*)stackaddr + stacksize;
    pthread_attr_destroy(&attr);
}

// Adaptive buffer shrinking (called after yield to prevent unbounded growth)
static inline void __co_try_shrink(uint8_t** buf, size_t* cap, size_t target) {
    if (*buf && *cap >= (target << 2)) {  // Only shrink if 4x oversized
        size_t newcap = target ? (target << 1) : 4096;  // 2x target or 4KB minimum
        uint8_t* p = (uint8_t*)realloc(*buf, newcap);
        if (p) {
            *buf = p;
            *cap = newcap;
        }
    }
}

// Free coroutine buffers (called on termination or explicit free)
static void __co_free_buffers(__co_builtin_t* co) {
    if (!co) return;

    free(co->stack_buf);
    co->stack_buf = NULL;
    co->stack_cap = 0;
    co->stack_len = 0;

    free(co->host_shadow);
    co->host_shadow = NULL;
    co->host_cap = 0;
    co->host_len = 0;
    co->host_top = NULL;
}

// Yield from coroutine (ONLY called from system-compiled code)
static int __co_builtin_yield_internal(__co_builtin_t* co, void* value) {
    co->return_value = value;

    uint8_t *low, *top;
    __co_get_stack_bounds(&low, &top);

    void* sp = __builtin_frame_address(0);
    uint8_t* want_low = (uint8_t*)sp;
    if ((size_t)(want_low - low) > CO_REDZONE) {
        want_low -= CO_REDZONE;
    } else {
        want_low = low;
    }

    // Save continuation point
    if (setjmp(co->jb_self) != 0) {
        // Resumed - return to coroutine
        return 0;
    }

    // Save coroutine stack
    co->stack_top = top;
    co->stack_len = (size_t)(top - want_low);

    if (co->stack_len > co->stack_cap) {
        size_t cap = co->stack_cap ? co->stack_cap : 4096;
        while (cap < co->stack_len) cap <<= 1;
        co->stack_buf = (uint8_t*)realloc(co->stack_buf, cap);
        co->stack_cap = cap;
        if (!co->stack_buf) return -1;
    }

    memcpy(co->stack_buf, want_low, co->stack_len);

    // Adaptive shrinking after save
    __co_try_shrink(&co->stack_buf, &co->stack_cap, co->stack_len);

    // SHIELD: Restore caller's stack from thread-local state
    if (g_shield.active && g_shield.shadow_buf && g_shield.saved_len > 0) {
        uint8_t* target_low = g_shield.saved_top - g_shield.saved_len;
        memcpy(target_low, g_shield.shadow_buf, g_shield.saved_len);
    }

    // Jump back to caller
    longjmp(co->jb_caller, 1);
}

// Resume coroutine (BARRIER policy - no caller protection)
// NOTE: For SHIELD policy, protection is handled in resume_with
static void __co_builtin_resume(__co_builtin_t* co, co_policy_t policy) {
    (void)policy;  // Unused - SHIELD handled in resume_with

    uint8_t *low, *top;
    __co_get_stack_bounds(&low, &top);

    uint8_t* target_low = co->stack_top - co->stack_len;
    if (!(low <= target_low && co->stack_top <= top)) {
        return;
    }

    // Move SP below target
    uint8_t* cur = (uint8_t*)__builtin_frame_address(0);
    if (cur > target_low) {
        size_t need = (size_t)(cur - target_low) + 256;
        volatile uint8_t* sink = (volatile uint8_t*)__builtin_alloca(need);
        (void)sink;
    }

    // Restore coroutine stack
    memcpy(target_low, co->stack_buf, co->stack_len);

    // Jump to coroutine
    longjmp(co->jb_self, 1);
}

// Coroutine entry wrapper
static void __co_builtin_entry(__co_builtin_t* co) {
    // Call user function and capture return value
    co->return_value = co->entry(co->arg);
    co->state = CO_BUILTIN_TERMINATED;

    // SHIELD: Restore caller's stack from thread-local state
    if (g_shield.active && g_shield.shadow_buf && g_shield.saved_len > 0) {
        uint8_t* target_low = g_shield.saved_top - g_shield.saved_len;
        memcpy(target_low, g_shield.shadow_buf, g_shield.saved_len);
    }

    // Immediately free buffers on termination (prevents memory bloat)
    __co_free_buffers(co);

    // Directly return to caller
    longjmp(co->jb_caller, 1);
}

// Public API: Create coroutine
void* __co_builtin_create(co_builtin_func_t func, void* arg) {
    if (!func) return NULL;

    __co_builtin_t* co = (__co_builtin_t*)calloc(1, sizeof(__co_builtin_t));
    if (!co) return NULL;

    co->entry = func;
    co->arg = arg;
    co->state = CO_BUILTIN_CREATED;
    co->owner = pthread_self();  // Record owner thread

    return (void*)co;
}

// Public API: Start/Resume coroutine with policy
void* __co_builtin_resume_with(void* handle, co_policy_t policy) {
    __co_builtin_t* co = (__co_builtin_t*)handle;
    if (!co || co->state == CO_BUILTIN_TERMINATED) {
        return co ? co->return_value : NULL;
    }

    // Check ownership: prevent cross-thread resume
    if (!__co_check_owner(co)) {
        return NULL;  // Error: trying to resume from wrong thread
    }

    __co_builtin_t* prev = g_builtin_current;
    g_builtin_current = co;

    // SHIELD: Save caller's stack region to thread-local state (for current resume)
    g_shield.active = 0;
    if (policy == CO_POLICY_SHIELD && co->state == CO_BUILTIN_SUSPENDED) {
        uint8_t *low, *top;
        __co_get_stack_bounds(&low, &top);

        // Calculate the region that will be overwritten by coroutine stack restore
        uint8_t* target_low = co->stack_top - co->stack_len;
        if (low <= target_low && co->stack_top <= top) {
            g_shield.saved_top = co->stack_top;
            g_shield.saved_len = co->stack_len;

            // Allocate/resize buffer if needed
            if (g_shield.saved_len > g_shield.shadow_cap) {
                size_t cap = g_shield.shadow_cap ? g_shield.shadow_cap : 4096;
                while (cap < g_shield.saved_len) cap <<= 1;
                g_shield.shadow_buf = (uint8_t*)realloc(g_shield.shadow_buf, cap);
                g_shield.shadow_cap = cap;
            }

            // Save caller's stack region that will be overwritten
            if (g_shield.shadow_buf) {
                memcpy(g_shield.shadow_buf, target_low, g_shield.saved_len);
                g_shield.active = 1;  // Mark as active for this resume
            }
        }
    }

    // Save caller's return point
    if (setjmp(co->jb_caller) != 0) {
        // Returned from coroutine
        g_builtin_current = prev;
        return co->return_value;
    }

    if (co->state == CO_BUILTIN_CREATED) {
        // First run - no SHIELD needed (no stack to overwrite yet)
        co->state = CO_BUILTIN_RUNNING;
        __co_builtin_entry(co);
    } else if (co->state == CO_BUILTIN_SUSPENDED) {
        // Resume with BARRIER policy (SHIELD already saved in this function)
        co->state = CO_BUILTIN_RUNNING;
        __co_builtin_resume(co, CO_POLICY_BARRIER);
    }

    // Should not reach here
    co->host_len = 0;
    g_builtin_current = prev;
    return NULL;
}

// Public API: Start/Resume coroutine (default SHIELD policy)
// SHIELD protects caller's stack from being corrupted by resume operations
void* __co_builtin_resume_api(void* handle) {
    return __co_builtin_resume_with(handle, CO_POLICY_SHIELD);
}

// Public API: Fast resume (BARRIER policy - caller must ensure isolation)
void* __co_builtin_resume_fast(void* handle) {
    return __co_builtin_resume_with(handle, CO_POLICY_BARRIER);
}

// Public API: Yield (called from TCC-compiled code)
int __co_builtin_yield(void* value) {
    if (!g_builtin_current) return -1;

    __co_builtin_t* co = g_builtin_current;

    // Check ownership: prevent cross-thread yield
    if (!__co_check_owner(co)) {
        return -1;  // Error: trying to yield from wrong thread
    }
    co->state = CO_BUILTIN_SUSPENDED;

    int ret = __co_builtin_yield_internal(co, value);
    if (ret == 0) {
        // Resumed
        co->state = CO_BUILTIN_RUNNING;
    }

    return ret;
}

// Public API: Free coroutine
void __co_builtin_free(void* handle) {
    __co_builtin_t* co = (__co_builtin_t*)handle;
    if (!co) return;

    __co_free_buffers(co);
    free(co);
}

// Public API: Get state
int __co_builtin_state(void* handle) {
    __co_builtin_t* co = (__co_builtin_t*)handle;
    return co ? (int)co->state : (int)CO_BUILTIN_TERMINATED;
}

// Public API: Is alive
int __co_builtin_is_alive(void* handle) {
    __co_builtin_t* co = (__co_builtin_t*)handle;
    return co && co->state != CO_BUILTIN_TERMINATED;
}
