// @cosmorun.md @cosmorun_build.sh
#ifndef _COSMO_SOURCE
#define _COSMO_SOURCE
#endif
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <sched.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <dlfcn.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "libtcc.h"
#include "tcc.h"
#include "config.h"

#include "libc/dce.h"
#include "libc/log/backtrace.internal.h"
#include "libc/nt/memory.h"
#include "libc/nt/enum/pageflags.h"

/* macOS ARM64 JIT support - export pthread_jit_write_protect_np for TCC weak reference */
#ifdef __APPLE__
extern void pthread_jit_write_protect_np(int enabled);
#endif

// #if defined(_WIN32) || defined(_WIN64)
// #define cosmorun_unlink _unlink
// #else
// #define cosmorun_unlink unlink
// #endif

#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup

// 增强的符号缓存条目（支持哈希优化）
typedef struct {
    const char* name;
    void* address;
    int is_cached;
    uint32_t hash;  // 预计算哈希值，用于快速查找
} SymbolEntry;

static void register_builtin_symbols(TCCState *s);
static void *cosmorun_dlsym(void *handle, const char *symbol);

/**
 * ============================================================================
 * 统一错误处理系统
 * ============================================================================
 */

// 统一错误类型枚举
typedef enum {
    COSMORUN_SUCCESS = 0,
    COSMORUN_ERROR_MEMORY,
    COSMORUN_ERROR_TCC_INIT,
    COSMORUN_ERROR_COMPILATION,
    COSMORUN_ERROR_SYMBOL_NOT_FOUND,
    COSMORUN_ERROR_FILE_NOT_FOUND,
    COSMORUN_ERROR_INVALID_ARGUMENT,
    COSMORUN_ERROR_PLATFORM,
    COSMORUN_ERROR_CONFIG
} cosmorun_result_t;

// 统一错误报告函数
static void cosmorun_perror(cosmorun_result_t result, const char* context) {
    const char* error_msg;
    switch (result) {
        case COSMORUN_SUCCESS: return;
        case COSMORUN_ERROR_MEMORY: error_msg = "Memory allocation failed"; break;
        case COSMORUN_ERROR_TCC_INIT: error_msg = "TinyCC initialization failed"; break;
        case COSMORUN_ERROR_COMPILATION: error_msg = "Compilation failed"; break;
        case COSMORUN_ERROR_SYMBOL_NOT_FOUND: error_msg = "Symbol not found"; break;
        case COSMORUN_ERROR_FILE_NOT_FOUND: error_msg = "File not found"; break;
        case COSMORUN_ERROR_INVALID_ARGUMENT: error_msg = "Invalid argument"; break;
        case COSMORUN_ERROR_PLATFORM: error_msg = "Platform operation failed"; break;
        case COSMORUN_ERROR_CONFIG: error_msg = "Configuration error"; break;
        default: error_msg = "Unknown error"; break;
    }

    if (context) {
        fprintf(stderr, "cosmorun: %s: %s\n", context, error_msg);
    } else {
        fprintf(stderr, "cosmorun: %s\n", error_msg);
    }
}

// 参数解析结果结构体
typedef struct {
    int inline_mode;
    const char *inline_code;
    int inline_code_index;
    int dashdash_index;
    int source_index;
} parse_result_t;

/**
 * ============================================================================
 * 平台抽象层
 * ============================================================================
 */

// 跨平台操作接口
typedef struct {
    void* (*dlopen)(const char* path, int flags);
    void* (*dlsym)(void* handle, const char* symbol);
    int (*dlclose)(void* handle);
    const char* (*dlerror)(void);
    const char* (*get_path_separator)(void);
} platform_ops_t;

// 平台特定实现
#ifdef _WIN32
static const char* win32_get_path_separator(void) { return ";"; }
static const char* win32_dlerror(void) { return "Windows error"; }
static platform_ops_t g_platform_ops = {
    .dlopen = (void*(*)(const char*, int))LoadLibraryA,
    .dlsym = (void*(*)(void*, const char*))GetProcAddress,
    .dlclose = (int(*)(void*))FreeLibrary,
    .dlerror = win32_dlerror,
    .get_path_separator = win32_get_path_separator
};
#else
static const char* posix_get_path_separator(void) { return ":"; }
static platform_ops_t g_platform_ops = {
    .dlopen = dlopen,
    .dlsym = dlsym,
    .dlclose = dlclose,
    .dlerror = (const char*(*)(void))dlerror,
    .get_path_separator = posix_get_path_separator
};
#endif

