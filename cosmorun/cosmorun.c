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
#include "cosmo_tcc.h"
#include "cosmo_utils.h"

#define COSMORUN_VERSION "0.6.8"

// Additional constants
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

cosmorun_config_t g_config = {0};

// 初始化配置系统
cosmorun_result_t init_config(void) {
    if (g_config.initialized) return COSMORUN_SUCCESS;

    // 获取系统信息
    if (uname(&g_config.uts) != 0) {
        return COSMORUN_ERROR_PLATFORM;
    }

    // trace_enabled: 0=off, 1=normal, 2=verbose (controlled by -v/-vv flags)
    g_config.trace_enabled = 0;  // default off

    g_config.initialized = 1;
    return COSMORUN_SUCCESS;
}

typedef struct {
    int output_type;
    const char* options;
    int enable_symbol_resolver;
    int enable_default_paths;
} tcc_config_t;

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

// TCC 状态清理函数（使用GCC cleanup attribute）
static void tcc_state_cleanup(void* resource) {
    if (!resource) return;

    TCCState **state_ptr = (TCCState**)resource;
    if (state_ptr && *state_ptr) {
        tcc_delete(*state_ptr);
        *state_ptr = NULL;
    }
}

// RAII 风格的自动清理宏（GCC/Clang）
#if defined(__GNUC__) || defined(__clang__)
#define AUTO_CLEANUP(cleanup_fn) __attribute__((cleanup(cleanup_fn)))
#define AUTO_TCC_STATE TCCState* AUTO_CLEANUP(tcc_state_cleanup)
#else
#define AUTO_CLEANUP(cleanup_fn)
#define AUTO_TCC_STATE TCCState*
#endif

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
    fprintf(stderr, "- Use -vv flag for detailed execution trace\n");
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

// Note: Crash handler functions moved to cosmo_utils.c
// Use cosmo_crash_init() and cosmo_crash_set_context() instead

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

#define AUTO_CHAR_ARRAY(name) \
    char **__attribute__((cleanup(char_array_cleanup))) name = NULL

//@hack tcc at tccelf.c: addr = cosmorun_resolve_symbol(name_ud);
void* cosmorun_resolve_symbol(const char* symbol_name) {
    return cosmorun_dlsym_libc(symbol_name);
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
// Module import functions moved to cosmo_tcc.c

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
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) return MODE_HELP;
    if (argc < 2) return MODE_HELP;
    if (argc >= 3 && strcmp(argv[1], "--eval") == 0) return MODE_INLINE_CODE;

    // Check for TCC compilation flags that require COMPILE_AND_RUN mode
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "-c") == 0 ||
            strcmp(argv[i], "-E") == 0 || strcmp(argv[i], "-v") == 0 ||
            strcmp(argv[i], "-vv") == 0) {
            return MODE_COMPILE_AND_RUN;
        }
    }

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
    printf("cosmorun - Cross-platform C JIT Compiler and Dynamic Module Loader\n");
    printf("Version: "
        COSMORUN_VERSION
        " (Built with TinyCC and Cosmopolitan)\n\n");

    printf("USAGE:\n");
    printf("  %s [options] <sources> [args...]   Compile and run C source files\n", program_name);
    printf("  %s <program.c> [args...]           Direct execution (single file)\n\n", program_name);

    printf("STANDARD OPTIONS (TCC-compatible):\n");
    printf("  -o <file>         Output file (executable or object)\n");
    printf("  -c                Compile to object file only (.o)\n");
    printf("  -E                Preprocess only (output to stdout or -o file)\n");
    printf("  -v                Verbose mode (show paths and configuration)\n");
    printf("  -vv               Extra verbose mode (include builtin symbols)\n");
    printf("  -I <path>         Add include path\n");
    printf("  -L <path>         Add library path\n");
    printf("  -D<macro>[=val]   Define preprocessor macro\n");
    printf("  -U<macro>         Undefine preprocessor macro\n\n");

    printf("COSMORUN EXTENSIONS:\n");
    printf("  --eval 'code'     Execute inline C code\n");
    printf("  --repl            Interactive C shell (REPL mode)\n");
    printf("  --help, -h        Show this help message\n\n");

    printf("EXECUTION MODES:\n");
    printf("  File Output       Use -o to generate executable or object file\n");
    printf("  Memory Exec       Default: compile and run directly in memory (JIT)\n");
    printf("  Direct Import     Single file uses fast module import API\n");
    printf("  REPL              Interactive mode (no args or --repl)\n\n");

    printf("EXAMPLES (TCC-compatible):\n");
    printf("  %s hello.c                          # Run hello.c in memory\n", program_name);
    printf("  %s -o hello hello.c                 # Compile to executable\n", program_name);
    printf("  %s -c module.c                      # Compile to object file\n", program_name);
    printf("  %s -E source.c -o output.i          # Preprocess only\n", program_name);
    printf("  %s -v hello.c                       # Verbose compilation\n", program_name);
    printf("  %s hello.c arg1 arg2                # Pass arguments to program\n\n", program_name);

    printf("EXAMPLES (cosmorun extensions):\n");
    printf("  %s --eval 'int main(){return 42;}'  # Quick inline code\n", program_name);
    printf("  %s --repl                           # Start interactive shell\n\n", program_name);

    printf("COSMORUN-SPECIFIC FEATURES:\n\n");

    printf("Module Import API (for C code):\n");
    printf("  void* __import(const char* path);\n");
    printf("  void* __import_sym(void* module, const char* symbol);\n");
    printf("  void __import_free(void* module);\n\n");

    printf("Caching System:\n");
    printf("  - Modules cached as .{arch}.o files (e.g., module.x86_64.o)\n");
    printf("  - Auto-invalidated when source file modified\n");
    printf("  - 10-100x speedup on repeated execution\n\n");

    printf("Cross-platform Features:\n");
    printf("  - Dynamic loading: __dlopen, __dlsym, __dlclose\n");
    printf("  - Platform detection: IsWindows(), IsLinux(), IsXnu()\n");
    printf("  - Automatic symbol resolution from system libraries\n");
    printf("  - ~30 high-frequency libc functions cached\n");
    printf("  - Smart crash handler with recovery\n\n");

    printf("PLATFORM SUPPORT:\n");
    printf("  Linux x86-64, ARM64  |  Windows x86-64, ARM64  |  macOS x86-64, Apple Silicon\n\n");

    printf("For more information, see: cosmorun.md\n");
}

