/* cosmo_utils.h - Shared Utility Functions
 * Common utilities for cosmorun and cosmo_tcc modules
 */

#ifndef COSMO_UTILS_H
#define COSMO_UTILS_H

#include "cosmo_libc.h"
#include "libtcc.h"

#ifdef __cplusplus
extern "C" {
#endif

// Constants first (needed by struct definitions)
#define COSMORUN_MAX_CODE_SIZE      98304   // 96KB for large programs
#define COSMORUN_MAX_PATH_SIZE      4096
#define COSMORUN_MAX_OPTIONS_SIZE   512     // TCC options buffer

/* Argument parsing result structure */
typedef struct {
    int inline_mode;
    const char *inline_code;
    int inline_code_index;
    int dashdash_index;
    int source_index;  // First source file index (for compatibility)
    int *source_indices;  // Array of all source file indices
    int source_count;  // Number of source files
    const char *output_file;  // -o output file
    int compile_only;         // -c flag (object file only)
    int verbose;              // -v/-vv flag (show paths and config)
    int preprocess_only;      // -E flag (preprocessor only)
} parse_result_t;

// Global config structure
typedef struct {
    char tcc_options[COSMORUN_MAX_OPTIONS_SIZE];
    struct utsname uts;
    int trace_enabled;
    int initialized;
} cosmorun_config_t;
// ============================================================================
// Error Handling
// ============================================================================

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

void cosmorun_perror(cosmorun_result_t result, const char* context);

// ============================================================================
// String Utilities
// ============================================================================

int str_iequals(const char *a, const char *b);
int str_istartswith(const char *str, const char *prefix);
int ends_with(const char *str, const char *suffix);

void append_string_option(char *buffer, size_t size, const char *opt);

// ============================================================================
// Tracing/Debugging
// ============================================================================

int trace_enabled(void);
void tracef(const char *fmt, ...);

// ============================================================================
// File System Utilities
// ============================================================================

int dir_exists(const char *path);

// ============================================================================
// Host API Default Implementations
// ============================================================================

void host_api_log_default(const char *message);
int host_api_puts_default(const char *message);
int host_api_write_default(const char *data, size_t length);
const char *host_api_getenv_default(const char *name);

// ============================================================================
// Platform Abstraction Layer
// ============================================================================

typedef struct {
    void* (*dlopen)(const char* path, int flags);
    void* (*dlsym)(void* handle, const char* symbol);
    int (*dlclose)(void* handle);
    const char* (*dlerror)(void);
    const char* (*get_path_separator)(void);
} platform_ops_t;

extern platform_ops_t g_platform_ops;

const char* get_platform_name(void);

// ============================================================================
// Compilation Cache Management
// ============================================================================

int check_o_cache(const char* src_path, char* cb_path, size_t cb_path_size);
int save_o_cache(const char* src_path, TCCState* s);
void* load_o_file(const char* path);

// Validation functions
int validate_string_param(const char* str, const char* param_name, size_t max_len);
int validate_file_path(const char* path);

// API declaration injection
extern const char* COSMORUN_API_DECLARATIONS;
char* inject_api_declarations(const char* user_code);
char* read_file_with_api_declarations(const char* filename);

// ============================================================================
// Resource Management (RAII Pattern)
// ============================================================================

typedef void (*cosmo_cleanup_fn)(void* resource);

typedef struct {
    void* resource;
    cosmo_cleanup_fn cleanup_fn;
    const char* name;  // for debugging
} cosmo_resource_t;

// Create and manage resources
cosmo_resource_t cosmo_resource_create(void* resource, cosmo_cleanup_fn cleanup_fn, const char* name);
void cosmo_resource_cleanup(cosmo_resource_t* manager);

// Common cleanup functions
void cosmo_cleanup_memory(void* resource);
void cosmo_cleanup_char_array(char*** argv);

// RAII macros (GCC/Clang)
#if defined(__GNUC__) || defined(__clang__)
#define COSMO_AUTO_CLEANUP(cleanup_fn) __attribute__((cleanup(cleanup_fn)))
#define COSMO_AUTO_MEMORY char* COSMO_AUTO_CLEANUP(cosmo_cleanup_memory)
#define COSMO_AUTO_CHAR_ARRAY char** COSMO_AUTO_CLEANUP(cosmo_cleanup_char_array)
#else
#define COSMO_AUTO_CLEANUP(cleanup_fn)
#define COSMO_AUTO_MEMORY char*
#define COSMO_AUTO_CHAR_ARRAY char**
#endif

// ============================================================================
// Crash Handler (Signal Management)
// ============================================================================

#include <setjmp.h>
#include <signal.h>

typedef struct {
    const char* source_file;
    const char* function;
    int line;
    void* user_context;
    jmp_buf recovery;
    int recovery_active;
} cosmo_crash_context_t;

// Crash handler API
void cosmo_crash_init(void);
void cosmo_crash_set_context(const char* file, const char* func, int line);
void cosmo_crash_set_user_context(void* ctx);
int cosmo_crash_try_recover(int signal);
cosmo_crash_context_t* cosmo_crash_get_context(void);

// ============================================================================
// Command-Line Argument Processing
// ============================================================================

// Option descriptor for filtering
typedef struct {
    const char* option;      // e.g., "-o", "-c", "--eval"
    int takes_value;         // 1 if option takes a value, 0 otherwise
} cosmo_skip_option_t;

// Find the position of "--" separator in argv
// Returns: index of "--" or -1 if not found
int cosmo_args_find_separator(int argc, char** argv);

// Build execution arguments starting from a given index
// Skips any remaining "--" separators
// Returns: newly allocated argv array (caller must free)
// out_argc: receives the count of arguments in returned array
char** cosmo_args_build_exec_argv(int argc, char** argv,
                                   int start_index,
                                   const char* program_name,
                                   int* out_argc);

// Build filtered argv, skipping specified options and their values
// Returns: newly allocated argv array (caller must free)
// skip_list: array of options to skip
// skip_count: number of elements in skip_list
char** cosmo_args_build_filtered_argv(int argc, char** argv,
                                       const cosmo_skip_option_t* skip_list,
                                       int skip_count);

// Free argv array allocated by cosmo_args_* functions
void cosmo_args_free(char** argv);

#ifdef __cplusplus
}
#endif

#endif // COSMO_UTILS_H