/**
 * ============================================================================
 * Windows calling-convention bridge helpers
 * ============================================================================
 */

#if defined(__x86_64__)
extern void __sysv2nt14(void);

typedef struct {
    void *orig;
    void *stub;
} WinThunkEntry;

#define COSMORUN_MAX_WIN_THUNKS 256

static WinThunkEntry g_win_thunks[COSMORUN_MAX_WIN_THUNKS];
static size_t g_win_thunk_count = 0;
static atomic_flag g_win_thunk_lock = ATOMIC_FLAG_INIT;
static void *g_win_host_module = NULL;

static inline void win_thunk_lock(void) {
    while (atomic_flag_test_and_set(&g_win_thunk_lock)) {
        sched_yield();
    }
}

static inline void win_thunk_unlock(void) {
    atomic_flag_clear(&g_win_thunk_lock);
}

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

static inline void cosmorun_set_host_module(void *module) {
    g_win_host_module = module;
}

static void *wrap_windows_symbol(void *module, void *addr) {
    if (!addr) return NULL;
    if (!IsWindows()) return addr;
    if (!module || module == g_win_host_module) return addr;
    if (!windows_address_is_executable(addr)) return addr;

    win_thunk_lock();
    for (size_t i = 0; i < g_win_thunk_count; ++i) {
        if (g_win_thunks[i].orig == addr) {
            void *stub = g_win_thunks[i].stub;
            win_thunk_unlock();
            return stub ? stub : addr;
        }
    }

    void *stub = windows_make_trampoline(addr);
    if (stub && g_win_thunk_count < COSMORUN_MAX_WIN_THUNKS) {
        g_win_thunks[g_win_thunk_count].orig = addr;
        g_win_thunks[g_win_thunk_count].stub = stub;
        ++g_win_thunk_count;
    }
    win_thunk_unlock();
    return stub ? stub : addr;
}

#else
static inline void cosmorun_set_host_module(void *module) {
    (void)module;
}

static inline void *wrap_windows_symbol(void *module, void *addr) {
    (void)module;
    return addr;
}
#endif

/**
 * ============================================================================
 * 常量定义 - 消除魔法数字和字符串
 * ============================================================================
 */

// 缓冲区大小常量
#define COSMORUN_MAX_CODE_SIZE      98304   // 96KB for large programs
#define COSMORUN_MAX_OPTIONS_SIZE   512     // TCC options buffer
#define COSMORUN_MAX_PATH_SIZE      PATH_MAX
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

static cosmorun_config_t g_config = {0};

