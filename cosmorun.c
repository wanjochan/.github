// @cosmorun.md @cosmorun_build.sh
/* arch notes
  |-------------|----------------|----------------------------|
  | __x86_64__  | CPU            | Windows/Linux/macOS x86_64 |
  | __aarch64__ | CPU            | Windows/Linux/macOS ARM64  |
  | _WIN32      | Windows 32/64  | Windows                  |
  | _WIN64      | Windows 64     | Windows x64              |
  | __linux__   | Linux          | Linux                    |
  | __APPLE__   | Apple          | macOS/iOS                |
*/

#include "cosmo_libc.h"
//#include "cosmo_trampoline.h"
#include "cosmo_tcc.h"
#include "cosmo_utils.h"

/**
 * ============================================================================
 * 统一错误处理系统
 * ============================================================================
 */

// Error handling is now in cosmo_utils.h

// 参数解析结果结构体
typedef struct {
    int inline_mode;
    const char *inline_code;
    int inline_code_index;
    int dashdash_index;
    int source_index;  // First source file index (for compatibility)
    int *source_indices;  // Array of all source file indices
    int source_count;  // Number of source files
} parse_result_t;

/**
 * ============================================================================
 * 常量定义 - 消除魔法数字和字符串
 * ============================================================================
 */

// 缓冲区大小常量
// COSMORUN_MAX_CODE_SIZE and COSMORUN_MAX_PATH_SIZE defined in cosmo_utils.h
#define COSMORUN_MAX_OPTIONS_SIZE   512     // TCC options buffer
#define COSMORUN_MAX_EXEC_ARGS      256     // Maximum execution arguments
#define COSMORUN_REPL_GLOBAL_SIZE   65536   // 64KB for REPL global code
#define COSMORUN_REPL_STMT_SIZE     32768   // 32KB for REPL statement body
#define COSMORUN_REPL_LINE_SIZE     4096    // 4KB for REPL input line

// 字符串常量
#define COSMORUN_REPL_PROMPT        ">>> "
#define COSMORUN_REPL_WELCOME       "cosmorun REPL - C interactive shell\nType C code, :help for commands, :quit to exit\n"
#define COSMORUN_REPL_GOODBYE       "\nBye!\n"

// 性能调优常量
#define COSMORUN_SYMBOL_CACHE_SIZE  64      // 内置符号缓存大小
#define COSMORUN_HASH_SEED          5381    // DJB2 哈希种子

/**
 * ============================================================================
 * 全局配置系统
 * ============================================================================
 */

// 全局配置结构
typedef struct {
    char tcc_options[COSMORUN_MAX_OPTIONS_SIZE];
    struct utsname uts;
    int trace_enabled;
    char include_paths[COSMORUN_MAX_PATH_SIZE];
    char library_paths[COSMORUN_MAX_PATH_SIZE];
    char host_libs[COSMORUN_MAX_PATH_SIZE];
    int initialized;
} cosmorun_config_t;

cosmorun_config_t g_config = {0};

// 初始化配置系统
cosmorun_result_t init_config(void) {
    if (g_config.initialized) return COSMORUN_SUCCESS;

    // 获取系统信息
    if (uname(&g_config.uts) != 0) {
        return COSMORUN_ERROR_PLATFORM;
    }

    // 读取环境变量
    const char *trace_env = getenv("COSMORUN_TRACE");
    if (!trace_env || trace_env[0] == '\0' || trace_env[0] != '0') {
        g_config.trace_enabled = 1;  // default on
    } else {
        g_config.trace_enabled = 0;
    }

    const char* include_paths = getenv("COSMORUN_INCLUDE_PATHS");
    if (include_paths) {
        strncpy(g_config.include_paths, include_paths, sizeof(g_config.include_paths) - 1);
    }

    const char* library_paths = getenv("COSMORUN_LIBRARY_PATHS");
    if (library_paths) {
        strncpy(g_config.library_paths, library_paths, sizeof(g_config.library_paths) - 1);
    }

    const char* host_libs = getenv("COSMORUN_HOST_LIBS");
    if (host_libs) {
        strncpy(g_config.host_libs, host_libs, sizeof(g_config.host_libs) - 1);
    }

    g_config.initialized = 1;
    return COSMORUN_SUCCESS;
}

/**
 * ============================================================================
 * 哈希优化符号缓存系统
 * ============================================================================
 */

// 优化的字符串哈希函数（DJB2算法）
//static inline uint32_t hash_string(const char* str) {
//    if (!str) return 0;
//
//    uint32_t hash = COSMORUN_HASH_SEED;
//    int c;
//    while ((c = *str++)) {
//        hash = ((hash << 5) + hash) + c; // hash * 33 + c
//    }
//    return hash;
//}

// 带长度限制的哈希函数，避免过长字符串的性能问题
//static inline uint32_t hash_string_bounded(const char* str, size_t max_len) {
//    if (!str) return 0;
//
//    uint32_t hash = COSMORUN_HASH_SEED;
//    size_t len = 0;
//    int c;
//    while ((c = *str++) && len < max_len) {
//        hash = ((hash << 5) + hash) + c;
//        len++;
//    }
//    return hash;
//}

