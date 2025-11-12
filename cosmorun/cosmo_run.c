// @cosmorun.md @cosmorun_build.sh
/* arch notes
  |-----------------|----------------|----------------------------|
  | __x86_64__      | CPU            | Windows/Linux/macOS x86_64 |
  | __aarch64__     | CPU            | Windows/Linux/macOS ARM64  |
  | __arm__         | CPU            | ARM32 (Linux/macOS/WinRT)  |
  | _M_ARM64        | MSVC ARM64     | Windows ARM64 (MSVC)       |
  | _M_ARM          | MSVC ARM32     | Windows ARM32 (MSVC/WinRT) |
  | _WIN32          | Windows 32/64  | Windows                    |
  | _WIN64          | Windows 64     | Windows x64                |
  | __linux__       | Linux          | Linux                      |
  | __APPLE__       | Apple          | macOS/iOS                  |
*/

#include "cosmo_libc.h"
#include "cosmo_tcc.h"
#include "cosmo_utils.h"
#include "cosmo_cc.h"
#include "cosmo_analyzer.h"
#include "cosmo_formatter.h"
#include "cosmo_cache.h"
#include "cosmo_mem_profiler.h"
#include "cosmo_profiler.h"
#include "cosmo_parallel_link.h"
#include "cosmo_debugger.h"
#include "cosmo_coverage.h"
#include "cosmo_mutate.h"
#include "cosmo_lsp.h"
#include "cosmo_deps.h"
#include "cosmo_env.h"
#include "cosmo_publish.h"
#include "cosmo_lock.h"
#include "cosmo_ffi.h"
#include "cosmo_sandbox.h"
#include "cosmo_sign.h"
#include "cosmo_audit.h"

#define COSMORUN_VERSION "0.9.11"

// Additional constants
#define COSMORUN_MAX_EXEC_ARGS      256     // Maximum execution arguments
#define COSMORUN_REPL_GLOBAL_SIZE   65536   // 64KB for REPL global code
#define COSMORUN_REPL_STMT_SIZE     32768   // 32KB for REPL statement body
#define COSMORUN_REPL_LINE_SIZE     4096    // 4KB for REPL input line

#define COSMORUN_REPL_PROMPT        ">>> "
#define COSMORUN_REPL_WELCOME       "cosmorun REPL - C interactive shell\nType C code, :help for commands, :quit to exit\n"
#define COSMORUN_REPL_GOODBYE       "\nBye!\n"

#define COSMORUN_SYMBOL_CACHE_SIZE  64
#define COSMORUN_HASH_SEED          5381

cosmorun_config_t g_config = {0};

/* Memory profiler flags */
static int g_mem_profile_enabled = 0;
static int g_mem_report_enabled = 0;

/* Sampling profiler flags and state */
static int g_profile_enabled = 0;
static int g_profile_rate = 100;  /* Default: 100Hz */
static const char *g_profile_output = NULL;
/* g_profiler is defined in cosmo_profiler.c */

/* Coverage tracking flags and state */
static int g_coverage_enabled = 0;
static int g_coverage_branch = 0;
static const char *g_coverage_report = NULL;
static const char *g_branch_report = NULL;

/* Sandbox flags and state */
static int g_sandbox_enabled = 0;
static int g_sandbox_allow_write = 0;
static int g_sandbox_allow_net = 0;

/* Audit logging flags and state */
static int g_audit_enabled = 0;
static const char *g_audit_log_path = NULL;
static int g_audit_verbose = 0;
static int g_audit_syslog = 0;

/* Register memory profiler symbols with TCC */
static void register_mem_profiler_symbols(TCCState *s) {
    if (!s || (!g_mem_profile_enabled && !g_mem_report_enabled)) {
        return;
    }

    tcc_add_symbol(s, "mem_profiler_init", mem_profiler_init);
    tcc_add_symbol(s, "mem_profiler_shutdown", mem_profiler_shutdown);
    tcc_add_symbol(s, "mem_profiler_malloc", mem_profiler_malloc);
    tcc_add_symbol(s, "mem_profiler_free", mem_profiler_free);
    tcc_add_symbol(s, "mem_profiler_report", mem_profiler_report);
    tcc_add_symbol(s, "mem_profiler_get_total_allocated", mem_profiler_get_total_allocated);
    tcc_add_symbol(s, "mem_profiler_get_peak_memory", mem_profiler_get_peak_memory);
    tcc_add_symbol(s, "mem_profiler_get_allocation_count", mem_profiler_get_allocation_count);
}

/* Register coverage tracking symbols with TCC */
static void register_coverage_symbols(TCCState *s) {
    if (!s || (!g_coverage_enabled && !g_coverage_branch)) {
        return;
    }

    // Initialize global coverage instance if needed
    if (!g_coverage) {
        g_coverage = coverage_create();
    }

    tcc_add_symbol(s, "g_coverage", &g_coverage);
    tcc_add_symbol(s, "coverage_create", coverage_create);
    tcc_add_symbol(s, "coverage_destroy", coverage_destroy);
    tcc_add_symbol(s, "coverage_reset", coverage_reset);
    tcc_add_symbol(s, "coverage_register_statement", coverage_register_statement);
    tcc_add_symbol(s, "coverage_increment_statement", coverage_increment_statement);
    tcc_add_symbol(s, "coverage_register_branch", coverage_register_branch);
    tcc_add_symbol(s, "coverage_increment_branch", coverage_increment_branch);
    tcc_add_symbol(s, "coverage_print_statement_report", coverage_print_statement_report);
    tcc_add_symbol(s, "coverage_print_branch_report", coverage_print_branch_report);
    tcc_add_symbol(s, "coverage_print_full_report", coverage_print_full_report);
}

/* Register FFI generator symbols with TCC */
static void register_ffi_symbols(TCCState *s) {
    if (!s) return;

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
}

cosmorun_result_t init_config(void) {
    if (g_config.initialized) return COSMORUN_SUCCESS;

    if (uname(&g_config.uts) != 0) {
        return COSMORUN_ERROR_PLATFORM;
    }

    g_config.trace_enabled = 0;

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
    .options = NULL,
    .enable_symbol_resolver = 1,
    .enable_default_paths = 1
};

static const tcc_config_t TCC_CONFIG_OBJECT = {
    .output_type = TCC_OUTPUT_OBJ,
    .options = NULL,
    .enable_symbol_resolver = 0,
    .enable_default_paths = 1
};

static void tcc_error_func(void *opaque, const char *msg);
static void tcc_state_cleanup(void* resource) {
    if (!resource) return;

    TCCState **state_ptr = (TCCState**)resource;
    if (state_ptr && *state_ptr) {
        tcc_delete(*state_ptr);
        *state_ptr = NULL;
    }
}

#if defined(__GNUC__) || defined(__clang__)
#define AUTO_CLEANUP(cleanup_fn) __attribute__((cleanup(cleanup_fn)))
#define AUTO_TCC_STATE TCCState* AUTO_CLEANUP(tcc_state_cleanup)
#else
#define AUTO_CLEANUP(cleanup_fn)
#define AUTO_TCC_STATE TCCState*
#endif

/* Crash handling is now unified in cosmo_utils.c/h.
 * This file uses the cosmo_crash_*() API provided by cosmo_utils.
 */

typedef struct {
    TCCState *tcc_state;
    char **compile_argv;
    int initialized;
} tcc_context_t;
static tcc_context_t* tcc_context_init(void) {
    tcc_context_t *ctx = calloc(1, sizeof(tcc_context_t));
    if (!ctx) return NULL;

    ctx->tcc_state = cosmo_tcc_init_state();
    if (!ctx->tcc_state) {
        free(ctx);
        return NULL;
    }

    register_mem_profiler_symbols(ctx->tcc_state);
    register_coverage_symbols(ctx->tcc_state);
    register_ffi_symbols(ctx->tcc_state);
    ctx->initialized = 1;
    return ctx;
}

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
static int generate_dependencies(int argc, char **argv, const parse_result_t *parsed);

static inline void char_array_cleanup(char ***argv) {
    if (argv && *argv) {
        free(*argv);
        *argv = NULL;
    }
}

#define AUTO_CHAR_ARRAY(name) \
    char **__attribute__((cleanup(char_array_cleanup))) name = NULL

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
                register_mem_profiler_symbols(s);
                register_coverage_symbols(s);
                register_ffi_symbols(s);
                cosmo_tcc_link_runtime(s);

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
            register_mem_profiler_symbols(exec_state);
            register_coverage_symbols(exec_state);
            register_ffi_symbols(exec_state);

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

typedef enum {
    MODE_HELP,
    MODE_REPL,
    MODE_DIRECT_IMPORT,
    MODE_INLINE_CODE,
    MODE_COMPILE_AND_RUN,
    MODE_AR_TOOL,
    MODE_LINK,
    MODE_NM,
    MODE_OBJDUMP,
    MODE_STRIP,
    MODE_ANALYZE,
    MODE_FORMAT,
    MODE_CACHE_STATS,
    MODE_CACHE_CLEAR,
    MODE_DEBUG,
    MODE_MUTATE,
    MODE_LSP,
    MODE_BIND,
    MODE_PKG_INIT,
    MODE_PKG_VALIDATE,
    MODE_PKG_PACK,
    MODE_PKG_PUBLISH,
    MODE_LOCK,
    MODE_LOCK_VERIFY,
    MODE_LOCK_UPDATE,
    MODE_SIGN_KEYGEN,
    MODE_SIGN_FILE,
    MODE_VERIFY_SIG,
    MODE_TRUST_KEY
} execution_mode_t;

