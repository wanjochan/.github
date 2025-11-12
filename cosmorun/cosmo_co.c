// cosmo_co.c - Stackful Coroutine Builtins for cosmorun
// Compiled with system compiler (gcc/clang) into cosmorun.exe
// Exposed to TCC-compiled modules via tcc_add_symbol
//
// ARCHITECTURE: Stackful coroutines with dedicated malloc'd stacks
// Each coroutine has its own independent stack, eliminating the stack
// corruption issues found in copy-stack implementations.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

// ============================================================================
// Architecture-Specific Context Definitions
// ============================================================================

#if defined(__aarch64__) || defined(__arm64__)
    #define CO_ARCH_ARM64
    // ARM64 context structure
    typedef struct {
        void* x30;  // 0x00: Link register/PC
        void* sp;   // 0x08: Stack pointer
        void* x29;  // 0x10: Frame pointer
        void* x19;  // 0x18: Callee-saved registers start
        void* x20;  // 0x20
        void* x21;  // 0x28
        void* x22;  // 0x30
        void* x23;  // 0x38
        void* x24;  // 0x40
        void* x25;  // 0x48
        void* x26;  // 0x50
        void* x27;  // 0x58
        void* x28;  // 0x60
    } co_ctx_t;

    typedef unsigned long co_uintptr_t;

#elif defined(__x86_64__) || defined(__amd64__)
    #define CO_ARCH_X64
    // x64 System V ABI context structure
    typedef struct {
        void* rip;  // Return address
        void* rsp;  // Stack pointer
        void* rbp;  // Base pointer
        void* rbx;  // Callee-saved
        void* r12;
        void* r13;
        void* r14;
        void* r15;
    } co_ctx_t;

    typedef unsigned long co_uintptr_t;

#else
    #error "Unsupported architecture for coroutines"
#endif

// ============================================================================
// Constants
// ============================================================================

#define CO_MIN_STACK_SIZE (4 * 1024)
#define CO_DEFAULT_STACK_SIZE (128 * 1024)
#define CO_PAGE_SIZE 4096

// ============================================================================
// Coroutine State and Structure
// ============================================================================

typedef void* (*co_builtin_func_t)(void* arg);

typedef enum {
    CO_BUILTIN_CREATED = 0,
    CO_BUILTIN_RUNNING = 1,
    CO_BUILTIN_SUSPENDED = 2,
    CO_BUILTIN_TERMINATED = 3
} co_builtin_state_t;

typedef struct __co_builtin_t {
    co_ctx_t ctx;                // Execution context
    co_builtin_state_t state;    // Current state
    co_builtin_func_t entry;     // Entry function
    void* arg;                   // Function argument
    void* return_value;          // Return value
    void* stack;                 // Allocated stack
    size_t stack_size;           // Stack size
    pthread_t owner;             // Owner thread (for safety)
} __co_builtin_t;

// Thread-local storage for current coroutine (thread-safe)
static _Thread_local __co_builtin_t* g_builtin_current = NULL;

// Main coroutine (represents the caller's context)
static _Thread_local __co_builtin_t* g_main_co = NULL;

// ============================================================================
// Thread Safety
// ============================================================================

// Check if coroutine is owned by current thread
static inline int __co_check_owner(__co_builtin_t* co) {
    return pthread_equal(co->owner, pthread_self());
}

// ============================================================================
// Context Switching - ARM64
// ============================================================================

#ifdef CO_ARCH_ARM64

// Helper macros for label generation
#define CO_STRINGIFY(x) #x
#define CO_TOSTRING(x) CO_STRINGIFY(x)

// Initialize context for a new coroutine
static inline void __co_ctx_init(co_ctx_t* ctx, void* entry, co_uintptr_t sp_val) {
    co_uintptr_t aligned = sp_val & ~15UL;  // 16-byte alignment
    memset(ctx, 0, sizeof(co_ctx_t));
    ctx->x30 = entry;
    ctx->sp = (void*)aligned;
    ctx->x29 = (void*)aligned;
}

// HYBRID context swap: Save full context, restore minimal
// Used for first launch from main to coroutine
#define __CO_CTX_SWAP_HYBRID(from_ptr, to_ptr, label_num) do { \
    register void *_from_reg __asm__("x0") = (from_ptr); \
    register void *_to_reg __asm__("x1") = (to_ptr); \
    __asm__ volatile( \
        /* SAVE current context - FULL (all registers) */ \
        "adr x9, " CO_TOSTRING(label_num) "f\n" \
        "mov x10, sp\n" \
        "stp x9, x10, [%0]\n"         /* PC, SP at 0x00 */ \
        "str x29, [%0, #0x10]\n"       /* FP at 0x10 */ \
        "stp x19, x20, [%0, #0x18]\n"  /* x19-x28 start at 0x18 */ \
        "stp x21, x22, [%0, #0x28]\n" \
        "stp x23, x24, [%0, #0x38]\n" \
        "stp x25, x26, [%0, #0x48]\n" \
        "stp x27, x28, [%0, #0x58]\n" \
        /* RESTORE target context - MINIMAL (only PC/SP/FP, inherit x19-x28) */ \
        "ldp x9, x10, [%1]\n"         /* Restore PC, SP */ \
        "ldr x29, [%1, #0x10]\n"       /* Restore FP */ \
        "mov sp, x10\n" \
        "br x9\n" \
        CO_TOSTRING(label_num) ":\n" \
        : "+r"(_from_reg), "+r"(_to_reg) \
        : \
        : "x9", "x10", "x29", "memory" \
    ); \
} while(0)