// // 初始化符号缓存的哈希值
// static void init_symbol_cache_hashes(void) {
//     // 暂时跳过哈希初始化，因为 cosmo_tcc_get_builtin_symbols() 是 const
//     // 在实际使用时动态计算哈希值
// }



// Input validation and API injection moved to cosmo_validation.{c,h}

/**
 * ============================================================================
 * 统一 TCC 状态管理 - 提升健壮性
 * ============================================================================
 */

// TCC 状态配置选项
typedef struct {
    int output_type;
    const char* options;
    int enable_symbol_resolver;
    int enable_default_paths;
} tcc_config_t;

// 标准配置预设
static const tcc_config_t TCC_CONFIG_MEMORY = {
    .output_type = TCC_OUTPUT_MEMORY,
    .options = NULL,  // 使用全局配置
    .enable_symbol_resolver = 1,
    .enable_default_paths = 1
};

static const tcc_config_t TCC_CONFIG_OBJECT = {
    .output_type = TCC_OUTPUT_OBJ,
    .options = NULL,  // 使用全局配置
    .enable_symbol_resolver = 0,
    .enable_default_paths = 1
};

// 前向声明
static void tcc_error_func(void *opaque, const char *msg);
// static int init_symbol_resolver(void);

// Forward declarations for Cosmopolitan-style dlopen helpers
extern void *cosmo_dlopen(const char *filename, int flags);
extern void *cosmo_dlsym(void *handle, const char *symbol);
extern int cosmo_dlclose(void *handle);
extern char *cosmo_dlerror(void);
void *cosmo_dlopen_ext(const char *filename, int flags){
    if (!filename || !*filename) {
        return cosmo_dlopen(filename, flags);
    }

    void *handle = cosmo_dlopen(filename, flags);
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

#if defined(_WIN32) || defined(_WIN64)
    static const char *exts[] = { ".dll", ".so", ".dylib", NULL };
#elif defined(__APPLE__)
    static const char *exts[] = { ".dylib", ".so", ".dll", NULL };
#else
    static const char *exts[] = { ".so", ".dylib", ".dll", NULL };
#endif

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

            tracef("cosmo_dlopen_ext: retry '%s'", candidate);
            handle = cosmo_dlopen(candidate, flags);
            if (handle) {
                return handle;
            }
        }
    }

    return NULL;
}

// 统一的 TCC 状态创建和初始化函数

/**
 * ============================================================================
 * 通用资源管理抽象 - 提升复用性
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
static void tcc_state_cleanup(void* resource) {
    if (!resource) return;

    TCCState **state_ptr = (TCCState**)resource;
    if (state_ptr && *state_ptr) {
        tcc_delete(*state_ptr);
        *state_ptr = NULL;
    }
}

// 内存清理函数
static void memory_cleanup(void* resource) {
    if (!resource) return;

    void **ptr = (void**)resource;
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

// 创建资源管理器
static inline resource_manager_t create_resource_manager(void* resource,
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
static inline void cleanup_resource_manager(resource_manager_t* manager) {
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

// 前向声明（需要在使用前声明）

/**
 * ============================================================================
 * Core Dump 和崩溃处理系统
 * ============================================================================
 *
 * 提供比标准 core dump 更友好的崩溃信息，包括：
 * - 源代码上下文
 * - 符号信息
 * - 执行状态
 * - 调试建议
 */

// 全局状态用于崩溃处理
typedef struct {
    const char *current_source_file;
    int current_line;
    const char *current_function;
    TCCState *current_tcc_state;
    jmp_buf crash_recovery;
    int crash_recovery_active;
} crash_context_t;

static crash_context_t g_crash_context = {0};

