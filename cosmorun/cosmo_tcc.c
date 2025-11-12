/* cosmo_tcc.c - TinyCC Integration Module
 * This module provides comprehensive TCC functionality including:
 * - Runtime library support
 * - Symbol table management
 * - State initialization
 * - Path configuration
 * - Error handling
 */

//#include <stdatomic.h>  // C11 atomic operations
//#include <dirent.h>     // Directory operations for .S file discovery
#include "cosmo_tcc.h"
#include "cosmo_utils.h"
#include "cosmo_ffi.h"
#include "cosmo_lock.h"
#include "xdl.h"

// Forward declarations for wrapper functions
size_t cosmorun_strlen(const char *s);
int cosmorun_strcmp(const char *s1, const char *s2);
char *cosmorun_strcpy(char *dest, const char *src);
char *cosmorun_strcat(char *dest, const char *src);
int cosmorun_strncmp(const char *s1, const char *s2, size_t n);
int cosmorun_strcasecmp(const char *s1, const char *s2);
char *cosmorun_strrchr(const char *s, int c);
char *cosmorun_strchr(const char *s, int c);
char *cosmorun_strncpy(char *dest, const char *src, size_t n);
char *cosmorun_strstr(const char *haystack, const char *needle);
char *cosmorun_strtok(char *str, const char *delim);
long cosmorun_strtol(const char *nptr, char **endptr, int base);
char *cosmorun_strerror(int errnum);
size_t cosmorun_strftime(char *s, size_t max, const char *format, const struct tm *tm);
void *cosmorun_memcpy(void *dest, const void *src, size_t n);
void *cosmorun_memset(void *s, int c, size_t n);
void *cosmorun_memmove(void *dest, const void *src, size_t n);
int cosmorun_uname(struct utsname *buf);
int cosmorun_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

// Atomic operations wrappers - Phase 1: Fetch-and-Operate
int sync_fetch_and_add_int(int *ptr, int val);
int sync_fetch_and_sub_int(int *ptr, int val);
long sync_fetch_and_add_long(long *ptr, long val);
long sync_fetch_and_sub_long(long *ptr, long val);
int sync_fetch_and_or_int(int *ptr, int val);
int sync_fetch_and_and_int(int *ptr, int val);
int sync_fetch_and_xor_int(int *ptr, int val);

// Atomic operations wrappers - Phase 2: Operate-and-Fetch
int sync_add_and_fetch_int(int *ptr, int val);
int sync_sub_and_fetch_int(int *ptr, int val);
long sync_add_and_fetch_long(long *ptr, long val);
long sync_sub_and_fetch_long(long *ptr, long val);
int sync_or_and_fetch_int(int *ptr, int val);
int sync_and_and_fetch_int(int *ptr, int val);
int sync_xor_and_fetch_int(int *ptr, int val);

// Atomic operations wrappers - Phase 3: Compare-and-Swap
int sync_bool_compare_and_swap_int(int *ptr, int oldval, int newval);
int sync_val_compare_and_swap_int(int *ptr, int oldval, int newval);
long sync_bool_compare_and_swap_long(long *ptr, long oldval, long newval);
long sync_val_compare_and_swap_long(long *ptr, long oldval, long newval);
void* sync_bool_compare_and_swap_ptr(void **ptr, void *oldval, void *newval);
void* sync_val_compare_and_swap_ptr(void **ptr, void *oldval, void *newval);

// Atomic operations wrappers - Phase 4: Lock/Synchronization
int sync_lock_test_and_set_int(int *ptr, int val);
void sync_lock_release_int(int *ptr);
void sync_synchronize(void);

// C11-style atomic operations
int atomic_fetch_add_int(int *ptr, int val);
int atomic_fetch_sub_int(int *ptr, int val);
int atomic_fetch_or_int(int *ptr, int val);
int atomic_fetch_and_int(int *ptr, int val);
int atomic_fetch_xor_int(int *ptr, int val);
int atomic_load_int(int *ptr);
void atomic_store_int(int *ptr, int val);
int atomic_exchange_int(int *ptr, int val);

extern cosmorun_config_t g_config;
extern cosmorun_result_t init_config(void);

//#include "cosmo_trampoline.h"
extern void *cosmo_trampoline_wrap(void *module, void *addr);

// Coroutine runtime support (from cosmo_co.c)
// NOTE: Stackful coroutines use inline assembly, these old helpers are no longer needed
// extern void* __co_stack_alloc(size_t size);       // REMOVED: stackful uses malloc directly
// extern void __co_stack_free(void* stack);         // REMOVED: stackful uses free directly
// extern void* __co_alloca(size_t size);            // REMOVED: copy-stack only
// extern void* __co_read_sp(void);                  // REMOVED: copy-stack only
// extern int __co_setjmp(jmp_buf env);              // REMOVED: copy-stack only
// extern void __co_longjmp(jmp_buf env, int val);   // REMOVED: copy-stack only

// pthread stack APIs (cosmopolitan cross-platform)
extern int pthread_getattr_np(pthread_t thread, pthread_attr_t *attr);
extern int pthread_attr_getstack(const pthread_attr_t *attr, void **stackaddr, size_t *stacksize);
extern int pthread_attr_destroy(pthread_attr_t *attr);

// Pure builtin coroutine API (from cosmo_co.c)
typedef void (*co_builtin_func_t)(void* arg);
extern void* __co_builtin_create(co_builtin_func_t func, void* arg);
extern void* __co_builtin_resume_api(void* handle);
extern int __co_builtin_yield(void* value);
extern void __co_builtin_free(void* handle);
extern int __co_builtin_state(void* handle);
extern int __co_builtin_is_alive(void* handle);

// Forward declarations
void tcc_error_func(void *opaque, const char *msg);
void register_default_include_paths(TCCState *s, const struct utsname *uts);
void register_default_library_paths(TCCState *s);
void register_builtin_symbols(TCCState *s);
void link_tcc_runtime(TCCState *s);
static void print_module_cache_stats(void);

#if defined(__aarch64__) || defined(__arm64__)
// ARM64: Long double (128-bit) runtime helpers
const char* tcc_runtime_lib =
"typedef unsigned long long uint64_t;\n"
"typedef struct { uint64_t x0, x1; } u128_t;\n"
"static void *__runtime_memcpy(void *d, const void *s, unsigned long n) {\n"
"    char *dest = d; const char *src = s;\n"
"    while (n--) *dest++ = *src++;\n"
"    return d;\n"
"}\n"
"#define memcpy __runtime_memcpy\n"
// __extenddftf2: double -> long double
"long double __extenddftf2(double f) {\n"
"    long double fx; u128_t x; uint64_t a;\n"
"    memcpy(&a, &f, 8);\n"
"    x.x0 = a << 60;\n"
"    if (!(a << 1))\n"
"        x.x1 = a;\n"
"    else if (a << 1 >> 53 == 2047)\n"
"        x.x1 = (0x7fff000000000000ULL | a >> 63 << 63 | a << 12 >> 16 | (uint64_t)!!(a << 12) << 47);\n"
"    else if (a << 1 >> 53 == 0) {\n"
"        uint64_t adj = 0;\n"
"        while (!(a << 1 >> 1 >> (52 - adj))) adj++;\n"
"        x.x0 <<= adj;\n"
"        x.x1 = a >> 63 << 63 | (15360 - adj + 1) << 48 | a << adj << 12 >> 16;\n"
"    } else\n"
"        x.x1 = a >> 63 << 63 | ((a >> 52 & 2047) + 15360) << 48 | a << 12 >> 16;\n"
"    memcpy(&fx, &x, 16);\n"
"    return fx;\n"
"}\n"
// __trunctfdf2: long double -> double
"double __trunctfdf2(long double f) {\n"
"    u128_t x; memcpy(&x, &f, 16);\n"
"    int exp = x.x1 >> 48 & 32767, sgn = x.x1 >> 63;\n"
"    uint64_t r;\n"
"    if (exp == 32767 && (x.x0 | x.x1 << 16))\n"
"        r = 0x7ff8000000000000ULL | (uint64_t)sgn << 63 | x.x1 << 16 >> 12 | x.x0 >> 60;\n"
"    else if (exp > 17406) r = 0x7ff0000000000000ULL | (uint64_t)sgn << 63;\n"
"    else if (exp < 15308) r = (uint64_t)sgn << 63;\n"
"    else {\n"
"        exp -= 15361;\n"
"        r = x.x1 << 6 | x.x0 >> 58 | !!(x.x0 << 6);\n"
"        if (exp < 0) { r = r >> -exp | !!(r << (64 + exp)); exp = 0; }\n"
"        if ((r & 3) == 3 || (r & 7) == 6) r += 4;\n"
"        r = ((r >> 2) + ((uint64_t)exp << 52)) | (uint64_t)sgn << 63;\n"
"    }\n"
"    double d; memcpy(&d, &r, 8); return d;\n"
"}\n"
// __lttf2: long double comparison (<)
"int __lttf2(long double a, long double b) {\n"
"    u128_t ua, ub; memcpy(&ua, &a, 16); memcpy(&ub, &b, 16);\n"
"    return (!(ua.x0 | ua.x1 << 1 | ub.x0 | ub.x1 << 1) ? 0 :\n"
"            ((ua.x1 << 1 >> 49 == 0x7fff && (ua.x0 | ua.x1 << 16)) ||\n"
"             (ub.x1 << 1 >> 49 == 0x7fff && (ub.x0 | ub.x1 << 16))) ? 2 :\n"
"            ua.x1 >> 63 != ub.x1 >> 63 ? (int)(ub.x1 >> 63) - (int)(ua.x1 >> 63) :\n"
"            ua.x1 < ub.x1 ? (int)(ua.x1 >> 63 << 1) - 1 :\n"
"            ua.x1 > ub.x1 ? 1 - (int)(ua.x1 >> 63 << 1) :\n"
"            ua.x0 < ub.x0 ? (int)(ua.x1 >> 63 << 1) - 1 :\n"
"            ub.x0 < ua.x0 ? 1 - (int)(ua.x1 >> 63 << 1) : 0);\n"
"}\n"
// __gttf2: long double comparison (>)
"int __gttf2(long double a, long double b) {\n"
"    return -__lttf2(b, a);\n"
"}\n"
// __letf2: long double comparison (<=)
"int __letf2(long double a, long double b) {\n"
"    return __lttf2(a, b);\n"
"}\n"
// __getf2: long double comparison (>=)
"int __getf2(long double a, long double b) {\n"
"    return -__lttf2(b, a);\n"
"}\n";

#elif defined(__x86_64__) || defined(__amd64__)
// x86_64: 64-bit integer division/modulo runtime helpers
const char* tcc_runtime_lib =
"typedef long long int64_t;\n"
"typedef unsigned long long uint64_t;\n"
// Simplified 64-bit division (note: full implementation needs more complexity)
"int64_t __divdi3(int64_t a, int64_t b) {\n"
"    int neg = 0;\n"
"    if (a < 0) { a = -a; neg = !neg; }\n"
"    if (b < 0) { b = -b; neg = !neg; }\n"
"    uint64_t q = (uint64_t)a / (uint64_t)b;\n"
"    return neg ? -(int64_t)q : (int64_t)q;\n"
"}\n"
"int64_t __moddi3(int64_t a, int64_t b) {\n"
"    int neg = (a < 0);\n"
"    if (a < 0) a = -a;\n"
"    if (b < 0) b = -b;\n"
"    uint64_t r = (uint64_t)a % (uint64_t)b;\n"
"    return neg ? -(int64_t)r : (int64_t)r;\n"
"}\n"
"int64_t __ashrdi3(int64_t a, int b) {\n"
"    return a >> b;\n"
"}\n"
"int64_t __ashldi3(int64_t a, int b) {\n"
"    return a << b;\n"
"}\n";

#else
// Other architectures: empty runtime (may need platform-specific implementations)
const char* tcc_runtime_lib = "";
#endif

// Universal stdarg definitions (works on all platforms)
const char* tcc_stdarg_defs =
"/* stdarg.h - variadic function support using TCC builtins */\n"
"#ifndef _STDARG_H_DEFINED\n"
"#define _STDARG_H_DEFINED\n"
"\n"
"/* Use TCC's builtin varargs support directly */\n"
"typedef __builtin_va_list va_list;\n"
"#define va_start(ap, last) __builtin_va_start(ap, last)\n"
"#define va_arg(ap, type)   __builtin_va_arg(ap, type)\n"
"#define va_end(ap)         __builtin_va_end(ap)\n"
"#define va_copy(dest, src) __builtin_va_copy(dest, src)\n"
"\n"
"#endif /* _STDARG_H_DEFINED */\n";

// Link runtime library into TCC state
void link_tcc_runtime(TCCState *s) {
    // First, inject stdarg definitions (required by many modules)
    if (tcc_compile_string(s, tcc_stdarg_defs) < 0) {
        fprintf(stderr, "[cosmorun] Warning: Failed to compile stdarg definitions\n");
    }

    // Then, inject platform-specific runtime helpers
    if (tcc_runtime_lib && tcc_runtime_lib[0] != '\0') {
        if (tcc_compile_string(s, tcc_runtime_lib) < 0) {
            fprintf(stderr, "[cosmorun] Warning: Failed to compile runtime library\n");
        }
    }
}

/* ============================================================================
 * String and Memory Wrapper Functions
 * These wrappers avoid ABI issues with __attribute__ annotations
 * ============================================================================ */

// String copy/manipulation functions (memcpyesque)
char* cosmorun_strcpy(char* dest, const char* src) {
    return strcpy(dest, src);
}

char* cosmorun_strcat(char* dest, const char* src) {
    return strcat(dest, src);
}

// Memory operations (memcpyesque)
void* cosmorun_memcpy(void* dest, const void* src, size_t n) {
    return memcpy(dest, src, n);
}

void* cosmorun_memset(void* s, int c, size_t n) {
    return memset(s, c, n);
}

void* cosmorun_memmove(void* dest, const void* src, size_t n) {
    return memmove(dest, src, n);
}

// String comparison/length (strlenesque/nosideeffect)
size_t cosmorun_strlen(const char* s) {
    return strlen(s);
}

int cosmorun_strcmp(const char* s1, const char* s2) {
    return strcmp(s1, s2);
}

int cosmorun_strncmp(const char* s1, const char* s2, size_t n) {
    return strncmp(s1, s2, n);
}

// Case-insensitive string comparison (strlenesque)
int cosmorun_strcasecmp(const char* s1, const char* s2) {
    return strcasecmp(s1, s2);
}

// Find last occurrence of character (strlenesque)
char* cosmorun_strrchr(const char* s, int c) {
    return strrchr(s, c);
}

// Find first occurrence of character (strlenesque)
char* cosmorun_strchr(const char* s, int c) {
    return strchr(s, c);
}

// Copy string with length limit (libcesque)
char* cosmorun_strncpy(char* dest, const char* src, size_t n) {
    return strncpy(dest, src, n);
}

// Find substring (strlenesque)
char* cosmorun_strstr(const char* haystack, const char* needle) {
    return strstr(haystack, needle);
}

// String tokenization (libcesque)
char* cosmorun_strtok(char* str, const char* delim) {
    return strtok(str, delim);
}

// String to long conversion (libcesque)
long cosmorun_strtol(const char* str, char** endptr, int base) {
    return strtol(str, endptr, base);
}

// Get error string (libcesque)
char* cosmorun_strerror(int errnum) {
    return strerror(errnum);
}

// Format time (libcesque)
size_t cosmorun_strftime(char* s, size_t max, const char* format, const struct tm* tm) {
    return strftime(s, max, format, tm);
}

// System information (libcesque = dontthrow + dontcallback)
int cosmorun_uname(struct utsname* uts) {
    return uname(uts);
}

// Signal handling (libcesque)
int cosmorun_sigaction(int sig, const struct sigaction* act, struct sigaction* oldact) {
    return sigaction(sig, act, oldact);
}

/* ============================================================================
 * Atomic Operations Wrapper Functions
 * These wrappers provide atomic operations for TCC-compiled code
 * ============================================================================ */

// ===== Phase 1: Fetch-and-Operate (returns old value) =====

int sync_fetch_and_add_int(int *ptr, int val) {
    return __sync_fetch_and_add(ptr, val);
}

int sync_fetch_and_sub_int(int *ptr, int val) {
    return __sync_fetch_and_sub(ptr, val);
}

