/* cosmo_tcc.c - TinyCC Integration Module
 * Extracted from cosmorun.c
 *
 * This module provides comprehensive TCC functionality including:
 * - Runtime library support
 * - Symbol table management
 * - State initialization
 * - Path configuration
 * - Error handling
 */

#include "cosmo_tcc.h"
#include "cosmo_utils.h"

// Type definitions from cosmorun.c
#define COSMORUN_MAX_OPTIONS_SIZE 512
#define COSMORUN_MAX_PATH_SIZE 4096

typedef struct {
    char tcc_options[COSMORUN_MAX_OPTIONS_SIZE];
    struct utsname uts;
    int trace_enabled;
    char include_paths[COSMORUN_MAX_PATH_SIZE];
    char library_paths[COSMORUN_MAX_PATH_SIZE];
    char host_libs[COSMORUN_MAX_PATH_SIZE];
    int initialized;
} cosmorun_config_t;

// External references from cosmorun.c
extern void *cosmo_dlopen_ext(const char *filename, int flags);
extern void *cosmo_import(const char *path);
extern void *cosmo_import_sym(void *module, const char *symbol);
extern void cosmo_import_free(void *module);
extern void *cosmo_dlopen(const char *filename, int flags);
extern void *cosmo_dlsym(void *handle, const char *symbol);
extern int cosmo_dlclose(void *handle);
extern char *cosmo_dlerror(void);
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
/* Part B: Symbol Table and Registration */
/* Extracted from cosmorun.c lines 1091-1324 */

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

// Cosmopolitan dynamic loading helpers
extern void *cosmo_dlopen_ext(const char *filename, int flags);
extern void *cosmorun_dlsym(void *handle, const char *symbol);
extern int cosmo_dlclose(void *handle);
extern char *cosmo_dlerror(void);
extern void *cosmo_dlopen(const char *filename, int flags);

// Cosmopolitan dynamic module loading API
extern void *cosmo_import(const char *module);
extern void *cosmo_import_sym(void *handle, const char *symbol);
extern void cosmo_import_free(void *handle);

// Platform-specific functions
extern int cosmorun_uname(struct utsname *buf);
extern int cosmorun_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

// Debug trace function
extern void tracef(const char *fmt, ...);

