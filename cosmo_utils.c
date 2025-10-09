/* cosmo_utils.c - Shared Utility Functions Implementation
 * Common utilities for cosmorun and cosmo_tcc modules
 */

#include "cosmo_utils.h"
//#include <limits.h>
//#include <sys/utsname.h>
//#ifndef _WIN32
//#include <dlfcn.h>
//#endif

// Forward declare config structure
typedef struct {
    char tcc_options[512];
    struct utsname uts;
    int trace_enabled;
    char include_paths[4096];
    char library_paths[4096];
    char host_libs[4096];
    int initialized;
} cosmorun_config_t;

// External references
extern cosmorun_config_t g_config;
void tcc_error_func(void *opaque, const char *msg);

// TCC functions are already declared in libtcc.h via cosmo_utils.h

// Forward declare cosmo_tcc functions
void cosmo_tcc_build_default_options(char *buffer, size_t size, const struct utsname *uts);
void cosmo_tcc_register_include_paths(TCCState *s, const struct utsname *uts);
void cosmo_tcc_register_library_paths(TCCState *s);

// ============================================================================
// Error Handling Implementation
// ============================================================================

void cosmorun_perror(cosmorun_result_t result, const char* context) {
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

// ============================================================================
// String Utilities Implementation
// ============================================================================

int str_iequals(const char *a, const char *b) {
    if (!a || !b) return 0;
    return strcasecmp(a, b) == 0;
}

int str_istartswith(const char *str, const char *prefix) {
    if (!str || !prefix) return 0;
    size_t prefix_len = strlen(prefix);
    return strncasecmp(str, prefix, prefix_len) == 0;
}

int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return 0;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

void append_string_option(char *buffer, size_t size, const char *opt) {
    if (!buffer || size == 0 || !opt || !*opt) return;

    size_t len = strlen(buffer);
    if (len >= size - 1) return;

    const char *prefix = len > 0 ? " " : "";
    snprintf(buffer + len, size - len, "%s%s", prefix, opt);
}

// ============================================================================
// Tracing/Debugging Implementation
// ============================================================================

int trace_enabled(void) {
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

void tracef(const char *fmt, ...) {
    if (!trace_enabled()) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[cosmorun] ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

// ============================================================================
// File System Utilities Implementation
// ============================================================================

int dir_exists(const char *path) {
    if (!path || !*path) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

// ============================================================================
// Host API Default Implementations
// ============================================================================

void host_api_log_default(const char *message) {
    if (!message) message = "";
    fprintf(stderr, "[cosmorun-host] %s\n", message);
}

int host_api_puts_default(const char *message) {
    if (!message) message = "";
    int rc = fputs(message, stdout);
    if (rc < 0) return rc;
    fputc('\n', stdout);
    fflush(stdout);
    return 0;
}

int host_api_write_default(const char *data, size_t length) {
    if (!data) return -1;
    if (length == 0) length = strlen(data);
    size_t written = fwrite(data, 1, length, stdout);
    fflush(stdout);
    return written == length ? (int)written : -1;
}

const char *host_api_getenv_default(const char *name) {
    if (!name || !*name) return NULL;
    return getenv(name);
}

// ============================================================================
// Compilation Cache Management
// ============================================================================

// Include tcc.h for direct field access in cache functions
// This is needed because libtcc.h only provides opaque TCCState
#include "tcc.h"

// Temporarily undefine TCC's memory macros for standard C functions
#ifdef malloc
#define COSMO_UTILS_TCC_MALLOC_DEFINED
#undef malloc
#undef free
#undef realloc
#endif

// Re-declare dlopen functions after tcc.h (for non-Windows)
#ifndef _WIN32
extern void *dlopen(const char *filename, int flags);
extern void *dlsym(void *handle, const char *symbol);
extern int dlclose(void *handle);
extern char *dlerror(void);
#endif

int check_o_cache(const char* src_path, char* cb_path, size_t cb_path_size) {
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
        if (g_config.trace_enabled) {
            fprintf(stderr, "[cosmorun] No cache file: %s\n", cb_path);
        }
        return 0;
    }

    if (!src_exists) {
        // Release mode: only .o file exists, no source
        if (g_config.trace_enabled) {
            fprintf(stderr, "[cosmorun] Release mode: using .o file without source: %s\n", cb_path);
        }
        return 1;
    }

    // Development mode: compare timestamps
    int cache_is_newer = (cb_st.st_mtime >= src_st.st_mtime);
    if (g_config.trace_enabled) {
        fprintf(stderr, "[cosmorun] Development mode: source=%ld, cache=%ld, %s\n",
                src_st.st_mtime, cb_st.st_mtime,
                cache_is_newer ? "using cache" : "cache outdated");
    }

    return cache_is_newer;
}

int save_o_cache(const char* src_path, TCCState* s) {
    if (!src_path || !s) return -1;

    char cb_path[PATH_MAX];
    size_t len = strlen(src_path);
    if (len < 3 || !ends_with(src_path, ".c")) return -1;

    struct utsname uts;
    if (uname(&uts) != 0) return -1;

    // Generate arch-specific .o name
    snprintf(cb_path, sizeof(cb_path), "%.*s.%s.o", (int)(len - 2), src_path, uts.machine);

    tracef("saving cache to '%s'", cb_path);

    // Save and restore output_type
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

void* load_o_file(const char* path) {
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

    char tcc_options[512] = {0};
    cosmo_tcc_build_default_options(tcc_options, sizeof(tcc_options), &uts);
    if (tcc_options[0]) {
        tcc_set_options(s, tcc_options);
    }

    cosmo_tcc_register_include_paths(s, &uts);
    cosmo_tcc_register_library_paths(s);

    // NOTE: Do NOT register builtin symbols or runtime library for .o files
    // They are already embedded and re-registering causes "defined twice" errors

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

// ============================================================================
// Input Validation and Safety
// ============================================================================

// API declarations for user code
const char* COSMORUN_API_DECLARATIONS =
    "// Auto-injected cosmorun API declarations\n"
    "extern void* __import(const char* path);\n"
    "extern void* __sym(void* module, const char* symbol);\n"
    "\n";

int validate_string_param(const char* str, const char* param_name, size_t max_len) {
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

int validate_file_path(const char* path) {
    if (!validate_string_param(path, "file path", COSMORUN_MAX_PATH_SIZE)) {
        return 0;
    }

    // Check for dangerous path patterns
    if (strstr(path, "..") || strstr(path, "//")) {
        fprintf(stderr, "cosmorun: potentially unsafe path: %s\n", path);
        return 0;
    }

    return 1;
}

char* inject_api_declarations(const char* user_code) {
    if (!validate_string_param(user_code, "user code", COSMORUN_MAX_CODE_SIZE)) {
        return NULL;
    }

    size_t decl_len = strlen(COSMORUN_API_DECLARATIONS);
    size_t user_len = strlen(user_code);
    size_t total_len = decl_len + user_len + 1;

    if (total_len > COSMORUN_MAX_CODE_SIZE) {
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

char* read_file_with_api_declarations(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        cosmorun_perror(COSMORUN_ERROR_FILE_NOT_FOUND, filename);
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(file);
        cosmorun_perror(COSMORUN_ERROR_FILE_NOT_FOUND, "file size detection");
        return NULL;
    }

    // Allocate memory: API declarations + file content + null terminator
    size_t decl_len = strlen(COSMORUN_API_DECLARATIONS);
    size_t total_len = decl_len + file_size + 1;

    char* enhanced_code = malloc(total_len);
    if (!enhanced_code) {
        fclose(file);
        cosmorun_perror(COSMORUN_ERROR_MEMORY, "read_file_with_api_declarations");
        return NULL;
    }

    // Copy API declarations first
    strncpy(enhanced_code, COSMORUN_API_DECLARATIONS, decl_len);
    enhanced_code[decl_len] = '\0';

    // Read file content
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

// ============================================================================
// Platform Abstraction Layer
// ============================================================================
// Note: This must be after tcc.h include to avoid macro conflicts

#ifdef _WIN32
static const char* win32_get_path_separator(void) {
    return ";";
}

static const char* win32_dlerror(void) {
    return "Windows error";
}

platform_ops_t g_platform_ops = {
    .dlopen = (void*(*)(const char*, int))LoadLibraryA,
    .dlsym = (void*(*)(void*, const char*))GetProcAddress,
    .dlclose = (int(*)(void*))FreeLibrary,
    .dlerror = win32_dlerror,
    .get_path_separator = win32_get_path_separator
};
#else
static const char* posix_get_path_separator(void) {
    return ":";
}

platform_ops_t g_platform_ops = {
    .dlopen = dlopen,
    .dlsym = dlsym,
    .dlclose = dlclose,
    .dlerror = (const char*(*)(void))dlerror,
    .get_path_separator = posix_get_path_separator
};
#endif

const char* get_platform_name(void) {
#ifdef _WIN32
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}