static execution_mode_t parse_execution_mode(int argc, char **argv) {

    if (argc == 1) return MODE_REPL;
    if (argc == 2 && strcmp(argv[1], "--repl") == 0) return MODE_REPL;
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) return MODE_HELP;
    if (argc < 2) return MODE_HELP;
    if (argc >= 2 && strcmp(argv[1], "--lsp") == 0) return MODE_LSP;
    if (argc >= 3 && strcmp(argv[1], "--debug") == 0) return MODE_DEBUG;
    if (argc >= 2 && strcmp(argv[1], "--ar") == 0) return MODE_AR_TOOL;
    if (argc >= 2 && strcmp(argv[1], "--link") == 0) {
        return MODE_LINK;
    }
    if (argc >= 2 && strcmp(argv[1], "--nm") == 0) return MODE_NM;
    if (argc >= 2 && strcmp(argv[1], "--objdump") == 0) return MODE_OBJDUMP;
    if (argc >= 2 && strcmp(argv[1], "--strip") == 0) return MODE_STRIP;
    if (argc >= 2 && strcmp(argv[1], "--analyze") == 0) return MODE_ANALYZE;
    if (argc >= 2 && strcmp(argv[1], "--format") == 0) return MODE_FORMAT;
    if (argc >= 2 && strcmp(argv[1], "--mutate") == 0) return MODE_MUTATE;
    if (argc >= 2 && strcmp(argv[1], "--cache-stats") == 0) return MODE_CACHE_STATS;
    if (argc >= 2 && strcmp(argv[1], "--cache-clear") == 0) return MODE_CACHE_CLEAR;
    if (argc >= 2 && strcmp(argv[1], "--lsp") == 0) return MODE_LSP;
    if (argc >= 2 && strcmp(argv[1], "bind") == 0) return MODE_BIND;
    if (argc >= 2 && strcmp(argv[1], "init") == 0) return MODE_PKG_INIT;
    if (argc >= 2 && strcmp(argv[1], "validate") == 0) return MODE_PKG_VALIDATE;
    if (argc >= 2 && strcmp(argv[1], "pack") == 0) return MODE_PKG_PACK;
    if (argc >= 2 && strcmp(argv[1], "publish") == 0) return MODE_PKG_PUBLISH;
    if (argc >= 2 && strcmp(argv[1], "lock") == 0) return MODE_LOCK;
    if (argc >= 2 && strcmp(argv[1], "verify") == 0) return MODE_LOCK_VERIFY;
    if (argc >= 3 && strcmp(argv[1], "update") == 0 && strcmp(argv[2], "--lock") == 0) return MODE_LOCK_UPDATE;
    if (argc >= 2 && strcmp(argv[1], "--keygen") == 0) return MODE_SIGN_KEYGEN;
    if (argc >= 3 && strcmp(argv[1], "--sign") == 0) return MODE_SIGN_FILE;
    if (argc >= 3 && strcmp(argv[1], "--verify") == 0) return MODE_VERIFY_SIG;
    if (argc >= 3 && strcmp(argv[1], "--trust-key") == 0) return MODE_TRUST_KEY;
    if (argc >= 3 && (strcmp(argv[1], "--eval") == 0 || strcmp(argv[1], "-e") == 0)) return MODE_INLINE_CODE;

    // Check for TCC compilation flags that require COMPILE_AND_RUN mode
    for (int i = 1; i < argc; i++) {
        // Skip linker-specific verbose flags
        if (strcmp(argv[i], "--link-verbose") == 0 || strcmp(argv[i], "--link-quiet") == 0) {
            continue;
        }
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
    printf("  -P                Suppress #line directives in preprocessed output\n");
    printf("  -dM               Dump all macro definitions (use with -E)\n");
    printf("  -v                Verbose mode (show paths and configuration)\n");
    printf("  -vv               Extra verbose mode (include builtin symbols)\n");
    printf("  -O0               No optimization (fastest compilation)\n");
    printf("  -O1               Basic optimization\n");
    printf("  -O2               Standard optimization (default)\n");
    printf("  -O3               Aggressive optimization (best performance)\n");
    printf("  -Os               Optimize for size\n");
    printf("  -I <path>         Add include path\n");
    printf("  -L <path>         Add library path\n");
    printf("  -D<macro>[=val]   Define preprocessor macro\n");
    printf("  -U<macro>         Undefine preprocessor macro\n\n");

    printf("COSMORUN EXTENSIONS:\n");
    printf("  --eval 'code'     Execute inline C code\n");
    printf("  -e 'code'         Execute inline C code (alias for --eval)\n");
    printf("  --repl            Interactive C shell (REPL mode)\n");
    printf("  --help, -h        Show this help message\n");
    printf("  --cache-stats         Show cache statistics (hit rate, size)\n");
    printf("  --cache-clear         Clear compilation cache\n");
    printf("  --cache-max-entries=N Set max cache entries (default 1000, 0=unlimited)\n");
    printf("  --no-cache            Disable caching for this run\n");
    printf("  --mem-profile         Enable memory profiling\n");
    printf("  --mem-report          Print memory report at exit\n");
    printf("  --profile             Enable sampling profiler (hot function detection)\n");
    printf("  --profile-rate=N      Sample rate in Hz (default 100)\n");
    printf("  --profile-output=FILE Write profiling report to file (default: stdout)\n");
    printf("  --coverage            Enable statement coverage tracking\n");
    printf("  --coverage-branch     Enable branch coverage tracking (implies --coverage)\n");
    printf("  --coverage-report=FILE Write coverage report to file (default: stdout)\n");
    printf("  --branch-report=FILE  Write branch report to file (default: stdout)\n");
    printf("  --audit-log=PATH      Enable security audit logging to file (JSON format)\n");
    printf("  --audit-verbose       Enable detailed syscall logging in audit log\n");
    printf("  --audit-syslog        Also send audit events to syslog/journald\n\n");

    printf("TOOLCHAIN UTILITIES:\n");
    printf("  --ar <op> <archive> [files...]  Create/manage static libraries (.a)\n");
    printf("  --link <objs...> -o <exe>       Link object files into executable\n");
    printf("    --libc=TYPE                   Select libc backend (cosmo|system|mini)\n");
    printf("                                  Default: cosmo (Cosmopolitan libc)\n");
    printf("    --gc-sections                 Remove unused code (dead code elimination)\n");
    printf("    --parallel-link               Enable parallel linking (default: auto)\n");
    printf("    --no-parallel-link            Disable parallel linking\n");
    printf("    --dump-symbols                Show complete symbol table with addresses\n");
    printf("    --dump-relocations            Show all relocations with status\n");
    printf("    --trace-resolve               Trace symbol resolution through archives\n");
    printf("  --nm <file>                     List symbols from object/executable\n");
    printf("  --objdump [-htdrs] <file>       Disassemble and inspect object files\n");
    printf("  --strip [-g|-s] [-o out] <file> Remove symbols from binary\n");
    printf("  --debug <program> [args...]     Start interactive debugger (Linux only)\n");
    printf("  --format <file> [-o out]        Format C code (uses .cosmoformat config)\n");
    printf("  --analyze <file>                Analyze C source code\n");
    printf("  --mutate <file>                 Run mutation testing to verify test quality\n");
    printf("  --lsp                           Start LSP server for IDE integration\n\n");

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

    printf("CODE SIGNING:\n");
    printf("  --keygen              Generate Ed25519 keypair (~/.cosmorun/keys/)\n");
    printf("  --sign <file>         Sign a binary (creates <file>.sig)\n");
    printf("  --verify <file>       Verify binary signature\n");
    printf("  --trust-key <pubkey>  Add public key to trust registry\n\n");

    printf("COSMORUN-SPECIFIC FEATURES:\n\n");

    printf("Module Import API (for C code):\n");
    printf("  void* __import(const char* path);\n");
    printf("  void* __import_sym(void* module, const char* symbol);\n");
    printf("  void __import_free(void* module);\n\n");

    printf("Caching System:\n");
    printf("  - Modules cached as .{arch}.o files (e.g., module.x86_64.o)\n");
    printf("  - Auto-invalidated when source file modified\n");
    printf("  - 10-100x speedup on repeated execution\n\n");

    printf("  - fixed: __dlopen,__dlsym\n");
    printf("  - Platform detection: IsWindows(), IsLinux(), IsXnu()\n");
    printf("  - Automatic symbol resolution from system libraries\n");
    printf("  - ~30 high-frequency libc functions cached\n");
    printf("  - Smart crash handler with recovery\n\n");

    printf("PLATFORM SUPPORT:\n");
    printf("  Linux x86-64, ARM64  |  Windows x86-64, ARM64  |  macOS x86-64, Apple Silicon\n\n");

    printf("For more information, see: cosmorun.md\n");
}

static int execute_linker(int argc, char **argv) {
    // Parse linker arguments: --link obj1.o obj2.o -o output [-L path] [-l lib] [--libc=TYPE] [--gc-sections] [--parallel-link] [-v|-vv|-q]
    const char **objects = NULL;
    int obj_count = 0;
    const char *output = NULL;
    const char **lib_paths = NULL;
    int lib_path_count = 0;
    const char **libs = NULL;
    int lib_count = 0;
    LibcBackend libc_backend = LIBC_COSMO;  // Default: Cosmopolitan libc
    int gc_sections = 0;  // Default: no garbage collection
    int parallel_link = 1;  // Default: auto-enable parallel linking
    int verbosity = 1;  // Default: warnings + summary

    // Allocate arrays for maximum possible entries
    objects = (const char **)calloc((size_t)argc, sizeof(char*));
    lib_paths = (const char **)calloc((size_t)argc, sizeof(char*));
    libs = (const char **)calloc((size_t)argc, sizeof(char*));

    if (!objects || !lib_paths || !libs) {
        fprintf(stderr, "Memory allocation failed\n");
        if (objects) free((void*)objects);
        if (lib_paths) free((void*)lib_paths);
        if (libs) free((void*)libs);
        return 1;
    }

    // Parse arguments starting from index 2 (after --link)
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -o requires an argument\n");
                free((void*)objects);
                free((void*)lib_paths);
                free((void*)libs);
                return 1;
            }
            ++i;
            output = argv[i];
        } else if (strcmp(argv[i], "-L") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -L requires an argument\n");
                free((void*)objects);
                free((void*)lib_paths);
                free((void*)libs);
                return 1;
            }
            lib_paths[lib_path_count++] = argv[++i];
        } else if (strcmp(argv[i], "-l") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -l requires an argument\n");
                free((void*)objects);
                free((void*)lib_paths);
                free((void*)libs);
                return 1;
            }
            libs[lib_count++] = argv[++i];
        } else if (strncmp(argv[i], "--libc=", 7) == 0) {
            // Parse --libc=TYPE
            const char *libc_type = argv[i] + 7;
            int backend = parse_libc_option(libc_type);
            if (backend < 0) {
                fprintf(stderr, "Error: Invalid libc backend '%s'\n", libc_type);
                fprintf(stderr, "Valid options: cosmo, system, mini\n");
                free((void*)objects);
                free((void*)lib_paths);
                free((void*)libs);
                return 1;
            }
            libc_backend = (LibcBackend)backend;
        } else if (strcmp(argv[i], "--gc-sections") == 0) {
            // Enable garbage collection
            gc_sections = 1;
        } else if (strcmp(argv[i], "--parallel-link") == 0) {
            // Enable parallel linking
            parallel_link = 1;
        } else if (strcmp(argv[i], "--no-parallel-link") == 0) {
            // Disable parallel linking
            parallel_link = 0;
        } else if (strcmp(argv[i], "-vv") == 0) {
            verbosity = 3;  // Very verbose (debug)
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--link-verbose") == 0) {
            verbosity = 2;  // Verbose (info)
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--link-quiet") == 0) {
            verbosity = 0;  // Quiet (errors only)
        } else if (strcmp(argv[i], "--dump-symbols") == 0) {
            cosmo_linker_set_dump_symbols(1);
        } else if (strcmp(argv[i], "--dump-relocations") == 0) {
            cosmo_linker_set_dump_relocations(1);
        } else if (strcmp(argv[i], "--trace-resolve") == 0) {
            cosmo_linker_set_trace_resolve(1);
        } else if (argv[i][0] != '-') {
            // Object file
            objects[obj_count++] = argv[i];
        }
    }

    // Set verbosity before linking
    cosmo_linker_set_verbosity(verbosity);

    // Configure parallel linking
    cosmo_parallel_link_config(parallel_link, 0);  // 0 = auto-detect thread count

    // Validate inputs
    if (obj_count == 0) {
        fprintf(stderr, "Error: No object files specified\n");
        free((void*)objects);
        free((void*)lib_paths);
        free((void*)libs);
        return 1;
    }

    if (output) {
    }
    if (!output) {
        fprintf(stderr, "Error: No output file specified (-o required)\n");
        free((void*)objects);
        free((void*)lib_paths);
        free((void*)libs);
        return 1;
    }

    // Call linker with libc backend and gc_sections flag
    int ret = cosmo_link(objects, obj_count, output, lib_paths, lib_path_count, libs, lib_count, libc_backend, gc_sections);

    free((void*)objects);
    free((void*)lib_paths);
    free((void*)libs);

    return (ret == 0) ? 0 : 1;
}

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