// 信号处理函数
static void crash_signal_handler(int sig) {
    const char *sig_name = "UNKNOWN";
    const char *sig_desc = "Unknown signal";

    switch (sig) {
        case SIGSEGV:
            sig_name = "SIGSEGV";
            sig_desc = "Segmentation fault (invalid memory access)";
            break;
        case SIGFPE:
            sig_name = "SIGFPE";
            sig_desc = "Floating point exception (division by zero, etc.)";
            break;
        case SIGILL:
            sig_name = "SIGILL";
            sig_desc = "Illegal instruction";
            break;
        case SIGABRT:
            sig_name = "SIGABRT";
            sig_desc = "Program aborted";
            break;
#ifdef SIGBUS
        case SIGBUS:
            sig_name = "SIGBUS";
            sig_desc = "Bus error (alignment or memory access issue)";
            break;
#endif
    }

    fprintf(stderr, "\n================================================================================\n");
    fprintf(stderr, "🚨 COSMORUN CRASH DETECTED\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "Signal: %s (%d)\n", sig_name, sig);
    fprintf(stderr, "Description: %s\n", sig_desc);
    ShowBacktrace(2, (const struct StackFrame *)__builtin_frame_address(0));

    if (g_crash_context.current_source_file) {
        fprintf(stderr, "Source File: %s\n", g_crash_context.current_source_file);
    }

    if (g_crash_context.current_function) {
        fprintf(stderr, "Function: %s\n", g_crash_context.current_function);
    }

    if (g_crash_context.current_line > 0) {
        fprintf(stderr, "Line: %d\n", g_crash_context.current_line);
    }

    fprintf(stderr, "\n💡 DEBUGGING SUGGESTIONS:\n");

    switch (sig) {
        case SIGSEGV:
            fprintf(stderr, "- Check for null pointer dereferences\n");
            fprintf(stderr, "- Verify array bounds access\n");
            fprintf(stderr, "- Check for use-after-free errors\n");
            fprintf(stderr, "- Ensure proper pointer initialization\n");
            break;
        case SIGFPE:
            fprintf(stderr, "- Check for division by zero\n");
            fprintf(stderr, "- Verify floating point operations\n");
            fprintf(stderr, "- Check for integer overflow\n");
            break;
        case SIGILL:
            fprintf(stderr, "- Code may be corrupted or invalid\n");
            fprintf(stderr, "- Check for buffer overflows\n");
            fprintf(stderr, "- Verify function pointers\n");
            break;
    }

    fprintf(stderr, "\n🔧 RECOVERY OPTIONS:\n");
    fprintf(stderr, "- Add debug prints around the crash location\n");
    fprintf(stderr, "- Use COSMORUN_TRACE=1 for detailed execution trace\n");
    fprintf(stderr, "- Try running with smaller input data\n");
    fprintf(stderr, "- Check memory usage patterns\n");

    fprintf(stderr, "================================================================================\n");

    // 如果有恢复点，尝试恢复
    if (g_crash_context.crash_recovery_active) {
        fprintf(stderr, "Attempting graceful recovery...\n");
        longjmp(g_crash_context.crash_recovery, sig);
    }

    // 否则正常退出
    exit(128 + sig);
}

// 初始化崩溃处理系统
static void init_crash_handler(void) {
    signal(SIGSEGV, crash_signal_handler);
    signal(SIGFPE, crash_signal_handler);
    signal(SIGILL, crash_signal_handler);
    signal(SIGABRT, crash_signal_handler);
#ifdef SIGBUS
    signal(SIGBUS, crash_signal_handler);
#endif
}

// 设置当前执行上下文（用于更好的错误报告）
static void set_crash_context(const char *source_file, const char *function, int line) {
    g_crash_context.current_source_file = source_file;
    g_crash_context.current_function = function;
    g_crash_context.current_line = line;
}

// RAII 风格的资源管理结构体
typedef struct {
    TCCState *tcc_state;
    char **compile_argv;
    int initialized;
} tcc_context_t;

// 初始化 TCC 上下文
static tcc_context_t* tcc_context_init(void) {
    tcc_context_t *ctx = calloc(1, sizeof(tcc_context_t));
    if (!ctx) return NULL;

    ctx->tcc_state = cosmo_tcc_init_state();
    if (!ctx->tcc_state) {
        free(ctx);
        return NULL;
    }

    ctx->initialized = 1;
    return ctx;
}

// 清理 TCC 上下文
static void tcc_context_cleanup(tcc_context_t *ctx) {
    if (!ctx) return;

    if (ctx->compile_argv) {
        free(ctx->compile_argv);
        ctx->compile_argv = NULL;
    }

    if (ctx->tcc_state) {
        tcc_delete(ctx->tcc_state);
        ctx->tcc_state = NULL;
    }

    ctx->initialized = 0;
    free(ctx);
}

static int execute_tcc_compilation_auto(int argc, char **argv);

/**
 * 自动清理 char** 数组
 */
static inline void char_array_cleanup(char ***argv) {
    if (argv && *argv) {
        free(*argv);
        *argv = NULL;
    }
}

// 便利宏：自动清理的变量声明（使用统一的清理函数）
#define AUTO_CHAR_ARRAY(name) \
    char **__attribute__((cleanup(char_array_cleanup))) name = NULL

typedef struct {
    const char* name;
    void* address;
    int is_cached;
} SymbolCacheEntry;

typedef struct {
    const char* alias;
    const char* target;
} SymbolAlias;

#define MAX_LIBRARY_HANDLES 16
typedef struct {
    void* handles[MAX_LIBRARY_HANDLES];
    int handle_count;
    int initialized;
} SymbolResolver;

static SymbolResolver g_resolver = {{0}, 0, 0};

static void tcc_error_func(void *opaque, const char *msg);

void* cosmo_import(const char* path);
void* cosmo_import_sym(void* module, const char* symbol);
void cosmo_import_free(void* module);

// // 哈希优化的符号查找
// static void* find_symbol_by_hash(const char* name, uint32_t hash) {
//     // 在内置符号表中查找
//     for (size_t i = 0; cosmo_tcc_get_builtin_symbols()[i].name; i++) {
//         // 动态计算哈希值进行比较
//         if (hash_string(cosmo_tcc_get_builtin_symbols()[i].name) == hash &&
//             strcmp(cosmo_tcc_get_builtin_symbols()[i].name, name) == 0) {
//             return cosmo_tcc_get_builtin_symbols()[i].address;
//         }
//     }
//     return NULL;
// }

// String and utility functions moved to cosmo_utils.{c,h}

// Initialize dynamic symbol resolver
//static void add_library_handle(void *handle, const char *label) {
//    if (!handle) return;
//#if (defined(_WIN32) || defined(_WIN64)) && defined(__x86_64__)
//    if (!g_win_host_module && label && strcmp(label, "self") == 0) {
//        cosmorun_set_host_module(handle);
//    }
//#endif
//    for (int i = 0; i < g_resolver.handle_count; ++i) {
//        if (g_resolver.handles[i] == handle) {
//            return;
//        }
//    }
//    if (g_resolver.handle_count >= MAX_LIBRARY_HANDLES) {
//        tracef("symbol resolver handle limit reached, skipping %s", label ? label : "(unknown)");
//        return;
//    }
//    g_resolver.handles[g_resolver.handle_count++] = handle;
//    tracef("registered host library handle %s -> %p", label ? label : "(unnamed)", handle);
//}

static void tcc_add_path_if_exists(TCCState *s, const char *path, int include_mode) {
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

static void register_env_paths(TCCState *s, const char *env_name, int include_mode) {
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
        "libm.so.6",               // Linux math library
        "libc.so.6",               // Linux glibc
        "libm.so",                 // Generic math library
        "libc.so",                 // Generic libc
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
        void* handle = cosmo_dlopen(lib_names_ptr[i], flags);
        if (handle) {
            g_resolver.handles[g_resolver.handle_count++] = handle;
            tracef("Loaded library: %s (handle=%p)", lib_names_ptr[i], handle);
        } else {
            const char* err = cosmo_dlerror();
            tracef("Failed to load %s: %s", lib_names_ptr[i], err ? err : "unknown error");
        }
    }

    g_resolver.initialized = 1;
    tracef("Symbol resolver initialized with %d libraries", g_resolver.handle_count);
}

// //@hack tcc at tccelf.c: addr = cosmorun_resolve_symbol(name_ud);
void* cosmorun_resolve_symbol(const char* symbol_name) {
    if (!symbol_name || !*symbol_name) {
        return NULL;
    }
    tracef("cosmorun_resolve_symbol: %s", symbol_name);

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

    // Dynamic resolution from system libraries (all platforms now supported)
    if (!g_resolver.initialized) {
        init_symbol_resolver();
    }

    void* addr = NULL;
    for (int i = 0; i < g_resolver.handle_count; i++) {
        if (!g_resolver.handles[i]) continue;
        addr = cosmo_dlsym(g_resolver.handles[i], symbol_name);
        if (addr) {
            tracef("Resolved from library %d: %s -> %p", i, symbol_name, addr);
            return addr;
        }
    }

    // Symbol not found anywhere
    tracef("Symbol not found: %s", symbol_name);
    return NULL;
}

static void tcc_error_func(void *opaque, const char *msg) {
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

// Cache management moved to cosmo_cache.{c,h}

void* cosmo_import(const char* path) {
    if (!path || !*path) {
        tracef("cosmo_import: null or empty path");
        return NULL;
    }
    tracef("cosmo_import: path=%s", path);

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
        tracef("cosmo_import: tcc_new failed");
        return NULL;
    }

    tracef("cosmo_import: tcc_state=%p", s);
    tcc_set_error_func(s, NULL, tcc_error_func);
    tracef("cosmo_import: set error func");

    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    tracef("cosmo_import: set output type");

    char tcc_options[COSMORUN_MAX_OPTIONS_SIZE] = {0};
    cosmo_tcc_build_default_options(tcc_options, sizeof(tcc_options), &uts);
    tracef("cosmo_import: built tcc options");

    if (tcc_options[0]) {
        tcc_set_options(s, tcc_options);
        tracef("cosmo_import: set tcc options");
    }

    cosmo_tcc_register_include_paths(s, &uts);
    tracef("cosmo_import: registered include paths");

    cosmo_tcc_register_library_paths(s);
    tracef("cosmo_import: registered library paths");

    cosmo_tcc_register_builtin_symbols(s);
    tracef("cosmo_import: registered builtin symbols");

    cosmo_tcc_link_runtime(s);
    tracef("cosmo_import: linked tcc runtime");

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

void* cosmo_import_sym(void* module, const char* symbol) {
    if (!module || !symbol) {
        tracef("cosmo_import_sym: null module or symbol");
        return NULL;
    }

    TCCState* s = (TCCState*)module;
    void* addr = tcc_get_symbol(s, symbol);

    if (addr) {
        tracef("cosmo_import_sym: found '%s' -> %p", symbol, addr);
    } else {
        tracef("cosmo_import_sym: symbol '%s' not found", symbol);
    }

    return addr;
}

void cosmo_import_free(void* module) {
    if (!module) return;

    tracef("cosmo_import_free: freeing module %p", module);
    tcc_delete((TCCState*)module);
}

// REPL mode implementation
static int repl_mode(void) {
    printf(COSMORUN_REPL_WELCOME);

    TCCState* s = tcc_new();
    if (!s) {
        fprintf(stderr, "Failed to create TCC state\n");
        return 1;
    }

    tcc_set_error_func(s, NULL, tcc_error_func);
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

    // Configure environment
    struct utsname uts;
    memset(&uts, 0, sizeof(uts));
    uname(&uts);

    char tcc_options[COSMORUN_MAX_OPTIONS_SIZE] = {0};
    cosmo_tcc_build_default_options(tcc_options, sizeof(tcc_options), &uts);
    if (tcc_options[0]) {
        tcc_set_options(s, tcc_options);
    }

    cosmo_tcc_register_include_paths(s, &uts);
    cosmo_tcc_register_library_paths(s);
    // init_symbol_resolver();

    // Accumulate global code (declarations, functions, etc.)
    char global_code[COSMORUN_REPL_GLOBAL_SIZE] = {0};
    int global_len = 0;

    // Accumulate statement body (for persistent variables)
    char stmt_body[COSMORUN_REPL_STMT_SIZE] = {0};
    int stmt_len = 0;

    // Persistent execution state (avoid recreating on every statement)
    TCCState* exec_state = NULL;

    // Start with empty global code (user can add includes as needed)
    global_code[0] = '\0';

    char line[COSMORUN_REPL_LINE_SIZE];
    int exec_counter = 0;

    while (1) {
        printf(COSMORUN_REPL_PROMPT);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;  // EOF
        }

        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
            len--;
        }

        if (len == 0) continue;  // Empty line

        // Handle REPL commands
        if (line[0] == ':') {
            if (strcmp(line, ":quit") == 0 || strcmp(line, ":q") == 0) {
                break;
            } else if (strcmp(line, ":help") == 0 || strcmp(line, ":h") == 0) {
                printf("REPL Commands:\n");
                printf("  :quit, :q    - Exit REPL\n");
                printf("  :show, :s    - Show accumulated code\n");
                printf("  :reset, :r   - Reset REPL state\n");
                printf("  :help, :h    - Show this help\n");
                printf("\nUsage:\n");
                printf("  Declarations/functions are added globally\n");
                printf("  Statements/expressions are executed immediately\n");
                continue;
            } else if (strcmp(line, ":show") == 0 || strcmp(line, ":s") == 0) {
                printf("=== Current Code ===\n%s", global_code);
                printf("=== End ===\n");
                continue;
            } else if (strcmp(line, ":reset") == 0 || strcmp(line, ":r") == 0) {
                tcc_delete(s);
                s = tcc_new();
                tcc_set_error_func(s, NULL, tcc_error_func);
                tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
                if (tcc_options[0]) tcc_set_options(s, tcc_options);
                cosmo_tcc_register_include_paths(s, &uts);
                cosmo_tcc_register_library_paths(s);
                // init_symbol_resolver();
                cosmo_tcc_register_builtin_symbols(s);
                cosmo_tcc_link_runtime(s);  // Link runtime library

                if (exec_state) {
                    tcc_delete(exec_state);
                    exec_state = NULL;
                }

                global_code[0] = '\0';
                global_len = 0;
                stmt_body[0] = '\0';
                stmt_len = 0;
                exec_counter = 0;
                printf("REPL reset\n");
                continue;
            } else {
                printf("Unknown command: %s (type :help)\n", line);
                continue;
            }
        }

        // Detect if it's a declaration/definition or statement/expression
        // Simple heuristic: if starts with type keyword or has '=' or ';' at specific positions
        int is_function_def = 0;

        // Check for function definition pattern: type name(...) {
        if (strstr(line, "(") && strstr(line, ")") && strstr(line, "{")) {
            is_function_def = 1;
        }

        if (is_function_def) {
            // Add function definition to global code
            if (global_len + len + 3 < COSMORUN_REPL_GLOBAL_SIZE) {
                strncat(global_code, line, COSMORUN_REPL_GLOBAL_SIZE - global_len - 1);
                strncat(global_code, "\n", COSMORUN_REPL_GLOBAL_SIZE - strlen(global_code) - 1);
                global_len = strlen(global_code);
                printf("(added to global scope)\n");
            } else {
                printf("Error: code buffer full\n");
            }
        } else {
            // Execute as statement - accumulate in persistent main function
            // Add statement to stmt_body
            if (stmt_len + len + 10 < COSMORUN_REPL_STMT_SIZE) {
                strncat(stmt_body, "    ", COSMORUN_REPL_STMT_SIZE - strlen(stmt_body) - 1);
                strncat(stmt_body, line, COSMORUN_REPL_STMT_SIZE - strlen(stmt_body) - 1);
                if (line[len-1] != ';') {
                    strncat(stmt_body, ";", COSMORUN_REPL_STMT_SIZE - strlen(stmt_body) - 1);
                }
                strncat(stmt_body, "\n", COSMORUN_REPL_STMT_SIZE - strlen(stmt_body) - 1);
                stmt_len = strlen(stmt_body);
            } else {
                printf("Error: statement buffer full\n");
                continue;
            }

            // Delete previous exec_state if exists
            if (exec_state) {
                tcc_delete(exec_state);
            }

            // Create complete program with persistent main function and API declarations
            char exec_code[COSMORUN_MAX_CODE_SIZE];  // 96KB
            snprintf(exec_code, sizeof(exec_code),
                     "%s%s\nint __repl_main() {\n%s    return 0;\n}\n",
                     COSMORUN_API_DECLARATIONS, global_code, stmt_body);

            if (g_config.trace_enabled) {
                fprintf(stderr, "[cosmorun] REPL: Injected API declarations\n");
            }

            // Compile with fresh state
            exec_state = tcc_new();
            tcc_set_error_func(exec_state, NULL, tcc_error_func);
            tcc_set_output_type(exec_state, TCC_OUTPUT_MEMORY);
            if (tcc_options[0]) tcc_set_options(exec_state, tcc_options);
            cosmo_tcc_register_include_paths(exec_state, &uts);
            cosmo_tcc_register_library_paths(exec_state);
            // init_symbol_resolver();
            cosmo_tcc_register_builtin_symbols(exec_state);

            if (tcc_compile_string(exec_state, exec_code) == 0) {
                if (tcc_relocate(exec_state) >= 0) {
                    // Get and execute the main function
                    int (*exec_fn)() = tcc_get_symbol(exec_state, "__repl_main");
                    if (exec_fn) {
                        exec_fn();
                    }
                }
            }
            // Keep exec_state alive for next iteration
        }
    }

    if (exec_state) {
        tcc_delete(exec_state);
    }
    tcc_delete(s);
    printf(COSMORUN_REPL_GOODBYE);
    return 0;
}