// 初始化配置系统
static cosmorun_result_t init_config(void) {
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
static inline uint32_t hash_string(const char* str) {
    if (!str) return 0;

    uint32_t hash = COSMORUN_HASH_SEED;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash;
}

// 带长度限制的哈希函数，避免过长字符串的性能问题
static inline uint32_t hash_string_bounded(const char* str, size_t max_len) {
    if (!str) return 0;

    uint32_t hash = COSMORUN_HASH_SEED;
    size_t len = 0;
    int c;
    while ((c = *str++) && len < max_len) {
        hash = ((hash << 5) + hash) + c;
        len++;
    }
    return hash;
}

// // 初始化符号缓存的哈希值
// static void init_symbol_cache_hashes(void) {
//     // 暂时跳过哈希初始化，因为 builtin_symbol_table 是 const
//     // 在实际使用时动态计算哈希值
// }



/**
 * ============================================================================
 * 自动声明注入系统
 * ============================================================================
 */

// 标准的 cosmorun API 声明
static const char* COSMORUN_API_DECLARATIONS =
    "// Auto-injected cosmorun API declarations\n"
    "extern void* __import(const char* path);\n"
    "extern void* __sym(void* module, const char* symbol);\n"
    // "extern void* cosmo_import(const char* path);\n"
    // "extern void* cosmo_import_sym(void* module, const char* symbol);\n"
    // "extern void cosmo_import_free(void* module);\n"
    "\n";

/**
 * ============================================================================
 * 输入验证和边界检查 - 提升完备性
 * ============================================================================
 */

// 验证字符串参数
static inline int validate_string_param(const char* str, const char* param_name, size_t max_len) {
    if (!str) {
        cosmorun_perror(COSMORUN_ERROR_INVALID_ARGUMENT, param_name);
        return 0;
    }

    size_t len = strlen(str);
    if (len == 0) {
        if (g_config.trace_enabled) {
            fprintf(stderr, "[cosmorun] Warning: empty %s\n", param_name);
        }
        return 0;
    }

    if (len > max_len) {
        fprintf(stderr, "cosmorun: %s too long (%zu > %zu)\n", param_name, len, max_len);
        return 0;
    }

    return 1;
}

// 验证文件路径
static inline int validate_file_path(const char* path) {
    if (!validate_string_param(path, "file path", COSMORUN_MAX_PATH_SIZE)) {
        return 0;
    }

    // 检查危险路径模式
    if (strstr(path, "..") || strstr(path, "//")) {
        fprintf(stderr, "cosmorun: potentially unsafe path: %s\n", path);
        return 0;
    }

    return 1;
}

// 为 --eval 和 REPL 模式添加 API 声明（带完整验证）
static char* inject_api_declarations(const char* user_code) {
    if (!validate_string_param(user_code, "user code", COSMORUN_MAX_CODE_SIZE)) {  // 96KB
        return NULL;
    }

    size_t decl_len = strlen(COSMORUN_API_DECLARATIONS);
    size_t user_len = strlen(user_code);
    size_t total_len = decl_len + user_len + 1;

    // 检查总长度是否超限
    if (total_len > COSMORUN_MAX_CODE_SIZE) {  // 96KB
        fprintf(stderr, "cosmorun: combined code size too large (%zu > %d)\n",
                total_len, COSMORUN_MAX_CODE_SIZE);
        return NULL;
    }

    char* enhanced_code = malloc(total_len);
    if (!enhanced_code) {
        cosmorun_perror(COSMORUN_ERROR_MEMORY, "inject_api_declarations");
        return NULL;
    }

    strncpy(enhanced_code, COSMORUN_API_DECLARATIONS, total_len - 1);
    enhanced_code[decl_len] = '\0';
    strncat(enhanced_code, user_code, total_len - decl_len - 1);

    if (g_config.trace_enabled) {
        fprintf(stderr, "[cosmorun] Injected API declarations for --eval/REPL code (%zu bytes)\n", total_len);
    }

    return enhanced_code;
}

// 读取文件内容并注入 API 声明
static char* read_file_with_api_declarations(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        cosmorun_perror(COSMORUN_ERROR_FILE_NOT_FOUND, filename);
        return NULL;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(file);
        cosmorun_perror(COSMORUN_ERROR_FILE_NOT_FOUND, "file size detection");
        return NULL;
    }

    // 分配内存：API声明 + 文件内容 + null terminator
    size_t decl_len = strlen(COSMORUN_API_DECLARATIONS);
    size_t total_len = decl_len + file_size + 1;

    char* enhanced_code = malloc(total_len);
    if (!enhanced_code) {
        fclose(file);
        cosmorun_perror(COSMORUN_ERROR_MEMORY, "read_file_with_api_declarations");
        return NULL;
    }

    // 先复制 API 声明
    strncpy(enhanced_code, COSMORUN_API_DECLARATIONS, decl_len);
    enhanced_code[decl_len] = '\0';

    // 然后读取文件内容
    size_t bytes_read = fread(enhanced_code + decl_len, 1, file_size, file);
    fclose(file);

    if (bytes_read != (size_t)file_size) {
        free(enhanced_code);
        cosmorun_perror(COSMORUN_ERROR_FILE_NOT_FOUND, "file read error");
        return NULL;
    }

    enhanced_code[decl_len + file_size] = '\0';

    if (g_config.trace_enabled) {
        fprintf(stderr, "[cosmorun] Injected API declarations for file: %s\n", filename);
    }

    return enhanced_code;
}

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
static void register_default_include_paths(TCCState *s, const struct utsname *uts);
static void register_default_library_paths(TCCState *s);

// Forward declarations for Cosmopolitan-style dlopen helpers
extern void *cosmo_dlopen(const char *filename, int flags);
extern void *cosmo_dlsym(void *handle, const char *symbol);
extern int cosmo_dlclose(void *handle);
extern char *cosmo_dlerror(void);
static void tracef(const char *fmt, ...);
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
static TCCState* create_tcc_state_with_config(int output_type, const char* options, int enable_paths, int enable_resolver) {
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

    return s;
}

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
static TCCState* init_tcc_state(void);

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

    ctx->tcc_state = init_tcc_state();
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