// FULL context swap: Save and restore all registers
// Used for yield/resume within coroutines
#define __CO_CTX_SWAP_FULL(from_ptr, to_ptr, label_num) do { \
    register void *_from_reg __asm__("x0") = (from_ptr); \
    register void *_to_reg __asm__("x1") = (to_ptr); \
    __asm__ volatile( \
        /* Save current context */ \
        "adr x9, " CO_TOSTRING(label_num) "f\n" \
        "mov x10, sp\n" \
        "stp x9, x10, [%0]\n"         /* PC, SP at 0x00 */ \
        "str x29, [%0, #0x10]\n"       /* FP at 0x10 */ \
        "stp x19, x20, [%0, #0x18]\n"  /* x19-x28 start at 0x18 */ \
        "stp x21, x22, [%0, #0x28]\n" \
        "stp x23, x24, [%0, #0x38]\n" \
        "stp x25, x26, [%0, #0x48]\n" \
        "stp x27, x28, [%0, #0x58]\n" \
        /* Restore target context */ \
        "ldr x29, [%1, #0x10]\n"       /* Restore FP */ \
        "ldp x27, x28, [%1, #0x58]\n"  /* Restore x19-x28 */ \
        "ldp x25, x26, [%1, #0x48]\n" \
        "ldp x23, x24, [%1, #0x38]\n" \
        "ldp x21, x22, [%1, #0x28]\n" \
        "ldp x19, x20, [%1, #0x18]\n" \
        "ldp x9, x10, [%1]\n"         /* Restore PC, SP */ \
        "mov sp, x10\n" \
        "br x9\n" \
        CO_TOSTRING(label_num) ":\n" \
        : "+r"(_from_reg), "+r"(_to_reg) \
        : \
        : "x9", "x10", "x29", "memory" \
    ); \
} while(0)

// Restore FULL context and jump (for termination after FULL swap)
// Must restore ALL registers that were saved by FULL swap
#define __CO_CTX_RESTORE_AND_JUMP_FULL(to_ptr) do { \
    register void *_to_reg __asm__("x0") = (to_ptr); \
    __asm__ volatile( \
        "ldr x29, [%0, #0x10]\n"       /* Restore FP */ \
        "ldp x27, x28, [%0, #0x58]\n"  /* Restore x19-x28 */ \
        "ldp x25, x26, [%0, #0x48]\n" \
        "ldp x23, x24, [%0, #0x38]\n" \
        "ldp x21, x22, [%0, #0x28]\n" \
        "ldp x19, x20, [%0, #0x18]\n" \
        "ldp x9, x10, [%0]\n"         /* Restore PC, SP */ \
        "mov sp, x10\n" \
        "br x9\n" \
        : \
        : "r"(_to_reg) \
        : "x9", "x10", "x19", "x20", "x21", "x22", "x23", "x24", \
          "x25", "x26", "x27", "x28", "x29", "memory" \
    ); \
} while(0)

// Restore minimal context and jump (for termination after HYBRID swap)
#define __CO_CTX_RESTORE_AND_JUMP_MINIMAL(to_ptr) do { \
    register void *_to_reg __asm__("x0") = (to_ptr); \
    __asm__ volatile( \
        "ldp x9, x10, [%0]\n" \
        "ldr x29, [%0, #0x10]\n" \
        "mov sp, x10\n" \
        "br x9\n" \
        : \
        : "r"(_to_reg) \
        : "x9", "x10", "x29", "memory" \
    ); \
} while(0)

#endif // CO_ARCH_ARM64

// ============================================================================
// Context Switching - x86-64
// ============================================================================

#ifdef CO_ARCH_X64

// Initialize context for a new coroutine
static inline void __co_ctx_init(co_ctx_t* ctx, void* entry, co_uintptr_t sp_val) {
    // x86-64 ABI requires RSP to be (16n - 8) when entering a function
    // because 'call' pushes 8-byte return address. Since we use 'jmp',
    // we need to subtract 8 to simulate the pushed return address.
    co_uintptr_t aligned = (sp_val & ~15UL) - 8;
    memset(ctx, 0, sizeof(co_ctx_t));
    ctx->rip = entry;
    ctx->rsp = (void*)aligned;
    ctx->rbp = (void*)aligned;
}

