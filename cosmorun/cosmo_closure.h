#ifndef COSMO_CLOSURE_H
#define COSMO_CLOSURE_H

#include "cosmo_libc.h"

// Closure structure: binds a function pointer with a 'this' pointer
typedef struct {
    void* func;      // Function implementation address
    void* this_ptr;  // Object instance pointer (bound 'this')
} Closure;

// Virtual table structure for OOP-style method dispatch
typedef struct {
    Closure** methods;  // Array of closure pointers
    size_t method_count;
} VTable;

// Object structure with vtable
typedef struct {
    VTable* vtable;
    void* data;
} Object;

// === Core Closure API ===

// Create a closure binding function and this pointer
static inline Closure* closure_create(void* func, void* this_ptr) {
    Closure* closure = (Closure*)malloc(sizeof(Closure));
    if (closure) {
        closure->func = func;
        closure->this_ptr = this_ptr;
    }
    return closure;
}

// Destroy a closure
static inline void closure_destroy(Closure* closure) {
    free(closure);
}

// === Closure Invocation (Architecture-specific) ===

// Call closure with 0 arguments (returns void*)
static inline void* closure_call0(Closure* closure) {
    void* (*func)(void*) = (void* (*)(void*))closure->func;
    return func(closure->this_ptr);
}

// Call closure with 1 argument
static inline void* closure_call1(Closure* closure, void* arg1) {
    void* (*func)(void*, void*) = (void* (*)(void*, void*))closure->func;
    return func(closure->this_ptr, arg1);
}

// Call closure with 2 arguments
static inline void* closure_call2(Closure* closure, void* arg1, void* arg2) {
    void* (*func)(void*, void*, void*) = (void* (*)(void*, void*, void*))closure->func;
    return func(closure->this_ptr, arg1, arg2);
}

// Call closure with 3 arguments
static inline void* closure_call3(Closure* closure, void* arg1, void* arg2, void* arg3) {
    void* (*func)(void*, void*, void*, void*) = (void* (*)(void*, void*, void*, void*))closure->func;
    return func(closure->this_ptr, arg1, arg2, arg3);
}

// === Low-level Assembly Closure Call ===

// x86_64: rdi=this, rsi=arg1, rdx=arg2, rcx=arg3, r8=arg4, r9=arg5
// ARM64:  x0=this,  x1=arg1,  x2=arg2,  x3=arg3,  x4=arg4,  x5=arg5

// Disable inline assembly in TCC runtime mode
#ifndef __COSMORUN__

#ifdef __x86_64__
static inline void* closure_call_asm(Closure* closure) {
    void* result;
    __asm__ volatile(
        "movq 8(%1), %%rdi\n\t"   // Load this_ptr into rdi (1st arg)
        "movq 0(%1), %%rax\n\t"   // Load func into rax
        "callq *%%rax\n\t"        // Call func
        : "=a"(result)
        : "r"(closure)
        : "rdi", "memory"
    );
    return result;
}

static inline void* closure_call1_asm(Closure* closure, void* arg1) {
    void* result;
    __asm__ volatile(
        "movq 8(%1), %%rdi\n\t"   // Load this_ptr into rdi
        "movq %2, %%rsi\n\t"      // Load arg1 into rsi (2nd arg)
        "movq 0(%1), %%rax\n\t"   // Load func into rax
        "callq *%%rax\n\t"
        : "=a"(result)
        : "r"(closure), "r"(arg1)
        : "rdi", "rsi", "memory"
    );
    return result;
}

static inline void* closure_call2_asm(Closure* closure, void* arg1, void* arg2) {
    void* result;
    __asm__ volatile(
        "movq 8(%1), %%rdi\n\t"   // this
        "movq %2, %%rsi\n\t"      // arg1
        "movq %3, %%rdx\n\t"      // arg2
        "movq 0(%1), %%rax\n\t"
        "callq *%%rax\n\t"
        : "=a"(result)
        : "r"(closure), "r"(arg1), "r"(arg2)
        : "rdi", "rsi", "rdx", "memory"
    );
    return result;
}