long sync_fetch_and_add_long(long *ptr, long val) {
    return __sync_fetch_and_add(ptr, val);
}

long sync_fetch_and_sub_long(long *ptr, long val) {
    return __sync_fetch_and_sub(ptr, val);
}

int sync_fetch_and_or_int(int *ptr, int val) {
    return __sync_fetch_and_or(ptr, val);
}

int sync_fetch_and_and_int(int *ptr, int val) {
    return __sync_fetch_and_and(ptr, val);
}

int sync_fetch_and_xor_int(int *ptr, int val) {
    return __sync_fetch_and_xor(ptr, val);
}

// ===== Phase 2: Operate-and-Fetch (returns new value) =====

int sync_add_and_fetch_int(int *ptr, int val) {
    return __sync_add_and_fetch(ptr, val);
}

int sync_sub_and_fetch_int(int *ptr, int val) {
    return __sync_sub_and_fetch(ptr, val);
}

long sync_add_and_fetch_long(long *ptr, long val) {
    return __sync_add_and_fetch(ptr, val);
}

long sync_sub_and_fetch_long(long *ptr, long val) {
    return __sync_sub_and_fetch(ptr, val);
}

int sync_or_and_fetch_int(int *ptr, int val) {
    return __sync_or_and_fetch(ptr, val);
}

int sync_and_and_fetch_int(int *ptr, int val) {
    return __sync_and_and_fetch(ptr, val);
}

int sync_xor_and_fetch_int(int *ptr, int val) {
    return __sync_xor_and_fetch(ptr, val);
}

// ===== Phase 3: Compare-and-Swap =====

int sync_bool_compare_and_swap_int(int *ptr, int oldval, int newval) {
    return __sync_bool_compare_and_swap(ptr, oldval, newval);
}

int sync_val_compare_and_swap_int(int *ptr, int oldval, int newval) {
    return __sync_val_compare_and_swap(ptr, oldval, newval);
}

long sync_bool_compare_and_swap_long(long *ptr, long oldval, long newval) {
    return __sync_bool_compare_and_swap(ptr, oldval, newval);
}

long sync_val_compare_and_swap_long(long *ptr, long oldval, long newval) {
    return __sync_val_compare_and_swap(ptr, oldval, newval);
}

void* sync_bool_compare_and_swap_ptr(void **ptr, void *oldval, void *newval) {
    return (void*)(long)__sync_bool_compare_and_swap(ptr, oldval, newval);
}

void* sync_val_compare_and_swap_ptr(void **ptr, void *oldval, void *newval) {
    return __sync_val_compare_and_swap(ptr, oldval, newval);
}

// ===== Phase 4: Lock/Synchronization =====

int sync_lock_test_and_set_int(int *ptr, int val) {
    return __sync_lock_test_and_set(ptr, val);
}

void sync_lock_release_int(int *ptr) {
    __sync_lock_release(ptr);
}

void sync_synchronize(void) {
    __sync_synchronize();
}

// ===== C11-style atomic wrappers =====

int atomic_fetch_add_int(int *ptr, int val) {
    return __sync_fetch_and_add(ptr, val);
}

int atomic_fetch_sub_int(int *ptr, int val) {
    return __sync_fetch_and_sub(ptr, val);
}

int atomic_fetch_or_int(int *ptr, int val) {
    return __sync_fetch_and_or(ptr, val);
}

int atomic_fetch_and_int(int *ptr, int val) {
    return __sync_fetch_and_and(ptr, val);
}

int atomic_fetch_xor_int(int *ptr, int val) {
    return __sync_fetch_and_xor(ptr, val);
}

int atomic_load_int(int *ptr) {
    return __sync_fetch_and_add(ptr, 0);  // Load with 0 addition
}

void atomic_store_int(int *ptr, int val) {
    __sync_lock_test_and_set(ptr, val);
    __sync_synchronize();  // Full memory barrier
}

int atomic_exchange_int(int *ptr, int val) {
    return __sync_lock_test_and_set(ptr, val);
}

#include "cosmo_libc.h"
#include "libtcc.h"

// Provide use_tcc_malloc/free implementations before including tcc.h
static inline void *use_tcc_malloc(size_t size) {
    return malloc(size);
}

static inline void use_tcc_free(void *ptr) {
    free(ptr);
}

#include "tcc.h"

// Platform operations - now in cosmo_utils.h

// Enhanced symbol cache entry (supports hash optimization)
typedef struct {
    const char* name;
    void* address;
    int is_cached;
    uint32_t hash;  // Pre-computed hash for fast lookup
} SymbolEntry;

// String wrapper functions (to avoid ABI issues)
extern size_t cosmorun_strlen(const char *s);
extern int cosmorun_strcmp(const char *s1, const char *s2);
extern char *cosmorun_strcpy(char *dest, const char *src);
extern char *cosmorun_strcat(char *dest, const char *src);
extern int cosmorun_strncmp(const char *s1, const char *s2, size_t n);
extern int cosmorun_strcasecmp(const char *s1, const char *s2);
extern char *cosmorun_strrchr(const char *s, int c);
extern char *cosmorun_strchr(const char *s, int c);
extern char *cosmorun_strncpy(char *dest, const char *src, size_t n);
extern char *cosmorun_strstr(const char *haystack, const char *needle);
extern char *cosmorun_strtok(char *str, const char *delim);
extern long cosmorun_strtol(const char *str, char **endptr, int base);
extern char *cosmorun_strerror(int errnum);
extern size_t cosmorun_strftime(char *s, size_t max, const char *format, const struct tm *tm);

// Memory wrapper functions (to avoid ABI issues)
extern void *cosmorun_memcpy(void *dest, const void *src, size_t n);
extern void *cosmorun_memset(void *s, int c, size_t n);
extern void *cosmorun_memmove(void *dest, const void *src, size_t n);

// Cosmopolitan dynamic module loading API
 void *__import(const char *module);
 void *__import_sym(void *handle, const char *symbol);
 void __import_free(void *handle);

// Platform-specific functions
extern int cosmorun_uname(struct utsname *buf);
extern int cosmorun_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

// Coroutine context switch from libco (coctx_swap.o)
// Supports ARM64, x86-64, and ARM32
#if defined(__aarch64__) || defined(__arm64__) || defined(__x86_64__) || defined(__amd64__)
// Temporarily stub out for build
// extern void _coctx_swap(void *from, void *to);
static void _coctx_swap(void *from, void *to) {
    (void)from; (void)to;
    // Stub implementation for build
}
#define coctx_swap _coctx_swap
#endif

// Debug trace function
//extern void tracef(const char *fmt, ...);

void *cosmorun_dlopen(const char *filename, int flags){
    if (!filename || !*filename) {
        return xdl_open(filename, flags);
    }

    // Auto-optimize flags if not specified (0 means use smart defaults)
    if (flags == 0) {
        // Windows doesn't support RTLD_GLOBAL, use RTLD_LAZY only
        // Other platforms use RTLD_LAZY | RTLD_GLOBAL for better symbol visibility
        flags = IsWindows() ? RTLD_LAZY : (RTLD_LAZY | RTLD_GLOBAL);
        tracef("cosmorun_dlopen: auto-optimized flags=%d for %s", flags,
               IsWindows() ? "Windows" : "Unix");
    }

    void *handle = xdl_open(filename, flags);
    if (handle) {
        return handle;
    }

    const char *last_sep = NULL;
    for (const char *p = filename; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            last_sep = p;
        }
    }

    size_t dir_len = last_sep ? (size_t)(last_sep - filename + 1) : 0;
    const char *basename = filename + dir_len;
    const char *dot = NULL;
    for (const char *p = basename; *p; ++p) {
        if (*p == '.') {
            dot = p;
        }
    }

    int has_ext = dot && dot != basename;
    size_t base_len = has_ext ? (size_t)(dot - basename) : strlen(basename);

    char base_name[PATH_MAX];
    if (base_len >= sizeof(base_name)) {
        return NULL;
    }
    memcpy(base_name, basename, base_len);
    base_name[base_len] = '\0';

    // Use runtime platform detection (Cosmopolitan APE)
    const char **exts = NULL;
    static const char *win_exts[] = { ".dll", ".so", ".dylib", NULL };
    static const char *mac_exts[] = { ".dylib", ".so", ".dll", NULL };
    static const char *linux_exts[] = { ".so", ".dylib", ".dll", NULL };

    if (IsWindows()) {
        exts = win_exts;
    } else if (IsXnu()) {
        exts = mac_exts;
    } else {
        exts = linux_exts;
    }

    int base_has_lib_prefix = (base_len >= 3) &&
        (base_name[0] == 'l' || base_name[0] == 'L') &&
        (base_name[1] == 'i' || base_name[1] == 'I') &&
        (base_name[2] == 'b' || base_name[2] == 'B');

    const char *prefixes[2];
    size_t prefix_count = 0;
    prefixes[prefix_count++] = "";
    if (!base_has_lib_prefix) {
        prefixes[prefix_count++] = "lib";
    }

    char original_ext[16] = {0};
    if (has_ext) {
        size_t ext_len = strlen(dot);
        if (ext_len < sizeof(original_ext)) {
            memcpy(original_ext, dot, ext_len + 1);
        }
    }

    char candidate[PATH_MAX];

    for (size_t pi = 0; pi < prefix_count; ++pi) {
        const char *prefix = prefixes[pi];
        for (const char **ext = exts; *ext; ++ext) {
            if (has_ext && strcmp(*ext, original_ext) == 0 && prefix[0] == '\0') {
                continue; // already tried original name
            }

            size_t needed = dir_len + strlen(prefix) + base_len + strlen(*ext) + 1;
            if (needed > sizeof(candidate)) {
                continue;
            }

            if (dir_len) {
                memcpy(candidate, filename, dir_len);
            }
            int written = snprintf(candidate + dir_len,
                                   sizeof(candidate) - dir_len,
                                   "%s%s%s",
                                   prefix,
                                   base_name,
                                   *ext);
            if (written < 0 || (size_t)written >= sizeof(candidate) - dir_len) {
                continue;
            }

            if (strcmp(candidate, filename) == 0) {
                continue;
            }

            tracef("cosmorun_dlopen: retry '%s'", candidate);
            handle = xdl_open(candidate, flags);
            if (handle) {
                return handle;
            }
        }
    }

    return NULL;
}

// Platform detection - exported functions for TCC-compiled code
// These wrappers ensure we have actual symbols for the builtin_symbol_table

#ifdef __COSMOPOLITAN__
// Cosmopolitan provides runtime platform detection
#include "libc/dce.h"

// Create wrapper functions with actual symbols (dce.h may use macros/inline)
int cosmorun_IsLinux(void) { return IsLinux(); }
int cosmorun_IsWindows(void) { return IsWindows(); }
int cosmorun_IsXnu(void) { return IsXnu(); }
int cosmorun_IsBsd(void) { return IsBsd(); }
int cosmorun_IsFreebsd(void) { return IsFreebsd(); }
int cosmorun_IsOpenbsd(void) { return IsOpenbsd(); }
int cosmorun_IsNetbsd(void) { return IsNetbsd(); }
int cosmorun_IsQemuUser(void) { return IsQemuUser(); }

#else

//for non cosmopoitan, we make it simple...
// Fallback: compile-time platform detection for non-Cosmopolitan builds
int cosmorun_IsWindows(void) {
#ifdef _WIN32
    return 1;
#else
    return 0;
#endif
}

int cosmorun_IsLinux(void) {
#ifdef __linux__
    return 1;
#else
    return 0;
#endif
}

int cosmorun_IsXnu(void) {
#ifdef __APPLE__
    return 1;
#else
    return 0;
#endif
}

int cosmorun_IsBsd(void) {
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
    return 1;
#else
    return 0;
#endif
}

int cosmorun_IsFreebsd(void) {
#ifdef __FreeBSD__
    return 1;
#else
    return 0;
#endif
}

int cosmorun_IsOpenbsd(void) {
#ifdef __OpenBSD__
    return 1;
#else
    return 0;
#endif
}

int cosmorun_IsNetbsd(void) {
#ifdef __NetBSD__
    return 1;
#else
    return 0;
#endif
}

int cosmorun_IsQemuUser(void) {
    // Simple check - QEMU sets QEMU_* environment variables
    return getenv("QEMU_LD_PREFIX") != NULL || getenv("QEMU_EXECVE") != NULL;
}
#endif

// Forward declaration for ARM64 vararg trampoline wrapper
#ifdef __aarch64__
void *__dlsym_varg(void *vfunc, int variadic_type);
#endif

