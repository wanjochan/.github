/**
 * @file cosmo_link.h
 * @brief Enhanced Dynamic Linking Support
 *
 * Provides advanced library search, resolution, and runtime path support:
 * - Library search path management (-L)
 * - Library name resolution (-l with lib prefix/.so/.a suffix)
 * - Runtime path support (-rpath/-Wl,-rpath with $ORIGIN expansion)
 * - Versioned library support (libfoo.so.1.2.3)
 * - Search priority: -L paths, LD_LIBRARY_PATH, system paths
 * - Library search caching for performance
 */

#ifndef COSMO_LINK_H
#define COSMO_LINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* ========== Constants ========== */

#define COSMO_LINK_MAX_SEARCH_PATHS   64
#define COSMO_LINK_MAX_RPATHS         32
#define COSMO_LINK_MAX_PATH_LEN       4096
#define COSMO_LINK_CACHE_SIZE         256

/* ========== Data Structures ========== */

/**
 * Library search context with path priority
 */
typedef struct {
    const char *search_paths[COSMO_LINK_MAX_SEARCH_PATHS];
    int search_path_count;

    const char *rpaths[COSMO_LINK_MAX_RPATHS];
    int rpath_count;

    /* Cache for resolved library paths */
    struct {
        char lib_name[256];
        char resolved_path[COSMO_LINK_MAX_PATH_LEN];
        int valid;
    } cache[COSMO_LINK_CACHE_SIZE];
    int cache_count;
} LibrarySearchContext;

/**
 * Library type
 */
typedef enum {
    LIB_TYPE_STATIC,    /* .a archive */
    LIB_TYPE_SHARED,    /* .so dynamic library */
    LIB_TYPE_UNKNOWN
} LibraryType;

/**
 * Resolved library information
 */
typedef struct {
    char path[COSMO_LINK_MAX_PATH_LEN];
    LibraryType type;
    int found;
} LibraryInfo;

/* ========== Library Search API ========== */

/**
 * Initialize library search context
 * @param ctx Context to initialize
 */
void cosmo_link_init_context(LibrarySearchContext *ctx);

/**
 * Add library search path (-L)
 * @param ctx Search context
 * @param path Directory path to add
 * @return 0 on success, -1 on error
 */
int cosmo_link_add_search_path(LibrarySearchContext *ctx, const char *path);

/**
 * Add runtime library path (-rpath)
 * @param ctx Search context
 * @param rpath Runtime path (may contain $ORIGIN)
 * @return 0 on success, -1 on error
 */
int cosmo_link_add_rpath(LibrarySearchContext *ctx, const char *rpath);

/**
 * Add LD_LIBRARY_PATH to search context
 * @param ctx Search context
 * @return Number of paths added
 */
int cosmo_link_add_ld_library_path(LibrarySearchContext *ctx);

/**
 * Add system default library paths
 * @param ctx Search context
 * @return Number of paths added
 */
int cosmo_link_add_system_paths(LibrarySearchContext *ctx);

/**
 * Resolve library name to full path
 *
 * Search order:
 * 1. -L paths (from context)
 * 2. LD_LIBRARY_PATH
 * 3. System paths (/usr/lib, /usr/local/lib, etc.)
 *
 * Naming conventions:
 * - Input: "math" or "ssl.1.1"
 * - Try: libmath.so, libmath.a, libssl.so.1.1, etc.
 *
 * @param ctx Search context
 * @param lib_name Library name (without lib prefix or extension)
 * @param prefer_static Prefer .a over .so if both exist
 * @param info Output resolved library information
 * @return 0 on success, -1 if not found
 */
int cosmo_link_resolve_library(LibrarySearchContext *ctx,
                                const char *lib_name,
                                int prefer_static,
                                LibraryInfo *info);

/**
 * Expand $ORIGIN in rpath
 * @param rpath Input path (may contain $ORIGIN)
 * @param executable_dir Directory containing the executable
 * @param output Output buffer for expanded path
 * @param output_size Size of output buffer
 * @return 0 on success, -1 on error
 */
int cosmo_link_expand_rpath(const char *rpath,
                             const char *executable_dir,
                             char *output,
                             size_t output_size);

/**
 * Clear library search cache
 * @param ctx Search context
 */
void cosmo_link_clear_cache(LibrarySearchContext *ctx);

/**
 * Parse -Wl,<options> linker flag
 * Handles: -Wl,-rpath,/path or -Wl,-rpath=/path
 * @param wl_arg The -Wl argument string
 * @param ctx Search context (for extracting rpath)
 * @return 0 on success, -1 on error
 */
int cosmo_link_parse_wl_option(const char *wl_arg, LibrarySearchContext *ctx);

/**
 * Check if library exists at path
 * @param path Full path to library file
 * @return 1 if exists, 0 otherwise
 */
int cosmo_link_library_exists(const char *path);

/**
 * Get library type from path
 * @param path Library file path
 * @return Library type
 */
LibraryType cosmo_link_get_library_type(const char *path);

/**
 * Normalize library name (handle versioned names)
 * Examples:
 *   "ssl" -> "ssl"
 *   "ssl.1.1" -> "ssl.1.1"
 *   "libssl.so" -> "ssl"
 * @param lib_name Input library name
 * @param output Output buffer
 * @param output_size Size of output buffer
 * @return 0 on success, -1 on error
 */
int cosmo_link_normalize_library_name(const char *lib_name,
                                       char *output,
                                       size_t output_size);

/* ========== Debugging/Diagnostics ========== */

/**
 * Print library search paths (for debugging)
 * @param ctx Search context
 */
void cosmo_link_print_search_paths(const LibrarySearchContext *ctx);

/**
 * Print library search statistics
 * @param ctx Search context
 */
void cosmo_link_print_stats(const LibrarySearchContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* COSMO_LINK_H */