// Context swap for x86-64
#define __CO_CTX_SWAP_HYBRID(from_ptr, to_ptr, label_num) do { \
    __asm__ volatile( \
        "leaq " #label_num "f(%%rip), %%rax\n" \
        "movq %%rax, 0x00(%0)\n" \
        "movq %%rsp, 0x08(%0)\n" \
        "movq %%rbp, 0x10(%0)\n" \
        "movq %%rbx, 0x18(%0)\n" \
        "movq %%r12, 0x20(%0)\n" \
        "movq %%r13, 0x28(%0)\n" \
        "movq %%r14, 0x30(%0)\n" \
        "movq %%r15, 0x38(%0)\n" \
        "movq 0x38(%1), %%r15\n" \
        "movq 0x30(%1), %%r14\n" \
        "movq 0x28(%1), %%r13\n" \
        "movq 0x20(%1), %%r12\n" \
        "movq 0x18(%1), %%rbx\n" \
        "movq 0x10(%1), %%rbp\n" \
        "movq 0x08(%1), %%rsp\n" \
        "movq 0x00(%1), %%rax\n" \
        "jmp *%%rax\n" \
        #label_num ":\n" \
        : \
        : "r"(from_ptr), "r"(to_ptr) \
        : "rax", "rbx", "r12", "r13", "r14", "r15", "memory" \
    ); \
} while(0)

#define __CO_CTX_SWAP_FULL __CO_CTX_SWAP_HYBRID

// Restore FULL context and jump for x86-64
#define __CO_CTX_RESTORE_AND_JUMP_FULL(to_ptr) do { \
    __asm__ volatile( \
        "movq 0x38(%0), %%r15\n" \
        "movq 0x30(%0), %%r14\n" \
        "movq 0x28(%0), %%r13\n" \
        "movq 0x20(%0), %%r12\n" \
        "movq 0x18(%0), %%rbx\n" \
        "movq 0x10(%0), %%rbp\n" \
        "movq 0x08(%0), %%rsp\n" \
        "movq 0x00(%0), %%rax\n" \
        "jmp *%%rax\n" \
        : \
        : "r"(to_ptr) \
        : "rax", "rbx", "r12", "r13", "r14", "r15", "memory" \
    ); \
} while(0)

// Restore minimal context and jump for x86-64
#define __CO_CTX_RESTORE_AND_JUMP_MINIMAL(to_ptr) do { \
    __asm__ volatile( \
        "movq 0x10(%0), %%rbp\n" \
        "movq 0x08(%0), %%rsp\n" \
        "movq 0x00(%0), %%rax\n" \
        "jmp *%%rax\n" \
        : \
        : "r"(to_ptr) \
        : "rax", "memory" \
    ); \
} while(0)

#endif // CO_ARCH_X64

// ============================================================================
// Stack Management
// ============================================================================

static inline size_t __co_calc_stack_pages(size_t size) {
    return ((size + CO_PAGE_SIZE - 1) / CO_PAGE_SIZE) * CO_PAGE_SIZE;
}

// ============================================================================
// Coroutine Entry Point
// ============================================================================

static void __co_entry_point(void) {
    __co_builtin_t* co = g_builtin_current;

    if (!co) {
        // Fatal error - should never happen
        while (1) {}
    }

    // Execute user's coroutine function
    if (co->entry) {
        co->return_value = co->entry(co->arg);
    }

    // Mark as terminated
    co->state = CO_BUILTIN_TERMINATED;

    // Switch back to main context
    // Use FULL restore to match the FULL swap used in resume (except for first launch)
    // This ensures all callee-saved registers are properly restored
    if (g_main_co) {
        g_builtin_current = g_main_co;
        __CO_CTX_RESTORE_AND_JUMP_FULL(&g_main_co->ctx);
    }

    // Should never reach here
    while (1) {}
}

// ============================================================================
// Main Coroutine Management
// ============================================================================

static __co_builtin_t* __co_get_or_create_main(void) {
    if (g_main_co) {
        return g_main_co;
    }

    g_main_co = (__co_builtin_t*)malloc(sizeof(__co_builtin_t));
    if (!g_main_co) {
        return NULL;
    }

    memset(g_main_co, 0, sizeof(__co_builtin_t));
    g_main_co->state = CO_BUILTIN_RUNNING;
    g_main_co->owner = pthread_self();

    return g_main_co;
}

// ============================================================================
// Public API Implementation
// ============================================================================

/**
 * Create a new coroutine
 * @param func - Coroutine function (will be called from host code)
 * @param arg - Argument to pass to func
 * @return New coroutine handle, or NULL on error
 */
