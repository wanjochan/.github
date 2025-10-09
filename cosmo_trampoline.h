/* cosmo_trampoline.h - Cross-platform function call trampolines
 *
 * Extracted from cosmorun_tiny.c for reuse in cosmorun.c
 *
 * Provides two types of trampolines:
 * 1. Windows x86_64: SysV -> Microsoft x64 calling convention conversion
 * 2. ARM64: Variadic function argument marshalling
 */

#ifndef COSMO_TRAMPOLINE_H
#define COSMO_TRAMPOLINE_H

//#include <stddef.h>
//#include <stdbool.h>
#include "cosmo_libc.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Windows x86_64 Calling Convention Trampolines
// ============================================================================
#ifdef __x86_64__

/**
 * Initialize Windows trampoline system
 * Should be called once at startup
 * @param host_module Handle to the host executable (to skip trampolines for it)
 */
void cosmo_trampoline_win_init(void *host_module);

/**
 * Wrap a Windows DLL function with SysV->Microsoft x64 CC conversion
 * @param module DLL module handle
 * @param addr Function address to wrap
 * @return Trampoline address or original address if not needed
 */
void *cosmo_trampoline_win_wrap(void *module, void *addr);

/**
 * Get trampoline statistics
 * @return Number of trampolines created
 */
size_t cosmo_trampoline_win_count(void);

#endif // __x86_64__

// ============================================================================
// ARM64 Variadic Function Trampolines
// ============================================================================
#ifdef __aarch64__

/**
 * Variadic function types for ARM64 trampoline generation
 * Based on number of fixed parameters before variadic args
 */
typedef enum {
    VARARG_TYPE_1 = 1,  // 1 fixed param (e.g., printf)
    VARARG_TYPE_2 = 2,  // 2 fixed params (e.g., sprintf)
    VARARG_TYPE_3 = 3,  // 3 fixed params (e.g., snprintf)
} cosmo_vararg_type_t;

/**
 * Create ARM64 variadic function trampoline
 * Generates machine code to properly marshal variadic arguments
 * @param vfunc Target variadic function (v* variant, e.g., vprintf)
 * @param variadic_type Number of fixed parameters
 * @param name Function name for debugging
 * @return Trampoline address or NULL on failure
 */
void *cosmo_trampoline_arm64_vararg(void *vfunc, int variadic_type, const char *name);

/**
 * Get trampoline statistics
 * @return Number of trampolines created
 */
size_t cosmo_trampoline_arm64_count(void);

#endif // __aarch64__

// ============================================================================
// Generic Trampoline Interface
// ============================================================================

/**
 * Initialize trampoline system for current platform
 * @param host_module Host executable handle (for Windows)
 */
void cosmo_trampoline_init(void *host_module);

/**
 * Wrap a dynamically loaded function if needed
 * Automatically detects platform and applies appropriate trampoline
 * @param module Library handle
 * @param addr Function address
 * @return Wrapped address or original if no trampoline needed
 */
void *cosmo_trampoline_wrap(void *module, void *addr);

// ============================================================================
// Libc Function Resolution with Automatic Trampoline
// ============================================================================

/**
 * Initialize libc/libm loading for current platform
 * Loads appropriate C library:
 * - Windows: msvcrt.dll
 * - Linux: libc.so.6 + libm.so.6
 * - macOS: libSystem.B.dylib
 *
 * Must be called before using cosmo_trampoline_libc_resolve()
 */
void cosmo_trampoline_libc_init(void);

/**
 * Resolve function from libc/libm with automatic trampoline
 * @param name Function name (e.g., "printf", "snprintf")
 * @param variadic_type Variadic type for ARM64 trampolines:
 *                      0 = fixed-args (no trampoline needed)
 *                      1 = printf-style (1 fixed param: format)
 *                      2 = sprintf-style (2 fixed params: buf, format)
 *                      3 = snprintf-style (3 fixed params: buf, size, format)
 * @return Function pointer with trampoline applied, or NULL if not found
 *
 * Note: On ARM64, automatically resolves v* variant (vprintf, vsnprintf, etc.)
 *       and creates trampoline for proper va_list handling
 */
void *cosmo_trampoline_libc_resolve(const char *name, int variadic_type);

/**
 * Check if libc has been initialized
 * @return true if cosmo_trampoline_libc_init() has been called
 */
bool cosmo_trampoline_libc_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // COSMO_TRAMPOLINE_H