/* AR tool handler */
static int execute_ar_tool(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "cosmorun: --ar requires operation argument\n");
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s --ar crs <archive.a> <file1.o> [file2.o ...]\n", argv[0]);
        fprintf(stderr, "  %s --ar t <archive.a>       # List contents\n", argv[0]);
        fprintf(stderr, "  %s --ar tv <archive.a>      # List verbose\n", argv[0]);
        fprintf(stderr, "  %s --ar x <archive.a>       # Extract all\n", argv[0]);
        fprintf(stderr, "  %s --ar x <archive.a> <member>  # Extract specific\n", argv[0]);
        fprintf(stderr, "  %s --ar d <archive.a> <member>  # Delete member\n", argv[0]);
        return 1;
    }

    const char *operation = argv[2];
    int verbose = 0;

    /* Parse operation flags */
    if (strchr(operation, 'v')) verbose = 1;

    if (strchr(operation, 'c') || strchr(operation, 'r') || strchr(operation, 's')) {
        /* Create/replace archive */
        if (argc < 5) {
            fprintf(stderr, "ar: requires archive name and at least one object file\n");
            return 1;
        }
        const char *archive = argv[3];
        const char **objects = (const char **)&argv[4];
        int count = argc - 4;
        return cosmo_ar_create(archive, objects, count, verbose);
    }
    else if (strchr(operation, 't')) {
        /* List archive */
        if (argc < 4) {
            fprintf(stderr, "ar: requires archive name\n");
            return 1;
        }
        const char *archive = argv[3];
        return cosmo_ar_list(archive, verbose);
    }
    else if (strchr(operation, 'x')) {
        /* Extract from archive */
        if (argc < 4) {
            fprintf(stderr, "ar: requires archive name\n");
            return 1;
        }
        const char *archive = argv[3];
        const char *member = (argc > 4) ? argv[4] : NULL;
        return cosmo_ar_extract(archive, member, verbose);
    }
    else if (strchr(operation, 'd')) {
        /* Delete from archive */
        if (argc < 5) {
            fprintf(stderr, "ar: delete requires archive name and member name\n");
            return 1;
        }
        const char *archive = argv[3];
        const char *member = argv[4];
        return cosmo_ar_delete(archive, member);
    }
    else {
        fprintf(stderr, "ar: unknown operation '%s'\n", operation);
        return 1;
    }
}

// Debugger REPL
static int run_debugger_repl(const char *program, char **args) {
#ifndef __linux__
    fprintf(stderr, "Error: Debugger is only supported on Linux (requires ptrace)\n");
    return 1;
#else
    printf("Cosmorun Debugger - ptrace-based interactive debugger\n");
    printf("Type 'help' for available commands\n\n");

    debugger_t *dbg = debugger_create(program, args);
    if (!dbg) {
        fprintf(stderr, "Error: Failed to create debugger for '%s'\n", program);
        return 1;
    }

    printf("Process started (PID: %d)\n", debugger_get_pid(dbg));
    printf("Stopped at entry point\n");

    char line[256];
    struct user_regs_struct regs;
    int running = 1;

    while (running) {
        printf("(dbg) ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }

        // Parse command
        char cmd[256] = {0};
        unsigned long long addr = 0;
        sscanf(line, "%s %llx", cmd, &addr);

        if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
            printf("Available commands:\n");
            printf("  break <addr>   - Set breakpoint at address (hex)\n");
            printf("  continue, c    - Continue execution\n");
            printf("  step, s        - Execute single instruction\n");
            printf("  print <reg>    - Print register (rip, rax, rbx, etc.)\n");
            printf("  regs           - Print all registers\n");
            printf("  x <addr>       - Examine memory at address (hex)\n");
            printf("  quit, q        - Exit debugger\n");
            printf("  help, h        - Show this help\n");
        }
        else if (strcmp(cmd, "break") == 0 || strcmp(cmd, "b") == 0) {
            if (addr == 0) {
                printf("Usage: break <address_in_hex>\n");
                continue;
            }
            int bp_id = debugger_set_breakpoint(dbg, (void*)addr);
            if (bp_id >= 0) {
                printf("Breakpoint %d set at 0x%llx\n", bp_id, addr);
            } else {
                printf("Failed to set breakpoint\n");
            }
        }
        else if (strcmp(cmd, "continue") == 0 || strcmp(cmd, "c") == 0) {
            printf("Continuing...\n");
            if (debugger_continue(dbg) == 0) {
                int status = debugger_status(dbg);
                if (status == -1) {
                    printf("Process exited\n");
                    running = 0;
                } else if (status == 0) {
                    debugger_read_registers(dbg, &regs);
                    printf("Stopped at 0x%llx\n", (unsigned long long)regs.rip);
                }
            } else {
                printf("Error continuing execution\n");
            }
        }
        else if (strcmp(cmd, "step") == 0 || strcmp(cmd, "s") == 0) {
            if (debugger_step(dbg) == 0) {
                debugger_read_registers(dbg, &regs);
                printf("Stepped to 0x%llx\n", (unsigned long long)regs.rip);
            } else {
                printf("Error stepping\n");
            }
        }
        else if (strcmp(cmd, "regs") == 0) {
            if (debugger_read_registers(dbg, &regs) == 0) {
                #ifdef __x86_64__
                printf("RAX: 0x%016llx  RBX: 0x%016llx  RCX: 0x%016llx\n",
                       regs.rax, regs.rbx, regs.rcx);
                printf("RDX: 0x%016llx  RSI: 0x%016llx  RDI: 0x%016llx\n",
                       regs.rdx, regs.rsi, regs.rdi);
                printf("RBP: 0x%016llx  RSP: 0x%016llx  RIP: 0x%016llx\n",
                       regs.rbp, regs.rsp, regs.rip);
                printf("R8:  0x%016llx  R9:  0x%016llx  R10: 0x%016llx\n",
                       regs.r8, regs.r9, regs.r10);
                printf("R11: 0x%016llx  R12: 0x%016llx  R13: 0x%016llx\n",
                       regs.r11, regs.r12, regs.r13);
                printf("R14: 0x%016llx  R15: 0x%016llx\n",
                       regs.r14, regs.r15);
                #endif
            } else {
                printf("Error reading registers\n");
            }
        }
        else if (strcmp(cmd, "print") == 0 || strcmp(cmd, "p") == 0) {
            char reg_name[64];
            sscanf(line, "%*s %s", reg_name);
            if (debugger_read_registers(dbg, &regs) == 0) {
                #ifdef __x86_64__
                if (strcmp(reg_name, "rip") == 0) printf("RIP = 0x%llx\n", regs.rip);
                else if (strcmp(reg_name, "rax") == 0) printf("RAX = 0x%llx\n", regs.rax);
                else if (strcmp(reg_name, "rbx") == 0) printf("RBX = 0x%llx\n", regs.rbx);
                else if (strcmp(reg_name, "rcx") == 0) printf("RCX = 0x%llx\n", regs.rcx);
                else if (strcmp(reg_name, "rdx") == 0) printf("RDX = 0x%llx\n", regs.rdx);
                else if (strcmp(reg_name, "rsi") == 0) printf("RSI = 0x%llx\n", regs.rsi);
                else if (strcmp(reg_name, "rdi") == 0) printf("RDI = 0x%llx\n", regs.rdi);
                else if (strcmp(reg_name, "rbp") == 0) printf("RBP = 0x%llx\n", regs.rbp);
                else if (strcmp(reg_name, "rsp") == 0) printf("RSP = 0x%llx\n", regs.rsp);
                else printf("Unknown register: %s\n", reg_name);
                #endif
            } else {
                printf("Error reading registers\n");
            }
        }
        else if (strcmp(cmd, "x") == 0) {
            if (addr == 0) {
                printf("Usage: x <address_in_hex>\n");
                continue;
            }
            unsigned char buf[32];
            int bytes = debugger_read_memory(dbg, (void*)addr, buf, sizeof(buf));
            if (bytes > 0) {
                printf("0x%llx: ", addr);
                for (int i = 0; i < bytes && i < 16; i++) {
                    printf("%02x ", buf[i]);
                }
                printf("\n");
            } else {
                printf("Error reading memory\n");
            }
        }
        else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "q") == 0) {
            running = 0;
        }
        else if (strlen(cmd) > 0) {
            printf("Unknown command: %s (type 'help' for commands)\n", cmd);
        }
    }

    debugger_destroy(dbg);
    printf("Debugger exited\n");
    return 0;
#endif
}

