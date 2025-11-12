/* cosmo_ldd.h - Dynamic Library Dependency Viewer
 *
 * Purpose: Display shared library dependencies similar to standard ldd
 *
 * Features:
 * - Parse ELF DT_NEEDED entries
 * - Resolve library paths using standard search paths
 * - Detect circular dependencies
 * - Support tree format output
 * - Show missing libraries
 */

#ifndef COSMO_LDD_H
#define COSMO_LDD_H

#include <stdint.h>
#include <stddef.h>

/* ========== Output Formats ========== */

typedef enum {
    LDD_FORMAT_STANDARD,    /* Standard ldd output format */
    LDD_FORMAT_TREE,        /* Tree-based hierarchical format */
    LDD_FORMAT_VERBOSE      /* Verbose format with additional details */
} ldd_output_format_t;

/* ========== Library Dependency Information ========== */

typedef struct ldd_library {
    char *name;                     /* Library name (e.g., "libc.so.6") */
    char *path;                     /* Resolved path or NULL if not found */
    uint64_t load_addr;             /* Load address (0 if not loaded) */
    int found;                      /* 1 if library was found, 0 otherwise */
    int processed;                  /* 1 if dependencies already processed */

    struct ldd_library **deps;     /* Array of dependencies */
    int num_deps;                   /* Number of dependencies */
    int deps_capacity;              /* Allocated capacity for deps array */
} ldd_library_t;

/* ========== LDD Context ========== */

typedef struct {
    ldd_library_t *root;            /* Root library being analyzed */
    ldd_library_t **all_libs;       /* All libraries encountered */
    int num_libs;                   /* Number of libraries */
    int libs_capacity;              /* Allocated capacity */

    char **search_paths;            /* Library search paths */
    int num_search_paths;           /* Number of search paths */

    ldd_output_format_t format;     /* Output format */
    int show_missing_only;          /* Only show missing libraries */
    int max_depth;                  /* Maximum recursion depth (-1 = unlimited) */
} ldd_context_t;

/* ========== Main API ========== */

/**
 * Create and initialize ldd context
 *
 * @return: Initialized context or NULL on error
 */
ldd_context_t* ldd_context_create(void);

/**
 * Free ldd context and all associated resources
 *
 * @param ctx: Context to free
 */
void ldd_context_free(ldd_context_t *ctx);

/**
 * Set output format
 *
 * @param ctx: LDD context
 * @param format: Output format to use
 */
void ldd_set_format(ldd_context_t *ctx, ldd_output_format_t format);

/**
 * Add library search path
 *
 * @param ctx: LDD context
 * @param path: Directory path to add
 * @return: 0 on success, -1 on error
 */
int ldd_add_search_path(ldd_context_t *ctx, const char *path);

/**
 * Analyze library dependencies
 *
 * @param ctx: LDD context
 * @param library_path: Path to library or executable to analyze
 * @return: 0 on success, -1 on error
 */
int ldd_analyze(ldd_context_t *ctx, const char *library_path);

/**
 * Print dependency information
 *
 * @param ctx: LDD context with analyzed dependencies
 */
void ldd_print(const ldd_context_t *ctx);

/* ========== Utility Functions ========== */

/**
 * Initialize default library search paths
 *
 * @param ctx: LDD context
 * @return: 0 on success, -1 on error
 */
int ldd_init_default_paths(ldd_context_t *ctx);

/**
 * Find library in search paths
 *
 * @param ctx: LDD context
 * @param lib_name: Library name (e.g., "libc.so.6")
 * @return: Allocated string with full path, or NULL if not found
 */
char* ldd_find_library(const ldd_context_t *ctx, const char *lib_name);

/**
 * Check if library creates circular dependency
 *
 * @param lib: Library to check
 * @param ancestor: Potential ancestor in dependency chain
 * @return: 1 if circular, 0 otherwise
 */
int ldd_is_circular(const ldd_library_t *lib, const ldd_library_t *ancestor);

#endif /* COSMO_LDD_H */