/**
 * 执行模式枚举
 */
typedef enum {
    MODE_HELP,
    MODE_REPL,
    MODE_DIRECT_IMPORT,
    MODE_INLINE_CODE,
    MODE_COMPILE_AND_RUN
} execution_mode_t;

/**
 * 解析命令行参数并确定执行模式
 */
static execution_mode_t parse_execution_mode(int argc, char **argv) {
    if (argc == 1) return MODE_REPL;
    if (argc == 2 && strcmp(argv[1], "--repl") == 0) return MODE_REPL;
    if (argc < 2) return MODE_HELP;
    if (argc >= 3 && strcmp(argv[1], "--eval") == 0) return MODE_INLINE_CODE;

    // Check if first argument is a file (not an option)
    if (argv[1][0] != '-') {
        // Count source files (non-option arguments before --)
        int source_count = 0;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--") == 0) break;
            if (argv[i][0] != '-') source_count++;
        }

        // Use COMPILE_AND_RUN mode for multiple files, DIRECT_IMPORT for single file
        if (source_count > 1) {
            return MODE_COMPILE_AND_RUN;
        }
        return MODE_DIRECT_IMPORT;
    }

    return MODE_COMPILE_AND_RUN;
}

/**
 * 显示帮助信息
 */
static void show_help(const char *program_name) {
    printf("cosmorun - dynamic C module loader\n");
    printf("Usage: %s <program.c> [args...]\n", program_name);
    printf("       %s --eval 'C code'\n", program_name);
    printf("       %s --repl (interactive REPL)\n", program_name);
}