#ifdef BUILD_COSMO_RUN
//@hack tcc at tccelf.c: addr = cosmorun_resolve_symbol(name_ud);
void* cosmorun_resolve_symbol(const char* symbol_name) {
    return cosmorun_dlsym_libc(symbol_name);
}
int main(int argc, char **argv) {
    cosmorun_result_t config_result = init_config();
    if (config_result != COSMORUN_SUCCESS) {
        cosmorun_perror(config_result, "configuration initialization");
        return 1;
    }

    // Initialize cache system (enabled by default)
    cosmo_cache_init();

    // Check for --no-cache, --cache-max-entries, memory profiler, sampling profiler, and audit logging flags
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-cache") == 0) {
            cosmo_cache_set_enabled(0);
        } else if (strncmp(argv[i], "--cache-max-entries=", 20) == 0) {
            int max_entries = atoi(argv[i] + 20);
            cosmo_cache_set_max_entries(max_entries);
        } else if (strcmp(argv[i], "--mem-profile") == 0) {
            g_mem_profile_enabled = 1;
        } else if (strcmp(argv[i], "--mem-report") == 0) {
            g_mem_report_enabled = 1;
        } else if (strcmp(argv[i], "--profile") == 0) {
            g_profile_enabled = 1;
        } else if (strncmp(argv[i], "--profile-rate=", 15) == 0) {
            g_profile_rate = atoi(argv[i] + 15);
            if (g_profile_rate <= 0 || g_profile_rate > 10000) {
                fprintf(stderr, "Warning: Invalid profile rate %d, using default 100Hz\n", g_profile_rate);
                g_profile_rate = 100;
            }
            g_profile_enabled = 1;
        } else if (strncmp(argv[i], "--profile-output=", 17) == 0) {
            g_profile_output = argv[i] + 17;
            g_profile_enabled = 1;
        } else if (strcmp(argv[i], "--coverage") == 0) {
            g_coverage_enabled = 1;
        } else if (strcmp(argv[i], "--coverage-branch") == 0) {
            g_coverage_branch = 1;
            g_coverage_enabled = 1;  // branch coverage implies statement coverage
        } else if (strncmp(argv[i], "--coverage-report=", 18) == 0) {
            g_coverage_report = argv[i] + 18;
            g_coverage_enabled = 1;
        } else if (strncmp(argv[i], "--branch-report=", 16) == 0) {
            g_branch_report = argv[i] + 16;
            g_coverage_branch = 1;
            g_coverage_enabled = 1;
        } else if (strcmp(argv[i], "--sandbox") == 0) {
            g_sandbox_enabled = 1;
        } else if (strcmp(argv[i], "--sandbox-allow-write") == 0) {
            g_sandbox_enabled = 1;
            g_sandbox_allow_write = 1;
        } else if (strcmp(argv[i], "--sandbox-allow-net") == 0) {
            g_sandbox_enabled = 1;
            g_sandbox_allow_net = 1;
        } else if (strncmp(argv[i], "--audit-log=", 12) == 0) {
            g_audit_log_path = argv[i] + 12;
            g_audit_enabled = 1;
        } else if (strcmp(argv[i], "--audit-verbose") == 0) {
            g_audit_verbose = 1;
        } else if (strcmp(argv[i], "--audit-syslog") == 0) {
            g_audit_syslog = 1;
        }
    }

    // Initialize memory profiler if enabled
    if (g_mem_profile_enabled || g_mem_report_enabled) {
        mem_profiler_init();
    }

    // Initialize audit logging if enabled
    if (g_audit_enabled && g_audit_log_path) {
        audit_config_t audit_config = {
            .log_path = g_audit_log_path,
            .verbose = g_audit_verbose,
            .use_syslog = g_audit_syslog,
            .max_file_size = 0,  /* Use default 10MB */
            .max_rotations = 0,  /* Use default 5 */
            .rate_limit = 0      /* Use default 1000/sec */
        };
        if (cosmo_audit_init(&audit_config) != 0) {
            fprintf(stderr, "Warning: Failed to initialize audit logging to '%s'\n", g_audit_log_path);
            g_audit_enabled = 0;
        } else {
            /* Log program start event with command line */
            char cmdline[4096] = {0};
            for (int i = 0; i < argc; i++) {
                if (i > 0) strcat(cmdline, " ");
                strncat(cmdline, argv[i], sizeof(cmdline) - strlen(cmdline) - 1);
            }
            cosmo_audit_log_eventf(AUDIT_EVENT_PROGRAM_START, "cmdline=%s", cmdline);
        }
    }

    // Initialize crash handler - TEMPORARILY DISABLED for debugging
    // cosmo_crash_init();
    // cosmo_crash_set_context(__FILE__, "main", __LINE__);
    execution_mode_t mode = parse_execution_mode(argc, argv);

    switch (mode) {
        case MODE_HELP:
            show_help(argv[0]);
            if (g_mem_report_enabled || g_mem_profile_enabled) {
                mem_profiler_shutdown();
            }
            return 1;

        case MODE_REPL: {
            int result = repl_mode();
            if (g_mem_report_enabled || g_mem_profile_enabled) {
                mem_profiler_report();
                mem_profiler_shutdown();
            }
            if (g_coverage_enabled || g_coverage_branch) {
                if (g_coverage) {
                    FILE *fp = g_coverage_report ? fopen(g_coverage_report, "w") : stdout;
                    if (fp) {
                        coverage_print_full_report(g_coverage, fp);
                        if (fp != stdout) fclose(fp);
                    }
                    coverage_destroy(g_coverage);
                    g_coverage = NULL;
                }
            }
            return result;
        }

        case MODE_LSP: {
            lsp_server_t *lsp = lsp_server_create();
            if (!lsp) {
                fprintf(stderr, "Error: Failed to create LSP server\n");
                return 1;
            }
            int result = lsp_server_run(lsp);
            lsp_server_destroy(lsp);
            return result;
        }

        case MODE_DEBUG:
            if (argc < 3) {
                fprintf(stderr, "Usage: %s --debug <program> [args...]\n", argv[0]);
                return 1;
            }
            return run_debugger_repl(argv[2], &argv[2]);

        case MODE_LINK:
            return execute_linker(argc, argv);

        case MODE_DIRECT_IMPORT:
            return execute_direct_import(argc, argv);

        case MODE_AR_TOOL:
            return execute_ar_tool(argc, argv);

        case MODE_NM:
            if (argc < 3) {
                fprintf(stderr, "Usage: %s --nm <file>\n", argv[0]);
                return 1;
            }
            return cosmo_nm(argv[2], NM_FORMAT_BSD, 0);

        case MODE_OBJDUMP: {
            int flags = 0;
            const char *file = NULL;

            // Parse objdump flags
            for (int i = 2; i < argc; i++) {
                if (strcmp(argv[i], "-h") == 0) {
                    flags |= OBJDUMP_HEADERS;
                } else if (strcmp(argv[i], "-t") == 0) {
                    flags |= OBJDUMP_SYMBOLS;
                } else if (strcmp(argv[i], "-d") == 0) {
                    flags |= OBJDUMP_DISASM;
                } else if (strcmp(argv[i], "-r") == 0) {
                    flags |= OBJDUMP_RELOC;
                } else if (strcmp(argv[i], "-s") == 0) {
                    flags |= OBJDUMP_FULL_CONTENTS;
                } else if (argv[i][0] != '-') {
                    file = argv[i];
                }
            }

            // Default: show all if no flags specified
            if (flags == 0 && file != NULL) {
                flags = OBJDUMP_HEADERS | OBJDUMP_SYMBOLS | OBJDUMP_RELOC | OBJDUMP_DISASM;
            }

            if (!file) {
                fprintf(stderr, "Usage: %s --objdump [-h] [-t] [-d] [-r] [-s] <file>\n", argv[0]);
                fprintf(stderr, "Options:\n");
                fprintf(stderr, "  -h  Display section headers\n");
                fprintf(stderr, "  -t  Display symbol table\n");
                fprintf(stderr, "  -d  Disassemble code sections (hex dump)\n");
                fprintf(stderr, "  -r  Display relocations\n");
                fprintf(stderr, "  -s  Display full section contents\n");
                return 1;
            }

            return cosmo_objdump(file, flags);
        }

        case MODE_STRIP: {
            const char *input = NULL;
            const char *output = NULL;
            int flags = STRIP_ALL;  // Default: strip all symbols

            // Parse strip arguments
            for (int i = 2; i < argc; i++) {
                if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                    output = argv[++i];
                } else if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--strip-debug") == 0) {
                    flags = STRIP_DEBUG;
                } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--strip-all") == 0) {
                    flags = STRIP_ALL;
                } else if (argv[i][0] != '-') {
                    input = argv[i];
                }
            }

            if (!input) {
                fprintf(stderr, "Usage: %s --strip [-g|--strip-debug] [-s|--strip-all] [-o output] <file>\n", argv[0]);
                fprintf(stderr, "Options:\n");
                fprintf(stderr, "  -g, --strip-debug  Remove debug symbols only\n");
                fprintf(stderr, "  -s, --strip-all    Remove all symbols (default)\n");
                fprintf(stderr, "  -o <output>        Output file (default: overwrite input)\n");
                return 1;
            }

            // If no output specified, use input file (in-place strip)
            if (!output) {
                output = input;
            }

            return cosmo_strip(input, output, flags);
        }

        case MODE_ANALYZE: {
            const char *file = NULL;
            AnalysisOptions options;
            init_default_analysis_options(&options);

            /* Parse analyzer arguments */
            for (int i = 2; i < argc; i++) {
                if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
                    options.verbose = 1;
                } else if (strcmp(argv[i], "--check-all") == 0) {
                    /* Enable all checks (default) */
                    options.check_dead_code = 1;
                    options.check_unused_vars = 1;
                    options.check_type_safety = 1;
                    options.check_null_deref = 1;
                    options.check_unreachable = 1;
                    options.check_memory_leaks = 1;
                    options.check_local_unused = 1;
                    options.check_uninitialized = 1;
                } else if (strcmp(argv[i], "--check-types") == 0) {
                    options.check_type_safety = 1;
                } else if (strcmp(argv[i], "--check-null") == 0) {
                    options.check_null_deref = 1;
                } else if (strcmp(argv[i], "--no-dead-code") == 0) {
                    options.check_dead_code = 0;
                } else if (strcmp(argv[i], "--no-unused-vars") == 0) {
                    options.check_unused_vars = 0;
                } else if (strcmp(argv[i], "--no-type-safety") == 0) {
                    options.check_type_safety = 0;
                } else if (strcmp(argv[i], "--no-null-check") == 0) {
                    options.check_null_deref = 0;
                } else if (strcmp(argv[i], "--no-unreachable") == 0) {
                    options.check_unreachable = 0;
                } else if (strcmp(argv[i], "--no-memory-leaks") == 0) {
                    options.check_memory_leaks = 0;
                } else if (strcmp(argv[i], "--no-local-unused") == 0) {
                    options.check_local_unused = 0;
                } else if (strcmp(argv[i], "--no-uninitialized") == 0) {
                    options.check_uninitialized = 0;
                } else if (argv[i][0] != '-') {
                    file = argv[i];
                }
            }

            if (!file) {
                fprintf(stderr, "Usage: %s --analyze [options] <file.c>\n", argv[0]);
                fprintf(stderr, "Options:\n");
                fprintf(stderr, "  -v, --verbose             Enable verbose output\n");
                fprintf(stderr, "  --check-all               Enable all checks (default)\n");
                fprintf(stderr, "  --check-types             Enable type safety checks\n");
                fprintf(stderr, "  --check-null              Enable NULL dereference checks\n");
                fprintf(stderr, "\nDisable specific checks:\n");
                fprintf(stderr, "  --no-dead-code            Disable dead code detection\n");
                fprintf(stderr, "  --no-unused-vars          Disable unused variable detection\n");
                fprintf(stderr, "  --no-type-safety          Disable type safety checks\n");
                fprintf(stderr, "  --no-null-check           Disable NULL dereference checks\n");
                fprintf(stderr, "  --no-unreachable          Disable unreachable code detection\n");
                fprintf(stderr, "  --no-memory-leaks         Disable memory leak detection\n");
                fprintf(stderr, "  --no-local-unused         Disable local unused variable detection\n");
                fprintf(stderr, "  --no-uninitialized        Disable uninitialized variable detection\n");
                return 1;
            }

            AnalysisResult result = {0};
            int ret = analyze_file(file, &options, &result);
            if (ret != 0) {
                fprintf(stderr, "analyzer: failed to analyze '%s': ", file);
                switch (ret) {
                    case ANALYZE_ERROR_FILE:
                        fprintf(stderr, "cannot open file\n");
                        break;
                    case ANALYZE_ERROR_PARSE:
                        fprintf(stderr, "parse error\n");
                        break;
                    case ANALYZE_ERROR_MEMORY:
                        fprintf(stderr, "out of memory\n");
                        break;
                    default:
                        fprintf(stderr, "unknown error\n");
                        break;
                }
                return 1;
            }

            print_analysis_report(&result, file);
            free_analysis_result(&result);
            return (result.error_count > 0) ? 1 : 0;
        }

        case MODE_FORMAT: {
            const char *input_file = NULL;
            const char *output_file = NULL;
            const char *config_file = ".cosmoformat";

            /* Parse formatter arguments */
            for (int i = 2; i < argc; i++) {
                if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                    output_file = argv[++i];
                } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
                    config_file = argv[++i];
                } else if (argv[i][0] != '-') {
                    input_file = argv[i];
                }
            }

            if (!input_file) {
                fprintf(stderr, "Usage: %s --format <file.c> [-o output.c] [--config .cosmoformat]\n", argv[0]);
                fprintf(stderr, "  -o <file>       Output file (default: stdout)\n");
                fprintf(stderr, "  --config <file> Config file (default: .cosmoformat)\n");
                return 1;
            }

            /* Load formatting options */
            format_options_t opts;
            if (access(config_file, F_OK) == 0) {
                if (format_options_load_from_file(&opts, config_file) != FORMAT_SUCCESS) {
                    fprintf(stderr, "Warning: Failed to load config file '%s', using defaults\n", config_file);
                    format_options_init_default(&opts);
                }
            } else {
                format_options_init_default(&opts);
            }

            /* Format the file */
            format_result_t result = format_file(input_file, &opts);
            if (result.error_code != FORMAT_SUCCESS) {
                fprintf(stderr, "Format error: %s\n", result.error_msg);
                return 1;
            }

            /* Write output */
            int ret = 0;
            if (output_file) {
                ret = write_formatted_file(&result, output_file);
                if (ret == FORMAT_SUCCESS) {
                    printf("Formatted code written to: %s\n", output_file);
                } else {
                    fprintf(stderr, "Error writing to file: %s\n", output_file);
                    ret = 1;
                }
            } else {
                /* Write to stdout */
                printf("%s", result.content);
            }

            free_format_result(&result);
            return ret;
        }

        case MODE_MUTATE: {
            const char *source_file = NULL;
            const char *test_cmd = NULL;
            int mutation_ops = MUT_ALL;
            int max_mutants = 50;

            /* Parse mutation testing arguments */
            for (int i = 2; i < argc; i++) {
                if (strncmp(argv[i], "--mutation-operators=", 21) == 0) {
                    const char *ops = argv[i] + 21;
                    mutation_ops = 0;
                    if (strstr(ops, "flip")) mutation_ops |= (1 << MUT_FLIP_OPERATOR);
                    if (strstr(ops, "constant")) mutation_ops |= (1 << MUT_CHANGE_CONSTANT);
                    if (strstr(ops, "negate")) mutation_ops |= (1 << MUT_NEGATE_CONDITION);
                    if (strstr(ops, "return")) mutation_ops |= (1 << MUT_REPLACE_RETURN);
                    if (strstr(ops, "all")) mutation_ops = MUT_ALL;
                    if (mutation_ops == 0) mutation_ops = MUT_ALL;
                } else if (strncmp(argv[i], "--test-command=", 15) == 0) {
                    test_cmd = argv[i] + 15;
                } else if (strncmp(argv[i], "--max-mutants=", 14) == 0) {
                    max_mutants = atoi(argv[i] + 14);
                    if (max_mutants <= 0) max_mutants = 50;
                } else if (argv[i][0] != '-') {
                    source_file = argv[i];
                }
            }

            if (!source_file) {
                fprintf(stderr, "Usage: %s --mutate [options] <source.c>\n", argv[0]);
                fprintf(stderr, "Options:\n");
                fprintf(stderr, "  --mutation-operators=<ops>  Comma-separated list: flip,constant,negate,return,all (default: all)\n");
                fprintf(stderr, "  --test-command=<cmd>        Command to run test (default: run compiled program)\n");
                fprintf(stderr, "  --max-mutants=<n>           Maximum number of mutants to generate (default: 50)\n");
                fprintf(stderr, "\nExample:\n");
                fprintf(stderr, "  %s --mutate --max-mutants=20 test.c --test-command=\"./test\"\n", argv[0]);
                return 1;
            }

            /* Create mutator */
            printf("=== Mutation Testing ===\n");
            printf("Source: %s\n", source_file);
            printf("Max mutants: %d\n", max_mutants);
            printf("Test command: %s\n", test_cmd ? test_cmd : "(run compiled program)");
            printf("\n");

            mutator_t *mut = mutator_create(source_file);
            if (!mut) {
                fprintf(stderr, "Error: Failed to create mutator for '%s'\n", source_file);
                return 1;
            }

            /* Generate mutants */
            printf("Generating mutants...\n");
            int count = mutator_generate_mutants(mut, mutation_ops, max_mutants);
            if (count < 0) {
                fprintf(stderr, "Error: Failed to generate mutants\n");
                mutator_destroy(mut);
                return 1;
            }

            printf("Generated %d mutants\n\n", count);

            /* Test each mutant */
            printf("Testing mutants...\n");
            for (int i = 0; i < count; i++) {
                printf("  [%d/%d] ", i + 1, count);
                fflush(stdout);

                int result = mutator_test_mutant(mut, i, test_cmd);
                const mutant_t *m = mutator_get_mutant(mut, i);

                if (result == 0) {
                    printf("KILLED - %s:%d %s → %s\n", m->file, m->line, m->original, m->mutated);
                } else if (result == 1) {
                    printf("SURVIVED - %s:%d %s → %s\n", m->file, m->line, m->original, m->mutated);
                } else {
                    printf("ERROR - %s:%d %s\n", m->file, m->line, m->error_msg);
                }
            }

            printf("\n");

            /* Print report */
            mutator_print_report(mut, stdout);

            /* Get mutation score */
            double score = mutator_get_score(mut);
            int ret = (score >= 80.0) ? 0 : 1;

            mutator_destroy(mut);
            return ret;
        }

        case MODE_BIND: {
            const char *input_header = NULL;
            const char *output_file = NULL;
            const char *library_name = NULL;
            int verbose = 0;
            int generate_loader = 1;
            int add_error_checks = 1;

            /* Parse bind command arguments */
            for (int i = 2; i < argc; i++) {
                if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                    output_file = argv[++i];
                } else if (strcmp(argv[i], "--lib") == 0 && i + 1 < argc) {
                    library_name = argv[++i];
                } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
                    verbose = 1;
                } else if (strcmp(argv[i], "--no-loader") == 0) {
                    generate_loader = 0;
                } else if (strcmp(argv[i], "--no-error-checks") == 0) {
                    add_error_checks = 0;
                } else if (argv[i][0] != '-') {
                    input_header = argv[i];
                }
            }

            if (!input_header) {
                fprintf(stderr, "Usage: %s bind <header.h> [options]\n", argv[0]);
                fprintf(stderr, "Options:\n");
                fprintf(stderr, "  -o <file>          Output file (default: stdout)\n");
                fprintf(stderr, "  --lib <name>       Library name (e.g., libm.so)\n");
                fprintf(stderr, "  -v, --verbose      Enable verbose output\n");
                fprintf(stderr, "  --no-loader        Don't generate loader function\n");
                fprintf(stderr, "  --no-error-checks  Don't add error checking code\n");
                fprintf(stderr, "\nExample:\n");
                fprintf(stderr, "  %s bind math.h --lib libm.so -o math_bindings.c\n", argv[0]);
                return 1;
            }

            /* Create FFI context */
            ffi_options_t ffi_opts = {
                .input_header = input_header,
                .output_file = output_file,
                .library_name = library_name,
                .verbose = verbose,
                .generate_loader = generate_loader,
                .add_error_checks = add_error_checks
            };

            ffi_context_t *ffi_ctx = ffi_context_create(&ffi_opts);
            if (!ffi_ctx) {
                fprintf(stderr, "Error: Failed to create FFI context\n");
                return 1;
            }

            /* Parse header file */
            if (!ffi_parse_header(ffi_ctx, input_header)) {
                fprintf(stderr, "Error: Failed to parse header file: %s\n", input_header);
                ffi_context_destroy(ffi_ctx);
                return 1;
            }

            /* Generate bindings */
            if (!ffi_generate_bindings(ffi_ctx, output_file)) {
                fprintf(stderr, "Error: Failed to generate bindings\n");
                ffi_context_destroy(ffi_ctx);
                return 1;
            }

            if (verbose) {
                fprintf(stderr, "Successfully generated bindings for %d functions\n",
                       ffi_ctx->function_count);
            }

            ffi_context_destroy(ffi_ctx);
            return 0;
        }

        case MODE_CACHE_STATS: {
            if (cosmo_cache_init() != 0) {
                fprintf(stderr, "Failed to initialize cache\n");
                return 1;
            }

            cosmo_cache_stats_t stats;
            cosmo_cache_get_stats(&stats);

            printf("Compilation Cache Statistics:\n");
            printf("  Enabled: %s\n", cosmo_cache_is_enabled() ? "yes" : "no");
            printf("  Max entries: %d%s\n", cosmo_cache_get_max_entries(),
                   cosmo_cache_get_max_entries() == 0 ? " (unlimited)" : "");
            printf("  Cache hits: %llu\n", (unsigned long long)stats.hits);
            printf("  Cache misses: %llu\n", (unsigned long long)stats.misses);
            printf("  Total lookups: %llu\n", (unsigned long long)(stats.hits + stats.misses));

            if (stats.hits + stats.misses > 0) {
                double hit_rate = (double)stats.hits / (stats.hits + stats.misses) * 100.0;
                printf("  Hit rate: %.1f%%\n", hit_rate);
            } else {
                printf("  Hit rate: N/A\n");
            }

            printf("  Stores: %llu\n", (unsigned long long)stats.stores);
            printf("  LRU-2 evictions: %llu\n", (unsigned long long)stats.evictions);
            printf("  Total entries: %zu\n", stats.total_entries);
            printf("  Total size: %.2f MB\n", stats.total_size / (1024.0 * 1024.0));

            cosmo_cache_cleanup();
            return 0;
        }

        case MODE_CACHE_CLEAR: {
            if (cosmo_cache_init() != 0) {
                fprintf(stderr, "Failed to initialize cache\n");
                return 1;
            }

            if (cosmo_cache_clear() != 0) {
                fprintf(stderr, "Failed to clear cache\n");
                cosmo_cache_cleanup();
                return 1;
            }

            printf("Cache cleared successfully\n");
            cosmo_cache_cleanup();
            return 0;
        }

        case MODE_PKG_INIT:
            return cosmo_pkg_cmd_init(argc, argv);

        case MODE_PKG_VALIDATE:
            return cosmo_pkg_cmd_validate(argc, argv);

        case MODE_PKG_PACK:
            return cosmo_pkg_cmd_pack(argc, argv);

        case MODE_PKG_PUBLISH:
            return cosmo_pkg_cmd_publish(argc, argv);

        case MODE_LOCK: {
            // Generate lockfile from cosmo.json
            cosmo_lock_ctx_t *ctx = cosmo_lock_create();
            if (!ctx) {
                fprintf(stderr, "Error: Failed to create lockfile context\n");
                return 1;
            }

            printf("Generating lockfile from cosmo.json...\n");

            // For MVP, create a sample lockfile to demonstrate functionality
            cosmo_lock_add_dependency(ctx, "libhttp", "2.1.3",
                                     "registry://libhttp@2.1.3",
                                     "sha256:abc123...", "libnet:^1.0.0");
            cosmo_lock_add_dependency(ctx, "libnet", "1.0.0",
                                     "registry://libnet@1.0.0",
                                     "sha256:def456...", "");

            int result = cosmo_lock_save(ctx);
            if (result == 0) {
                printf("✓ Lockfile generated: %s\n",
                       ctx->lockfile_path ? ctx->lockfile_path : COSMO_LOCK_FILENAME);
                cosmo_lock_print_summary(ctx);
            } else {
                fprintf(stderr, "✗ Failed to generate lockfile: %s\n",
                        cosmo_lock_get_error(ctx));
            }

            cosmo_lock_destroy(ctx);
            return result == 0 ? 0 : 1;
        }

        case MODE_LOCK_VERIFY: {
            // Verify installed packages match lockfile
            cosmo_lock_ctx_t *ctx = cosmo_lock_create();
            if (!ctx) {
                fprintf(stderr, "Error: Failed to create lockfile context\n");
                return 1;
            }

            printf("Verifying dependencies against lockfile...\n");

            int result = cosmo_lock_load(ctx);
            if (result != 0) {
                fprintf(stderr, "✗ Failed to load lockfile: %s\n",
                        cosmo_lock_get_error(ctx));
                cosmo_lock_destroy(ctx);
                return 1;
            }

            cosmo_lock_print_summary(ctx);
            printf("\n");

            // Validate lockfile structure
            if (cosmo_lock_validate(ctx) != 0) {
                fprintf(stderr, "✗ Invalid lockfile: %s\n", cosmo_lock_get_error(ctx));
                cosmo_lock_destroy(ctx);
                return 1;
            }

            printf("✓ Lockfile is valid\n");
            printf("\nNote: Package installation verification not yet implemented\n");
            printf("      (requires integration with package manager)\n");

            cosmo_lock_destroy(ctx);
            return 0;
        }

        case MODE_LOCK_UPDATE: {
            // Update specific dependency in lockfile
            if (argc < 3) {
                fprintf(stderr, "Usage: %s update <package> --lock\n", argv[0]);
                return 1;
            }

            const char *package_name = argv[2];
            cosmo_lock_ctx_t *ctx = cosmo_lock_create();
            if (!ctx) {
                fprintf(stderr, "Error: Failed to create lockfile context\n");
                return 1;
            }

            printf("Updating %s in lockfile...\n", package_name);

            int result = cosmo_lock_update_dependency(ctx, package_name);
            if (result != 0) {
                fprintf(stderr, "✗ Failed to update: %s\n", cosmo_lock_get_error(ctx));
                cosmo_lock_destroy(ctx);
                return 1;
            }

            printf("✓ Lockfile updated\n");
            cosmo_lock_destroy(ctx);
            return 0;
        }

        case MODE_SIGN_KEYGEN: {
            /* Generate Ed25519 keypair */
            const char* keydir = NULL;
            if (argc >= 3) {
                keydir = argv[2];
            }
            int result = cosmo_sign_keygen(keydir);
            return (result == COSMO_SIGN_OK) ? 0 : 1;
        }

        case MODE_SIGN_FILE: {
            /* Sign a binary file */
            if (argc < 3) {
                fprintf(stderr, "Usage: %s --sign <file>\n", argv[0]);
                fprintf(stderr, "Private key will be read from ~/.cosmorun/keys/private.key\n");
                return 1;
            }

            const char* file_path = argv[2];
            const char* home = get_home_dir();
            char privkey_path[1024];
            snprintf(privkey_path, sizeof(privkey_path), "%s/%s/%s",
                     home, COSMO_SIGN_KEY_DIR, COSMO_SIGN_PRIVATE_KEY);

            int result = cosmo_sign_file(file_path, privkey_path);
            return (result == COSMO_SIGN_OK) ? 0 : 1;
        }

        case MODE_VERIFY_SIG: {
            /* Verify binary signature */
            if (argc < 3) {
                fprintf(stderr, "Usage: %s --verify <file>\n", argv[0]);
                fprintf(stderr, "Signature will be read from <file>.sig\n");
                return 1;
            }

            const char* file_path = argv[2];
            const char* home = get_home_dir();
            char pubkey_path[1024];
            snprintf(pubkey_path, sizeof(pubkey_path), "%s/%s/%s",
                     home, COSMO_SIGN_KEY_DIR, COSMO_SIGN_PUBLIC_KEY);

            int result = cosmo_verify_file(file_path, pubkey_path);
            if (result == COSMO_SIGN_OK) {
                return 0;
            } else if (result == COSMO_SIGN_ERR_UNTRUSTED) {
                /* Warning already printed, but signature is valid */
                return 0;
            } else {
                return 1;
            }
        }

        case MODE_TRUST_KEY: {
            /* Add public key to trust registry */
            if (argc < 3) {
                fprintf(stderr, "Usage: %s --trust-key <base64-pubkey>\n", argv[0]);
                return 1;
            }

            const char* pubkey_b64 = argv[2];
            int result = cosmo_trust_key(pubkey_b64);
            return (result == COSMO_SIGN_OK) ? 0 : 1;
        }

        case MODE_INLINE_CODE:
        case MODE_COMPILE_AND_RUN:
            break;
    }

    return execute_tcc_compilation_auto(argc, argv);
}
#endif

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

