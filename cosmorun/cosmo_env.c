/* cosmo_env.c - Environment Variable Support for GCC/Clang Compatibility
 *
 * This module provides environment variable support for cosmorun to be
 * compatible with gcc/clang build systems. Supported variables:
 * - C_INCLUDE_PATH: Additional include directories (colon-separated)
 * - CPLUS_INCLUDE_PATH: C++ include directories (colon-separated)
 * - LIBRARY_PATH: Additional library directories (colon-separated)
 * - LD_LIBRARY_PATH: Runtime library search path (colon-separated)
 * - PKG_CONFIG_PATH: pkg-config search path (colon-separated)
 * - CFLAGS: Additional compiler flags
 * - LDFLAGS: Additional linker flags
 */

#include "cosmo_libc.h"
#include "cosmo_tcc.h"
#include "cosmo_utils.h"

#include <stdlib.h>
#include <string.h>

// Platform-specific path separator
#ifdef _WIN32
#define PATH_SEPARATOR ';'
#define PATH_SEPARATOR_STR ";"
#else
#define PATH_SEPARATOR ':'
#define PATH_SEPARATOR_STR ":"
#endif

/* Split path string by separator and call callback for each path */
static void split_paths(const char *path_str, void (*callback)(TCCState *s, const char *path, void *user),
                        TCCState *s, void *user_data) {
    if (!path_str || !*path_str || !callback) return;

    char *path_copy = strdup(path_str);
    if (!path_copy) return;

    char *path = path_copy;
    char *next;

    while (path) {
        // Find next separator
        next = strchr(path, PATH_SEPARATOR);
        if (next) {
            *next = '\0';
            next++;
        }

        // Skip empty paths
        if (*path) {
            callback(s, path, user_data);
        }

        path = next;
    }

    free(path_copy);
}

/* Callback to add include path */
static void add_include_path_cb(TCCState *s, const char *path, void *user_data) {
    (void)user_data;  // Unused

    // Check if directory exists
    if (!cosmo_tcc_dir_exists(path)) {
        if (trace_enabled()) {
            tracef("[cosmorun] Skipping non-existent include path: %s\n", path);
        }
        return;
    }

    tcc_add_include_path(s, path);

    if (trace_enabled()) {
        tracef("[cosmorun] Added include path from environment: %s\n", path);
    }
}

/* Callback to add library path */
static void add_library_path_cb(TCCState *s, const char *path, void *user_data) {
    (void)user_data;  // Unused

    // Check if directory exists
    if (!cosmo_tcc_dir_exists(path)) {
        if (trace_enabled()) {
            tracef("[cosmorun] Skipping non-existent library path: %s\n", path);
        }
        return;
    }

    tcc_add_library_path(s, path);

    if (trace_enabled()) {
        tracef("[cosmorun] Added library path from environment: %s\n", path);
    }
}

/* Apply C_INCLUDE_PATH environment variable */
void cosmo_env_apply_c_include_path(TCCState *s) {
    const char *include_path = getenv("C_INCLUDE_PATH");
    if (include_path && *include_path) {
        if (trace_enabled()) {
            tracef("[cosmorun] Processing C_INCLUDE_PATH: %s\n", include_path);
        }
        split_paths(include_path, add_include_path_cb, s, NULL);
    }
}

/* Apply CPLUS_INCLUDE_PATH environment variable (for C++ compatibility) */
void cosmo_env_apply_cplus_include_path(TCCState *s) {
    const char *include_path = getenv("CPLUS_INCLUDE_PATH");
    if (include_path && *include_path) {
        if (trace_enabled()) {
            tracef("[cosmorun] Processing CPLUS_INCLUDE_PATH: %s\n", include_path);
        }
        split_paths(include_path, add_include_path_cb, s, NULL);
    }
}

/* Apply LIBRARY_PATH environment variable */
void cosmo_env_apply_library_path(TCCState *s) {
    const char *library_path = getenv("LIBRARY_PATH");
    if (library_path && *library_path) {
        if (trace_enabled()) {
            tracef("[cosmorun] Processing LIBRARY_PATH: %s\n", library_path);
        }
        split_paths(library_path, add_library_path_cb, s, NULL);
    }
}

/* Apply LD_LIBRARY_PATH environment variable (runtime linking) */
void cosmo_env_apply_ld_library_path(TCCState *s) {
    const char *ld_library_path = getenv("LD_LIBRARY_PATH");
    if (ld_library_path && *ld_library_path) {
        if (trace_enabled()) {
            tracef("[cosmorun] Processing LD_LIBRARY_PATH: %s\n", ld_library_path);
        }
        // For runtime linking, we add these as library search paths
        split_paths(ld_library_path, add_library_path_cb, s, NULL);
    }
}

/* Apply PKG_CONFIG_PATH environment variable */
void cosmo_env_apply_pkg_config_path(TCCState *s) {
    const char *pkg_config_path = getenv("PKG_CONFIG_PATH");
    if (pkg_config_path && *pkg_config_path) {
        if (trace_enabled()) {
            tracef("[cosmorun] PKG_CONFIG_PATH found: %s\n", pkg_config_path);
            tracef("[cosmorun] Note: pkg-config integration not yet fully implemented\n");
        }
        // TODO: Implement pkg-config file parsing and path extraction
        // For now, we just log the presence of PKG_CONFIG_PATH
    }
}

/* Apply CFLAGS environment variable */
void cosmo_env_apply_cflags(TCCState *s) {
    const char *cflags = getenv("CFLAGS");
    if (cflags && *cflags) {
        if (trace_enabled()) {
            tracef("[cosmorun] Applying CFLAGS: %s\n", cflags);
        }
        tcc_set_options(s, cflags);
    }
}

/* Apply LDFLAGS environment variable */
void cosmo_env_apply_ldflags(TCCState *s) {
    const char *ldflags = getenv("LDFLAGS");
    if (ldflags && *ldflags) {
        if (trace_enabled()) {
            tracef("[cosmorun] Applying LDFLAGS: %s\n", ldflags);
        }
        tcc_set_options(s, ldflags);
    }
}

/* Apply all environment variables */
void cosmo_env_apply_all(TCCState *s) {
    if (!s) return;

    // Apply include paths
    cosmo_env_apply_c_include_path(s);
    cosmo_env_apply_cplus_include_path(s);

    // Apply library paths
    cosmo_env_apply_library_path(s);
    cosmo_env_apply_ld_library_path(s);

    // Apply pkg-config path
    cosmo_env_apply_pkg_config_path(s);

    // Apply compiler/linker flags
    cosmo_env_apply_cflags(s);
    cosmo_env_apply_ldflags(s);
}
