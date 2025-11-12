/* cosmo_deps.h - Dependency Generation for CosmoRun
 * Implements GCC-compatible dependency generation (-M/-MM/-MD/-MMD/-MP/-MT)
 * for incremental builds with Make/Ninja
 */

#ifndef COSMO_DEPS_H
#define COSMO_DEPS_H

#include "cosmo_libc.h"
#include "cosmo_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of included files to track
#define COSMO_DEPS_MAX_INCLUDES 1024
#define COSMO_DEPS_MAX_PATH_LEN 4096

// Dependency tracking context
typedef struct {
    // Array of included file paths
    char **includes;
    int include_count;
    int include_capacity;

    // Configuration
    int exclude_system;     // Exclude system headers (<>)
    int gen_phony;          // Generate phony targets
    const char *target;     // Custom target name (or NULL for default)
    const char *dep_file;   // Output file (or NULL for stdout)

    // Source file info
    const char *source_file;
    const char *output_file;
} cosmo_deps_ctx_t;

// Create and destroy dependency context
cosmo_deps_ctx_t* cosmo_deps_create(void);
void cosmo_deps_destroy(cosmo_deps_ctx_t *ctx);

// Configure dependency generation
void cosmo_deps_set_exclude_system(cosmo_deps_ctx_t *ctx, int exclude);
void cosmo_deps_set_phony_targets(cosmo_deps_ctx_t *ctx, int enable);
void cosmo_deps_set_target(cosmo_deps_ctx_t *ctx, const char *target);
void cosmo_deps_set_dep_file(cosmo_deps_ctx_t *ctx, const char *dep_file);
void cosmo_deps_set_source(cosmo_deps_ctx_t *ctx, const char *source, const char *output);

// Track included file
int cosmo_deps_add_include(cosmo_deps_ctx_t *ctx, const char *path, int is_system);

// Extract dependencies from preprocessor output (parses #line directives)
int cosmo_deps_extract_from_preprocess(cosmo_deps_ctx_t *ctx, const char *preprocess_output);

// Generate dependency output
int cosmo_deps_generate(cosmo_deps_ctx_t *ctx);

// Helper: Determine if path is a system header
int cosmo_deps_is_system_path(const char *path);

// Helper: Get default target name from source file
const char* cosmo_deps_get_default_target(const char *source_file, const char *output_file);

#ifdef __cplusplus
}
#endif

#endif // COSMO_DEPS_H