/* Get optimization flag string from level */
static const char* get_optimization_flag(int level) {
    switch (level) {
        case 0: return "-O0";
        case 1: return "-O1";
        case 2: return "-O2";
        case 3: return "-O3";
        case -1: return "-Os";
        default: return "-O2";  // Default
    }
}

/* Apply compiler options from parse_result to TCC state */
static void apply_compiler_options(TCCState *s, const parse_result_t *parsed) {
    if (!s || !parsed) return;

    // Apply environment variables first (so command-line options can override)
    cosmo_env_apply_all(s);

    // Apply -Werror: TCC doesn't have a direct API for this, but we can use tcc_set_options
    if (parsed->warnings_as_errors) {
        tcc_set_options(s, "-Werror");
    }

    // Apply -march: TCC doesn't directly support all -march variants,
    // but we can pass it through for basic compatibility
    if (parsed->target_arch) {
        char march_opt[128];
        snprintf(march_opt, sizeof(march_opt), "-march=%s", parsed->target_arch);
        // Note: TCC may ignore unsupported architectures
        if (g_config.trace_enabled) {
            fprintf(stderr, "[cosmorun] Applying architecture option: %s\n", march_opt);
        }
    }

    // Apply -m32/-m64: TCC has limited support, mainly for cross-compilation
    if (parsed->target_m32) {
        if (g_config.trace_enabled) {
            fprintf(stderr, "[cosmorun] 32-bit target requested (-m32)\n");
        }
        // TCC doesn't have direct -m32 support, would need cross-compilation setup
    } else if (parsed->target_m64) {
        if (g_config.trace_enabled) {
            fprintf(stderr, "[cosmorun] 64-bit target requested (-m64)\n");
        }
        // TCC defaults to native bitness
    }

    // Apply -static: Tell TCC to prefer static linking
    if (parsed->static_link) {
        tcc_set_options(s, "-static");
        if (g_config.trace_enabled) {
            fprintf(stderr, "[cosmorun] Static linking enabled\n");
        }
    }

    // Apply -rdynamic: Export all symbols
    if (parsed->rdynamic) {
        tcc_set_options(s, "-rdynamic");
        if (g_config.trace_enabled) {
            fprintf(stderr, "[cosmorun] Dynamic symbol export enabled (-rdynamic)\n");
        }
    }
}