/**
 * 直接导入并执行 C 模块
 */
static int execute_direct_import(int argc, char **argv) {
    extern char** environ;

    void* module = __import(argv[1]);
    if (!module) {
        fprintf(stderr, "Failed to import: %s\n", argv[1]);
        return 1;
    }

    typedef int (*main_fn_t)(int, char**, char**);
    main_fn_t main_fn = (main_fn_t)__import_sym(module, "main");
    if (!main_fn) {
        fprintf(stderr, "Symbol 'main' not found in %s\n", argv[1]);
        __import_free(module);
        return 1;
    }

    int ret = main_fn(argc, argv, environ);
    __import_free(module);
    return ret;
}

int main(int argc, char **argv) {

    // 初始化配置系统
    cosmorun_result_t config_result = init_config();
    if (config_result != COSMORUN_SUCCESS) {
        cosmorun_perror(config_result, "configuration initialization");
        return 1;
    }
    // if (!IsXnu()) {
        cosmo_crash_init();
        cosmo_crash_set_context(__FILE__, "main", __LINE__);
    // }
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

static void show_verbose_info(int verbose_level, TCCState *s) {
    if (verbose_level == 0) return;

    printf("=== cosmorun Configuration ===\n");
    printf("Platform: %s\n", g_config.uts.sysname);
    printf("Machine: %s\n", g_config.uts.machine);
    printf("TCC Options: %s\n", g_config.tcc_options);

    // Show cached include paths
    int path_count = cosmo_tcc_get_cached_path_count();
    printf("\nInclude Paths (%d cached):\n", path_count);
    for (int i = 0; i < path_count; i++) {
        const char* path = cosmo_tcc_get_cached_path(i);
        if (path) {
            printf("  [%d] %s\n", i+1, path);
        }
    }

    if (verbose_level >= 2) {
        // Count builtin symbols from exported API
        const cosmo_symbol_entry_t* symbols = cosmo_tcc_get_builtin_symbols();
        int count = 0;
        while (symbols && symbols[count].name) count++;
        printf("\nBuiltin Symbols: %d registered\n", count);
    }

    printf("==============================\n\n");
}

/**
 * 解析 TCC 相关的命令行参数
 */
static parse_result_t parse_tcc_arguments(int argc, char **argv, TCCState *s) {
    parse_result_t result = {0, NULL, -1, -1, -1, NULL, 0, NULL, 0, 0};

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

        // Detect -o output file
        if (strcmp(arg, "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "cosmorun: -o requires an argument\n");
                result.inline_code_index = -2;  // Error flag
                free(temp_indices);
                return result;
            }
            result.output_file = argv[i + 1];
            ++i;  // Skip output filename
            continue;
        }

        // Detect -c compile only
        if (strcmp(arg, "-c") == 0) {
            result.compile_only = 1;
            continue;
        }

        // Detect -v or -vv verbose mode
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "-vv") == 0) {
            result.verbose = (strcmp(arg, "-vv") == 0) ? 2 : 1;
            continue;
        }

        // Detect -E preprocessor only
        if (strcmp(arg, "-E") == 0) {
            result.preprocess_only = 1;
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

        // For preprocessor mode, keep -o flag (TCC handles it internally)
        // For other modes, skip -o and its argument (we handle it ourselves)
        if (strcmp(argv[i], "-o") == 0) {
            if (!parsed->preprocess_only) {
                ++i;  // Skip the next argument (output filename)
                continue;
            }
            // Keep -o for preprocessor mode
        }

        // Skip -c flag (we handle it ourselves)
        if (strcmp(argv[i], "-c") == 0) {
            continue;
        }

        // Skip -v/-vv flag (we handle it ourselves)
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "-vv") == 0) {
            continue;
        }

        // Skip -E flag (we handle it ourselves)
        if (strcmp(argv[i], "-E") == 0) {
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
 * 构建执行参数数组（使用 cosmo_utils 中的通用实现）
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

    // 使用 cosmo_utils 中的通用函数构建执行参数数组
    return cosmo_args_build_exec_argv(argc, argv, runtime_start, program_name, out_argc);
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
    cosmo_crash_set_context(program_name, "user_main", 0);
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

static int execute_tcc_compilation_auto(int argc, char **argv) {
    AUTO_TCC_STATE(s);
    AUTO_CHAR_ARRAY(compile_argv);

    // Quick parse to check if we need file output
    parse_result_t parsed = {0, NULL, -1, -1, -1, NULL, 0, NULL, 0, 0, 0};
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            parsed.output_file = argv[i + 1];
            ++i;  // Skip the output filename
            continue;
        }
        if (strcmp(argv[i], "-c") == 0) {
            parsed.compile_only = 1;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "-vv") == 0) {
            parsed.verbose = (strcmp(argv[i], "-vv") == 0) ? 2 : 1;
            // Update trace_enabled based on verbose level (-vv sets trace to level 2)
            if (parsed.verbose == 2) {
                g_config.trace_enabled = 2;
            }
        }
        if (strcmp(argv[i], "-E") == 0) {
            parsed.preprocess_only = 1;
        }
    }

    // Use different initialization for file output vs memory execution
    if (parsed.output_file || parsed.preprocess_only) {
        // File output mode or preprocessor mode: simpler initialization without runtime linking
        s = tcc_new();
        if (!s) return 1;

        cosmo_tcc_set_error_handler(s);

        int output_type;
        if (parsed.preprocess_only) {
            output_type = TCC_OUTPUT_PREPROCESS;  // -E: preprocessor only (value = 5)
        } else if (parsed.compile_only) {
            output_type = TCC_OUTPUT_OBJ;  // -c: object file
        } else {
            output_type = TCC_OUTPUT_EXE;  // default: executable
        }
        tcc_set_output_type(s, output_type);

        if (g_config.trace_enabled) {
            const char *mode_desc;
            if (parsed.preprocess_only) {
                mode_desc = "preprocessor";
            } else if (parsed.compile_only) {
                mode_desc = "object file";
            } else {
                mode_desc = "executable";
            }

            if (parsed.output_file) {
                fprintf(stderr, "[cosmorun] Output mode: %s to '%s'\n",
                        mode_desc, parsed.output_file);
            } else {
                fprintf(stderr, "[cosmorun] Output mode: %s (to stdout)\n", mode_desc);
            }
        }

        // For executable output: don't use -nostdlib (need libc for __libc_start_main)
        // For object files and preprocessor: use -nostdlib
        if (!parsed.compile_only && !parsed.preprocess_only) {
            // Executable: minimal options, let TCC link libc
            char exe_options[COSMORUN_MAX_OPTIONS_SIZE] = {0};
            append_string_option(exe_options, sizeof(exe_options), "-D__COSMORUN__");
            if (IsLinux()) {
                append_string_option(exe_options, sizeof(exe_options), "-D__unix__");
                append_string_option(exe_options, sizeof(exe_options), "-D__linux__");
            } else if (IsWindows()) {
                append_string_option(exe_options, sizeof(exe_options), "-D_WIN32");
            }
            if (exe_options[0]) {
                tcc_set_options(s, exe_options);
            }
        } else {
            // Object file or preprocessor: use full options with -nostdlib
            cosmo_tcc_build_default_options(g_config.tcc_options, sizeof(g_config.tcc_options), &g_config.uts);
            if (g_config.tcc_options[0]) {
                tcc_set_options(s, g_config.tcc_options);
            }
        }

        // Register paths
        cosmo_tcc_register_include_paths(s, &g_config.uts);
        cosmo_tcc_register_library_paths(s);

        // DON'T register builtin symbols for executable output
        // Let TCC link against system libc instead
        // (Builtin symbols are memory addresses valid only in cosmorun process)
    } else {
        // Memory execution mode: full initialization with runtime
        s = cosmo_tcc_init_state();
        if (!s) return 1;
    }

    // Re-parse with TCC state
    parsed = parse_tcc_arguments(argc, argv, s);

    // Show verbose info if requested (-v/-vv)
    if (parsed.verbose > 0) {
        show_verbose_info(parsed.verbose, s);
    }
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

    // Redirect stdout for preprocessor file output
    int saved_stdout = -1;
    FILE *outfile = NULL;
    if (parsed.preprocess_only && parsed.output_file) {
        // Save current stdout
        saved_stdout = dup(STDOUT_FILENO);
        if (saved_stdout < 0) {
            perror("dup");
            if (parsed.source_indices) free(parsed.source_indices);
            return 1;
        }

        // Open output file
        outfile = fopen(parsed.output_file, "w");
        if (!outfile) {
            perror("fopen");
            close(saved_stdout);
            if (parsed.source_indices) free(parsed.source_indices);
            return 1;
        }

        // Redirect stdout to file
        if (dup2(fileno(outfile), STDOUT_FILENO) < 0) {
            perror("dup2");
            fclose(outfile);
            close(saved_stdout);
            if (parsed.source_indices) free(parsed.source_indices);
            return 1;
        }

        if (g_config.trace_enabled) {
            fprintf(stderr, "[cosmorun] Redirecting preprocessor output to '%s'\n", parsed.output_file);
        }
    }

    // 编译源代码
    if (!compile_source_code(s, &parsed)) {
        // Restore stdout before returning
        if (saved_stdout >= 0) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        if (outfile) {
            fclose(outfile);
        }
        if (parsed.source_indices) free(parsed.source_indices);
        return 1;
    }

    // Restore stdout after compilation
    if (saved_stdout >= 0) {
        fflush(stdout);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    if (outfile) {
        fclose(outfile);
    }

    // 决定执行模式：输出文件 vs 预处理 vs 直接运行
    if (parsed.preprocess_only) {
        // Preprocessor mode (-E): TCC outputs to stdout
        // For file output, we need to redirect stdout manually before compilation was done
        // This shouldn't happen because we redirect before compile_source_code()
        // Just cleanup and return
        if (g_config.trace_enabled) {
            fprintf(stderr, "[cosmorun] Preprocessor output completed\n");
        }
        if (parsed.source_indices) free(parsed.source_indices);
        return 0;
    }

    if (parsed.output_file) {
        // Output file mode: compile to file instead of running
        if (g_config.trace_enabled) {
            fprintf(stderr, "[cosmorun] Writing %s to '%s'\n",
                    parsed.compile_only ? "object file" : "executable",
                    parsed.output_file);
        }

        int result = tcc_output_file(s, parsed.output_file);
        if (result < 0) {
            fprintf(stderr, "cosmorun: failed to write output file '%s'\n", parsed.output_file);
            if (parsed.source_indices) free(parsed.source_indices);
            return 1;
        }

        if (parsed.source_indices) free(parsed.source_indices);
        return 0;
    }

    // Default mode: run in memory (cosmorun's signature feature)
    int result = execute_compiled_program(s, argc, argv, &parsed);
    if (parsed.source_indices) free(parsed.source_indices);
    return result;
}