static void register_default_include_paths(TCCState *s, const struct utsname *uts);
static void register_default_library_paths(TCCState *s);

void* cosmo_import(const char* path);
void* cosmo_import_sym(void* module, const char* symbol);
void cosmo_import_free(void* module);

// Level 1: Builtin symbol table (O(1) lookup)
static const SymbolEntry builtin_symbol_table[] = {
    // I/O functions (most frequently used)
    {"printf", (void*)printf},

    // Memory management (critical for most programs)
    {"malloc", (void*)malloc},
    {"calloc", (void*)calloc},
    {"realloc", (void*)realloc},
    {"free", (void*)free},

    // String functions (very common in C code)
    {"strlen", (void*)strlen},
    {"strcmp", (void*)strcmp},
    {"strncmp", (void*)strncmp},

    // Memory functions (performance critical)
    {"memcpy", (void*)memcpy},
    {"memset", (void*)memset},
    {"memmove", (void*)memmove},

    // Math functions (commonly used)
    {"abs", (void*)abs},
    {"labs", (void*)labs},
    {"sin", (void*)sin},
    {"cos", (void*)cos},
    {"sqrt", (void*)sqrt},

    // Cosmopolitan dynamic loading helpers (allow modules to call cosmo_* directly)
    {"dlopen", (void*)cosmo_dlopen_ext},
    {"dlsym", (void*)cosmorun_dlsym},
    {"dlclose", (void*)cosmo_dlclose},
    {"dlerror", (void*)cosmo_dlerror},
//for cosmo dev/test
{"cosmo_dlopen", (void*)cosmo_dlopen},
{"cosmo_dlsym", (void*)cosmorun_dlsym},
{"cosmo_dlclose", (void*)cosmo_dlclose},
{"cosmo_dlerror", (void*)cosmo_dlerror},

    // POSIX-style dynamic loading functions (mostly stubs under Cosmopolitan)
    // {"dlopen", (void*)dlopen},
    // {"dlsym", (void*)dlsym},
    // {"dlclose", (void*)dlclose},
    // {"dlerror", (void*)dlerror},
    // Note: cosmo_* functions are declared externally when available

    // File I/O functions (essential for cross-platform compatibility)
    {"fopen", (void*)fopen},
    {"fclose", (void*)fclose},
    {"fread", (void*)fread},
    {"fwrite", (void*)fwrite},
    {"fseek", (void*)fseek},
    {"ftell", (void*)ftell},
    {"fgets", (void*)fgets},
    {"fputs", (void*)fputs},
    {"fflush", (void*)fflush},

    // POSIX-style file descriptors
    {"open", (void*)open},
    {"read", (void*)read},
    {"write", (void*)write},
    {"close", (void*)close},
    {"unlink", (void*)unlink},
    {"access", (void*)access},
    {"fork", (void*)fork},
    {"waitpid", (void*)waitpid},
    {"_exit", (void*)_exit},
    {"execve", (void*)execve},

    // Program control
    {"exit", (void*)exit},
    {"abort", (void*)abort},

    // Dynamic module loading API
    // {"cosmo_import", (void*)cosmo_import},
    // {"cosmo_import_sym", (void*)cosmo_import_sym},
    // {"cosmo_import_free", (void*)cosmo_import_free},
    {"__import", (void*)cosmo_import},//./cosmorun.exe -e 'int main(){void* h=__import("std.c");printf("h=%d\n",h);}'
    {"__sym", (void*)cosmo_import_sym},
    {"__import_free", (void*)cosmo_import_free},

    // TCC functions (for nested TCC usage)
    {"tcc_new", (void*)tcc_new},
    {"tcc_delete", (void*)tcc_delete},
    {"tcc_set_error_func", (void*)tcc_set_error_func},
    {"tcc_set_output_type", (void*)tcc_set_output_type},
    {"tcc_add_include_path", (void*)tcc_add_include_path},
    {"tcc_add_sysinclude_path", (void*)tcc_add_sysinclude_path},
    {"tcc_add_library_path", (void*)tcc_add_library_path},
    {"tcc_add_library", (void*)tcc_add_library},
    {"tcc_add_symbol", (void*)tcc_add_symbol},
    {"tcc_compile_string", (void*)tcc_compile_string},
    {"tcc_add_file", (void*)tcc_add_file},
    {"tcc_relocate", (void*)tcc_relocate},
    {"tcc_get_symbol", (void*)tcc_get_symbol},
    {"tcc_set_options", (void*)tcc_set_options},
    {"tcc_output_file", (void*)tcc_output_file},

    {NULL, NULL}  // Sentinel - must be last
};

