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

// ============================================================================
// Input Validation and Safety
// ============================================================================

// Constants
#define COSMORUN_MAX_CODE_SIZE      98304   // 96KB for large programs
#define COSMORUN_MAX_PATH_SIZE      4096

// API declarations to inject into user code
extern const char* COSMORUN_API_DECLARATIONS;

// Validation functions
int validate_string_param(const char* str, const char* param_name, size_t max_len);
int validate_file_path(const char* path);

// API declaration injection
char* inject_api_declarations(const char* user_code);
char* read_file_with_api_declarations(const char* filename);

#ifdef __cplusplus
}
#endif

#endif // COSMO_UTILS_H
