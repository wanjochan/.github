/* cosmo_tcc.h - TinyCC Integration Module Header
 * Extracted from cosmorun.c
 */

#ifndef COSMO_TCC_H
#define COSMO_TCC_H

#include "cosmo_libc.h"

//#define TCC_VERSION "0.9.28rc"
//#define CONFIG_TCC_PREDEFS 1
//#include "config.h"
//#include "libtcc.h"
#include "tcc.h"
#include "libtcc.h"

#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Symbol Entry
// ============================================================================

typedef struct {
    const char* name;
    void* address;
    int is_cached;
    uint32_t hash;
} cosmo_symbol_entry_t;

// ============================================================================
// TCC Configuration
// ============================================================================

typedef struct {
    int output_type;
    const char* output_file;
    int relocate;
    int run_entry;
} cosmo_tcc_config_t;

extern const cosmo_tcc_config_t COSMO_TCC_CONFIG_MEMORY;
extern const cosmo_tcc_config_t COSMO_TCC_CONFIG_OBJECT;

// ============================================================================
// Public API
// ============================================================================

// Runtime library
const char* cosmo_tcc_get_runtime_lib(void);
void cosmo_tcc_link_runtime(TCCState *s);

// Symbol management
const cosmo_symbol_entry_t* cosmo_tcc_get_builtin_symbols(void);
void cosmo_tcc_register_builtin_symbols(TCCState *s);

// Symbol resolution (dynamic loading from system libraries)
void* cosmorun_dlsym_libc(const char* symbol_name);

// State management
TCCState* cosmo_tcc_create_state(int output_type, const char* options,
                                  int enable_paths, int enable_resolver);
TCCState* cosmo_tcc_init_state(void);
void cosmo_tcc_state_cleanup(void* resource);

// Path management
void cosmo_tcc_register_include_paths(TCCState *s, const struct utsname *uts);
void cosmo_tcc_register_library_paths(TCCState *s);
void cosmo_tcc_add_path_if_exists(TCCState *s, const char *path, int include_mode);
void cosmo_tcc_register_env_paths(TCCState *s, const char *env_name, int include_mode);
bool cosmo_tcc_dir_exists(const char *path);
int cosmo_tcc_get_cached_path_count(void);
const char* cosmo_tcc_get_cached_path(int index);

// Our Module API
void* __import(const char* path);
void* __import_sym(void* module, const char* symbol);
void __import_free(void* module);

// Options
void cosmo_tcc_build_default_options(char *buffer, size_t size, const struct utsname *uts);
void cosmo_tcc_append_option(char *buffer, size_t size, const char *opt);

// Error handling
void cosmo_tcc_error_func(void *opaque, const char *msg);
void cosmo_tcc_set_error_handler(TCCState *s);

#ifdef __cplusplus
}
#endif

#endif // COSMO_TCC_H