// Level 1: Builtin symbol table (O(1) lookup)
const SymbolEntry builtin_symbol_table[] = {
  // I/O functions (most frequently used) - MUST be builtin for varargs compatibility
  {"printf", printf},
  {"sprintf", sprintf},
  {"snprintf", snprintf},
  {"vsnprintf", vsnprintf},
  {"fprintf", fprintf},
  {"sscanf", sscanf},

  // String conversion functions
  {"atoi", atoi},
  {"atof", atof},
  {"atol", atol},

  // Environment variables
  {"getenv", getenv},

  // Memory management (critical for most programs)
  {"malloc", malloc},
  {"calloc", calloc},
  {"realloc", realloc},
  {"free", free},

  // String functions (use wrappers to avoid ABI issues with __attribute__((__leaf__)))
  {"strlen", cosmorun_strlen},
  {"strcmp", cosmorun_strcmp},
  {"strcpy", cosmorun_strcpy},
  {"strcat", cosmorun_strcat},
  {"strncmp", cosmorun_strncmp},
  {"strcasecmp", cosmorun_strcasecmp},
  {"strrchr", cosmorun_strrchr},
  {"strchr", cosmorun_strchr},
  {"strncpy", cosmorun_strncpy},
  {"strstr", cosmorun_strstr},
  {"strtok", cosmorun_strtok},
  {"strtol", cosmorun_strtol},
  {"strerror", cosmorun_strerror},
  {"strftime", cosmorun_strftime},

  // Memory functions (use wrappers to avoid ABI issues with memcpyesque attribute)
  {"memcpy", cosmorun_memcpy},
  {"memset", cosmorun_memset},
  {"memmove", cosmorun_memmove},
  {"memcmp", memcmp},       // Super high frequency - memory comparison

  // ===== Cosmopolitan Platform Detection (MUST be builtin) =====
  {"IsLinux", cosmorun_IsLinux},
  {"IsWindows", cosmorun_IsWindows},
  {"IsXnu", cosmorun_IsXnu},
  {"IsBsd", cosmorun_IsBsd},
  {"IsFreebsd", cosmorun_IsFreebsd},
  {"IsOpenbsd", cosmorun_IsOpenbsd},
  {"IsNetbsd", cosmorun_IsNetbsd},
  {"IsQemuUser", cosmorun_IsQemuUser},

  // ===== Super high frequency ctype.h (performance critical) =====
  {"isdigit", isdigit},     // Extremely high frequency - parsers/lexers
  {"isalpha", isalpha},     // Extremely high frequency - parsers/lexers

  // Math functions (commonly used)
  {"abs", abs},
  {"labs", labs},
  {"sin", sin},
  {"cos", cos},
  {"sqrt", sqrt},

  // OUR API
  {"__import", __import},//./cosmorun.exe -e 'int main(){void* h=__import("std.c");printf("h=%d\n",h);}'
  {"__import_sym", __import_sym},
  {"__import_free", __import_free},
  {"__print_cache_stats", print_module_cache_stats},
  // CosmoRun dynamic loading (platform abstraction)
  {"__dlopen", cosmorun_dlopen},
  {"__dlsym", cosmorun_dlsym},
#ifdef __aarch64__
  {"__dlsym_varg", __dlsym_varg},  // ARM64 vararg trampoline wrapper
#endif
//   {"__dlclose", xdl_close},
//   {"__dlerror", xdl_error},

  {"dlopen", cosmorun_dlopen},//TODO xdl_open
  {"dlsym", cosmorun_dlsym},//TODO xdl_sym
  {"dlclose", xdl_close},
  {"dlerror", xdl_error},

  //from xdl
  //{"xdl_open", xdl_open},
  //{"xdl_sym", xdl_sym},
  //{"xdl_close", xdl_close},
  //{"xdl_error", xdl_error},

  // File I/O functions (essential for cross-platform compatibility)
  {"fopen", fopen},
  {"fclose", fclose},
  {"fread", fread},
  {"fwrite", fwrite},
  {"fseek", fseek},
  {"ftell", ftell},
  {"fgets", fgets},
  {"fputs", fputs},
  {"fputc", fputc},
  {"fflush", fflush},
  {"perror", perror},

  // ===== POSIX Functions (organized by category) =====

  // POSIX File I/O (file descriptors)
  {"open", open},
  {"read", read},
  {"write", write},
  {"close", close},
  {"pipe", pipe},          // Create pipe for IPC
  {"dup", dup},            // Duplicate file descriptor
  {"dup2", dup2},          // Duplicate to specific fd
  {"fcntl", fcntl},        // File control operations

  // POSIX File System Operations
  {"stat", stat},          // Get file status
  {"fstat", fstat},        // Get file status by fd
  {"lstat", lstat},        // Get symlink status
  {"access", access},      // Check file accessibility
  {"unlink", unlink},      // Remove file
  {"mkdir", mkdir},        // Create directory
  {"rmdir", rmdir},        // Remove directory
  {"chmod", chmod},        // Change file permissions
  {"getcwd", getcwd},      // Get current directory
  {"realpath", realpath},  // Resolve absolute path
  {"symlink", symlink},    // Create symbolic link
  {"readlink", readlink},  // Read symbolic link

  // POSIX Directory Operations
  {"opendir", opendir},    // Open directory stream
  {"readdir", readdir},    // Read directory entry
  {"closedir", closedir},  // Close directory stream

  // POSIX Process Management
  {"fork", fork},          // Create child process (NULL on Windows)
  {"execl", execl},        // Execute with list args
  {"execv", execv},        // Execute with array args
  {"execve", execve},      // Execute with environment
  {"execlp", execlp},      // Execute with PATH search
  {"waitpid", waitpid},    // Wait for process
  {"_exit", _exit},        // Exit without cleanup
  {"getpid", getpid},      // Get process ID
  {"getppid", getppid},    // Get parent process ID
  {"getuid", getuid},      // Get user ID
  {"geteuid", geteuid},    // Get effective user ID
  {"getgid", getgid},      // Get group ID
  {"getegid", getegid},    // Get effective group ID
  {"kill", kill},          // Send signal to process
  {"setrlimit", setrlimit},// Set resource limits

  // POSIX Signal Handling
  {"sigaction", cosmorun_sigaction},
  {"sigemptyset", sigemptyset},
  {"sigaddset", sigaddset},    // Add signal to set
  {"sigdelset", sigdelset},    // Remove signal from set
  {"sigfillset", sigfillset},  // Fill signal set
  {"sigprocmask", sigprocmask},// Examine/change signal mask

  // POSIX Threading (pthread)
  {"pthread_create", pthread_create},
  {"pthread_join", pthread_join},
  {"pthread_detach", pthread_detach},
  {"pthread_self", pthread_self},
  {"pthread_mutex_init", pthread_mutex_init},
  {"pthread_mutex_destroy", pthread_mutex_destroy},
  {"pthread_mutex_lock", pthread_mutex_lock},
  {"pthread_mutex_unlock", pthread_mutex_unlock},
  {"pthread_mutex_trylock", pthread_mutex_trylock},
  {"pthread_cond_init", pthread_cond_init},
  {"pthread_cond_destroy", pthread_cond_destroy},
  {"pthread_cond_wait", pthread_cond_wait},
  {"pthread_cond_signal", pthread_cond_signal},
  {"pthread_cond_broadcast", pthread_cond_broadcast},

  // ===== Atomic Operations (sync_* and atomic_* wrappers for TCC compatibility) =====

  // Phase 1: sync_fetch_and_* (returns old value)
  {"sync_fetch_and_add_int", sync_fetch_and_add_int},
  {"sync_fetch_and_sub_int", sync_fetch_and_sub_int},
  {"sync_fetch_and_add_long", sync_fetch_and_add_long},
  {"sync_fetch_and_sub_long", sync_fetch_and_sub_long},
  {"sync_fetch_and_or_int", sync_fetch_and_or_int},
  {"sync_fetch_and_and_int", sync_fetch_and_and_int},
  {"sync_fetch_and_xor_int", sync_fetch_and_xor_int},

  // Phase 2: sync_*_and_fetch (returns new value)
  {"sync_add_and_fetch_int", sync_add_and_fetch_int},
  {"sync_sub_and_fetch_int", sync_sub_and_fetch_int},
  {"sync_add_and_fetch_long", sync_add_and_fetch_long},
  {"sync_sub_and_fetch_long", sync_sub_and_fetch_long},
  {"sync_or_and_fetch_int", sync_or_and_fetch_int},
  {"sync_and_and_fetch_int", sync_and_and_fetch_int},
  {"sync_xor_and_fetch_int", sync_xor_and_fetch_int},

  // Phase 3: sync_*_compare_and_swap
  {"sync_bool_compare_and_swap_int", sync_bool_compare_and_swap_int},
  {"sync_val_compare_and_swap_int", sync_val_compare_and_swap_int},
  {"sync_bool_compare_and_swap_long", sync_bool_compare_and_swap_long},
  {"sync_val_compare_and_swap_long", sync_val_compare_and_swap_long},
  {"sync_bool_compare_and_swap_ptr", sync_bool_compare_and_swap_ptr},
  {"sync_val_compare_and_swap_ptr", sync_val_compare_and_swap_ptr},

  // Phase 4: sync_lock_* and sync_synchronize
  {"sync_lock_test_and_set_int", sync_lock_test_and_set_int},
  {"sync_lock_release_int", sync_lock_release_int},
  {"sync_synchronize", sync_synchronize},

  // C11-style atomic_* operations
  {"atomic_fetch_add_int", atomic_fetch_add_int},
  {"atomic_fetch_sub_int", atomic_fetch_sub_int},
  {"atomic_fetch_or_int", atomic_fetch_or_int},
  {"atomic_fetch_and_int", atomic_fetch_and_int},
  {"atomic_fetch_xor_int", atomic_fetch_xor_int},
  {"atomic_load_int", atomic_load_int},
  {"atomic_store_int", atomic_store_int},
  {"atomic_exchange_int", atomic_exchange_int},

  // POSIX Time Functions
  {"clock_gettime", clock_gettime}, // High-resolution clock (POSIX.1-2001)
  {"gettimeofday", gettimeofday},   // Get time of day
  {"nanosleep", nanosleep},         // High-precision sleep
  {"sleep", sleep},                 // Sleep seconds
  {"usleep", usleep},               // Sleep microseconds
  {"time", time},                   // Get current time (C99)
  {"localtime", localtime},         // Convert to local time (C99)

  // POSIX Network Functions (Berkeley sockets)
  {"socket", socket},          // Create socket
  {"bind", bind},              // Bind socket to address
  {"listen", listen},          // Listen for connections
  {"accept", accept},          // Accept connection
  {"connect", connect},        // Connect to address
  {"send", send},              // Send data
  {"recv", recv},              // Receive data
  {"shutdown", shutdown},      // Shutdown socket
  {"setsockopt", setsockopt},  // Set socket options
  {"htons", htons},            // Host to network short
  {"htonl", htonl},            // Host to network long
  {"inet_addr", inet_addr},    // Convert IP string to binary
  {"inet_ntop", inet_ntop},    // Convert binary IP to string (modern)
  {"inet_pton", inet_pton},    // Convert string IP to binary (modern)

  // POSIX I/O Multiplexing
  {"select", select},      // Synchronous I/O multiplexing
  {"poll", poll},          // Poll file descriptors

  // POSIX Terminal I/O
  {"isatty", isatty},          // Check if fd is a terminal
  {"tcgetattr", tcgetattr},    // Get terminal attributes
  {"tcsetattr", tcsetattr},    // Set terminal attributes

  // POSIX System Information
  {"uname", cosmorun_uname},   // Get system information

  // POSIX Miscellaneous
  {"fileno", fileno},          // Get fd from FILE*
  {"getopt_long", getopt_long},// Parse command-line options
  {"strdup", strdup},          // Duplicate string (POSIX)

  // Process I/O
  {"popen", popen},
  {"pclose", pclose},

  // Signal handling (setjmp family)
  {"sigsetjmp", sigsetjmp},
  {"siglongjmp", siglongjmp},

  // Program control
  {"exit", exit},
  {"abort", abort},
  {"system", system},

//when remove cosmo_dlxxxx
  {"cbrt", cbrt},
  {"fmod", fmod},
  {"hypot", hypot},
  {"acosh", acosh},
  {"log1p", log1p},

//   //TODO
//   {"va_start",va_start},
//   {"va_end",va_end},
//   //va_list,__builtin_va_list
//   {"va_copy",va_copy},
//   {"va_arg",va_arg},

  // libtcc functions
  //{"tcc_new", tcc_new},
  //{"tcc_delete", tcc_delete},
  //{"tcc_set_error_func", tcc_set_error_func},
  //{"tcc_set_output_type", tcc_set_output_type},
  //{"tcc_add_include_path", tcc_add_include_path},
  //{"tcc_add_sysinclude_path", tcc_add_sysinclude_path},
  //{"tcc_add_library_path", tcc_add_library_path},
  //{"tcc_add_library", tcc_add_library},
  //{"tcc_add_symbol", tcc_add_symbol},
  //{"tcc_compile_string", tcc_compile_string},
  //{"tcc_add_file", tcc_add_file},
  //{"tcc_relocate", tcc_relocate},
  //{"tcc_get_symbol", tcc_get_symbol},
  //{"tcc_set_options", tcc_set_options},
  //{"tcc_output_file", tcc_output_file},

  {NULL, NULL}  // Sentinel - must be last
};

void register_builtin_symbols(TCCState *s) {
    if (!s) {
        return;
    }

    // Add standard I/O file descriptors
    tcc_add_symbol(s, "stdin", &stdin);
    tcc_add_symbol(s, "stdout", &stdout);
    tcc_add_symbol(s, "stderr", &stderr);

    tcc_add_symbol(s, "optind", &optind);
    tcc_add_symbol(s, "errno", &errno);
    tcc_add_symbol(s, "optarg", &optarg);
    //tcc_add_symbol(s, "sigaction", &sigaction);

    for (const SymbolEntry *entry = builtin_symbol_table; entry && entry->name; ++entry) {
        // Safety: verify entry is valid before accessing
        if (!entry->name) break;

        // Skip NULL addresses (platform-specific functions not available)
        if (!entry->address) {
            tracef("skipping NULL symbol: %s", entry->name);
            continue;
        }

        // Windows-specific: Skip POSIX-only functions that may be stubs
        if (IsWindows()) {
            if (strcmp(entry->name, "fork") == 0 ||
                strcmp(entry->name, "waitpid") == 0 ||
                strcmp(entry->name, "execve") == 0) {
                tracef("skipping POSIX symbol on Windows: %s", entry->name);
                continue;
            }
        }

        tracef("registering symbol: %s (addr=%p)", entry->name, entry->address);
        if (!s->symtab) {
            tracef("WARNING: symtab is NULL before adding symbol %s", entry->name);
        }
        if (tcc_add_symbol(s, entry->name, entry->address) != 0) {
            tracef("register_builtin_symbols: failed for %s", entry->name);
        }
    }

    /* Register FFI generator symbols */
    tcc_add_symbol(s, "ffi_context_create", ffi_context_create);
    tcc_add_symbol(s, "ffi_context_destroy", ffi_context_destroy);
    tcc_add_symbol(s, "ffi_parse_header", ffi_parse_header);
    tcc_add_symbol(s, "ffi_generate_bindings", ffi_generate_bindings);
    tcc_add_symbol(s, "ffi_parse_function_declaration", ffi_parse_function_declaration);
    tcc_add_symbol(s, "ffi_parse_struct", ffi_parse_struct);
    tcc_add_symbol(s, "ffi_parse_enum", ffi_parse_enum);
    tcc_add_symbol(s, "ffi_parse_typedef", ffi_parse_typedef);
    tcc_add_symbol(s, "ffi_get_type_category", ffi_get_type_category);
    tcc_add_symbol(s, "ffi_generate_function_pointer", ffi_generate_function_pointer);
    tcc_add_symbol(s, "ffi_generate_loader_code", ffi_generate_loader_code);
    tcc_add_symbol(s, "ffi_trim_whitespace", ffi_trim_whitespace);
    tcc_add_symbol(s, "ffi_is_comment_or_empty", ffi_is_comment_or_empty);
    tcc_add_symbol(s, "ffi_remove_preprocessor", ffi_remove_preprocessor);

    /* Register coroutine runtime symbols */
    /* NOTE: Stackful coroutines no longer need these copy-stack helpers */
    // tcc_add_symbol(s, "__co_stack_alloc", __co_stack_alloc);  /* REMOVED: stackful uses malloc */
    // tcc_add_symbol(s, "__co_stack_free", __co_stack_free);    /* REMOVED: stackful uses free */
    // tcc_add_symbol(s, "__co_alloca", __co_alloca);            /* REMOVED: copy-stack only */
    // tcc_add_symbol(s, "__co_read_sp", __co_read_sp);          /* REMOVED: copy-stack only */
    // tcc_add_symbol(s, "__co_setjmp", __co_setjmp);            /* REMOVED: copy-stack only */
    // tcc_add_symbol(s, "__co_longjmp", __co_longjmp);          /* REMOVED: copy-stack only */

    /* Register pthread stack APIs (cosmopolitan cross-platform) */
    tcc_add_symbol(s, "pthread_getattr_np", pthread_getattr_np);
    tcc_add_symbol(s, "pthread_attr_getstack", pthread_attr_getstack);
    tcc_add_symbol(s, "pthread_attr_destroy", pthread_attr_destroy);

    /* Register pure builtin coroutine API */
    tcc_add_symbol(s, "__co_builtin_create", __co_builtin_create);
    tcc_add_symbol(s, "__co_builtin_resume_api", __co_builtin_resume_api);
    tcc_add_symbol(s, "__co_builtin_yield", __co_builtin_yield);
    tcc_add_symbol(s, "__co_builtin_free", __co_builtin_free);
    tcc_add_symbol(s, "__co_builtin_state", __co_builtin_state);
    tcc_add_symbol(s, "__co_builtin_is_alive", __co_builtin_is_alive);

    /* Register lockfile system symbols */
    tcc_add_symbol(s, "cosmo_lock_create", cosmo_lock_create);
    tcc_add_symbol(s, "cosmo_lock_destroy", cosmo_lock_destroy);
    tcc_add_symbol(s, "cosmo_lock_set_lockfile_path", cosmo_lock_set_lockfile_path);
    tcc_add_symbol(s, "cosmo_lock_set_package_json_path", cosmo_lock_set_package_json_path);
    tcc_add_symbol(s, "cosmo_lock_generate", cosmo_lock_generate);
    tcc_add_symbol(s, "cosmo_lock_load", cosmo_lock_load);
    tcc_add_symbol(s, "cosmo_lock_save", cosmo_lock_save);
    tcc_add_symbol(s, "cosmo_lock_verify", cosmo_lock_verify);
    tcc_add_symbol(s, "cosmo_lock_update_dependency", cosmo_lock_update_dependency);
    tcc_add_symbol(s, "cosmo_lock_add_dependency", cosmo_lock_add_dependency);
    tcc_add_symbol(s, "cosmo_lock_find_dependency", cosmo_lock_find_dependency);
    tcc_add_symbol(s, "cosmo_lock_remove_dependency", cosmo_lock_remove_dependency);
    tcc_add_symbol(s, "cosmo_lock_resolve_conflicts", cosmo_lock_resolve_conflicts);
    tcc_add_symbol(s, "cosmo_lock_version_compare", cosmo_lock_version_compare);
    tcc_add_symbol(s, "cosmo_lock_version_satisfies", cosmo_lock_version_satisfies);
    tcc_add_symbol(s, "cosmo_lock_parse_semver", cosmo_lock_parse_semver);
    tcc_add_symbol(s, "cosmo_lock_calculate_integrity", cosmo_lock_calculate_integrity);
    tcc_add_symbol(s, "cosmo_lock_verify_integrity", cosmo_lock_verify_integrity);
    tcc_add_symbol(s, "cosmo_lock_to_json", cosmo_lock_to_json);
    tcc_add_symbol(s, "cosmo_lock_from_json", cosmo_lock_from_json);
    tcc_add_symbol(s, "cosmo_lock_should_install", cosmo_lock_should_install);
    tcc_add_symbol(s, "cosmo_lock_mark_installed", cosmo_lock_mark_installed);
    tcc_add_symbol(s, "cosmo_lock_get_install_version", cosmo_lock_get_install_version);
    tcc_add_symbol(s, "cosmo_lock_print_summary", cosmo_lock_print_summary);
    tcc_add_symbol(s, "cosmo_lock_get_error", cosmo_lock_get_error);
    tcc_add_symbol(s, "cosmo_lock_clear_error", cosmo_lock_clear_error);
    tcc_add_symbol(s, "cosmo_lock_validate", cosmo_lock_validate);
}