static void register_builtin_symbols(TCCState *s) {
    if (!s) {
        return;
    }

    for (const SymbolEntry *entry = builtin_symbol_table; entry->name; ++entry) {
        if (!entry->address) {
            continue;
        }
        tracef("registering symbol: %s (symtab=%p)", entry->name, (void*)s->symtab);
        if (!s->symtab) {
            tracef("symtab is NULL before adding symbol %s", entry->name);
        }
        if (tcc_add_symbol(s, entry->name, entry->address) != 0) {
            tracef("register_builtin_symbols: failed for %s", entry->name);
        }
    }
}

// // 哈希优化的符号查找
// static void* find_symbol_by_hash(const char* name, uint32_t hash) {
//     // 在内置符号表中查找
//     for (size_t i = 0; builtin_symbol_table[i].name; i++) {
//         // 动态计算哈希值进行比较
//         if (hash_string(builtin_symbol_table[i].name) == hash &&
//             strcmp(builtin_symbol_table[i].name, name) == 0) {
//             return builtin_symbol_table[i].address;
//         }
//     }
//     return NULL;
// }

static int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return 0;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static int str_iequals(const char *a, const char *b) {
    if (!a || !b) return 0;
    return strcasecmp(a, b) == 0;
}

static int str_istartswith(const char *str, const char *prefix) {
    if (!str || !prefix) return 0;
    size_t prefix_len = strlen(prefix);
    return strncasecmp(str, prefix, prefix_len) == 0;
}

static void append_tcc_option(char *buffer, size_t size, const char *opt) {
    if (!buffer || size == 0 || !opt || !*opt) return;

    size_t len = strlen(buffer);
    if (len >= size - 1) return;

    const char *prefix = len > 0 ? " " : "";
    snprintf(buffer + len, size - len, "%s%s", prefix, opt);
}

static void build_default_tcc_options(char *buffer, size_t size, const struct utsname *uts) {
    if (!buffer || size == 0) return;

    buffer[0] = '\0';
    append_tcc_option(buffer, size, "-nostdlib");
    append_tcc_option(buffer, size, "-nostdinc");
    append_tcc_option(buffer, size, "-D__COSMORUN__");

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
        append_tcc_option(buffer, size, "-D_WIN32");
        append_tcc_option(buffer, size, "-DWIN32");
        append_tcc_option(buffer, size, "-D_WINDOWS");
    } else if (is_macos) {
        append_tcc_option(buffer, size, "-D__APPLE__");
        append_tcc_option(buffer, size, "-D__MACH__");
    } else {
        append_tcc_option(buffer, size, "-D__unix__");
        if (is_linux) {
            append_tcc_option(buffer, size, "-D__linux__");
        }
    }
}

static int trace_enabled(void) {
    static int cached = -1;
    if (cached >= 0) return cached;
    const char *env = getenv("COSMORUN_TRACE");
    if (!env || !*env) {
        cached = 0;
    } else if (isdigit((unsigned char)env[0])) {
        cached = atoi(env) > 0;
    } else {
        cached = 1;
    }
    return cached;
}