/**
 * 直接导入并执行 C 模块
 */
static int execute_direct_import(int argc, char **argv) {
    extern char** environ;

    void* module = cosmo_import(argv[1]);
    if (!module) {
        fprintf(stderr, "Failed to import: %s\n", argv[1]);
        return 1;
    }

    typedef int (*main_fn_t)(int, char**, char**);
    main_fn_t main_fn = (main_fn_t)cosmo_import_sym(module, "main");
    if (!main_fn) {
        fprintf(stderr, "Symbol 'main' not found in %s\n", argv[1]);
        cosmo_import_free(module);
        return 1;
    }

    int ret = main_fn(argc, argv, environ);
    cosmo_import_free(module);
    return ret;
}

int main(int argc, char **argv) {

//    printf("Platform detection:\n");
//    printf("  IsLinux: %d\n", IsLinux());
//    printf("  IsWindows: %d\n", IsWindows());
//    //NOTES: apple failed update static var for now.  [tinycc-bugs]
//    printf("  IsXnu: %d\n", IsXnu());
//    printf("  IsXnuSilicon: %d\n", IsXnuSilicon());
//    printf("  IsOpenbsd: %d\n", IsOpenbsd());

    // 初始化配置系统
    cosmorun_result_t config_result = init_config();
    if (config_result != COSMORUN_SUCCESS) {
        cosmorun_perror(config_result, "configuration initialization");
        return 1;
    }
    // 初始化崩溃处理系统
    // Skip on macOS due to data segment write protection issues
    if (!IsXnu()) {
        init_crash_handler();
        set_crash_context("main", "main", __LINE__);
    }

    // 初始化符号缓存哈希
    // init_symbol_cache_hashes();

    execution_mode_t mode = parse_execution_mode(argc, argv);

    switch (mode) {
        case MODE_HELP:
            show_help(argv[0]);
            return 1;

        case MODE_REPL:
            return repl_mode();

        case MODE_DIRECT_IMPORT:
            return execute_direct_import(argc, argv);

        case MODE_INLINE_CODE:
        case MODE_COMPILE_AND_RUN:
            break;  // 继续到下面的复杂编译逻辑
    }

    return execute_tcc_compilation_auto(argc, argv);  // 使用自动清理版本
}