// ============================================================================
// Symbol Resolution (Dynamic Loading from System Libraries)
// ============================================================================

#define MAX_LIBRARY_HANDLES 16
typedef struct {
    void* handles[MAX_LIBRARY_HANDLES];
    int handle_count;
    int initialized;
} SymbolResolver;

static SymbolResolver g_resolver = {{0}, 0, 0};

// Initialize dynamic symbol resolver (lazy initialization)
static void init_symbol_resolver(void) {
    if (g_resolver.initialized) return;

    tracef("Initializing dynamic symbol resolver");

    // Runtime platform-specific library names for dynamic symbol resolution
    // Note: Cosmopolitan doesn't define _WIN32 at compile time, must use runtime checks
    // Static storage to avoid stack lifetime issues
    static const char* win_libs[] = {
        "msvcrt.dll",              // Windows C runtime
        "ucrtbase.dll",            // Universal CRT base
        "kernel32.dll",            // Windows kernel32
        NULL
    };
    static const char* mac_libs[] = {
        "libm.dylib",              // macOS math library
        "libSystem.B.dylib",       // macOS system library (includes libc)
        NULL
    };
    static const char* linux_libs[] = {
        "libm.so.6",                                    // Linux math library (standard)
        "libc.so.6",                                    // Linux glibc (standard)
        "/usr/lib/x86_64-linux-gnu/libm.so.6",        // Ubuntu/Debian x86_64 math
        "/usr/lib/x86_64-linux-gnu/libc.so.6",        // Ubuntu/Debian x86_64 libc
        "/usr/lib/aarch64-linux-gnu/libm.so.6",       // Ubuntu/Debian ARM64 math
        "/usr/lib/aarch64-linux-gnu/libc.so.6",       // Ubuntu/Debian ARM64 libc
        "/lib/x86_64-linux-gnu/libm.so.6",            // Alternative x86_64 location
        "/lib/x86_64-linux-gnu/libc.so.6",            // Alternative x86_64 location
        "libm.so",                                      // Generic math library
        "libc.so",                                      // Generic libc
        NULL
    };

    const char** lib_names_ptr;

    // Select library list based on runtime platform detection
    if (IsWindows()) {
        lib_names_ptr = win_libs;
        tracef("Platform: Windows");
    } else if (IsXnu()) {
        lib_names_ptr = mac_libs;
        tracef("Platform: macOS");
    } else {
        lib_names_ptr = linux_libs;
        tracef("Platform: Linux");
    }

    for (int i = 0; lib_names_ptr[i] && g_resolver.handle_count < MAX_LIBRARY_HANDLES; i++) {
        // Windows doesn't support RTLD_GLOBAL, use RTLD_LAZY only on Windows
        int flags = IsWindows() ? RTLD_LAZY : (RTLD_LAZY | RTLD_GLOBAL);
        void* handle = xdl_open(lib_names_ptr[i], flags);
        if (handle) {
            g_resolver.handles[g_resolver.handle_count++] = handle;
            tracef("Loaded library: %s (handle=%p)", lib_names_ptr[i], handle);
        } else {
            const char* err = xdl_error();
            tracef("Failed to load %s: %s", lib_names_ptr[i], err ? err : "unknown error");
        }
    }

    g_resolver.initialized = 1;
    tracef("Symbol resolver initialized with %d libraries", g_resolver.handle_count);
}

void* cosmorun_dlsym_libc(const char* symbol_name) {
    if (!symbol_name || !*symbol_name) {
        return NULL;
    }
    if (!g_resolver.initialized) {
        init_symbol_resolver();
    }
    tracef("cosmorun_dlsym_libc: %s", symbol_name);

    // Fast path: check the builtin table first (with safe double-check pattern)
    for (const cosmo_symbol_entry_t *entry = cosmo_tcc_get_builtin_symbols(); entry && entry->name; ++entry) {
        // Safety: verify entry is valid before accessing (cross-platform compatibility)
        if (!entry->name) break;

        if (strcmp(entry->name, symbol_name) == 0) {
            if (entry->address) {
                tracef("Found in builtin table: %s", symbol_name);
                return entry->address;
            }
        }
    }

    void* addr = NULL;
    for (int i = 0; i < g_resolver.handle_count; i++) {
        if (!g_resolver.handles[i]) continue;
        addr = xdl_sym(g_resolver.handles[i], symbol_name);
        if (addr) {
            tracef("Resolved from library %d: %s -> %p", i, symbol_name, addr);
            return addr;
        }
    }

    // Symbol not found anywhere
    tracef("Symbol not found: %s", symbol_name);
    return NULL;
}

/* Part C: TCC State Management and Path Functions */
#include "cosmo_tcc.h"

/**
 * ============================================================================
 * TCC Configuration Structures
 * ============================================================================
 */

const cosmo_tcc_config_t COSMO_TCC_CONFIG_MEMORY = {
    .output_type = TCC_OUTPUT_MEMORY,
    .output_file = NULL,
    .relocate = 1,
    .run_entry = 1
};

const cosmo_tcc_config_t COSMO_TCC_CONFIG_OBJECT = {
    .output_type = TCC_OUTPUT_OBJ,
    .output_file = NULL,
    .relocate = 0,
    .run_entry = 1
};

/**
 * ============================================================================
 * TCC State Creation with Configuration
 * ============================================================================
 */

TCCState* create_tcc_state_with_config(int output_type, const char* options, int enable_paths, int enable_resolver) {
    TCCState *s = tcc_new();
    if (!s) {
        cosmorun_perror(COSMORUN_ERROR_TCC_INIT, "tcc_new");
        return NULL;
    }

    tcc_set_error_func(s, NULL, tcc_error_func);
    tcc_set_output_type(s, output_type);

    if (options && options[0]) {
        tcc_set_options(s, options);
    }

    if (enable_paths) {
        register_default_include_paths(s, &g_config.uts);
        register_default_library_paths(s);
    }

    register_builtin_symbols(s);
    link_tcc_runtime(s);

    return s;
}

/**
 * ============================================================================
 * Resource Management Abstraction
 * ============================================================================
 */

/* Generic resource cleanup function type */
typedef void (*resource_cleanup_fn)(void* resource);

/* Resource manager */
typedef struct {
    void* resource;
    resource_cleanup_fn cleanup_fn;
    const char* name;
} resource_manager_t;

/* TCC state cleanup function */
void tcc_state_cleanup(void* resource) {
    if (!resource) return;

    TCCState **state_ptr = (TCCState**)resource;
    if (state_ptr && *state_ptr) {
        tcc_delete(*state_ptr);
        *state_ptr = NULL;
    }
}

/* Memory cleanup function */
void memory_cleanup(void* resource) {
    if (!resource) return;

    void **ptr = (void**)resource;
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

/* Create resource manager */
inline resource_manager_t create_resource_manager(void* resource,
                                                        resource_cleanup_fn cleanup_fn,
                                                        const char* name) {
    resource_manager_t manager = {
        .resource = resource,
        .cleanup_fn = cleanup_fn,
        .name = name ? name : "unnamed"
    };
    return manager;
}

/* Cleanup resource manager */
inline void cleanup_resource_manager(resource_manager_t* manager) {
    if (manager && manager->resource && manager->cleanup_fn) {
        if (g_config.trace_enabled) {
        }
        manager->cleanup_fn(manager->resource);
        manager->resource = NULL;
        manager->cleanup_fn = NULL;
    }
}

/* RAII-style automatic cleanup macro (GCC/Clang) */
#if defined(__GNUC__) || defined(__clang__)
#define AUTO_CLEANUP(cleanup_fn) __attribute__((cleanup(cleanup_fn)))
#define AUTO_TCC_STATE TCCState* AUTO_CLEANUP(tcc_state_cleanup)
#define AUTO_MEMORY char* AUTO_CLEANUP(memory_cleanup)
#else
#define AUTO_CLEANUP(cleanup_fn)
#define AUTO_TCC_STATE TCCState*
#define AUTO_MEMORY char*
#endif

/**
 * ============================================================================
 * TCC Options Building Functions
 * ============================================================================
 */

// append_tcc_option is now append_string_option in cosmo_utils

void build_default_tcc_options(char *buffer, size_t size, const struct utsname *uts) {
    if (!buffer || size == 0) return;

    buffer[0] = '\0';
    append_string_option(buffer, size, "-nostdlib");
    append_string_option(buffer, size, "-nostdinc");
    append_string_option(buffer, size, "-D__COSMORUN__");

    int is_windows = 0;
    int is_macos = 0;
    int is_linux = 0;

    const char *sys = (uts && uts->sysname[0]) ? uts->sysname : NULL;
    if (sys && *sys) {
        if (str_iequals(sys, "Windows") || str_istartswith(sys, "CYGWIN_NT") ||
            str_istartswith(sys, "MINGW")) {
            is_windows = 1;
        } else if (str_iequals(sys, "Darwin")) {
            is_macos = 1;
        } else if (str_iequals(sys, "Linux")) {
            is_linux = 1;
        }
    }

    if (!is_windows && !is_macos && !is_linux) {
        const char *windir = getenv("WINDIR");
        if (!windir || !*windir) windir = getenv("SystemRoot");
        if (windir && *windir) {
            is_windows = 1;
        }
    }

    if (!is_windows && !is_macos && !is_linux) {
        const char *platform = getenv("OSTYPE");
        if (platform && *platform) {
            if (strstr(platform, "darwin") || strstr(platform, "mac")) {
                is_macos = 1;
            } else if (strstr(platform, "linux")) {
                is_linux = 1;
            }
        }
    }

    if (!is_windows && !is_macos && !is_linux) {
        const char *home = getenv("HOME");
        if (home && strstr(home, "/Library/Application Support")) {
            is_macos = 1;
        }
    }

    if (is_windows) {
        append_string_option(buffer, size, "-D_WIN32");
        append_string_option(buffer, size, "-DWIN32");
        append_string_option(buffer, size, "-D_WINDOWS");
#if defined(_M_ARM) || defined(__arm__)
        append_string_option(buffer, size, "-D_M_ARM");
        append_string_option(buffer, size, "-D__ARM_ARCH_7__");
        append_string_option(buffer, size, "-D__thumb2__");
        append_string_option(buffer, size, "-DTCC_TARGET_ARM");
#elif defined(_M_ARM64) || defined(__aarch64__)
        append_string_option(buffer, size, "-DTCC_TARGET_ARM64");
#endif
    } else if (is_macos) {
        append_string_option(buffer, size, "-D__APPLE__");
        append_string_option(buffer, size, "-D__MACH__");
        append_string_option(buffer, size, "-DTCC_TARGET_MACHO");
#ifdef __aarch64__
        append_string_option(buffer, size, "-DTCC_TARGET_ARM64");
#elif defined(__arm__)
        append_string_option(buffer, size, "-DTCC_TARGET_ARM");
        append_string_option(buffer, size, "-D__ARM_ARCH_7A__");
#endif
    } else {
        append_string_option(buffer, size, "-D__unix__");
        if (is_linux) {
            append_string_option(buffer, size, "-D__linux__");
#if defined(__arm__)
            append_string_option(buffer, size, "-D__ARM_ARCH_7__");
            append_string_option(buffer, size, "-DTCC_TARGET_ARM");
#endif
        }
    }
}

// Utility functions moved to cosmo_utils.{c,h}

// Wrapper for dlsym with trampoline
void *cosmorun_dlsym(void *handle, const char *symbol) {
    void *addr = xdl_sym(handle, symbol);
    return cosmo_trampoline_wrap(handle, addr);
}

/**
 * ============================================================================
 * Path Management Functions
 * ============================================================================
 */

void tcc_add_path_if_exists(TCCState *s, const char *path, int include_mode) {
    if (!dir_exists(path)) return;
    if (include_mode) {
        tracef("adding include path: %s", path);
        tcc_add_include_path(s, path);
        tcc_add_sysinclude_path(s, path);
    } else {
        tracef("adding library path: %s", path);
        tcc_add_library_path(s, path);
    }
}


// Path cache - global static storage for optimization
#define MAX_CACHED_PATHS 16
static const char *g_cached_include_paths[MAX_CACHED_PATHS] = {NULL};
static int g_cached_path_count = 0;
static int g_paths_initialized = 0;

void register_default_include_paths(TCCState *s, const struct utsname *uts) {
    const char *sysname = uts && uts->sysname[0] ? uts->sysname : "";

    // Fast path: use cached paths (skip dir_exists checks ~2-4ms)
    if (g_paths_initialized) {
        if (g_config.trace_enabled) {
        }
        for (int i = 0; i < g_cached_path_count; i++) {
            tcc_add_include_path(s, g_cached_include_paths[i]);
            tcc_add_sysinclude_path(s, g_cached_include_paths[i]);
        }
        return;
    }

    // Slow path: first time, check and cache valid paths
    if (g_config.trace_enabled >= 2) {
    }

    // Universal local paths (checked on ALL platforms first)
    const char *local_candidates[] = {
        ".",                   // Current directory (for root-level headers)
        "./c_modules",         // Module directory (for module inter-dependencies)
        "./include",           // Current directory include
        "./lib/include",       // Local lib include
        "../include",          // Parent directory include (for project structures)
        NULL
    };

    // Platform-specific system paths
    const char *posix_candidates[] = {
        "/usr/lib/gcc/x86_64-linux-gnu/11/include",  // GCC builtin headers
        "/usr/lib/gcc/x86_64-linux-gnu/12/include",
        "/usr/local/include",
        "/usr/include/x86_64-linux-gnu",
        "/usr/include",
        "/opt/local/include",
        NULL
    };
    const char *mac_candidates[] = {
        "/opt/homebrew/include",
        "/usr/local/include",
        // SKIP macOS SDK paths - they cause architecture mismatch with TCC
        // "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include",
        // "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include",
        NULL
    };
    // Windows: only check local paths (no hardcoded C:\ paths)
    // System includes can be specified via -I command-line option
    const char *windows_candidates[] = {
        NULL  // Only local paths for Windows
    };

    // Select platform-specific candidates
    const char **platform_candidates = posix_candidates;
    if (IsWindows()) {
        platform_candidates = windows_candidates;
    } else if (!strcasecmp(sysname, "darwin")) {
        platform_candidates = mac_candidates;
    }

    // Phase 1: Check local paths first (all platforms)
    for (const char **p = local_candidates; *p; ++p) {
        if (dir_exists(*p)) {
            if (g_cached_path_count < MAX_CACHED_PATHS) {
                g_cached_include_paths[g_cached_path_count++] = *p;
            }
            tcc_add_include_path(s, *p);
            tcc_add_sysinclude_path(s, *p);
            tracef("cached local include path: %s", *p);
        }
    }

    // Phase 2: Check platform-specific system paths
    for (const char **p = platform_candidates; *p; ++p) {
        if (dir_exists(*p)) {
            if (g_cached_path_count < MAX_CACHED_PATHS) {
                g_cached_include_paths[g_cached_path_count++] = *p;
            }
            tcc_add_include_path(s, *p);
            tcc_add_sysinclude_path(s, *p);
            tracef("cached system include path: %s", *p);
        }
    }

    g_paths_initialized = 1;

    if (g_config.trace_enabled >= 2) {
    }
}

void register_default_library_paths(TCCState *s) {
    // Library paths can be specified via -L command-line option
    (void)s;  // Currently no default library paths
}

/**
 * ============================================================================
 * TCC Error Handling
 * ============================================================================
 */

void tcc_error_func(void *opaque, const char *msg) {
    (void)opaque;

    // Show warnings (except implicit declarations which are too noisy)
    if (strstr(msg, "warning: implicit declaration")) return;
    if (strstr(msg, "warning:")) {
        fprintf(stderr, "TCC Warning: %s\n", msg);
        return;
    }

    // Convert include file errors to warnings per user request
    if (strstr(msg, "include file") && strstr(msg, "not found")) {
        fprintf(stderr, "TCC Warning: %s\n", msg);
        return;
    }

    // Convert duplicate symbol errors to warnings per user request
    if (strstr(msg, "defined twice") || strstr(msg, "undefined symbol")) {
        fprintf(stderr, "TCC Warning: %s\n", msg);
        return;
    }

    fprintf(stderr, "TCC Error: %s\n", msg);
}

/**
 * ============================================================================
 * TCC State Initialization
 * ============================================================================
 */

TCCState* init_tcc_state(void) {
    // Ensure configuration is initialized
    if (!g_config.initialized) {
        cosmorun_result_t result = init_config();
        if (result != COSMORUN_SUCCESS) {
            cosmorun_perror(result, "init_config");
            return NULL;
        }

        // Register cache stats printer (one-time, at first init)
        static int stats_registered = 0;
        if (!stats_registered && getenv("COSMORUN_DEBUG_CACHE")) {
            atexit(print_module_cache_stats);
            stats_registered = 1;
        }
    }

    TCCState *s = tcc_new();
    if (!s) {
        cosmorun_perror(COSMORUN_ERROR_TCC_INIT, "tcc_new");
        return NULL;
    }

    tcc_set_error_func(s, NULL, tcc_error_func);

    /* Pre-set output type to ensure symbol table is ready */
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

    build_default_tcc_options(g_config.tcc_options, sizeof(g_config.tcc_options), &g_config.uts);
    if (g_config.tcc_options[0]) {
        if (g_config.trace_enabled) {
        }
        tcc_set_options(s, g_config.tcc_options);
    }

    register_default_include_paths(s, &g_config.uts);
    register_default_library_paths(s);

    // init_symbol_resolver();
    register_builtin_symbols(s);
    link_tcc_runtime(s);  // Link runtime library for --eval mode
    return s;
}

// ============================================================================
// Public API Implementation
// ============================================================================

const char* cosmo_tcc_get_runtime_lib(void) {
    return tcc_runtime_lib;
}

void cosmo_tcc_link_runtime(TCCState *s) {
    link_tcc_runtime(s);
}

const cosmo_symbol_entry_t* cosmo_tcc_get_builtin_symbols(void) {
    return (const cosmo_symbol_entry_t*)builtin_symbol_table;
}

void cosmo_tcc_register_builtin_symbols(TCCState *s) {
    register_builtin_symbols(s);
}

void cosmo_tcc_build_default_options(char *buffer, size_t size, const struct utsname *uts) {
    build_default_tcc_options(buffer, size, uts);
}

void cosmo_tcc_append_option(char *buffer, size_t size, const char *opt) {
    append_string_option(buffer, size, opt);
}

void cosmo_tcc_register_include_paths(TCCState *s, const struct utsname *uts) {
    register_default_include_paths(s, uts);
}

void cosmo_tcc_register_library_paths(TCCState *s) {
    register_default_library_paths(s);
}

void cosmo_tcc_add_path_if_exists(TCCState *s, const char *path, int include_mode) {
    tcc_add_path_if_exists(s, path, include_mode);
}


bool cosmo_tcc_dir_exists(const char *path) {
    return dir_exists(path);
}

void cosmo_tcc_error_func(void *opaque, const char *msg) {
    tcc_error_func(opaque, msg);
}

void cosmo_tcc_set_error_handler(TCCState *s) {
    tcc_set_error_func(s, NULL, tcc_error_func);
}

TCCState* cosmo_tcc_create_state(int output_type, const char* options,
                                  int enable_paths, int enable_resolver) {
    return create_tcc_state_with_config(output_type, options, enable_paths, enable_resolver);
}

TCCState* cosmo_tcc_init_state(void) {
    return init_tcc_state();
}

void cosmo_tcc_state_cleanup(void* resource) {
    tcc_state_cleanup(resource);
}

int cosmo_tcc_get_cached_path_count(void) {
    return g_cached_path_count;
}

const char* cosmo_tcc_get_cached_path(int index) {
    if (index < 0 || index >= g_cached_path_count) {
        return NULL;
    }
    return g_cached_include_paths[index];
}

// Simple JSON parser to extract dependencies from module.json
// Returns number of dependencies found (max 32), fills deps array with dependency names
static int parse_module_dependencies(const char* module_dir, char deps[][256], int max_deps) {
    char json_path[PATH_MAX];
    snprintf(json_path, sizeof(json_path), "%s/module.json", module_dir);

    FILE* f = fopen(json_path, "r");
    if (!f) {
        tracef("no module.json found at '%s'", json_path);
        return 0;
    }

    // Read entire file into buffer
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 1024 * 1024) {
        fclose(f);
        return 0;
    }

    char* content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return 0;
    }

    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    // Find "dependencies" field
    char* deps_start = strstr(content, "\"dependencies\"");
    if (!deps_start) {
        free(content);
        return 0;
    }

    // Find opening bracket [
    char* array_start = strchr(deps_start, '[');
    if (!array_start) {
        free(content);
        return 0;
    }

    // Find closing bracket ]
    char* array_end = strchr(array_start, ']');
    if (!array_end) {
        free(content);
        return 0;
    }

    // Extract dependency names from array
    int dep_count = 0;
    char* pos = array_start + 1;

    while (pos < array_end && dep_count < max_deps) {
        // Skip whitespace
        while (*pos == ' ' || *pos == '\n' || *pos == '\r' || *pos == '\t' || *pos == ',') {
            pos++;
        }

        if (*pos == '"') {
            pos++; // Skip opening quote
            char* name_start = pos;

            // Find closing quote
            while (*pos && *pos != '"') {
                pos++;
            }

            if (*pos == '"') {
                int len = pos - name_start;
                if (len > 0 && len < 255) {
                    strncpy(deps[dep_count], name_start, len);
                    deps[dep_count][len] = '\0';
                    tracef("found dependency: '%s'", deps[dep_count]);
                    dep_count++;
                }
                pos++; // Skip closing quote
            }
        } else if (*pos == ']') {
            break;
        } else {
            pos++;
        }
    }

    free(content);
    return dep_count;
}