#elif defined(__aarch64__)
static inline void* closure_call_asm(Closure* closure) {
    void* result;
    __asm__ volatile(
        "ldr x1, [%1]\n\t"        // Load func into x1
        "ldr x0, [%1, #8]\n\t"    // Load this_ptr into x0 (1st arg)
        "blr x1\n\t"              // Call func
        : "=r"(result)
        : "r"(closure)
        : "x0", "x1", "memory"
    );
    return result;
}

static inline void* closure_call1_asm(Closure* closure, void* arg1) {
    void* result;
    __asm__ volatile(
        "ldr x2, [%1]\n\t"        // Load func into x2
        "ldr x0, [%1, #8]\n\t"    // Load this_ptr into x0
        "mov x1, %2\n\t"          // Load arg1 into x1 (2nd arg)
        "blr x2\n\t"
        : "=r"(result)
        : "r"(closure), "r"(arg1)
        : "x0", "x1", "x2", "memory"
    );
    return result;
}

static inline void* closure_call2_asm(Closure* closure, void* arg1, void* arg2) {
    void* result;
    __asm__ volatile(
        "ldr x3, [%1]\n\t"        // func
        "ldr x0, [%1, #8]\n\t"    // this
        "mov x1, %2\n\t"          // arg1
        "mov x2, %3\n\t"          // arg2
        "blr x3\n\t"
        : "=r"(result)
        : "r"(closure), "r"(arg1), "r"(arg2)
        : "x0", "x1", "x2", "x3", "memory"
    );
    return result;
}
#else
#error "Unsupported architecture: only x86_64 and ARM64 are supported"
#endif

#endif // !__COSMORUN__ (end of inline assembly section)

// === VTable and Object Management ===

// Create vtable with method closures
static inline VTable* vtable_create(size_t method_count) {
    VTable* vtable = (VTable*)malloc(sizeof(VTable));
    if (vtable) {
        vtable->methods = (Closure**)calloc(method_count, sizeof(Closure*));
        vtable->method_count = method_count;
    }
    return vtable;
}

// Set method in vtable
static inline void vtable_set_method(VTable* vtable, size_t index, void* func, void* this_ptr) {
    if (index < vtable->method_count) {
        vtable->methods[index] = closure_create(func, this_ptr);
    }
}

// Destroy vtable
static inline void vtable_destroy(VTable* vtable) {
    for (size_t i = 0; i < vtable->method_count; i++) {
        if (vtable->methods[i]) {
            closure_destroy(vtable->methods[i]);
        }
    }
    free(vtable->methods);
    free(vtable);
}

// Create object with vtable
static inline Object* object_create(VTable* vtable, void* data) {
    Object* obj = (Object*)malloc(sizeof(Object));
    if (obj) {
        obj->vtable = vtable;
        obj->data = data;
    }
    return obj;
}

// Call method by index
static inline void* object_call_method(Object* obj, size_t method_index) {
    if (method_index < obj->vtable->method_count) {
        return closure_call0(obj->vtable->methods[method_index]);
    }
    return NULL;
}

// Call method with 1 argument
static inline void* object_call_method1(Object* obj, size_t method_index, void* arg1) {
    if (method_index < obj->vtable->method_count) {
        return closure_call1(obj->vtable->methods[method_index], arg1);
    }
    return NULL;
}

// Call method with 2 arguments
static inline void* object_call_method2(Object* obj, size_t method_index, void* arg1, void* arg2) {
    if (method_index < obj->vtable->method_count) {
        return closure_call2(obj->vtable->methods[method_index], arg1, arg2);
    }
    return NULL;
}

// Call method with 3 arguments
static inline void* object_call_method3(Object* obj, size_t method_index, void* arg1, void* arg2, void* arg3) {
    if (method_index < obj->vtable->method_count) {
        return closure_call3(obj->vtable->methods[method_index], arg1, arg2, arg3);
    }
    return NULL;
}

// Destroy object (does not destroy vtable)
static inline void object_destroy(Object* obj) {
    free(obj);
}

#endif // COSMO_CLOSURE_H