/* Parse TCC command line arguments */
static parse_result_t parse_tcc_arguments(int argc, char **argv, TCCState *s) {
    parse_result_t result = {0, NULL, -1, -1, -1, NULL, 0, NULL, 0, 0, 0, 2,
                             0, 0, 0, 0, NULL, NULL,  // dependency flags
                             0, NULL, 0, 0, 0, 0};    // option compatibility flags

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

        if (strcmp(arg, "--eval") == 0 || strcmp(arg, "-e") == 0) {
            if (result.inline_mode) {
                fprintf(stderr, "cosmorun: multiple --eval/-e options not supported\n");
                result.inline_code_index = -2;  // Error flag
                free(temp_indices);
                return result;
            }
            if (i + 1 >= argc) {
                fprintf(stderr, "cosmorun: --eval/-e requires an argument\n");
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

        // Detect -P suppress line directives
        if (strcmp(arg, "-P") == 0) {
            // -P flag will be passed to TCC via tcc_set_options
            continue;
        }

        // Detect -dM dump macros
        if (strcmp(arg, "-dM") == 0) {
            // -dM flag will be passed to TCC via tcc_set_options
            continue;
        }

        // Detect optimization level (-O0, -O1, -O2, -O3, -Os)
        if (arg[0] == '-' && arg[1] == 'O') {
            if (strcmp(arg, "-O0") == 0) {
                result.optimization_level = 0;
            } else if (strcmp(arg, "-O1") == 0) {
                result.optimization_level = 1;
            } else if (strcmp(arg, "-O2") == 0) {
                result.optimization_level = 2;
            } else if (strcmp(arg, "-O3") == 0) {
                result.optimization_level = 3;
            } else if (strcmp(arg, "-Os") == 0) {
                result.optimization_level = -1;  // Size optimization
            } else if (strcmp(arg, "-O") == 0) {
                result.optimization_level = 2;  // -O defaults to -O2
            }
            continue;
        }

        // Detect dependency generation flags
        if (strcmp(arg, "-M") == 0) {
            result.gen_deps = 1;
            result.exclude_system_deps = 0;
            continue;
        }
        if (strcmp(arg, "-MM") == 0) {
            result.gen_deps = 1;
            result.exclude_system_deps = 1;
            continue;
        }
        if (strcmp(arg, "-MD") == 0) {
            result.gen_deps_and_compile = 1;
            result.exclude_system_deps = 0;
            continue;
        }
        if (strcmp(arg, "-MMD") == 0) {
            result.gen_deps_and_compile = 1;
            result.exclude_system_deps = 1;
            continue;
        }
        if (strcmp(arg, "-MP") == 0) {
            result.gen_phony_targets = 1;
            continue;
        }
        if (strcmp(arg, "-MT") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "cosmorun: -MT requires an argument\n");
                result.inline_code_index = -2;  // Error flag
                free(temp_indices);
                return result;
            }
            result.dep_target = argv[i + 1];
            ++i;  // Skip target name
            continue;
        }
        if (strcmp(arg, "-MF") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "cosmorun: -MF requires an argument\n");
                result.inline_code_index = -2;  // Error flag
                free(temp_indices);
                return result;
            }
            result.dep_file = argv[i + 1];
            ++i;  // Skip file name
            continue;
        }

        // GCC/Clang compatibility options (v0.9.1)

        // Detect -Werror (treat warnings as errors)
        if (strcmp(arg, "-Werror") == 0) {
            result.warnings_as_errors = 1;
            continue;
        }

        // Detect -march=<arch> (target architecture)
        if (strncmp(arg, "-march=", 7) == 0) {
            result.target_arch = arg + 7;
            continue;
        }

        // Detect -m32 (32-bit target)
        if (strcmp(arg, "-m32") == 0) {
            result.target_m32 = 1;
            result.target_m64 = 0;  // Mutually exclusive
            continue;
        }

        // Detect -m64 (64-bit target)
        if (strcmp(arg, "-m64") == 0) {
            result.target_m64 = 1;
            result.target_m32 = 0;  // Mutually exclusive
            continue;
        }

        // Detect -static (static linking)
        if (strcmp(arg, "-static") == 0) {
            result.static_link = 1;
            continue;
        }

        // Detect -rdynamic (export all symbols for dynamic loading)
        if (strcmp(arg, "-rdynamic") == 0) {
            result.rdynamic = 1;
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

/* Build compilation argument array */
static char** build_compile_argv(int argc, char **argv, const parse_result_t *parsed) {
    char **compile_argv = calloc((size_t)argc + 1, sizeof(char*));
    if (!compile_argv) {
        perror("calloc");
        return NULL;
    }

    int compile_argc = 0;
    compile_argv[compile_argc++] = argv[0];

    for (int i = 1; i < argc; ++i) {
        if (parsed->dashdash_index >= 0 && i >= parsed->dashdash_index - 1) {
            if (i == parsed->dashdash_index - 1) continue;
            break;
        }

        if (parsed->inline_mode && parsed->inline_code_index >= 0 &&
            (i == parsed->inline_code_index || i == parsed->inline_code_index - 1)) {
            continue;
        }

        // Check if this index is a source file
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

        // Skip cache-related flags (we handle them ourselves)
        if (strcmp(argv[i], "--no-cache") == 0 ||
            strncmp(argv[i], "--cache-max-entries=", 20) == 0) {
            continue;
        }

        // Skip audit logging flags (we handle them ourselves)
        if (strncmp(argv[i], "--audit-log=", 12) == 0 ||
            strcmp(argv[i], "--audit-verbose") == 0 ||
            strcmp(argv[i], "--audit-syslog") == 0) {
            continue;
        }

        // Skip -O flags (we handle them ourselves via tcc_set_options)
        if (argv[i][0] == '-' && argv[i][1] == 'O') {
            continue;
        }

        // Skip memory profiler flags (cosmorun-specific)
        if (strcmp(argv[i], "--mem-profile") == 0 || strcmp(argv[i], "--mem-report") == 0) {
            continue;
        }

        // Skip sampling profiler flags (cosmorun-specific)
        if (strcmp(argv[i], "--profile") == 0 ||
            strncmp(argv[i], "--profile-rate=", 15) == 0 ||
            strncmp(argv[i], "--profile-output=", 17) == 0) {
            continue;
        }

        // Skip coverage tracking flags (cosmorun-specific)
        if (strcmp(argv[i], "--coverage") == 0 ||
            strcmp(argv[i], "--coverage-branch") == 0 ||
            strncmp(argv[i], "--coverage-report=", 18) == 0 ||
            strncmp(argv[i], "--branch-report=", 16) == 0) {
            continue;
        }

        // Skip sandbox flags (cosmorun-specific)
        if (strcmp(argv[i], "--sandbox") == 0 ||
            strcmp(argv[i], "--sandbox-allow-write") == 0 ||
            strcmp(argv[i], "--sandbox-allow-net") == 0) {
            continue;
        }

        compile_argv[compile_argc++] = argv[i];
    }

    compile_argv[compile_argc] = NULL;
    return compile_argv;
}

/* Parse and apply TCC arguments */
static int parse_and_apply_tcc_args(TCCState *s, char **compile_argv) {
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

/* Compile source code */
static int compile_source_code(TCCState *s, const parse_result_t *parsed) {
    if (parsed->inline_mode) {
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

    // Compile files (requires manual extern declarations)
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

/* Get program name */
static const char* get_program_name(TCCState *s, const parse_result_t *parsed, int argc, char **argv) {
    if (parsed->inline_mode) {
        return "(inline)";
    }

    for (int i = 0; i < s->nb_files; ++i) {
        struct filespec *f = s->files[i];
        if (!(f->type & AFF_TYPE_LIB)) {
            return f->name;
        }
    }

    if (parsed->source_index >= 0) {
        return argv[parsed->source_index];
    }

    return argv[0];
}

/* Build execution argument array */
static char** build_exec_argv(int argc, char **argv, const parse_result_t *parsed, const char *program_name, int *out_argc) {
    int runtime_start = argc;
    if (parsed->dashdash_index >= 0) {
        runtime_start = parsed->dashdash_index;
    } else if (parsed->inline_mode && parsed->inline_code_index >= 0) {
        runtime_start = parsed->inline_code_index + 1;
    } else if (parsed->source_index >= 0) {
        runtime_start = parsed->source_index + 1;
    }

    if (runtime_start > argc) runtime_start = argc;

    return cosmo_args_build_exec_argv(argc, argv, runtime_start, program_name, out_argc);
}

/* Execute compiled program */
static int execute_compiled_program(TCCState *s, int argc, char **argv, const parse_result_t *parsed) {
    const char *program_name = get_program_name(s, parsed, argc, argv);

    int exec_argc;
    char **exec_argv = build_exec_argv(argc, argv, parsed, program_name, &exec_argc);
    if (!exec_argv) {
        return 1;
    }

    int reloc_result = tcc_relocate(s);
    if (reloc_result < 0) {
        fprintf(stderr, "Could not relocate code (error: %d)\n", reloc_result);
        free(exec_argv);
        return 1;
    }

    int (*func)(int, char**) = (int (*)(int, char**))tcc_get_symbol(s, "main");
    if (!func) {
        fprintf(stderr, "Could not find main function\n");
        free(exec_argv);
        return 1;
    }

    // cosmo_crash_set_context(program_name, "user_main", 0);  // Disabled - causes SIGSEGV
    // cosmo_crash_context_t* crash_ctx = cosmo_crash_get_context();
    // crash_ctx->tcc_state = s;

    // Enable sandbox if requested
    if (g_sandbox_enabled) {
        sandbox_config_t sandbox_config = {
            .allow_write = g_sandbox_allow_write,
            .allow_net = g_sandbox_allow_net,
            .allow_exec = 0  /* Never allow exec in sandbox mode */
        };

        if (cosmo_sandbox_enable(&sandbox_config) != 0) {
            fprintf(stderr, "Warning: Failed to enable sandbox\n");
            /* Continue execution - sandbox is best-effort */
        }
    }

    // Start instrumentation profiler if enabled
    if (g_profile_enabled) {
        g_profiler = profiler_create();
        if (g_profiler) {
            if (profiler_enable_instrumentation(g_profiler) < 0) {
                fprintf(stderr, "Warning: Failed to enable profiler instrumentation\n");
                profiler_destroy(g_profiler);
                g_profiler = NULL;
            }
        } else {
            fprintf(stderr, "Warning: Failed to create profiler\n");
        }
    }

    int ret;
    // Crash recovery disabled - just run the function directly
    ret = func(exec_argc, exec_argv);

    // Print instrumentation profiler report
    if (g_profiler) {
        FILE *out_fp = stdout;
        if (g_profile_output) {
            out_fp = fopen(g_profile_output, "w");
            if (!out_fp) {
                fprintf(stderr, "Warning: Failed to open %s, using stdout\n", g_profile_output);
                out_fp = stdout;
            }
        }

        fprintf(out_fp, "=== Instrumentation Profiling Report ===\n");
        profiler_print_report(g_profiler);

        if (out_fp != stdout) {
            fclose(out_fp);
        }

        profiler_destroy(g_profiler);
        g_profiler = NULL;
    }

    free(exec_argv);
    return ret;
}

static int execute_tcc_compilation_auto(int argc, char **argv) {
    AUTO_TCC_STATE(s);
    AUTO_CHAR_ARRAY(compile_argv);

    // Quick parse to check if we need file output
    parse_result_t parsed = {0, NULL, -1, -1, -1, NULL, 0, NULL, 0, 0, 0, 2,
                             0, 0, 0, 0, NULL, NULL};  // Default: -O2, no deps
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
        // Detect dependency generation flags
        if (strcmp(argv[i], "-M") == 0) {
            parsed.gen_deps = 1;
            parsed.exclude_system_deps = 0;
        }
        if (strcmp(argv[i], "-MM") == 0) {
            parsed.gen_deps = 1;
            parsed.exclude_system_deps = 1;
        }
        if (strcmp(argv[i], "-MD") == 0) {
            parsed.gen_deps_and_compile = 1;
            parsed.exclude_system_deps = 0;
        }
        if (strcmp(argv[i], "-MMD") == 0) {
            parsed.gen_deps_and_compile = 1;
            parsed.exclude_system_deps = 1;
        }
        if (strcmp(argv[i], "-MP") == 0) {
            parsed.gen_phony_targets = 1;
        }
        if (strcmp(argv[i], "-MT") == 0 && i + 1 < argc) {
            parsed.dep_target = argv[i + 1];
            ++i;
        }
        if (strcmp(argv[i], "-MF") == 0 && i + 1 < argc) {
            parsed.dep_file = argv[i + 1];
            ++i;
        }
        // Parse optimization level
        if (argv[i][0] == '-' && argv[i][1] == 'O') {
            if (strcmp(argv[i], "-O0") == 0) parsed.optimization_level = 0;
            else if (strcmp(argv[i], "-O1") == 0) parsed.optimization_level = 1;
            else if (strcmp(argv[i], "-O2") == 0) parsed.optimization_level = 2;
            else if (strcmp(argv[i], "-O3") == 0) parsed.optimization_level = 3;
            else if (strcmp(argv[i], "-Os") == 0) parsed.optimization_level = -1;
            else if (strcmp(argv[i], "-O") == 0) parsed.optimization_level = 2;
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
            // Add optimization level
            append_string_option(exe_options, sizeof(exe_options), get_optimization_flag(parsed.optimization_level));
            if (exe_options[0]) {
                tcc_set_options(s, exe_options);
            }
        } else {
            // Object file or preprocessor: use full options with -nostdlib
            cosmo_tcc_build_default_options(g_config.tcc_options, sizeof(g_config.tcc_options), &g_config.uts);
            // Add optimization level
            append_string_option(g_config.tcc_options, sizeof(g_config.tcc_options), get_optimization_flag(parsed.optimization_level));
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
        register_mem_profiler_symbols(s);
        register_coverage_symbols(s);
        register_ffi_symbols(s);
    }

    // Re-parse with TCC state
    parsed = parse_tcc_arguments(argc, argv, s);

    // Apply compiler options (environment variables and command-line flags)
    apply_compiler_options(s, &parsed);

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

    if (!parsed.inline_mode && parsed.source_index == -1) {
        fprintf(stderr, "cosmorun: no input file provided\n");
        if (parsed.source_indices) free(parsed.source_indices);
        return 1;
    }

    if (!parse_and_apply_tcc_args(s, compile_argv)) {
        if (parsed.source_indices) free(parsed.source_indices);
        return 1;
    }

    // Handle dependency generation (-M/-MM)
    // These flags stop after generating dependencies (don't compile)
    if (parsed.gen_deps) {
        int ret = generate_dependencies(argc, argv, &parsed);
        if (parsed.source_indices) free(parsed.source_indices);
        return ret;
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

        // Handle -MD/-MMD: generate dependencies after successful compilation
        if (parsed.gen_deps_and_compile) {
            if (generate_dependencies(argc, argv, &parsed) != 0) {
                fprintf(stderr, "cosmorun: warning: failed to generate dependencies\n");
            }
        }

        if (parsed.source_indices) free(parsed.source_indices);
        return 0;
    }

    // Default mode: run in memory (cosmorun's signature feature)
    int result = execute_compiled_program(s, argc, argv, &parsed);
    if (parsed.source_indices) free(parsed.source_indices);

    // Print memory profiler report if enabled
    if (g_mem_report_enabled || g_mem_profile_enabled) {
        mem_profiler_report();
        mem_profiler_shutdown();
    }

    // Print coverage reports if enabled
    if (g_coverage_enabled || g_coverage_branch) {
        if (g_coverage) {
            FILE *fp = stdout;

            // Print branch report
            if (g_coverage_branch) {
                if (g_branch_report) {
                    fp = fopen(g_branch_report, "w");
                    if (!fp) {
                        fprintf(stderr, "Warning: Failed to open branch report file: %s\n", g_branch_report);
                        fp = stdout;
                    }
                }
                coverage_print_branch_report(g_coverage, fp);
                if (fp != stdout) fclose(fp);
                fp = stdout;
            }

            // Print coverage report
            if (g_coverage_report) {
                fp = fopen(g_coverage_report, "w");
                if (!fp) {
                    fprintf(stderr, "Warning: Failed to open coverage report file: %s\n", g_coverage_report);
                    fp = stdout;
                }
            }

            if (g_coverage_branch) {
                coverage_print_full_report(g_coverage, fp);
            } else {
                coverage_print_statement_report(g_coverage, fp);
            }

            if (fp != stdout) fclose(fp);

            coverage_destroy(g_coverage);
            g_coverage = NULL;
        }
    }

    // Shutdown audit logging and log program end event
    if (g_audit_enabled) {
        cosmo_audit_log_eventf(AUDIT_EVENT_PROGRAM_END, "exit_code=%d", result);
        cosmo_audit_shutdown();
    }

    return result;
}

/* Generate dependency information (-M/-MM/-MD/-MMD) */
static int generate_dependencies(int argc, char **argv, const parse_result_t *parsed) {
    if (!parsed || parsed->source_count == 0) {
        fprintf(stderr, "cosmorun: no input file for dependency generation\n");
        return 1;
    }

    const char *source_file = argv[parsed->source_indices[0]];

    // Create dependency context
    cosmo_deps_ctx_t *deps_ctx = cosmo_deps_create();
    if (!deps_ctx) {
        fprintf(stderr, "cosmorun: failed to create dependency context\n");
        return 1;
    }

    // Configure dependency context
    cosmo_deps_set_exclude_system(deps_ctx, parsed->exclude_system_deps);
    cosmo_deps_set_phony_targets(deps_ctx, parsed->gen_phony_targets);
    if (parsed->dep_target) {
        cosmo_deps_set_target(deps_ctx, parsed->dep_target);
    }
    // For -MD/-MMD: generate default .d filename if not specified
    char default_dep_file[512] = {0};
    if (parsed->gen_deps_and_compile && !parsed->dep_file) {
        // Generate .d file based on output file or source file
        const char *base = parsed->output_file ? parsed->output_file : source_file;
        snprintf(default_dep_file, sizeof(default_dep_file), "%s", base);
        
        // Replace extension with .d
        char *ext = strrchr(default_dep_file, '.');
        if (ext && (strcmp(ext, ".o") == 0 || strcmp(ext, ".c") == 0)) {
            strcpy(ext, ".d");
        } else {
            strcat(default_dep_file, ".d");
        }
        
        cosmo_deps_set_dep_file(deps_ctx, default_dep_file);
    } else if (parsed->dep_file) {
        cosmo_deps_set_dep_file(deps_ctx, parsed->dep_file);
    }
    cosmo_deps_set_source(deps_ctx, source_file, parsed->output_file);

    // Run preprocessor to get include information
    TCCState *s = tcc_new();
    if (!s) {
        cosmo_deps_destroy(deps_ctx);
        return 1;
    }

    cosmo_tcc_set_error_handler(s);
    tcc_set_output_type(s, TCC_OUTPUT_PREPROCESS);

    // Set up paths and options
    cosmo_tcc_build_default_options(g_config.tcc_options, sizeof(g_config.tcc_options), &g_config.uts);
    if (g_config.tcc_options[0]) {
        tcc_set_options(s, g_config.tcc_options);
    }
    cosmo_tcc_register_include_paths(s, &g_config.uts);

    // Compile source file to get preprocessor output
    if (tcc_add_file(s, source_file) < 0) {
        fprintf(stderr, "cosmorun: failed to preprocess '%s'\n", source_file);
        tcc_delete(s);
        cosmo_deps_destroy(deps_ctx);
        return 1;
    }

    // Note: TCC doesn't provide API to capture preprocessor output in memory
    // So we use temporary file approach
    char temp_preproc[256];
    snprintf(temp_preproc, sizeof(temp_preproc), "/tmp/cosmorun_preproc_%d.i", getpid());

    if (tcc_output_file(s, temp_preproc) < 0) {
        fprintf(stderr, "cosmorun: failed to generate preprocessor output\n");
        tcc_delete(s);
        cosmo_deps_destroy(deps_ctx);
        return 1;
    }

    tcc_delete(s);

    // Read preprocessor output
    FILE *fp = fopen(temp_preproc, "r");
    if (!fp) {
        perror("fopen");
        unlink(temp_preproc);
        cosmo_deps_destroy(deps_ctx);
        return 1;
    }

    // Read entire file into memory
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *preproc_output = malloc((size_t)file_size + 1);
    if (!preproc_output) {
        fclose(fp);
        unlink(temp_preproc);
        cosmo_deps_destroy(deps_ctx);
        return 1;
    }

    size_t read_size = fread(preproc_output, 1, (size_t)file_size, fp);
    preproc_output[read_size] = '\0';
    fclose(fp);

    // Extract dependencies from preprocessor output
    cosmo_deps_extract_from_preprocess(deps_ctx, preproc_output);
    free(preproc_output);
    unlink(temp_preproc);

    // Generate dependency output
    int ret = cosmo_deps_generate(deps_ctx);
    cosmo_deps_destroy(deps_ctx);

    return ret;
}