/**
 * 解析 TCC 相关的命令行参数
 */
static parse_result_t parse_tcc_arguments(int argc, char **argv, TCCState *s) {
    parse_result_t result = {0, NULL, -1, -1, -1, NULL, 0};

    // Pre-allocate array for potential source files
    int *temp_indices = calloc((size_t)argc, sizeof(int));
    if (!temp_indices) {
        result.inline_code_index = -2;  // Error flag
        return result;
    }

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--") == 0) {
            result.dashdash_index = i + 1;
            break;
        }

        if (strcmp(arg, "--eval") == 0) {
            if (result.inline_mode) {
                fprintf(stderr, "cosmorun: multiple --eval options not supported\n");
                result.inline_code_index = -2;  // Error flag
                free(temp_indices);
                return result;
            }
            if (i + 1 >= argc) {
                fprintf(stderr, "cosmorun: --eval requires an argument\n");
                result.inline_code_index = -2;  // Error flag
                free(temp_indices);
                return result;
            }
            result.inline_mode = 1;
            result.inline_code = argv[i + 1];
            result.inline_code_index = i + 1;
            ++i;  // Skip inline code value in scan
            continue;
        }

        // Collect all source files (anything that's not an option)
        if (!result.inline_mode && (arg[0] != '-' || strcmp(arg, "-") == 0)) {
            temp_indices[result.source_count++] = i;
            if (result.source_index == -1) {
                result.source_index = i;  // Keep first for compatibility
            }
        }
    }

    // Allocate exact-size array and copy indices
    if (result.source_count > 0) {
        result.source_indices = malloc((size_t)result.source_count * sizeof(int));
        if (result.source_indices) {
            memcpy(result.source_indices, temp_indices, (size_t)result.source_count * sizeof(int));
        }
    }
    free(temp_indices);

    return result;
}