// ============================================================================
// Circular Dependency Detection
// ============================================================================

#define MAX_LOADING_DEPTH 32

typedef struct {
    char paths[MAX_LOADING_DEPTH][PATH_MAX];
    int count;
} loading_stack_t;

// Thread-local storage: each thread has independent loading stack
// This prevents interference when multiple threads load modules concurrently
__thread loading_stack_t g_loading_stack = {0};

// ============================================================================
// Module Registry (Module Caching & Reference Counting)
// ============================================================================

// Module cache state
typedef enum {
    MODULE_ACTIVE = 0,    // Active (ref_count > 0)
    MODULE_IDLE = 1,      // Idle in cache (ref_count == 0)
    MODULE_EVICTED = 2    // Evicted from cache
} module_state_t;

typedef struct module {
    char path[PATH_MAX];          // Module path (normalized)
    TCCState* state;              // TCC state (compiled code)
    atomic_int ref_count;         // Reference count (user references) - ATOMIC for thread-safety
    module_state_t cache_state;   // Cache state (ACTIVE/IDLE/EVICTED)
    time_t load_time;             // Load timestamp
    atomic_long last_access;      // Last access timestamp (for LRU) - ATOMIC (time_t is long)
    struct module* next;          // Linked list
} module_t;

typedef struct {
    module_t* head;               // Linked list head (protected by g_registry_lock)
    int count;                    // Total modules (ACTIVE + IDLE) - protected by g_registry_lock
    atomic_int active_count;      // Active modules (ref_count > 0) - ATOMIC for lock-free incref/decref
    atomic_int idle_count;        // Idle modules (ref_count == 0, cached) - ATOMIC
    // Cache statistics (best-effort, lock-free updates acceptable)
    atomic_long cache_hits;       // Cache hits (found IDLE module)
    atomic_long cache_misses;     // Cache misses (need to compile)
    atomic_long evictions;        // Cache evictions (LRU)
} module_registry_t;

static module_registry_t g_module_registry = {0};

// Cache configuration
#define MODULE_CACHE_MAX_IDLE 32  // Max idle modules to keep cached

// Thread-safe access to registry with read-write lock
// Read lock: multiple threads can search concurrently
// Write lock: exclusive access for register/unregister operations
static pthread_rwlock_t g_registry_lock = PTHREAD_RWLOCK_INITIALIZER;

// Global compilation mutex to prevent concurrent TCC compilations
// TCC is not thread-safe, so we need to serialize all compilation operations
// This prevents TOCTOU race conditions where multiple threads try to compile
// the same module simultaneously
// IMPORTANT: This must be a RECURSIVE mutex to allow dependency loading within
// the same thread (mod_net depends on std, both need compilation)
static pthread_mutex_t g_compilation_lock;
static pthread_once_t g_compilation_lock_once = PTHREAD_ONCE_INIT;

static void init_compilation_lock(void) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_compilation_lock, &attr);
    pthread_mutexattr_destroy(&attr);
}

// Forward declarations
static void module_incref(module_t* mod);
static int module_decref(module_t* mod);

// Find module by path (thread-safe with read lock)
// Returns module in any state (ACTIVE or IDLE)
// NOTE: Caller must handle module_incref BEFORE releasing lock (use find_and_incref_module instead)
static module_t* find_module_unlocked(const char* path) {
    // Caller must hold g_registry_lock (rdlock or wrlock)
    for (module_t* mod = g_module_registry.head; mod; mod = mod->next) {
        // Skip evicted modules
        if (mod->cache_state == MODULE_EVICTED) continue;

        if (strcmp(mod->path, path) == 0) {
            return mod;
        }
    }
    return NULL;
}

// Find module and increment ref count atomically (thread-safe)
// This is the CORRECT way to get a module reference safely
// Returns module with ref_count already incremented, or NULL if not found
static module_t* find_and_incref_module(const char* path) {
    pthread_rwlock_rdlock(&g_registry_lock);

    module_t* mod = find_module_unlocked(path);
    if (mod) {
        // Increment ref_count while holding lock (safe from eviction)
        module_incref(mod);
    }

    pthread_rwlock_unlock(&g_registry_lock);
    return mod;
}

// Increment module reference count (lock-free using atomic operations)
// Transitions IDLE → ACTIVE when ref_count 0→1
// NOTE: Caller must NOT hold g_registry_lock (to avoid deadlock)
static void module_incref(module_t* mod) {
    if (!mod) return;

    // Atomically increment ref_count (no lock needed)
    int old_refs = atomic_fetch_add(&mod->ref_count, 1);
    atomic_store(&mod->last_access, time(NULL));

    // Update cache state if transitioning IDLE→ACTIVE
    // Use atomic CAS to avoid race (no lock needed for correctness)
    // Stats update is best-effort (may be slightly inaccurate but acceptable)
    if (old_refs == 0 && mod->cache_state == MODULE_IDLE) {
        // Atomically transition IDLE→ACTIVE (lock-free)
        // Multiple threads may race here, but only one will succeed
        module_state_t expected = MODULE_IDLE;
        if (__atomic_compare_exchange_n(&mod->cache_state, &expected, MODULE_ACTIVE,
                                        0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            // Successfully transitioned - update stats (best-effort, no lock)
            __atomic_fetch_sub(&g_module_registry.idle_count, 1, __ATOMIC_RELAXED);
            __atomic_fetch_add(&g_module_registry.active_count, 1, __ATOMIC_RELAXED);
            __atomic_fetch_add(&g_module_registry.cache_hits, 1, __ATOMIC_RELAXED);
            tracef("module_incref: '%s' IDLE→ACTIVE (cache hit, lock-free)",
                   mod->path);
        }
    }

    tracef("module_incref: '%s' (refs=%d, state=%d)", mod->path,
           atomic_load(&mod->ref_count), mod->cache_state);
}

// Decrement module reference count (lock-free using atomic operations)
// Transitions ACTIVE → IDLE when ref_count 1→0 (keeps module cached)
// Returns 1 if module was evicted/freed, 0 otherwise
// NOTE: Caller must NOT hold g_registry_lock (to avoid deadlock)
static int module_decref(module_t* mod) {
    if (!mod) return 0;

    // Atomically decrement ref_count (no lock needed)
    int old_refs = atomic_fetch_sub(&mod->ref_count, 1);
    atomic_store(&mod->last_access, time(NULL));

    tracef("module_decref: '%s' (refs=%d→%d)", mod->path, old_refs, old_refs - 1);

    // Update cache state if transitioning ACTIVE→IDLE
    // Use atomic CAS to avoid race (no lock needed for correctness)
    // Stats update is best-effort (may be slightly inaccurate but acceptable)
    if (old_refs == 1 && mod->cache_state == MODULE_ACTIVE) {
        // Double-check ref_count is still 0 (another thread may have incremented)
        if (atomic_load(&mod->ref_count) == 0) {
            // Atomically transition ACTIVE→IDLE (lock-free)
            module_state_t expected = MODULE_ACTIVE;
            if (__atomic_compare_exchange_n(&mod->cache_state, &expected, MODULE_IDLE,
                                            0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
                // Successfully transitioned - update stats (best-effort, no lock)
                __atomic_fetch_sub(&g_module_registry.active_count, 1, __ATOMIC_RELAXED);
                __atomic_fetch_add(&g_module_registry.idle_count, 1, __ATOMIC_RELAXED);
                int idle = __atomic_load_n(&g_module_registry.idle_count, __ATOMIC_RELAXED);
                tracef("module_decref: '%s' ACTIVE→IDLE (cached, idle=%d/%d, lock-free)",
                       mod->path, idle, MODULE_CACHE_MAX_IDLE);
            }
        }
    }

    // Note: module stays in cache, not freed immediately
    return 0;
}

// Evict least recently used IDLE module (caller must hold write lock)
// Returns 1 if module was evicted, 0 otherwise
static int evict_lru_idle_module(void) {
    module_t* lru_mod = NULL;
    time_t oldest_access = 0;

    // Find LRU IDLE module (using atomic load for last_access)
    for (module_t* m = g_module_registry.head; m; m = m->next) {
        if (m->cache_state == MODULE_IDLE) {
            time_t access_time = (time_t)atomic_load(&m->last_access);
            if (!lru_mod || access_time < oldest_access) {
                lru_mod = m;
                oldest_access = access_time;
            }
        }
    }

    if (!lru_mod) return 0;  // No IDLE modules to evict

    // Remove from linked list
    if (g_module_registry.head == lru_mod) {
        g_module_registry.head = lru_mod->next;
    } else {
        module_t* prev = g_module_registry.head;
        while (prev && prev->next != lru_mod) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = lru_mod->next;
        }
    }

    g_module_registry.count--;
    __atomic_fetch_sub(&g_module_registry.idle_count, 1, __ATOMIC_RELAXED);
    long evict_num = __atomic_fetch_add(&g_module_registry.evictions, 1, __ATOMIC_RELAXED) + 1;

    tracef("evicted LRU module '%s' (eviction #%ld, remaining=%d)",
           lru_mod->path, evict_num, g_module_registry.count);

    // Free TCC state (outside registry lock would be better, but acceptable here)
    if (lru_mod->state) {
        tcc_delete(lru_mod->state);
    }
    free(lru_mod);

    return 1;
}

// Register new module (thread-safe with write lock)
// Evicts LRU IDLE module if cache is full
static module_t* register_module(const char* path, TCCState* state) {
    module_t* mod = (module_t*)malloc(sizeof(module_t));
    if (!mod) return NULL;

    strncpy(mod->path, path, PATH_MAX - 1);
    mod->path[PATH_MAX - 1] = '\0';
    mod->state = state;
    mod->ref_count = 1;
    mod->cache_state = MODULE_ACTIVE;
    mod->load_time = time(NULL);
    mod->last_access = time(NULL);

    pthread_rwlock_wrlock(&g_registry_lock);

    // Evict LRU IDLE module if cache exceeds limit (use atomic load)
    while (__atomic_load_n(&g_module_registry.idle_count, __ATOMIC_RELAXED) >= MODULE_CACHE_MAX_IDLE) {
        if (!evict_lru_idle_module()) break;
    }

    mod->next = g_module_registry.head;
    g_module_registry.head = mod;
    g_module_registry.count++;
    __atomic_fetch_add(&g_module_registry.active_count, 1, __ATOMIC_RELAXED);
    long miss_num = __atomic_fetch_add(&g_module_registry.cache_misses, 1, __ATOMIC_RELAXED) + 1;

    int active = __atomic_load_n(&g_module_registry.active_count, __ATOMIC_RELAXED);
    int idle = __atomic_load_n(&g_module_registry.idle_count, __ATOMIC_RELAXED);
    tracef("registered module '%s' (total=%d, active=%d, idle=%d, miss #%ld)",
           path, g_module_registry.count, active, idle, miss_num);

    pthread_rwlock_unlock(&g_registry_lock);
    return mod;
}

// Print module cache statistics (debug mode)
// Call this at program exit or on-demand for diagnostics
static void print_module_cache_stats(void) {
    if (!getenv("COSMORUN_DEBUG_CACHE")) return;

    pthread_rwlock_rdlock(&g_registry_lock);

    // Use atomic loads for thread-safe statistics reading
    int active = __atomic_load_n(&g_module_registry.active_count, __ATOMIC_RELAXED);
    int idle = __atomic_load_n(&g_module_registry.idle_count, __ATOMIC_RELAXED);
    long hits = __atomic_load_n(&g_module_registry.cache_hits, __ATOMIC_RELAXED);
    long misses = __atomic_load_n(&g_module_registry.cache_misses, __ATOMIC_RELAXED);
    long evictions = __atomic_load_n(&g_module_registry.evictions, __ATOMIC_RELAXED);

    fprintf(stderr, "\n=== Module Cache Statistics ===\n");
    fprintf(stderr, "Total modules:  %d (active=%d, idle=%d)\n",
            g_module_registry.count, active, idle);
    fprintf(stderr, "Cache hits:     %ld\n", hits);
    fprintf(stderr, "Cache misses:   %ld\n", misses);
    fprintf(stderr, "Evictions:      %ld\n", evictions);

    long total_access = hits + misses;
    if (total_access > 0) {
        double hit_rate = (double)hits * 100.0 / total_access;
        fprintf(stderr, "Hit rate:       %.1f%%\n", hit_rate);
    }

    fprintf(stderr, "\nCached modules:\n");
    for (module_t* m = g_module_registry.head; m; m = m->next) {
        const char* state_str = (m->cache_state == MODULE_ACTIVE) ? "ACTIVE" :
                                (m->cache_state == MODULE_IDLE) ? "IDLE" : "EVICTED";
        int refs = atomic_load(&m->ref_count);
        fprintf(stderr, "  %s [%s, refs=%d]\n", m->path, state_str, refs);
    }
    fprintf(stderr, "================================\n\n");

    pthread_rwlock_unlock(&g_registry_lock);
}

// Push module path to loading stack
// Returns 0 on success, -1 if circular dependency detected
static int push_loading_module(const char* path) {
    // Check for circular dependency
    for (int i = 0; i < g_loading_stack.count; i++) {
        if (strcmp(g_loading_stack.paths[i], path) == 0) {
            // Circular dependency detected - print the chain
            fprintf(stderr, "\n[cosmorun] ERROR: Circular dependency detected:\n");
            for (int j = i; j < g_loading_stack.count; j++) {
                fprintf(stderr, "  %s ->\n", g_loading_stack.paths[j]);
            }
            fprintf(stderr, "  %s (循环)\n\n", path);
            return -1;
        }
    }

    // Check stack depth limit
    if (g_loading_stack.count >= MAX_LOADING_DEPTH) {
        fprintf(stderr, "[cosmorun] ERROR: Module dependency depth exceeds limit (%d)\n",
                MAX_LOADING_DEPTH);
        return -1;
    }

    // Push to stack
    strncpy(g_loading_stack.paths[g_loading_stack.count], path, PATH_MAX - 1);
    g_loading_stack.paths[g_loading_stack.count][PATH_MAX - 1] = '\0';
    g_loading_stack.count++;

    tracef("loading stack push: '%s' (depth=%d)", path, g_loading_stack.count);
    return 0;
}

// Pop module from loading stack
static void pop_loading_module(void) {
    if (g_loading_stack.count > 0) {
        g_loading_stack.count--;
        tracef("loading stack pop (depth=%d)", g_loading_stack.count);
    }
}

// Recursively check if any .h files in a directory are newer than cache
// Returns 1 if any header is newer, 0 otherwise
static int check_headers_in_dir(const char* dir_path, time_t cache_mtime, int recursive) {
    DIR *dir = opendir(dir_path);
    if (!dir) return 0;

    struct dirent *entry;
    int headers_modified = 0;

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode) && recursive) {
            // Recursively check subdirectory
            if (check_headers_in_dir(full_path, cache_mtime, recursive)) {
                headers_modified = 1;
                break;
            }
        } else if (S_ISREG(st.st_mode)) {
            // Check if it's a .h file
            size_t len = strlen(entry->d_name);
            if (len > 2 && strcmp(entry->d_name + len - 2, ".h") == 0) {
                if (st.st_mtime > cache_mtime) {
                    tracef("header '%s' newer than cache, invalidating", full_path);
                    headers_modified = 1;
                    break;
                }
            }
        }
    }

    closedir(dir);
    return headers_modified;
}

