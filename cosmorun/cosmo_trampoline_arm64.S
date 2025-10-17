// cosmo_trampoline_arm64.S - ARM64 Static Variadic Function Trampolines
//
// Purpose: Provide optimized static trampolines for common variadic functions
// instead of runtime code generation via mmap/mprotect.
//
// Background:
// - ARM64 variadic functions use registers x0-x7 for first 8 args
// - Standard functions: printf(fmt, ...) → vprintf(fmt, va_list)
// - va_list on ARM64 is a pointer to saved arguments on stack
//
// This file provides static wrappers for:
// - printf  (1 fixed param)  → vprintf(fmt, va_list)
// - sprintf (2 fixed params) → vsprintf(buf, fmt, va_list)
// - snprintf(3 fixed params) → vsnprintf(buf, size, fmt, va_list)

#ifdef __aarch64__

.text
.align 4

// ============================================================================
// External references to v* function pointers (set by C code)
// ============================================================================
.global g_vprintf_ptr
.global g_vsprintf_ptr
.global g_vsnprintf_ptr

// ============================================================================
// Static Trampoline for printf (variadic_type = 1)
// ============================================================================
// Signature: int arm64_static_printf_wrapper(const char *format, ...)
//
// Calling convention:
//   Input:  x0 = format, x1-x7 = variadic args
//   Output: x0 = return value from vprintf
//
// Implementation:
//   1. Save x1-x7 to stack (variadic args)
//   2. Call vprintf(x0, &stack)
//
.global arm64_static_printf_wrapper
arm64_static_printf_wrapper:
    stp     x29, x30, [sp, #-16]!   // Save frame pointer and link register
    mov     x29, sp                  // Set frame pointer
    sub     sp, sp, #64              // Allocate 64 bytes for args (x1-x7 + alignment)

    // Save variadic arguments (x1-x7) to stack
    str     x1, [sp, #0]             // arg[0]
    str     x2, [sp, #8]             // arg[1]
    str     x3, [sp, #16]            // arg[2]
    str     x4, [sp, #24]            // arg[3]
    str     x5, [sp, #32]            // arg[4]
    str     x6, [sp, #40]            // arg[5]
    str     x7, [sp, #48]            // arg[6]

    // Prepare vprintf(format, va_list) call
    // x0 already contains format (first fixed param)
    mov     x1, sp                   // x1 = va_list (pointer to saved args)

    // Load vprintf function pointer
    adrp    x9, g_vprintf_ptr
    ldr     x9, [x9, :lo12:g_vprintf_ptr]

    // Call vprintf(format, va_list)
    blr     x9

    // Restore stack and return
    add     sp, sp, #64
    ldp     x29, x30, [sp], #16
    ret

// ============================================================================
// Static Trampoline for sprintf (variadic_type = 2)
// ============================================================================
// Signature: int arm64_static_sprintf_wrapper(char *buf, const char *format, ...)
//
// Calling convention:
//   Input:  x0 = buf, x1 = format, x2-x7 = variadic args
//   Output: x0 = return value from vsprintf
//
// Implementation:
//   1. Save x2-x7 to stack (variadic args)
//   2. Call vsprintf(x0, x1, &stack)
//
.global arm64_static_sprintf_wrapper
arm64_static_sprintf_wrapper:
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp
    sub     sp, sp, #64

    // Save variadic arguments (x2-x7) to stack
    str     x2, [sp, #0]             // arg[0]
    str     x3, [sp, #8]             // arg[1]
    str     x4, [sp, #16]            // arg[2]
    str     x5, [sp, #24]            // arg[3]
    str     x6, [sp, #32]            // arg[4]
    str     x7, [sp, #40]            // arg[5]

    // Prepare vsprintf(buf, format, va_list) call
    // x0 already contains buf (first fixed param)
    // x1 already contains format (second fixed param)
    mov     x2, sp                   // x2 = va_list

    // Load vsprintf function pointer
    adrp    x9, g_vsprintf_ptr
    ldr     x9, [x9, :lo12:g_vsprintf_ptr]

    // Call vsprintf(buf, format, va_list)
    blr     x9

    add     sp, sp, #64
    ldp     x29, x30, [sp], #16
    ret

// ============================================================================
// Static Trampoline for snprintf (variadic_type = 3)
// ============================================================================
// Signature: int arm64_static_snprintf_wrapper(char *buf, size_t size, const char *format, ...)
//
// Calling convention:
//   Input:  x0 = buf, x1 = size, x2 = format, x3-x7 = variadic args
//   Output: x0 = return value from vsnprintf
//
// Implementation:
//   1. Save x3-x7 to stack (variadic args)
//   2. Call vsnprintf(x0, x1, x2, &stack)
//
.global arm64_static_snprintf_wrapper
arm64_static_snprintf_wrapper:
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp
    sub     sp, sp, #64

    // Save variadic arguments (x3-x7) to stack
    str     x3, [sp, #0]             // arg[0]
    str     x4, [sp, #8]             // arg[1]
    str     x5, [sp, #16]            // arg[2]
    str     x6, [sp, #24]            // arg[3]
    str     x7, [sp, #32]            // arg[4]

    // Prepare vsnprintf(buf, size, format, va_list) call
    // x0 already contains buf (first fixed param)
    // x1 already contains size (second fixed param)
    // x2 already contains format (third fixed param)
    mov     x3, sp                   // x3 = va_list

    // Load vsnprintf function pointer
    adrp    x9, g_vsnprintf_ptr
    ldr     x9, [x9, :lo12:g_vsnprintf_ptr]

    // Call vsnprintf(buf, size, format, va_list)
    blr     x9

    add     sp, sp, #64
    ldp     x29, x30, [sp], #16
    ret

// ============================================================================
// Data Section: Function Pointers (initialized by C code)
// ============================================================================
.data
.align 8

.global g_vprintf_ptr
g_vprintf_ptr:
    .quad   0

.global g_vsprintf_ptr
g_vsprintf_ptr:
    .quad   0

.global g_vsnprintf_ptr
g_vsnprintf_ptr:
    .quad   0

#endif // __aarch64__