static void tracef(const char *fmt, ...) {
    if (!trace_enabled()) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[cosmorun] ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static int dir_exists(const char *path) {
    if (!path || !*path) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

static void host_api_log_default(const char *message) {
    if (!message) message = "";
    fprintf(stderr, "[cosmorun-host] %s\n", message);
}

static int host_api_puts_default(const char *message) {
    if (!message) message = "";
    int rc = fputs(message, stdout);
    if (rc < 0) return rc;
    fputc('\n', stdout);
    fflush(stdout);
    return 0;
}

static int host_api_write_default(const char *data, size_t length) {
    if (!data) return -1;
    if (length == 0) length = strlen(data);
    size_t written = fwrite(data, 1, length, stdout);
    fflush(stdout);
    return written == length ? (int)written : -1;
}

static const char *host_api_getenv_default(const char *name) {
    if (!name || !*name) return NULL;
    return getenv(name);
}

// Initialize dynamic symbol resolver
static void add_library_handle(void *handle, const char *label) {
    if (!handle) return;
#if defined(__x86_64__)
    if (!g_win_host_module && label && strcmp(label, "self") == 0) {
        cosmorun_set_host_module(handle);
    }
#endif
    for (int i = 0; i < g_resolver.handle_count; ++i) {
        if (g_resolver.handles[i] == handle) {
            return;
        }
    }
    if (g_resolver.handle_count >= MAX_LIBRARY_HANDLES) {
        tracef("symbol resolver handle limit reached, skipping %s", label ? label : "(unknown)");
        return;
    }
    g_resolver.handles[g_resolver.handle_count++] = handle;
    tracef("registered host library handle %s -> %p", label ? label : "(unnamed)", handle);
}

static void *cosmorun_dlsym(void *handle, const char *symbol) {
    void *addr = cosmo_dlsym(handle, symbol);
    return wrap_windows_symbol(handle, addr);
}

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

static void register_default_include_paths(TCCState *s, const struct utsname *uts) {
    const char *sysname = uts && uts->sysname[0] ? uts->sysname : "";

    // 标准系统包含路径（移除 Cosmopolitan 特定路径）
    const char *posix_candidates[] = {
        "/usr/local/include",
        "/usr/include",
        "/opt/local/include",
        NULL
    };
    const char *mac_candidates[] = {
        "/opt/homebrew/include",
        "/usr/local/include",
        "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include",
        "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include",
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

static void register_default_library_paths(TCCState *s) {
    register_env_paths(s, "COSMORUN_LIBRARY_PATHS", 0);
}

// //@hack tcc at tccelf.c: addr = cosmorun_resolve_symbol(name_ud);
void* cosmorun_resolve_symbol(const char* symbol_name) {
    if (!symbol_name || !*symbol_name) {
        return NULL;
    }
    tracef("cosmorun_resolve_symbol: %s", symbol_name);
    return NULL;

//     // Fast path: check the builtin table first.
//     for (const SymbolEntry *entry = builtin_symbol_table; entry->name; ++entry) {
//         if (strcmp(entry->name, symbol_name) == 0) {
//             return entry->address;
//         }
//     }

// #if defined(_WIN32) || defined(_WIN64)
//     tracef("cosmorun_resolve_symbol fallback (win): %s", symbol_name);
//     // Prefer the module captured from the host process (set via cosmorun_set_host_module).
//     if (g_win_host_module) {
//         FARPROC proc = GetProcAddress((HMODULE)g_win_host_module, symbol_name);
//         if (proc) return (void*)proc;
//     }

//     // Fallback to the current process and a few common CRTs.
//     static const char *kCandidateDlls[] = {
//         NULL,              // GetModuleHandleA(NULL)
//         "ucrtbase.dll",
//         "msvcrt.dll",
//         "kernel32.dll",
//         NULL
//     };

//     for (size_t i = 0; kCandidateDlls[i] || i == 0; ++i) {
//         HMODULE mod;
//         if (kCandidateDlls[i]) {
//             mod = GetModuleHandleA(kCandidateDlls[i]);
//             if (!mod) mod = LoadLibraryA(kCandidateDlls[i]);
//         } else {
//             mod = GetModuleHandleA(NULL);
//         }
//         if (!mod) continue;

//         FARPROC proc = GetProcAddress(mod, symbol_name);
//         if (proc) return (void*)proc;
//     }

//     return NULL;
// #else
//     tracef("cosmorun_resolve_symbol fallback (posix): %s", symbol_name);
//     // POSIX-style fallback: try Cosmopolitan's resolver first, then dlsym.
//     void *addr = cosmorun_dlsym(NULL, symbol_name);
//     if (addr) {
//         return addr;
//     }

//     addr = dlsym(RTLD_DEFAULT, symbol_name);
//     if (addr) {
//         return addr;
//     }

//     return NULL;
// #endif
}

static void tcc_error_func(void *opaque, const char *msg) {
    (void)opaque;
    if (strstr(msg, "warning: implicit declaration")) return;
    if (strstr(msg, "warning:")) return;
    fprintf(stderr, "TCC Error: %s\n", msg);
}

// 改进的缓存检查：支持开发模式和发布模式
static int check_o_cache(const char* src_path, char* cb_path, size_t cb_path_size) {
    if (!src_path || !cb_path) return 0;

    size_t len = strlen(src_path);
    if (len < 3 || !ends_with(src_path, ".c")) return 0;

    struct utsname uts;
    if (uname(&uts) != 0) return 0;

    // Generate arch-specific .o name: file.c -> file.{arch}.o
    snprintf(cb_path, cb_path_size, "%.*s.%s.o", (int)(len - 2), src_path, uts.machine);

    struct stat src_st, cb_st;
    int src_exists = (stat(src_path, &src_st) == 0);
    int cb_exists = (stat(cb_path, &cb_st) == 0);

    if (!cb_exists) {
        // 没有缓存文件
        if (g_config.trace_enabled) {
            fprintf(stderr, "[cosmorun] No cache file: %s\n", cb_path);
        }
        return 0;
    }

    if (!src_exists) {
        // 发布模式：只有 .o 文件，没有源文件
        if (g_config.trace_enabled) {
            fprintf(stderr, "[cosmorun] Release mode: using .o file without source: %s\n", cb_path);
        }
        return 1;  // 直接使用 .o 文件
    }

    // 开发模式：源文件和 .o 文件都存在，比对时间
    int cache_is_newer = (cb_st.st_mtime >= src_st.st_mtime);
    if (g_config.trace_enabled) {
        fprintf(stderr, "[cosmorun] Development mode: source=%ld, cache=%ld, %s\n",
                src_st.st_mtime, cb_st.st_mtime,
                cache_is_newer ? "using cache" : "cache outdated");
    }

    return cache_is_newer;
}

static void* load_o_file(const char* path) {
    tracef("loading precompiled '%s'", path);

    TCCState* s = tcc_new();
    if (!s) {
        tracef("tcc_new failed");
        return NULL;
    }

    tcc_set_error_func(s, NULL, tcc_error_func);
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

    struct utsname uts;
    memset(&uts, 0, sizeof(uts));
    uname(&uts);

    char tcc_options[COSMORUN_MAX_OPTIONS_SIZE] = {0};
    build_default_tcc_options(tcc_options, sizeof(tcc_options), &uts);
    if (tcc_options[0]) {
        tcc_set_options(s, tcc_options);
    }

    register_default_include_paths(s, &uts);
    register_default_library_paths(s);
    // init_symbol_resolver();
    register_builtin_symbols(s);

    if (tcc_add_file(s, path) == -1) {
        tracef("failed to load .o '%s'", path);
        tcc_delete(s);
        return NULL;
    }

    if (tcc_relocate(s) < 0) {
        tracef("tcc_relocate failed for .o '%s'", path);
        tcc_delete(s);
        return NULL;
    }

    tracef("successfully loaded .o '%s' -> %p", path, s);
    return (void*)s;
}

static int save_o_cache(const char* src_path, TCCState* s) {
    if (!src_path || !s) return -1;

    char cb_path[PATH_MAX];
    size_t len = strlen(src_path);
    if (len < 3 || !ends_with(src_path, ".c")) return -1;

    struct utsname uts;
    if (uname(&uts) != 0) return -1;

    // Generate arch-specific .o name: file.c -> file.{arch}.o
    snprintf(cb_path, sizeof(cb_path), "%.*s.%s.o", (int)(len - 2), src_path, uts.machine);

    tracef("saving cache to '%s'", cb_path);

    int old_output_type = s->output_type;
    s->output_type = TCC_OUTPUT_OBJ;

    int result = tcc_output_file(s, cb_path);

    s->output_type = old_output_type;

    if (result == 0) {
        tracef("cache saved successfully");
    } else {
        tracef("failed to save cache");
    }

    return result;
}

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

    // 临时禁用 .o 缓存，避免宿主符号污染


    // 检查源文件是否存在
    struct stat src_st;
    if (stat(path, &src_st) != 0) {
        tracef("Source file '%s' not found", path);
        return NULL;
    }

    tracef("compiling '%s'", path);

    TCCState* s = tcc_new();
    if (!s) {
        tracef("cosmo_import: tcc_new failed");
        return NULL;
    }

    tracef("cosmo_import: tcc_state=%p", s);
    tcc_set_error_func(s, NULL, tcc_error_func);
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

    // Configure environment
    struct utsname uts;
    memset(&uts, 0, sizeof(uts));
    uname(&uts);

    char tcc_options[COSMORUN_MAX_OPTIONS_SIZE] = {0};
    build_default_tcc_options(tcc_options, sizeof(tcc_options), &uts);
    if (tcc_options[0]) {
        tcc_set_options(s, tcc_options);
    }

    register_default_include_paths(s, &uts);
    register_default_library_paths(s);
    // init_symbol_resolver();
    register_builtin_symbols(s);
    tracef("cosmo_import: registered builtin symbols");

    // Compile source file (standard way, requires manual extern declarations)
    if (tcc_add_file(s, path) == -1) {
        tracef("tcc_add_file failed for '%s'", path);
        tcc_delete(s);
        return NULL;
    }

    // Try to save .o cache before relocating
    if (ends_with(path, ".c")) {
        save_o_cache(path, s);
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
    build_default_tcc_options(tcc_options, sizeof(tcc_options), &uts);
    if (tcc_options[0]) {
        tcc_set_options(s, tcc_options);
    }

    register_default_include_paths(s, &uts);
    register_default_library_paths(s);
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
                register_default_include_paths(s, &uts);
                register_default_library_paths(s);
                // init_symbol_resolver();
                register_builtin_symbols(s);

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
            register_default_include_paths(exec_state, &uts);
            register_default_library_paths(exec_state);
            // init_symbol_resolver();
            register_builtin_symbols(exec_state);

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
    if (argv[1][0] != '-') return MODE_DIRECT_IMPORT;
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
    printf("Platform detection:\n");
    printf("  IsLinux: %d\n", IsLinux());
    printf("  IsWindows: %d\n", IsWindows());
    //NOTES: apple failed update static var for now.  [tinycc-bugs]
    printf("  IsXnu: %d\n", IsXnu());
    printf("  IsXnuSilicon: %d\n", IsXnuSilicon());
    printf("  IsOpenbsd: %d\n", IsOpenbsd());
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

static TCCState* init_tcc_state(void) {
    TCCState *s = tcc_new();
    if (!s) {
        cosmorun_perror(COSMORUN_ERROR_TCC_INIT, "tcc_new");
        return NULL;
    }

    tcc_set_error_func(s, NULL, tcc_error_func);

    // 使用全局配置
    if (!g_config.initialized) {
        cosmorun_perror(COSMORUN_ERROR_CONFIG, "configuration not initialized");
        tcc_delete(s);
        return NULL;
    }

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
    return s;
}

/**
 * 解析 TCC 相关的命令行参数
 */
static parse_result_t parse_tcc_arguments(int argc, char **argv, TCCState *s) {
    parse_result_t result = {0, NULL, -1, -1, -1};

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--") == 0) {
            result.dashdash_index = i + 1;
            break;
        }

        if (strcmp(arg, "--eval") == 0) {
            if (result.inline_mode) {
                fprintf(stderr, "cosmorun: multiple --eval options not supported\n");
                result.inline_code_index = -2;  // 错误标志
                return result;
            }
            if (i + 1 >= argc) {
                fprintf(stderr, "cosmorun: --eval requires an argument\n");
                result.inline_code_index = -2;  // 错误标志
                return result;
            }
            result.inline_mode = 1;
            result.inline_code = argv[i + 1];
            result.inline_code_index = i + 1;
            ++i;  // Skip inline code value in scan
            continue;
        }

        if (!result.inline_mode && result.source_index == -1 &&
            (arg[0] != '-' || strcmp(arg, "-") == 0)) {
            result.source_index = i;
        }
    }

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

        // 处理源文件参数
        if (!parsed->inline_mode && parsed->source_index != -1) {
            if (i == parsed->source_index) {
                compile_argv[compile_argc++] = argv[i];
            }
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
    for (int i = 0; i < s->nb_files; ++i) {
        struct filespec *f = s->files[i];
        int ret;
        if (f->type & AFF_TYPE_LIB) {
            ret = tcc_add_library(s, f->name);
        } else {
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

    s = init_tcc_state();
    if (!s) return 1;

    parse_result_t parsed = parse_tcc_arguments(argc, argv, s);
    if (parsed.inline_code_index == -2) return 1;

    compile_argv = build_compile_argv(argc, argv, &parsed);
    if (!compile_argv) return 1;

    // 验证输入
    if (!parsed.inline_mode && parsed.source_index == -1) {
        fprintf(stderr, "cosmorun: no input file provided\n");
        return 1;
    }

    // 解析编译参数
    if (!parse_and_apply_tcc_args(s, compile_argv)) return 1;

    // 编译源代码
    if (!compile_source_code(s, &parsed)) return 1;

    // 构建运行时参数并执行
    return execute_compiled_program(s, argc, argv, &parsed);
}