// Extract module name from file path (removes directory and extension)
// Example: "c_modules/foo/index.c" -> "foo", "bar.c" -> "bar"
static void extract_module_name(const char* path, char* out, size_t out_size) {
    if (!path || !out || out_size == 0) {
        return;
    }

    const char* last_slash = strrchr(path, '/');
    const char* basename = last_slash ? (last_slash + 1) : path;
    const char* dot = strrchr(basename, '.');
    size_t name_len = dot ? (size_t)(dot - basename) : strlen(basename);

    if (name_len > 0 && name_len < out_size) {
        strncpy(out, basename, name_len);
        out[name_len] = '\0';
    }
}

// Hardcoded dependency map for non-package modules (Phase 1 quick fix)
// Returns number of dependencies filled into deps array
static int get_hardcoded_dependencies(const char* module_name, char deps[][256], int max_deps) {
    // Dependency mapping table
    struct {
        const char* module;
        const char* deps[8];
    } dep_map[] = {
        {"net", {"mod_std", NULL}},
        {"http", {"mod_std", "net", NULL}},
        {"os", {"mod_std", NULL}},
        {NULL, {NULL}}
    };

    // Search for module in map
    for (int i = 0; dep_map[i].module; i++) {
        if (strcmp(module_name, dep_map[i].module) == 0) {
            // Copy dependencies to output array
            int count = 0;
            for (int j = 0; dep_map[i].deps[j] && count < max_deps; j++) {
                strncpy(deps[count], dep_map[i].deps[j], 255);
                deps[count][255] = '\0';
                count++;
            }
            return count;
        }
    }

    return 0;  // No hardcoded dependencies found
}

// Forward declaration of internal import function
static void* __import_internal(const char* path, int already_locked);

// Load dependencies for a module package
// Returns 0 on success, -1 on failure
// NOTE: This function is called with g_compilation_lock already held
static int load_module_dependencies(const char* module_name) {
    char module_dir[PATH_MAX];
    snprintf(module_dir, sizeof(module_dir), "c_modules/%s", module_name);

    char deps[32][256];
    int dep_count = parse_module_dependencies(module_dir, deps, 32);

    // Fallback to hardcoded dependencies if no module.json found
    if (dep_count == 0) {
        dep_count = get_hardcoded_dependencies(module_name, deps, 32);
        if (dep_count > 0) {
            tracef("using hardcoded dependencies for module '%s'", module_name);
        }
    }

    if (dep_count == 0) {
        tracef("module '%s' has no dependencies", module_name);
        return 0;
    }

    tracef("module '%s' has %d dependencies", module_name, dep_count);

    // Load each dependency (lock already held, use internal version)
    for (int i = 0; i < dep_count; i++) {
        tracef("auto-loading dependency: '%s'", deps[i]);
        void* dep_module = __import_internal(deps[i], 1);  // already_locked=1
        if (!dep_module) {
            tracef("failed to load dependency '%s' for module '%s'", deps[i], module_name);
            return -1;
        }
        tracef("successfully loaded dependency '%s'", deps[i]);
    }

    return 0;
}