/**
 * 构建编译参数数组
 */
static char** build_compile_argv(int argc, char **argv, const parse_result_t *parsed) {
    char **compile_argv = calloc((size_t)argc + 1, sizeof(char*));
    if (!compile_argv) {
        perror("calloc");
        return NULL;
    }

    int compile_argc = 0;
    compile_argv[compile_argc++] = argv[0];

    for (int i = 1; i < argc; ++i) {
        // 跳过 -- 分隔符及其后的参数
        if (parsed->dashdash_index >= 0 && i >= parsed->dashdash_index - 1) {
            if (i == parsed->dashdash_index - 1) continue;  // skip "--"
            break;
        }

        // 跳过内联代码参数
        if (parsed->inline_mode && parsed->inline_code_index >= 0 &&
            (i == parsed->inline_code_index || i == parsed->inline_code_index - 1)) {
            continue;
        }

        // 处理源文件参数 - check if this index is a source file
        if (!parsed->inline_mode && parsed->source_count > 0) {
            int is_source = 0;
            for (int j = 0; j < parsed->source_count; j++) {
                if (i == parsed->source_indices[j]) {
                    is_source = 1;
                    break;
                }
            }
            if (is_source) {
                compile_argv[compile_argc++] = argv[i];
                continue;  // Don't add it again below
            }
            // Skip arguments after first source file that aren't other source files
            if (i >= parsed->source_index) {
                continue;
            }
        }

        // 跳过内联代码后的运行时参数
        if (parsed->inline_mode && parsed->inline_code_index != -1 && i > parsed->inline_code_index) {
            continue;
        }

        compile_argv[compile_argc++] = argv[i];
    }

    compile_argv[compile_argc] = NULL;
    return compile_argv;
}

/**
 * 解析并应用 TCC 参数
 */
static int parse_and_apply_tcc_args(TCCState *s, char **compile_argv) {
    // 计算参数数量
    int compile_argc = 0;
    while (compile_argv[compile_argc]) compile_argc++;

    if (compile_argc > 1) {
        int parse_argc = compile_argc;
        char **parse_argv = compile_argv;
        int parse_result = tcc_parse_args(s, &parse_argc, &parse_argv);
        if (parse_result != 0) {
            fprintf(stderr, "cosmorun: unsupported TinyCC option combination (code=%d)\n", parse_result);
            return 0;
        }
    }
    return 1;
}

/**
 * 编译源代码
 */
static int compile_source_code(TCCState *s, const parse_result_t *parsed) {
    if (parsed->inline_mode) {
        // 为内联代码自动注入 API 声明
        char* enhanced_code = inject_api_declarations(parsed->inline_code);
        if (!enhanced_code) {
            return 0;
        }

        if (g_config.trace_enabled) {
            fprintf(stderr, "[cosmorun] inline code:\n%s\n", enhanced_code);
        }

        int result = tcc_compile_string(s, enhanced_code);
        free(enhanced_code);

        if (result == -1) {
            fprintf(stderr, "Compilation failed\n");
            return 0;
        }
        return 1;
    }

    // 编译文件（标准方式，需要手动 extern 声明）
    int compiled_files = 0;

    if (g_config.trace_enabled) {
        fprintf(stderr, "[cosmorun] TCC has %d files to compile\n", s->nb_files);
        for (int i = 0; i < s->nb_files; ++i) {
            struct filespec *f = s->files[i];
            fprintf(stderr, "[cosmorun]   file[%d]: %s (type=%d)\n", i, f->name, f->type);
        }
    }

    for (int i = 0; i < s->nb_files; ++i) {
        struct filespec *f = s->files[i];
        int ret;
        if (f->type & AFF_TYPE_LIB) {
            ret = tcc_add_library(s, f->name);
        } else {
            if (g_config.trace_enabled) {
                fprintf(stderr, "[cosmorun] compiling file: %s\n", f->name);
            }
            ret = tcc_add_file(s, f->name);
            ++compiled_files;
        }
        if (ret == -1) {
            fprintf(stderr, "Failed to process input '%s'\n", f->name);
            return 0;
        }
    }

    if (compiled_files == 0) {
        fprintf(stderr, "cosmorun: no source files compiled\n");
        return 0;
    }

    return 1;
}