// Platform detection
#ifdef __COSMOPOLITAN__
#include "libc/dce.h"
#else
static inline int IsWindows(void) {
#ifdef _WIN32
    return 1;
#else
    return 0;
#endif
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

  // Math functions (commonly used)
  {"abs", abs},
  {"labs", labs},
  {"sin", sin},
  {"cos", cos},
  {"sqrt", sqrt},

  // Cosmopolitan dynamic loading helpers (allow modules to call cosmo_* directly)
  {"dlopen", cosmo_dlopen_ext},
  {"dlsym", cosmorun_dlsym},
  {"dlclose", cosmo_dlclose},
  {"dlerror", cosmo_dlerror},
  //for cosmo dev/test
  {"cosmo_dlopen", cosmo_dlopen},
  {"cosmo_dlsym", cosmorun_dlsym},
  {"cosmo_dlclose", cosmo_dlclose},
  {"cosmo_dlerror", cosmo_dlerror},

  // POSIX-style dynamic loading functions (mostly stubs under Cosmopolitan)
  // {"dlopen", dlopen},
  // {"dlsym", dlsym},
  // {"dlclose", dlclose},
  // {"dlerror", dlerror},
  // Note: cosmo_* functions are declared externally when available

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

  // POSIX-style file descriptors
  {"open", open},
  {"read", read},
  {"write", write},
  {"close", close},
  {"unlink", unlink},
  {"access", access},
  // Note: fork/waitpid/execve may be NULL on Windows - checked at runtime
  {"fork", fork},
  {"waitpid", waitpid},
  {"execve", execve},
  {"_exit", _exit},

  //batch3(for win has unx)
  {"execv", execv},
  {"kill", kill},
  {"mkdir", mkdir},
  //{"errno", errno},
  {"connect", connect},
  {"stat", stat},
  {"fileno", fileno},
  {"dup2", dup2},
  {"getcwd", getcwd},
  {"execl", execl},
  {"pthread_create", pthread_create},
  {"pthread_join", pthread_join},
  {"pthread_mutex_init", pthread_mutex_init},
  {"pthread_mutex_lock", pthread_mutex_lock},
  {"pthread_mutex_unlock", pthread_mutex_unlock},
  {"pthread_mutex_destroy", pthread_mutex_destroy},
  {"setrlimit", setrlimit},
  {"htonl", htonl},
  {"sleep", sleep},
  {"bind", bind},
  {"listen", listen},
  {"accept", accept},

  {"htons", htons},
  {"inet_addr", inet_addr},
  {"socket", socket},
  {"recv", recv},
  {"select", select},

  {"isatty", isatty},
  {"tcgetattr", tcgetattr},
  {"tcsetattr", tcsetattr},
  {"fcntl", fcntl},
  {"uname", cosmorun_uname},
  {"getpid", getpid},

  //batch4(for cdp*)
  {"gettimeofday", gettimeofday},
  {"execlp", execlp},
  {"setsockopt", setsockopt},
  {"send", send},
  //{"optind", optind},
  {"sigaction", cosmorun_sigaction},
  {"sigemptyset", sigemptyset},
  {"getopt_long", getopt_long},
  {"strdup", strdup},

  // Time functions
  {"clock_gettime", clock_gettime},
  {"usleep", usleep},
  {"time", time},
  {"localtime", localtime},

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
  // {"cosmo_import", cosmo_import},
  // {"cosmo_import_sym", cosmo_import_sym},
  // {"cosmo_import_free", cosmo_import_free},
  {"__import", cosmo_import},//./cosmorun.exe -e 'int main(){void* h=__import("std.c");printf("h=%d\n",h);}'
  {"__sym", cosmo_import_sym},
  {"__import_free", cosmo_import_free},

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
                //tracef("skipping POSIX symbol on Windows: %s", entry->name);
                //continue;
                printf("DEBUG POSIX symbol on Windows: %s %d\n", entry->name,entry->address);
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
/* Part C: TCC State Management and Path Functions */
#include "cosmo_tcc.h"

/**
 * ============================================================================
 * TCC Configuration Structures
 * ============================================================================
 */

const cosmo_tcc_config_t COSMO_TCC_CONFIG_MEMORY = {
    .output_type = TCC_OUTPUT_MEMORY,
    .output_file = NULL,  // 使用全局配置
    .relocate = 1,
    .run_entry = 1
};

const cosmo_tcc_config_t COSMO_TCC_CONFIG_OBJECT = {
    .output_type = TCC_OUTPUT_OBJ,
    .output_file = NULL,  // 使用全局配置
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

    // 设置错误处理
    tcc_set_error_func(s, NULL, tcc_error_func);

    // 设置输出类型
    tcc_set_output_type(s, output_type);

    // 应用选项
    if (options && options[0]) {
        tcc_set_options(s, options);
    }

    // 注册路径
    if (enable_paths) {
        register_default_include_paths(s, &g_config.uts);
        register_default_library_paths(s);
    }

    // // 初始化符号解析器
    // if (enable_resolver) {
    //     init_symbol_resolver();
    // }

    register_builtin_symbols(s);
    link_tcc_runtime(s);  // Link runtime library for TCC compilation

    return s;
}

/**
 * ============================================================================
 * Resource Management Abstraction
 * ============================================================================
 */

// 通用资源清理函数类型
typedef void (*resource_cleanup_fn)(void* resource);

// 资源管理器
typedef struct {
    void* resource;
    resource_cleanup_fn cleanup_fn;
    const char* name;  // 用于调试
} resource_manager_t;

// TCC 状态清理函数
void tcc_state_cleanup(void* resource) {
    if (!resource) return;

    TCCState **state_ptr = (TCCState**)resource;
    if (state_ptr && *state_ptr) {
        tcc_delete(*state_ptr);
        *state_ptr = NULL;
    }
}

// 内存清理函数
void memory_cleanup(void* resource) {
    if (!resource) return;

    void **ptr = (void**)resource;
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

// 创建资源管理器
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

// 清理资源管理器
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

// RAII 风格的自动清理宏（GCC/Clang）
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

void *cosmorun_dlsym(void *handle, const char *symbol) {
    void *addr = cosmo_dlsym(handle, symbol);
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

void register_env_paths(TCCState *s, const char *env_name, int include_mode) {
    const char *value = getenv(env_name);
    if (!value || !*value) return;

    size_t len = strlen(value);
    char *buffer = malloc(len + 1);
    if (!buffer) return;
    memcpy(buffer, value, len + 1);

#if defined(_WIN32) || defined(_WIN64)
    const char delimiter = ';';
#else
    const char delimiter = strchr(buffer, ';') ? ';' : ':';
#endif

    char *iter = buffer;
    while (iter && *iter) {
        char *token_end = strchr(iter, delimiter);
        if (token_end) *token_end = '\0';

        char *path = iter;
        while (*path == ' ' || *path == '\t') path++;
        char *end = path + strlen(path);
        while (end > path && (end[-1] == ' ' || end[-1] == '\t')) {
            --end;
        }
        *end = '\0';

        if (*path) {
            tcc_add_path_if_exists(s, path, include_mode);
        }

        if (!token_end) break;
        iter = token_end + 1;
    }

    free(buffer);
}

void register_default_include_paths(TCCState *s, const struct utsname *uts) {
    const char *sysname = uts && uts->sysname[0] ? uts->sysname : "";

    // 标准系统包含路径（移除 Cosmopolitan 特定路径）
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

    const char **candidates = posix_candidates;
    if (!strcasecmp(sysname, "darwin")) {
        candidates = mac_candidates;
    }

    if (g_config.trace_enabled) {
        fprintf(stderr, "[cosmorun] Registering include paths for %s\n", sysname);
    }

    for (const char **p = candidates; *p; ++p) {
        tcc_add_path_if_exists(s, *p, 1);
    }

    register_env_paths(s, "COSMORUN_INCLUDE_PATHS", 1);
}

void register_default_library_paths(TCCState *s) {
    register_env_paths(s, "COSMORUN_LIBRARY_PATHS", 0);
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

    // 预设置输出类型，确保符号表等内部结构就绪
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

    // 构建 TCC 选项
    build_default_tcc_options(g_config.tcc_options, sizeof(g_config.tcc_options), &g_config.uts);
    if (g_config.tcc_options[0]) {
        if (g_config.trace_enabled) {
            fprintf(stderr, "[cosmorun] TCC options: %s\n", g_config.tcc_options);
        }
        tcc_set_options(s, g_config.tcc_options);
    }

    // 注册路径
    register_default_include_paths(s, &g_config.uts);
    register_default_library_paths(s);

    // 添加额外的包含路径
    if (g_config.include_paths[0]) {
        char include_copy[PATH_MAX];
        strncpy(include_copy, g_config.include_paths, sizeof(include_copy) - 1);
        include_copy[sizeof(include_copy) - 1] = '\0';

        char* path = strtok(include_copy, g_platform_ops.get_path_separator());
        while (path) {
            tcc_add_include_path(s, path);
            if (g_config.trace_enabled) {
                fprintf(stderr, "[cosmorun] Added include path: %s\n", path);
            }
            path = strtok(NULL, g_platform_ops.get_path_separator());
        }
    }

    // 添加额外的库路径
    if (g_config.library_paths[0]) {
        char library_copy[PATH_MAX];
        strncpy(library_copy, g_config.library_paths, sizeof(library_copy) - 1);
        library_copy[sizeof(library_copy) - 1] = '\0';

        char* path = strtok(library_copy, g_platform_ops.get_path_separator());
        while (path) {
            tcc_add_library_path(s, path);
            if (g_config.trace_enabled) {
                fprintf(stderr, "[cosmorun] Added library path: %s\n", path);
            }
            path = strtok(NULL, g_platform_ops.get_path_separator());
        }
    }

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

void cosmo_tcc_register_env_paths(TCCState *s, const char *env_name, int include_mode) {
    register_env_paths(s, env_name, include_mode);
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