// Internal import function that can skip lock acquisition when already locked
// already_locked: 0 = acquire lock, 1 = lock already held by caller
static void* __import_internal(const char* path, int already_locked) {
    if (!path || !*path) {
        tracef("__import: null or empty path");
        return NULL;
    }
    tracef("__import: path=%s, already_locked=%d", path, already_locked);

    // Module name resolution (Node.js-style)
    // If path doesn't contain '/' and doesn't end with '.c', treat as module name
    char resolved_path[PATH_MAX];
    const char* actual_path = path;
    char module_name[256] = {0};  // Store module name for dependency loading

    if (!strchr(path, '/') && !ends_with(path, ".c") && !ends_with(path, ".o")) {
        // Store original module name for potential dependency loading
        strncpy(module_name, path, sizeof(module_name) - 1);

        // Try c_modules/{name}.c
        snprintf(resolved_path, sizeof(resolved_path), "c_modules/%s.c", path);
        if (access(resolved_path, F_OK) == 0) {
            tracef("__import: resolved module '%s' -> '%s'", path, resolved_path);
            actual_path = resolved_path;
            // Dependencies will be loaded after acquiring the lock
        } else {
            // Try c_modules/{name}/index.c (package)
            snprintf(resolved_path, sizeof(resolved_path), "c_modules/%s/index.c", path);
            if (access(resolved_path, F_OK) == 0) {
                tracef("__import: resolved module '%s' -> '%s' (package)", path, resolved_path);
                actual_path = resolved_path;
                // Dependencies will be loaded after acquiring the lock
            } else {
                // Try c_modules/mod_{name}.c (backward compatibility)
                snprintf(resolved_path, sizeof(resolved_path), "c_modules/mod_%s.c", path);
                if (access(resolved_path, F_OK) == 0) {
                    tracef("__import: resolved module '%s' -> '%s' (mod_ prefix)", path, resolved_path);
                    actual_path = resolved_path;
                    // Dependencies will be loaded after acquiring the lock
                } else {
                    // Fall back to original path
                    tracef("__import: module '%s' not found in c_modules/, using as-is", path);
                }
            }
        }
    }

    // Update path to resolved path
    path = actual_path;

    // === FAST PATH: Check cache first (read lock inside find_and_incref_module) ===
    module_t* cached = find_and_incref_module(path);
    if (cached) {
        tracef("using cached module '%s'", path);
        return cached->state;
    }

    // === SLOW PATH: Need to compile - acquire compilation lock ===
    // Lock compilation phase to prevent TOCTOU race
    // (TCC is not thread-safe, only one thread can compile at a time)
    // If already_locked=1, skip lock acquisition (caller already holds it)
    if (!already_locked) {
        pthread_once(&g_compilation_lock_once, init_compilation_lock);
        pthread_mutex_lock(&g_compilation_lock);

        // Double-check: Another thread may have compiled while we waited
        cached = find_and_incref_module(path);
        if (cached) {
            if (!already_locked) pthread_mutex_unlock(&g_compilation_lock);
            return cached->state;
        }
    }

    // *** Load module dependencies inside the lock (P2.4 fix) ***
    // This must be done AFTER acquiring the lock to prevent race conditions
    // Check if this is a C module that needs dependency loading
    if (strstr(path, "c_modules/") != NULL) {
        // Extract module name for dependency loading
        char temp_module_name[256];
        const char* base = strrchr(path, '/');
        if (base) base++; else base = path;

        // Remove extension and prefixes
        strncpy(temp_module_name, base, sizeof(temp_module_name) - 1);
        char* dot = strrchr(temp_module_name, '.');
        if (dot) *dot = '\0';

        // Remove "mod_" prefix if present
        char* final_name = temp_module_name;
        if (strncmp(final_name, "mod_", 4) == 0) {
            final_name += 4;
        }

        tracef("loading dependencies for module '%s'", final_name);
        if (load_module_dependencies(final_name) < 0) {
            if (!already_locked) pthread_mutex_unlock(&g_compilation_lock);
            tracef("failed to load dependencies for module '%s'", final_name);
            return NULL;
        }
    }

    // *** Circular dependency detection: push to loading stack ***
    if (push_loading_module(path) < 0) {
        if (!already_locked) pthread_mutex_unlock(&g_compilation_lock);
        return NULL;  // Circular dependency detected
    }

    // Check if it's already a .o file
    // Normalize .o path to .c path for cache lookup
    char normalized_path[PATH_MAX];
    const char* cache_key = path;

    if (ends_with(path, ".o")) {
        // Convert "path/file.x86_64.o" -> "path/file.c" for cache key
        const char* last_dot = strrchr(path, '.');
        if (last_dot) {
            const char* second_last_dot = last_dot - 1;
            while (second_last_dot > path && *second_last_dot != '.') {
                second_last_dot--;
            }
            if (*second_last_dot == '.' && second_last_dot > path) {
                // Found pattern ".arch.o", extract base path
                size_t base_len = second_last_dot - path;
                snprintf(normalized_path, sizeof(normalized_path), "%.*s.c", (int)base_len, path);
                cache_key = normalized_path;
            }
        }

        // *** Check cache first (with normalized .c path as key) ***
        module_t* cached = find_and_incref_module(cache_key);
        if (cached) {
            tracef("using cached module for .o file '%s' (key='%s')", path, cache_key);
            pop_loading_module();
            if (!already_locked) pthread_mutex_unlock(&g_compilation_lock);
            return cached->state;
        }

        // Cache miss - load .o file and register
        tracef("cache miss for .o file '%s', loading fresh", path);
        void* result = load_o_file(path);
        if (result) {
            // Register loaded .o file in cache (using normalized .c path as key)
            module_t* mod = register_module(cache_key, (TCCState*)result);
            if (!mod) {
                tracef("WARNING: failed to register .o module '%s' in cache", cache_key);
            } else {
                tracef("registered .o module in cache with key '%s'", cache_key);
            }
        }
        pop_loading_module();
        if (!already_locked) pthread_mutex_unlock(&g_compilation_lock);
        return result;
    }

    // Get architecture for cache naming
    struct utsname uts;
    memset(&uts, 0, sizeof(uts));
    uname(&uts);

    // Generate arch-specific .o cache path
    char cache_path[PATH_MAX];
    int is_c_file = ends_with(path, ".c");
    if (is_c_file) {
        size_t len = strlen(path);
        snprintf(cache_path, sizeof(cache_path), "%.*s.%s.o", (int)(len - 2), path, uts.machine);
    } else {
        cache_path[0] = '\0';
    }

    // Check source and cache existence
    struct stat src_st, cache_st;
    int src_exists = (stat(path, &src_st) == 0);
    int cache_exists = is_c_file && (stat(cache_path, &cache_st) == 0);

    // Decision tree
    if (src_exists) {
        if (cache_exists) {
            // Compare modification times
            if (src_st.st_mtime == cache_st.st_mtime) {
                // Enhanced check: recursively check .h files in multiple directories
                // This catches header file changes without full dependency tracking
                int headers_modified = 0;

                // Check directories in order of likelihood
                const char* check_dirs[] = {
                    ".",                  // Current directory
                    "c_modules",          // Module headers
                    "include",            // Project includes
                    NULL
                };

                for (int i = 0; check_dirs[i] && !headers_modified; i++) {
                    // Use recursive check for c_modules, non-recursive for others
                    int recursive = (strcmp(check_dirs[i], "c_modules") == 0);
                    headers_modified = check_headers_in_dir(check_dirs[i],
                                                            cache_st.st_mtime,
                                                            recursive);
                }

                if (!headers_modified) {
                    tracef("using cached '%s' (mtime match)", cache_path);
                    void* result = load_o_file(cache_path);
                    pop_loading_module();
                    if (!already_locked) pthread_mutex_unlock(&g_compilation_lock);
                    return result;
                } else {
                    tracef("cache outdated due to header changes, recompiling '%s'", path);
                    // Fall through to compile from source
                }
            } else {
                tracef("cache outdated, recompiling '%s'", path);
                // Fall through to compile from source
            }
        } else {
            tracef("no cache found, compiling '%s'", path);
            // Fall through to compile from source
        }
    } else {
        // Source doesn't exist
        if (cache_exists) {
            tracef("source not found, using cached '%s'", cache_path);
            void* result = load_o_file(cache_path);
            pop_loading_module();
            if (!already_locked) pthread_mutex_unlock(&g_compilation_lock);
            return result;
        } else {
            tracef("neither source '%s' nor cache '%s' found", path, cache_path);
            pop_loading_module();
            if (!already_locked) pthread_mutex_unlock(&g_compilation_lock);
            return NULL;
        }
    }

    // Compile from source
    tracef("compiling '%s'", path);

    TCCState* s = tcc_new();
    if (!s) {
        tracef("__import: tcc_new failed");
        pop_loading_module();
        if (!already_locked) pthread_mutex_unlock(&g_compilation_lock);
        return NULL;
    }

    tracef("__import: tcc_state=%p", s);
    tcc_set_error_func(s, NULL, tcc_error_func);
    tracef("__import: set error func");

    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    tracef("__import: set output type");

    char tcc_options[COSMORUN_MAX_OPTIONS_SIZE] = {0};
    cosmo_tcc_build_default_options(tcc_options, sizeof(tcc_options), &uts);
    tracef("__import: built tcc options");

    if (tcc_options[0]) {
        tcc_set_options(s, tcc_options);
        tracef("__import: set tcc options");
    }

    cosmo_tcc_register_include_paths(s, &uts);
    tracef("__import: registered include paths");

    cosmo_tcc_register_library_paths(s);
    tracef("__import: registered library paths");

    cosmo_tcc_register_builtin_symbols(s);
    tracef("__import: registered builtin symbols");

    cosmo_tcc_link_runtime(s);
    tracef("__import: linked tcc runtime");

    // Export symbols from already loaded modules to this new module
    // This allows modules to use extern declarations to access symbols from other modules

    // Define known exported symbols from various modules
    const char* exported_symbols[] = {
        // mod_std symbols
        "std_string_new", "std_string_free", "std_string_clear",
        "std_string_append", "std_string_append_char", "std_string_cstr",
        "std_string_len", "std_string_format", "std_string_with_capacity",
        "std_vector_new", "std_vector_free", "std_vector_push",
        "std_vector_pop", "std_vector_get", "std_vector_set",
        "std_vector_len", "std_vector_clear", "std_vector_with_capacity",
        "std_hashmap_new", "std_hashmap_free", "std_hashmap_set",
        "std_hashmap_get", "std_hashmap_remove", "std_hashmap_has",
        "std_hashmap_size", "std_hashmap_foreach", "std_hashmap_clear",
        "std_error_new", "std_error_free", "std_error_message",
        "std_error_code", "std_error_wrap",
        // mod_net symbols
        "net_resolve", "net_ip_to_string", "net_string_to_ip",
        "net_tcp_connect", "net_tcp_connect_timeout",
        "net_tcp_listen", "net_tcp_accept", "net_tcp_accept_timeout",
        "net_udp_socket", "net_udp_send", "net_udp_recv",
        "net_send", "net_recv", "net_send_all", "net_recv_all",
        "net_set_timeout", "net_set_nonblocking", "net_set_nodelay",
        "net_set_reuseaddr", "net_set_sendbuf", "net_set_recvbuf",
        "net_socket_close", "net_socket_error",
        "net_socket_local_addr", "net_socket_remote_addr",
        "net_htons", "net_ntohs", "net_htonl", "net_ntohl",
        NULL
    };

    // Collect symbol addresses while holding lock (minimize lock time)
    typedef struct { const char* name; void* addr; } symbol_export_t;
    symbol_export_t exports[256];  // Max 256 symbols
    int export_count = 0;

    pthread_rwlock_rdlock(&g_registry_lock);
    for (module_t* mod = g_module_registry.head; mod && export_count < 256; mod = mod->next) {
        if (mod->state && mod->state != s) {
            // Try to export symbols from loaded modules
            for (int j = 0; exported_symbols[j] && export_count < 256; j++) {
                void* addr = tcc_get_symbol(mod->state, exported_symbols[j]);
                if (addr) {
                    exports[export_count].name = exported_symbols[j];
                    exports[export_count].addr = addr;
                    export_count++;
                    tracef("found symbol '%s' -> %p in loaded module", exported_symbols[j], addr);
                }
            }
        }
    }
    pthread_rwlock_unlock(&g_registry_lock);

    // Now add all collected symbols to new state (lock-free)
    for (int i = 0; i < export_count; i++) {
        tcc_add_symbol(s, exports[i].name, exports[i].addr);
        tracef("exported symbol '%s' to new state", exports[i].name);
    }
    tracef("__import: exported %d symbols from loaded modules", export_count);

    // Auto-link architecture-specific object files and assembly files if they exist
    // This allows modules to provide pre-compiled assembly implementations
    // Example: c_modules/mod_co.c -> c_modules/mod_co_arm64.o or c_modules/*.S
    if (is_c_file) {
        // Extract directory and base name from path
        char dir_path[1024] = {0};
        char base_name[256] = {0};

        const char* last_slash = strrchr(path, '/');
        if (last_slash) {
            size_t dir_len = last_slash - path;
            if (dir_len < sizeof(dir_path)) {
                memcpy(dir_path, path, dir_len);
                dir_path[dir_len] = '\0';
            }

            // Extract module name (e.g., "mod_co" from "mod_co.c")
            const char* name_start = last_slash + 1;
            const char* dot = strrchr(name_start, '.');
            if (dot) {
                size_t name_len = dot - name_start;
                if (name_len < sizeof(base_name)) {
                    memcpy(base_name, name_start, name_len);
                    base_name[name_len] = '\0';
                }
            }
        }

        // Try to link architecture-specific object file or assembly files
        if (dir_path[0] && base_name[0]) {
            #if defined(__aarch64__) || defined(__arm64__)
            const char* arch_suffix = "arm64";
            #elif defined(__x86_64__) || defined(__amd64__)
            const char* arch_suffix = "x64";
            #else
            const char* arch_suffix = "generic";
            #endif

            // Construct object file path: dir/basename_arch.o
            char obj_path[1024];
            int printed = snprintf(obj_path, sizeof(obj_path), "%s/%s_%s.o",
                                  dir_path, base_name, arch_suffix);

            if (printed > 0 && printed < (int)sizeof(obj_path)) {
                // Check if object file exists
                struct stat obj_st;
                if (stat(obj_path, &obj_st) == 0) {
                    tracef("found arch-specific object file: %s", obj_path);

                    // Link the object file
                    if (tcc_add_file(s, obj_path) == -1) {
                        tracef("warning: failed to link '%s', continuing...", obj_path);
                    } else {
                        tracef("successfully linked '%s'", obj_path);
                    }
                }
            }

            // Check module.json for additional source files (especially .S files)
            // This allows modules to explicitly declare their assembly dependencies
            // Example: c_modules/mod_co/module.json declares co_ctx_*.S files
            if (strstr(dir_path, "c_modules") != NULL) {
                char module_json_path[1024];
                snprintf(module_json_path, sizeof(module_json_path), "%s/module.json", dir_path);

                FILE* json_file = fopen(module_json_path, "r");
                if (json_file) {
                    tracef("found module.json: %s", module_json_path);

                    // Simple parser for "sources": [...] array
                    char line[1024];
                    int in_sources = 0;
                    while (fgets(line, sizeof(line), json_file)) {
                        // Look for "sources" key
                        if (strstr(line, "\"sources\"")) {
                            in_sources = 1;
                            continue;
                        }

                        if (in_sources) {
                            // End of array
                            if (strchr(line, ']')) {
                                in_sources = 0;
                                break;
                            }

                            // Extract filename from "filename.S"
                            char* quote1 = strchr(line, '"');
                            if (quote1) {
                                char* quote2 = strchr(quote1 + 1, '"');
                                if (quote2) {
                                    size_t len = quote2 - quote1 - 1;
                                    if (len > 0 && len < 256) {
                                        char source_file[256];
                                        memcpy(source_file, quote1 + 1, len);
                                        source_file[len] = '\0';

                                        // Only process .S files (skip the main .c file)
                                        if (strstr(source_file, ".S")) {
                                            char source_path[1024];
                                            snprintf(source_path, sizeof(source_path), "%s/%s", dir_path, source_file);

                                            tracef("linking declared source: %s", source_path);
                                            if (tcc_add_file(s, source_path) == -1) {
                                                tracef("warning: failed to link '%s', continuing...", source_path);
                                            } else {
                                                tracef("successfully linked '%s'", source_path);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    fclose(json_file);
                }
            }
        }
    }

    // Compile source file
    if (tcc_add_file(s, path) == -1) {
        tracef("tcc_add_file failed for '%s'", path);
        tcc_delete(s);
        pop_loading_module();
        if (!already_locked) pthread_mutex_unlock(&g_compilation_lock);
        return NULL;
    }

    // Save .o cache before relocating
    if (is_c_file) {
        save_o_cache(path, s);

        // Sync cache file mtime with source
        struct timespec times[2];
        times[0] = src_st.st_atim;  // access time
        times[1] = src_st.st_mtim;  // modification time
        utimensat(AT_FDCWD, cache_path, times, 0);
        tracef("synced cache mtime with source");
    }

    // Relocate
    if (tcc_relocate(s) < 0) {
        tracef("tcc_relocate failed for '%s'", path);
        tcc_delete(s);
        pop_loading_module();
        if (!already_locked) pthread_mutex_unlock(&g_compilation_lock);
        return NULL;
    }

    tracef("successfully loaded '%s' -> %p", path, s);

    // *** Auto-call module initialization function ***
    // Try multiple naming conventions: mod_{name}_init, {name}_init, __init__
    const char* init_candidates[] = {
        NULL,  // Will be filled with module-specific name
        NULL,  // Will be filled with module-specific name
        "__init__",      // Python-style
        "__module_init__",
        NULL
    };

    // Extract module name from path for smart init function naming
    // Reuse module_name from earlier if available, otherwise extract
    char init_module_name[256] = {0};
    if (module_name[0] != '\0') {
        // Reuse module name extracted earlier (performance optimization)
        strncpy(init_module_name, module_name, sizeof(init_module_name) - 1);
    } else {
        // Extract from path using helper function
        extract_module_name(path, init_module_name, sizeof(init_module_name));
    }

    // Build init function names
    // Use stack allocation for thread safety
    char init_name1[300];
    char init_name2[300];
    if (init_module_name[0] != '\0') {
        snprintf(init_name1, sizeof(init_name1), "%s_init", init_module_name);
        snprintf(init_name2, sizeof(init_name2), "mod_%s_init", init_module_name);
        init_candidates[0] = init_name2;  // Try mod_{name}_init first
        init_candidates[1] = init_name1;  // Then {name}_init
    }

    // Try to find and call initialization function
    // Init functions should return int: 0=success, non-zero=failure
    for (int i = 0; init_candidates[i]; i++) {
        int (*init_fn)(void) = (int (*)(void))tcc_get_symbol(s, init_candidates[i]);
        if (init_fn) {
            tracef("auto-calling init function: %s()", init_candidates[i]);
            int init_result = init_fn();  // Call init
            if (init_result != 0) {
                // Init failed - cleanup and return NULL
                tracef("init function %s() failed with code %d", init_candidates[i], init_result);
                tcc_delete(s);
                pop_loading_module();
                if (!already_locked) pthread_mutex_unlock(&g_compilation_lock);
                return NULL;
            }
            tracef("init function %s() completed successfully", init_candidates[i]);
            break;
        }
    }

    // *** Register module in cache ***
    module_t* mod = register_module(path, s);
    if (!mod) {
        tracef("WARNING: failed to register module '%s' in cache", path);
        // Still return the module, just won't be cached
    }

    pop_loading_module();  // Success - pop before returning
    if (!already_locked) pthread_mutex_unlock(&g_compilation_lock);  // Release compilation lock
    return (void*)s;
}

// Public wrapper for __import - always acquires lock
void* __import(const char* path) {
    return __import_internal(path, 0);  // already_locked=0
}

void* __import_sym(void* module, const char* symbol) {
    if (!module || !symbol) {
        tracef("__import_sym: null module or symbol");
        return NULL;
    }

    TCCState* s = (TCCState*)module;
    void* addr = tcc_get_symbol(s, symbol);

    if (addr) {
        tracef("__import_sym: found '%s' -> %p", symbol, addr);
    } else {
        tracef("__import_sym: symbol '%s' not found", symbol);
    }

    return addr;
}

void __import_free(void* module) {
    if (!module) return;

    TCCState* state = (TCCState*)module;
    tracef("__import_free: module %p", state);

    // Find module in registry (thread-safe read)
    // Hold rdlock ONLY during search, then immediately call decref
    // (decref uses atomic ops, so no lock nesting issue)
    pthread_rwlock_rdlock(&g_registry_lock);
    module_t* mod = NULL;
    for (module_t* m = g_module_registry.head; m; m = m->next) {
        if (m->state == state) {
            mod = m;
            break;
        }
    }
    pthread_rwlock_unlock(&g_registry_lock);

    if (mod) {
        // Module is in registry - decrement ref count (lock-free atomic)
        // Safe to call after releasing rdlock because:
        // 1. module_decref uses atomic operations for ref_count
        // 2. Module won't be freed until ref_count reaches 0 and LRU eviction
        // 3. Even if evicted, decref is safe (just decrements atomic counter)
        module_decref(mod);
    } else {
        // Module not in registry - direct free (shouldn't happen normally)
        tracef("WARNING: freeing unregistered module %p", state);
        tcc_delete(state);
    }
}

// ============================================================================
// ARM64 Windows JIT Support
// ============================================================================

#if defined(_WIN32) && (defined(__aarch64__) || defined(_M_ARM64))

/* ARM64 Windows calling convention helpers */
static int make_executable_arm64_win(void *ptr, size_t size) {
    unsigned old_protect;

    /* Change protection to executable (RWX on Windows) */
    if (!VirtualProtect(ptr, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return -1;
    }

    /* Flush instruction cache (critical on ARM64) */
    FlushInstructionCache(GetCurrentProcess(), ptr, size);

    return 0;
}

/* Allocate executable memory for JIT */
static void* alloc_executable_memory_arm64(size_t size) {
    void *ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (ptr) {
        FlushInstructionCache(GetCurrentProcess(), ptr, size);
    }
    return ptr;
}

/* Free executable memory */
static int free_executable_memory_arm64(void *ptr, size_t size) {
    return VirtualFree(ptr, 0, MEM_RELEASE) ? 0 : -1;
}

#endif /* _WIN32 && ARM64 */

// ============================================================================
// Trampoline System Implementation (from cosmo_trampoline.c)
// ============================================================================

// ============================================================================
//#ifdef __x86_64__
#if defined(_WIN32) || defined(__x86_64__)

extern void __sysv2nt14(void);

typedef struct {
    void *orig;
    void *stub;
} WinThunkEntry;

#define COSMORUN_MAX_WIN_THUNKS 256

static WinThunkEntry g_win_thunks[COSMORUN_MAX_WIN_THUNKS];
static size_t g_win_thunk_count = 0;
static void *g_win_host_module = NULL;
static int g_win_initialized = 0;

static bool windows_address_is_executable(const void *addr) {
    struct NtMemoryBasicInformation info;
    if (!addr) return false;
    if (!VirtualQuery(addr, &info, sizeof(info))) {
        return false;
    }
    unsigned prot = info.Protect & 0xffu;
    switch (prot) {
        case kNtPageExecute:
        case kNtPageExecuteRead:
        case kNtPageExecuteReadwrite:
        case kNtPageExecuteWritecopy:
            return true;
        default:
            return false;
    }
}

static void *windows_make_trampoline(void *func) {
    static const unsigned char kTemplate[] = {
        0x55,                               /* push %rbp */
        0x48, 0x89, 0xE5,                   /* mov %rsp,%rbp */
        0x48, 0xB8,                         /* movabs $func,%rax */
        0, 0, 0, 0, 0, 0, 0, 0,             /* placeholder */
        0x49, 0xBA,                         /* movabs $__sysv2nt14,%r10 */
        0, 0, 0, 0, 0, 0, 0, 0,             /* placeholder */
        0x41, 0xFF, 0xE2                    /* jmp *%r10 */
    };

    void *mem = mmap(NULL, sizeof(kTemplate),
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        return NULL;
    }

    memcpy(mem, kTemplate, sizeof(kTemplate));
    memcpy((unsigned char *)mem + 6, &func, sizeof(void *));
    void *bridge = (void *)__sysv2nt14;
    memcpy((unsigned char *)mem + 16, &bridge, sizeof(void *));
    __builtin___clear_cache((char *)mem, (char *)mem + sizeof(kTemplate));
    return mem;
}

void cosmo_trampoline_win_init(void *host_module) {
    g_win_host_module = host_module;
    g_win_thunk_count = 0;
    g_win_initialized = 1;
}

static inline void ensure_win_initialized(void) {
    if (!g_win_initialized) {
        cosmo_trampoline_win_init(NULL);
    }
}

void *cosmo_trampoline_win_wrap(void *module, void *addr) {
    if (!addr) return NULL;
    if (!IsWindows()) return addr;

    ensure_win_initialized();

    if (!module || module == g_win_host_module) return addr;
    if (!windows_address_is_executable(addr)) return addr;

    // Check if we already have a trampoline for this address
    for (size_t i = 0; i < g_win_thunk_count; ++i) {
        if (g_win_thunks[i].orig == addr) {
            void *stub = g_win_thunks[i].stub;
            return stub ? stub : addr;
        }
    }

    // Create new trampoline
    void *stub = windows_make_trampoline(addr);
    if (stub) {
        if (g_win_thunk_count < COSMORUN_MAX_WIN_THUNKS) {
            g_win_thunks[g_win_thunk_count].orig = addr;
            g_win_thunks[g_win_thunk_count].stub = stub;
            ++g_win_thunk_count;
        }
    }
    return stub ? stub : addr;
}

size_t cosmo_trampoline_win_count(void) {
    return g_win_thunk_count;
}

#endif // __x86_64__

// ============================================================================
// ARM64 Variadic Function Trampolines
// ============================================================================
#ifdef __aarch64__

// macOS MAP_JIT flag for JIT code generation
#ifndef MAP_JIT
  #ifdef __APPLE__
    #define MAP_JIT 0x0800
  #else
    #define MAP_JIT 0
  #endif
#endif

#define ARM64_MAX_VARARGS_TRAMPOLINES 64

typedef struct {
    void *orig;          // Original variadic function
    void *stub;          // Our generated trampoline
    const char *name;    // For debugging
} ARM64VarargEntry;

static ARM64VarargEntry g_arm64_vararg_trampolines[ARM64_MAX_VARARGS_TRAMPOLINES];
static size_t g_arm64_vararg_count = 0;

// ============================================================================
// Universal Trampoline Template - Pre-compiled machine code
// ============================================================================
static const uint32_t g_trampoline_template[] = {
    0xa9bf7bfd,  // [0]  stp x29, x30, [sp, #-16]!
    0x910003fd,  // [1]  mov x29, sp
    0xd10103ff,  // [2]  sub sp, sp, #64
    0xf90003e1,  // [3]  str x1, [sp, #0]   - Will patch to nop if not needed
    0xf90007e2,  // [4]  str x2, [sp, #8]   - Will patch to nop if not needed
    0xf9000be3,  // [5]  str x3, [sp, #16]  - Will patch to nop if not needed
    0xf9000fe4,  // [6]  str x4, [sp, #24]
    0xf90013e5,  // [7]  str x5, [sp, #32]
    0xf90017e6,  // [8]  str x6, [sp, #40]
    0xf9001be7,  // [9]  str x7, [sp, #48]
    0x910003e3,  // [10] mov x3, sp        - Will patch register number
    0xd2800009,  // [11] movz x9, #0x0000  - Will patch vfunc bits [15:0]
    0xf2a00009,  // [12] movk x9, #0x0000, lsl #16 - Will patch [31:16]
    0xf2c00009,  // [13] movk x9, #0x0000, lsl #32 - Will patch [47:32]
    0xf2e00009,  // [14] movk x9, #0x0000, lsl #48 - Will patch [63:48]
    0xd63f0120,  // [15] blr x9
    0x910103ff,  // [16] add sp, sp, #64
    0xa8c17bfd,  // [17] ldp x29, x30, [sp], #16
    0xd65f03c0,  // [18] ret
};
#define TEMPLATE_SIZE sizeof(g_trampoline_template)

// Runtime patching of universal template
static void *arm64_make_vararg_trampoline(void *vfunc, int variadic_type) {
    uint64_t vfunc_addr = (uint64_t)vfunc;
    int first_var_reg = 4 - variadic_type;  // 3, 2, or 1

    // Allocate writable+executable memory
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef __APPLE__
    flags |= MAP_JIT;
#endif

    void *mem = mmap(NULL, TEMPLATE_SIZE, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (mem == MAP_FAILED) {
        return NULL;
    }

    uint32_t *code = (uint32_t*)mem;
    memcpy(code, g_trampoline_template, TEMPLATE_SIZE);

    // PATCH 1: Disable unneeded str instructions (x1, x2, x3)
    for (int reg = 1; reg < first_var_reg; reg++) {
        code[3 + (reg - 1)] = 0xd503201f;  // nop
    }

    // PATCH 2: Adjust offsets for remaining str instructions
    int offset = 0;
    for (int reg = first_var_reg; reg <= 7; reg++) {
        int idx = 3 + (reg - 1);
        code[idx] = 0xf90003e0 | reg | ((offset / 8) << 10);
        offset += 8;
    }

    // PATCH 3: Set va_list register
    code[10] = 0x910003e0 | first_var_reg;  // mov xN, sp

    // PATCH 4: Set vfunc address
    code[11] = 0xd2800009 | ((vfunc_addr & 0xffff) << 5);
    code[12] = 0xf2a00009 | (((vfunc_addr >> 16) & 0xffff) << 5);
    code[13] = 0xf2c00009 | (((vfunc_addr >> 32) & 0xffff) << 5);
    code[14] = 0xf2e00009 | (((vfunc_addr >> 48) & 0xffff) << 5);

    if (mprotect(mem, TEMPLATE_SIZE, PROT_READ | PROT_EXEC) != 0) {
        munmap(mem, TEMPLATE_SIZE);
        return NULL;
    }

    __builtin___clear_cache(mem, (char*)mem + TEMPLATE_SIZE);

    return mem;
}

//void *cosmo_trampoline_arm64_vararg(void *vfunc, int variadic_type, const char *name) {
//    if (!vfunc) return NULL;
//
//    // Check if we already have a trampoline for this function
//    for (size_t i = 0; i < g_arm64_vararg_count; ++i) {
//        if (g_arm64_vararg_trampolines[i].orig == vfunc) {
//            return g_arm64_vararg_trampolines[i].stub;
//        }
//    }
//
//    // Create new trampoline
//    void *stub = arm64_make_vararg_trampoline(vfunc, variadic_type);
//    if (stub && g_arm64_vararg_count < ARM64_MAX_VARARGS_TRAMPOLINES) {
//        g_arm64_vararg_trampolines[g_arm64_vararg_count].orig = vfunc;
//        g_arm64_vararg_trampolines[g_arm64_vararg_count].stub = stub;
//        g_arm64_vararg_trampolines[g_arm64_vararg_count].name = name;
//        ++g_arm64_vararg_count;
//    }
//
//    return stub ? stub : vfunc;
//}

size_t cosmo_trampoline_arm64_count(void) {
    return g_arm64_vararg_count;
}

/* Wrapper for external use: wrap variadic function for cross-ABI calls
 * @param vfunc: Original variadic function pointer
 * @param variadic_type: Number of fixed args before varargs (1, 2, or 3)
 * @return: Wrapped trampoline function pointer, or original if wrapping fails
 */
void *__dlsym_varg(void *vfunc, int variadic_type) {
    if (!vfunc) return NULL;

    void *trampoline = arm64_make_vararg_trampoline(vfunc, variadic_type);
    return trampoline ? trampoline : vfunc;
}

#endif // __aarch64__

// ============================================================================
// ARM32 Windows Support (Thumb-2 mode)
// ============================================================================
#if defined(_WIN32) && (defined(_M_ARM) || defined(__arm__))

// Windows API for JIT memory management
#ifndef PAGE_EXECUTE_READWRITE
#define PAGE_EXECUTE_READWRITE 0x40
#endif

// ARM32 PE binary machine types
#ifndef IMAGE_FILE_MACHINE_ARM
#define IMAGE_FILE_MACHINE_ARM 0x01C0     // ARM Little-Endian
#endif
#ifndef IMAGE_FILE_MACHINE_THUMB
#define IMAGE_FILE_MACHINE_THUMB 0x01C2   // ARM Thumb-2 LE
#endif

// ARM32 PE relocation types
#ifndef IMAGE_REL_ARM_ABSOLUTE
#define IMAGE_REL_ARM_ABSOLUTE 0x0000
#endif
#ifndef IMAGE_REL_ARM_ADDR32
#define IMAGE_REL_ARM_ADDR32 0x0001
#endif
#ifndef IMAGE_REL_ARM_BRANCH24
#define IMAGE_REL_ARM_BRANCH24 0x0003
#endif
#ifndef IMAGE_REL_ARM_BLX24
#define IMAGE_REL_ARM_BLX24 0x0008
#endif

// ARM32 Windows calling convention helper
// Arguments in R0-R3, rest on stack (8-byte aligned)
// Return value in R0 (or S0/D0 for floats)
// Thumb mode required (LSB=1 for function pointers)

static int make_executable_arm32_win(void *ptr, size_t size) {
#ifdef _WIN32
    // VirtualProtect to make memory executable
    extern int VirtualProtect(void *addr, size_t size, int prot, int *old);
    int old_protect = 0;

    if (!VirtualProtect(ptr, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return -1;
    }

    // Flush instruction cache (CRITICAL on ARM)
    extern int FlushInstructionCache(void *process, void *addr, size_t size);
    extern void* GetCurrentProcess(void);
    FlushInstructionCache(GetCurrentProcess(), ptr, size);

    return 0;
#else
    (void)ptr; (void)size;
    return -1;
#endif
}

// Encode Thumb-2 modified immediate constant
static uint32_t encode_thumb_modified_imm(uint32_t imm) {
    // Simple encoding for small values
    // Full implementation would handle rotated patterns
    if (imm < 256) {
        return imm;
    }
    // For larger values, use a simple encoding
    // Real implementation needs full Thumb-2 encoding logic
    return imm & 0xFFFF;
}

// Generate Thumb-2 MOV instruction
static void gen_thumb2_mov_imm(void *code_ptr, int *offset, int reg, uint32_t imm) {
    uint16_t *code = (uint16_t*)((char*)code_ptr + *offset);

    if (imm < 256) {
        // MOVS Rreg, #imm (16-bit encoding)
        uint16_t insn = 0x2000 | ((reg & 7) << 8) | (imm & 0xFF);
        *code = insn;
        *offset += 2;
    } else {
        // MOVW Rreg, #imm (32-bit encoding)
        uint32_t insn = 0xF0400000 | ((reg & 0xF) << 8);
        uint32_t imm16 = imm & 0xFFFF;
        insn |= ((imm16 >> 12) & 0xF) << 16;
        insn |= ((imm16 >> 11) & 1) << 26;
        insn |= (imm16 & 0xFF);
        insn |= ((imm16 >> 8) & 7) << 12;

        *(uint32_t*)code = insn;
        *offset += 4;
    }
}

// Generate Thumb-2 BLX instruction (function call)
static void gen_thumb2_blx(void *code_ptr, int *offset, uint32_t target_addr) {
    uint32_t *code = (uint32_t*)((char*)code_ptr + *offset);
    uint32_t current_pc = (uint32_t)code + 4;  // PC is current + 4 in Thumb
    int32_t disp = (int32_t)((target_addr & ~1) - current_pc);

    // BLX encoding (32-bit Thumb-2)
    uint32_t s = (disp >> 24) & 1;
    uint32_t j1 = (disp >> 23) & 1;
    uint32_t j2 = (disp >> 22) & 1;
    uint32_t imm10 = (disp >> 12) & 0x3FF;
    uint32_t imm11 = (disp >> 1) & 0x7FF;

    uint32_t insn = 0xF000D000;  // BLX base encoding
    insn |= (s << 26) | (imm10 << 16) | (j1 << 13) | (j2 << 11) | imm11;

    *code = insn;
    *offset += 4;
}

// Create ARM32 Win32 calling convention wrapper
void* create_arm32_win_wrapper(void *target_func, int arg_count) {
    // Allocate executable memory
    size_t code_size = 128;  // Enough for wrapper code
    void *mem = malloc(code_size);
    if (!mem) return NULL;

    int offset = 0;

    // Simple wrapper: just branch to target
    // Real implementation would handle argument marshalling
    // For now, assume ARM32 calling convention is compatible

    // BLX target_func
    gen_thumb2_blx(mem, &offset, (uint32_t)target_func);

    // BX LR (return)
    uint16_t *code = (uint16_t*)((char*)mem + offset);
    *code = 0x4770;  // BX LR in Thumb mode
    offset += 2;

    // Make memory executable
    if (make_executable_arm32_win(mem, code_size) != 0) {
        free(mem);
        return NULL;
    }

    // Return with Thumb bit set (LSB=1)
    return (void*)((uintptr_t)mem | 1);
}

#endif // ARM32 Windows

// ============================================================================
// Generic Interface
// ============================================================================

void cosmo_trampoline_init(void *host_module) {
#ifdef __x86_64__
    if (!g_win_initialized) {
        cosmo_trampoline_win_init(host_module);
    }
#else
    (void)host_module;
#endif
}

void *cosmo_trampoline_wrap(void *module, void *addr) {
    if (!addr) return NULL;

#ifdef __x86_64__
    //if (IsWindows()) {
      return (void*)cosmo_trampoline_win_wrap(module, addr);
    //}
#endif
//    // Windows CC conversion on x86_64
//    // IsWindows(); should do better
//    return cosmo_trampoline_win_wrap(module, addr);
//#else
//    // No automatic wrapping on other platforms
    (void)module;
    return (void*)addr;
//#endif
}

// ============================================================================
// Libc Function Resolution with Automatic Trampoline
// ============================================================================

// Forward declarations for dlopen/dlsym
// Global library handles
static void* g_libc = NULL;
static void* g_libm = NULL;
static int g_libc_init = 0;

void cosmo_trampoline_libc_init(void) {
    if (g_libc_init) return;

    int dlopen_flag_win = 0;
    int dlopen_flag_unx = 0x101;  // RTLD_LAZY | RTLD_GLOBAL

    if (IsWindows()) {
        // Windows: load msvcrt.dll
        g_libc = xdl_open("msvcrt.dll", dlopen_flag_win);
        g_libm = g_libc;  // Windows: math functions in msvcrt
    }
    else if (IsLinux()) {
        // Linux: load libc.so.6 + libm.so.6
        g_libc = xdl_open("libc.so.6", dlopen_flag_unx);
        if (!g_libc) {
            g_libc = xdl_open("libc.so", dlopen_flag_unx);
        }

        g_libm = xdl_open("libm.so.6", dlopen_flag_unx);
        if (!g_libm) {
            g_libm = xdl_open("libm.so", dlopen_flag_unx);
        }
    }
    else {
        // macOS/other Unix: load libSystem.B.dylib
        g_libc = xdl_open("libSystem.B.dylib", dlopen_flag_unx);
        g_libm = g_libc;  // macOS: unified libSystem
    }

    g_libc_init = 1;
}

//void *cosmo_trampoline_libc_resolve(const char *name, int variadic_type) {
//    // Lazy initialization
//    if (!g_libc_init) {
//        cosmo_trampoline_libc_init();
//    }
//
//    // Resolve from libc/libm
//    void* addr = NULL;
//    if (g_libc) {
//        addr = cosmorun_dlsym(g_libc, name);  // Already applies Windows CC conversion
//    }
//    if (!addr && g_libm) {
//        addr = cosmorun_dlsym(g_libm, name);  // Already applies Windows CC conversion
//    }
//
//    if (!addr) {
//        return NULL;
//    }
//
//#ifdef __aarch64__
//    // ARM64: Create trampoline for variadic functions
//    if (variadic_type) {
//        // For variadic functions, resolve the v* variant (vsnprintf, vsprintf, vprintf)
//        char vname[64];
//        snprintf(vname, sizeof(vname), "v%s", name);
//        void *vfunc = cosmorun_dlsym(g_libc, vname);
//        if (!vfunc) {
//            // Fallback to original function if v* variant not found
//            return addr;
//        }
//
//        // Create trampoline for va_list marshalling
//        void* trampoline = cosmo_trampoline_arm64_vararg(vfunc, variadic_type, name);
//        if (trampoline) {
//            return trampoline;
//        }
//        // Fallback: if trampoline creation failed, use direct call
//    }
//#else
//    (void)variadic_type;  // Unused on non-ARM64 platforms
//#endif
//
//    return addr;
//}

bool cosmo_trampoline_libc_is_initialized(void) {
    return g_libc_init != 0;
}