/**
 * 获取程序名称
 */
static const char* get_program_name(TCCState *s, const parse_result_t *parsed, int argc, char **argv) {
    if (parsed->inline_mode) {
        return "(inline)";
    }

    // 从编译的文件中查找程序名
    for (int i = 0; i < s->nb_files; ++i) {
        struct filespec *f = s->files[i];
        if (!(f->type & AFF_TYPE_LIB)) {
            return f->name;
        }
    }

    // 回退到源文件索引
    if (parsed->source_index >= 0) {
        return argv[parsed->source_index];
    }

    return argv[0];
}

/**
 * 构建执行参数数组
 */
static char** build_exec_argv(int argc, char **argv, const parse_result_t *parsed, const char *program_name, int *out_argc) {
    // 确定运行时参数的起始位置
    int runtime_start = argc;
    if (parsed->dashdash_index >= 0) {
        runtime_start = parsed->dashdash_index;
    } else if (parsed->inline_mode && parsed->inline_code_index >= 0) {
        runtime_start = parsed->inline_code_index + 1;
    } else if (parsed->source_index >= 0) {
        runtime_start = parsed->source_index + 1;
    }

    if (runtime_start > argc) runtime_start = argc;

    // 跳过多余的 "--" 分隔符
    while (runtime_start < argc && strcmp(argv[runtime_start], "--") == 0) {
        ++runtime_start;
    }

    int user_argc = (runtime_start < argc) ? argc - runtime_start : 0;
    char **exec_argv = malloc(sizeof(char*) * (size_t)(user_argc + 2));
    if (!exec_argv) {
        perror("malloc");
        return NULL;
    }

    exec_argv[0] = (char*)program_name;
    int out = 1;
    for (int i = runtime_start; i < argc; ++i) {
        if (strcmp(argv[i], "--") == 0) continue;
        exec_argv[out++] = argv[i];
    }
    exec_argv[out] = NULL;

    *out_argc = out;
    return exec_argv;
}

/**
 * 执行编译后的程序
 */
static int execute_compiled_program(TCCState *s, int argc, char **argv, const parse_result_t *parsed) {
    const char *program_name = get_program_name(s, parsed, argc, argv);

    int exec_argc;
    char **exec_argv = build_exec_argv(argc, argv, parsed, program_name, &exec_argc);
    if (!exec_argv) {
        return 1;
    }

    // 重定位代码
    int reloc_result = tcc_relocate(s);
    if (reloc_result < 0) {
        fprintf(stderr, "Could not relocate code (error: %d)\n", reloc_result);
        free(exec_argv);
        return 1;
    }

    // 获取 main 函数
    int (*func)(int, char**) = (int (*)(int, char**))tcc_get_symbol(s, "main");
    if (!func) {
        fprintf(stderr, "Could not find main function\n");
        free(exec_argv);
        return 1;
    }

    // 执行程序（带崩溃保护）
    set_crash_context(program_name, "user_main", 0);
    g_crash_context.current_tcc_state = s;

    int ret;
    if (setjmp(g_crash_context.crash_recovery) == 0) {
        g_crash_context.crash_recovery_active = 1;
        ret = func(exec_argc, exec_argv);
        g_crash_context.crash_recovery_active = 0;
    } else {
        // 从崩溃中恢复
        fprintf(stderr, "Program crashed but recovered gracefully.\n");
        ret = 1;
        g_crash_context.crash_recovery_active = 0;
    }

    free(exec_argv);
    return ret;
}

/**
 * 使用自动清理的 TCC 编译执行版本（GCC/Clang 专用）
 *
 * 这个版本展示了如何使用 cleanup 属性来自动管理资源，
 * 避免重复的清理代码
 */
static int execute_tcc_compilation_auto(int argc, char **argv) {
    AUTO_TCC_STATE(s);
    AUTO_CHAR_ARRAY(compile_argv);

    s = cosmo_tcc_init_state();
    if (!s) return 1;

    parse_result_t parsed = parse_tcc_arguments(argc, argv, s);
    if (parsed.inline_code_index == -2) {
        if (parsed.source_indices) free(parsed.source_indices);
        return 1;
    }

    compile_argv = build_compile_argv(argc, argv, &parsed);
    if (!compile_argv) {
        if (parsed.source_indices) free(parsed.source_indices);
        return 1;
    }

    // 验证输入
    if (!parsed.inline_mode && parsed.source_index == -1) {
        fprintf(stderr, "cosmorun: no input file provided\n");
        if (parsed.source_indices) free(parsed.source_indices);
        return 1;
    }

    // 解析编译参数
    if (!parse_and_apply_tcc_args(s, compile_argv)) {
        if (parsed.source_indices) free(parsed.source_indices);
        return 1;
    }

    // 编译源代码
    if (!compile_source_code(s, &parsed)) {
        if (parsed.source_indices) free(parsed.source_indices);
        return 1;
    }

    // 构建运行时参数并执行
    int result = execute_compiled_program(s, argc, argv, &parsed);
    if (parsed.source_indices) free(parsed.source_indices);
    return result;
}
