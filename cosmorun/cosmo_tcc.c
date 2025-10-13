/* cosmo_tcc.c - TinyCC Integration Module
 * This module provides comprehensive TCC functionality including:
 * - Runtime library support
 * - Symbol table management
 * - State initialization
 * - Path configuration
 * - Error handling
 */

#include "cosmo_tcc.h"
#include "cosmo_utils.h"
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

extern cosmorun_config_t g_config;
extern cosmorun_result_t init_config(void);

//#include "cosmo_trampoline.h"
extern void *cosmo_trampoline_wrap(void *module, void *addr);

// Forward declarations
void tcc_error_func(void *opaque, const char *msg);
void register_default_include_paths(TCCState *s, const struct utsname *uts);
void register_default_library_paths(TCCState *s);
void register_builtin_symbols(TCCState *s);
void link_tcc_runtime(TCCState *s);

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

// Link runtime library into TCC state
void link_tcc_runtime(TCCState *s) {
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
extern void *__import(const char *module);
extern void *__import_sym(void *handle, const char *symbol);
extern void __import_free(void *handle);

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

  // CosmoRun dynamic loading (platform abstraction)
  {"__dlopen", cosmorun_dlopen},
  {"__dlsym", cosmorun_dlsym},
  {"dlopen", cosmorun_dlopen},
  {"dlsym", cosmorun_dlsym},
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
  {"pthread_mutex_init", pthread_mutex_init},
  {"pthread_mutex_lock", pthread_mutex_lock},
  {"pthread_mutex_unlock", pthread_mutex_unlock},
  {"pthread_mutex_destroy", pthread_mutex_destroy},

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

  // Dynamic module loading API
  // {"__import", __import},
  // {"__import_sym", __import_sym},
  // {"__import_free", __import_free},
  {"__import", __import},//./cosmorun.exe -e 'int main(){void* h=__import("std.c");printf("h=%d\n",h);}'
  {"__import_sym", __import_sym},
  {"__import_free", __import_free},

  // TCC functions (testing nested TCC usage)
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

  // Coroutine support from libco (ARM64, x86-64, ARM32)
  // TODO: Link libco properly
//#if defined(__aarch64__) || defined(__arm64__) || defined(__x86_64__) || defined(__amd64__)
//  {"coctx_swap", _coctx_swap},
//  {"_coctx_swap", _coctx_swap},  // Also register with underscore for macOS
//#endif

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
            fprintf(stderr, "[cosmorun] Cleaning up resource: %s\n", manager->name);
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
    } else if (is_macos) {
        append_string_option(buffer, size, "-D__APPLE__");
        append_string_option(buffer, size, "-D__MACH__");
        append_string_option(buffer, size, "-DTCC_TARGET_MACHO");
#ifdef __aarch64__
        append_string_option(buffer, size, "-DTCC_TARGET_ARM64");
#endif
    } else {
        append_string_option(buffer, size, "-D__unix__");
        if (is_linux) {
            append_string_option(buffer, size, "-D__linux__");
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
            fprintf(stderr, "[cosmorun] Using %d cached include paths (fast path)\n", g_cached_path_count);
        }
        for (int i = 0; i < g_cached_path_count; i++) {
            tcc_add_include_path(s, g_cached_include_paths[i]);
            tcc_add_sysinclude_path(s, g_cached_include_paths[i]);
        }
        return;
    }

    // Slow path: first time, check and cache valid paths
    if (g_config.trace_enabled >= 2) {
        fprintf(stderr, "[cosmorun] Initializing include paths for %s (slow path)\n", sysname);
    }

    // Universal local paths (checked on ALL platforms first)
    const char *local_candidates[] = {
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
        fprintf(stderr, "[cosmorun] Path cache initialized with %d valid paths\n", g_cached_path_count);
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
            fprintf(stderr, "[cosmorun] TCC options: %s\n", g_config.tcc_options);
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

void* __import(const char* path) {
    if (!path || !*path) {
        tracef("__import: null or empty path");
        return NULL;
    }
    tracef("__import: path=%s", path);

    // Check if it's already a .o file
    if (ends_with(path, ".o")) {
        return load_o_file(path);
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
                // Additional check: any .h file in current dir newer than cache?
                // This catches header file changes without full dependency tracking
                int headers_modified = 0;
                DIR *dir = opendir(".");
                if (dir) {
                    struct dirent *entry;
                    while ((entry = readdir(dir)) != NULL) {
                        size_t len = strlen(entry->d_name);
                        if (len > 2 && strcmp(entry->d_name + len - 2, ".h") == 0) {
                            struct stat h_st;
                            if (stat(entry->d_name, &h_st) == 0 && h_st.st_mtime > cache_st.st_mtime) {
                                tracef("header '%s' newer than cache, invalidating", entry->d_name);
                                headers_modified = 1;
                                break;
                            }
                        }
                    }
                    closedir(dir);
                }

                if (!headers_modified) {
                    tracef("using cached '%s' (mtime match)", cache_path);
                    return load_o_file(cache_path);
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
            return load_o_file(cache_path);
        } else {
            tracef("neither source '%s' nor cache '%s' found", path, cache_path);
            return NULL;
        }
    }

    // Compile from source
    tracef("compiling '%s'", path);

    TCCState* s = tcc_new();
    if (!s) {
        tracef("__import: tcc_new failed");
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

    // Compile source file
    if (tcc_add_file(s, path) == -1) {
        tracef("tcc_add_file failed for '%s'", path);
        tcc_delete(s);
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
        return NULL;
    }

    tracef("successfully loaded '%s' -> %p", path, s);
    return (void*)s;
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

    tracef("__import_free: freeing module %p", module);
    tcc_delete((TCCState*)module);
}

// ============================================================================
// Trampoline System Implementation (from cosmo_trampoline.c)
// ============================================================================

// ============================================================================
#ifdef __x86_64__

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

void *cosmo_trampoline_arm64_vararg(void *vfunc, int variadic_type, const char *name) {
    if (!vfunc) return NULL;

    // Check if we already have a trampoline for this function
    for (size_t i = 0; i < g_arm64_vararg_count; ++i) {
        if (g_arm64_vararg_trampolines[i].orig == vfunc) {
            return g_arm64_vararg_trampolines[i].stub;
        }
    }

    // Create new trampoline
    void *stub = arm64_make_vararg_trampoline(vfunc, variadic_type);
    if (stub && g_arm64_vararg_count < ARM64_MAX_VARARGS_TRAMPOLINES) {
        g_arm64_vararg_trampolines[g_arm64_vararg_count].orig = vfunc;
        g_arm64_vararg_trampolines[g_arm64_vararg_count].stub = stub;
        g_arm64_vararg_trampolines[g_arm64_vararg_count].name = name;
        ++g_arm64_vararg_count;
    }

    return stub ? stub : vfunc;
}

size_t cosmo_trampoline_arm64_count(void) {
    return g_arm64_vararg_count;
}

#endif // __aarch64__

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
    // Windows CC conversion on x86_64
    // IsWindows(); should do better
    return cosmo_trampoline_win_wrap(module, addr);
#else
    // No automatic wrapping on other platforms
    (void)module;
    return addr;
#endif
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

void *cosmo_trampoline_libc_resolve(const char *name, int variadic_type) {
    // Lazy initialization
    if (!g_libc_init) {
        cosmo_trampoline_libc_init();
    }

    // Resolve from libc/libm
    void* addr = NULL;
    if (g_libc) {
        addr = cosmorun_dlsym(g_libc, name);  // Already applies Windows CC conversion
    }
    if (!addr && g_libm) {
        addr = cosmorun_dlsym(g_libm, name);  // Already applies Windows CC conversion
    }

    if (!addr) {
        return NULL;
    }

#ifdef __aarch64__
    // ARM64: Create trampoline for variadic functions
    if (variadic_type) {
        // For variadic functions, resolve the v* variant (vsnprintf, vsprintf, vprintf)
        char vname[64];
        snprintf(vname, sizeof(vname), "v%s", name);
        void *vfunc = cosmorun_dlsym(g_libc, vname);
        if (!vfunc) {
            // Fallback to original function if v* variant not found
            return addr;
        }

        // Create trampoline for va_list marshalling
        void* trampoline = cosmo_trampoline_arm64_vararg(vfunc, variadic_type, name);
        if (trampoline) {
            return trampoline;
        }
        // Fallback: if trampoline creation failed, use direct call
    }
#else
    (void)variadic_type;  // Unused on non-ARM64 platforms
#endif

    return addr;
}

bool cosmo_trampoline_libc_is_initialized(void) {
    return g_libc_init != 0;
}