void* __co_builtin_create(co_builtin_func_t func, void* arg) {
    if (!func) {
        return NULL;
    }

    __co_builtin_t* co = (__co_builtin_t*)malloc(sizeof(__co_builtin_t));
    if (!co) {
        return NULL;
    }

    memset(co, 0, sizeof(__co_builtin_t));
    co->entry = func;
    co->arg = arg;
    co->state = CO_BUILTIN_CREATED;
    co->owner = pthread_self();

    // Allocate stack
    co->stack_size = __co_calc_stack_pages(CO_DEFAULT_STACK_SIZE);
    co->stack = malloc(co->stack_size);
    if (!co->stack) {
        free(co);
        return NULL;
    }

    // Initialize context
    co_uintptr_t sp = (co_uintptr_t)co->stack + co->stack_size;
    __co_ctx_init(&co->ctx, __co_entry_point, sp);

    return (void*)co;
}

/**
 * Start or resume a coroutine
 * @param handle - Coroutine handle
 * @return Value passed to co_yield_current(), or NULL on termination
 */
void* __co_builtin_resume_api(void* handle) {
    __co_builtin_t* co = (__co_builtin_t*)handle;
    if (!co) {
        return NULL;
    }

    // Check ownership
    if (!__co_check_owner(co)) {
        return NULL;
    }

    // Return if already terminated
    if (co->state == CO_BUILTIN_TERMINATED) {
        return co->return_value;
    }

    // Create main coroutine if needed
    __co_builtin_t* main_co = __co_get_or_create_main();
    if (!main_co) {
        return NULL;
    }

    // Check if this is first launch (before updating state)
    int is_first_launch = (co->state == CO_BUILTIN_CREATED);

    // Update state
    if (co->state == CO_BUILTIN_CREATED) {
        co->state = CO_BUILTIN_RUNNING;
    } else if (co->state == CO_BUILTIN_SUSPENDED) {
        co->state = CO_BUILTIN_RUNNING;
    }

    // Save previous coroutine
    __co_builtin_t* prev = g_builtin_current;
    g_builtin_current = co;

    // Context switch strategy:
    // - First launch (CREATED): Use HYBRID (save full, restore minimal)
    //   New coroutine inherits x19-x28 from caller (contains GOT/TLS on macOS)
    // - Resume (SUSPENDED): Use FULL (save and restore all registers)
    //   Protects main's local variables (co, prev, etc.) in callee-saved registers
    if (is_first_launch) {
        __CO_CTX_SWAP_HYBRID(&main_co->ctx, &co->ctx, 1);
    } else {
        __CO_CTX_SWAP_FULL(&main_co->ctx, &co->ctx, 1);
    }

    // Back in main context
    g_builtin_current = prev;

    return co->return_value;
}

/**
 * Yield from current coroutine
 * @param value - Value to return to caller
 *
 * IMPORTANT: This can ONLY be called from within a coroutine function.
 * Calling from main() will fail.
 */
int __co_builtin_yield(void* value) {
    if (!g_builtin_current || g_builtin_current == g_main_co) {
        return -1;
    }

    __co_builtin_t* co = g_builtin_current;

    // Check ownership
    if (!__co_check_owner(co)) {
        return -1;
    }

    co->return_value = value;
    co->state = CO_BUILTIN_SUSPENDED;

    // Switch back to main
    g_builtin_current = g_main_co;

    __CO_CTX_SWAP_FULL(&co->ctx, &g_main_co->ctx, 2);

    // Resumed - update state
    co->state = CO_BUILTIN_RUNNING;

    return 0;
}

/**
 * Free coroutine resources
 * @param handle - Coroutine handle
 */
void __co_builtin_free(void* handle) {
    __co_builtin_t* co = (__co_builtin_t*)handle;
    if (!co) return;

    if (co == g_builtin_current) {
        // Cannot free currently running coroutine
        return;
    }

    if (co == g_main_co) {
        g_main_co = NULL;
    }

    if (co->stack) {
        free(co->stack);
    }

    free(co);
}

/**
 * Get coroutine state
 * @param handle - Coroutine handle
 * @return State (CO_BUILTIN_CREATED, CO_BUILTIN_RUNNING, CO_BUILTIN_SUSPENDED, CO_BUILTIN_TERMINATED)
 */
int __co_builtin_state(void* handle) {
    __co_builtin_t* co = (__co_builtin_t*)handle;
    return co ? (int)co->state : (int)CO_BUILTIN_TERMINATED;
}

/**
 * Check if coroutine is alive
 * @param handle - Coroutine handle
 * @return 1 if alive (not terminated), 0 otherwise
 */
int __co_builtin_is_alive(void* handle) {
    __co_builtin_t* co = (__co_builtin_t*)handle;
    return co && co->state != CO_BUILTIN_TERMINATED;
}
