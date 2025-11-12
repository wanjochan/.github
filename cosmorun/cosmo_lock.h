/* cosmo_lock.h - Dependency Lockfile System for CosmoRun
 * Ensures reproducible builds by locking exact dependency versions
 * Implements lockfile generation, loading, verification, and conflict resolution
 */

#ifndef COSMO_LOCK_H
#define COSMO_LOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Lockfile version
#define COSMO_LOCK_VERSION "1.0"
#define COSMO_LOCK_FILENAME "cosmo.lock"

// Maximum limits
#define COSMO_LOCK_MAX_DEPS 512
#define COSMO_LOCK_MAX_NAME_LEN 128
#define COSMO_LOCK_MAX_VERSION_LEN 64
#define COSMO_LOCK_MAX_URL_LEN 512
#define COSMO_LOCK_MAX_HASH_LEN 128
#define COSMO_LOCK_MAX_PATH_LEN 4096

// Dependency structure in lockfile
typedef struct cosmo_lock_dep {
    char name[COSMO_LOCK_MAX_NAME_LEN];          // Package name (e.g., "libhttp")
    char version[COSMO_LOCK_MAX_VERSION_LEN];    // Exact version (e.g., "2.1.3")
    char resolved[COSMO_LOCK_MAX_URL_LEN];       // Resolved URL (e.g., "registry://libhttp@2.1.3")
    char integrity[COSMO_LOCK_MAX_HASH_LEN];     // Integrity hash (e.g., "sha256:abc123...")

    // Nested dependencies (stored as comma-separated string for simplicity)
    char dependencies[COSMO_LOCK_MAX_PATH_LEN];  // e.g., "libnet:^1.0.0,libjson:>=1.2.0"

    // Metadata
    int installed;                                // Flag: is this dependency currently installed?
} cosmo_lock_dep_t;

// Lockfile context
typedef struct cosmo_lock_ctx {
    char lockfile_version[32];                    // Lockfile format version
    char timestamp[64];                           // ISO 8601 timestamp

    cosmo_lock_dep_t *dependencies;               // Array of locked dependencies
    int dep_count;                                // Number of dependencies
    int dep_capacity;                             // Allocated capacity

    // Configuration
    const char *lockfile_path;                    // Path to cosmo.lock (or NULL for default)
    const char *package_json_path;                // Path to cosmo.json (or NULL for default)

    // Conflict resolution state
    char error_message[512];                      // Last error message
} cosmo_lock_ctx_t;

// Version comparison result
typedef enum {
    VERSION_LESS = -1,
    VERSION_EQUAL = 0,
    VERSION_GREATER = 1,
    VERSION_INCOMPATIBLE = -999
} version_cmp_result_t;

// =============================================================================
// Core Functions
// =============================================================================

// Create and destroy lockfile context
cosmo_lock_ctx_t* cosmo_lock_create(void);
void cosmo_lock_destroy(cosmo_lock_ctx_t *ctx);

// Set custom paths (optional)
void cosmo_lock_set_lockfile_path(cosmo_lock_ctx_t *ctx, const char *path);
void cosmo_lock_set_package_json_path(cosmo_lock_ctx_t *ctx, const char *path);

// =============================================================================
// Lockfile Operations
// =============================================================================

// Generate lockfile from cosmo.json
// Reads cosmo.json, resolves all dependencies, and writes cosmo.lock
int cosmo_lock_generate(cosmo_lock_ctx_t *ctx);

// Load and parse existing lockfile
// Returns 0 on success, -1 on failure
int cosmo_lock_load(cosmo_lock_ctx_t *ctx);

// Save lockfile to disk
// Returns 0 on success, -1 on failure
int cosmo_lock_save(cosmo_lock_ctx_t *ctx);

// Verify installed packages match lockfile
// Checks that all locked dependencies are installed with correct versions
// Returns 0 if all match, -1 if mismatches found
int cosmo_lock_verify(cosmo_lock_ctx_t *ctx);

// Update specific dependency in lockfile
// Updates the locked version of a single package and regenerates lockfile
int cosmo_lock_update_dependency(cosmo_lock_ctx_t *ctx, const char *package_name);

// =============================================================================
// Dependency Management
// =============================================================================

// Add dependency to lockfile context
int cosmo_lock_add_dependency(cosmo_lock_ctx_t *ctx,
                              const char *name,
                              const char *version,
                              const char *resolved,
                              const char *integrity,
                              const char *dependencies);

// Find dependency by name
cosmo_lock_dep_t* cosmo_lock_find_dependency(cosmo_lock_ctx_t *ctx, const char *name);

// Remove dependency from lockfile context
int cosmo_lock_remove_dependency(cosmo_lock_ctx_t *ctx, const char *name);

// =============================================================================
// Version Resolution and Conflict Handling
// =============================================================================

// Resolve version conflicts between dependencies
// Analyzes dependency tree and finds compatible versions for all packages
// Returns 0 if resolved, -1 if unresolvable conflicts exist
int cosmo_lock_resolve_conflicts(cosmo_lock_ctx_t *ctx);

// Compare semantic versions (supports semver: 1.2.3, ^1.0.0, ~1.2.0, >=1.0.0, etc.)
version_cmp_result_t cosmo_lock_version_compare(const char *v1, const char *v2);

// Check if version satisfies requirement
// e.g., version="2.1.3", requirement="^2.0.0" -> returns 1 (true)
int cosmo_lock_version_satisfies(const char *version, const char *requirement);

// Parse semver string into components (major.minor.patch)
// Returns 0 on success, -1 on parse error
int cosmo_lock_parse_semver(const char *version, int *major, int *minor, int *patch);

// =============================================================================
// Integrity and Validation
// =============================================================================

// Calculate integrity hash for a package file
// Uses SHA-256 and returns hash in format "sha256:hexdigest"
int cosmo_lock_calculate_integrity(const char *package_path, char *integrity_out, size_t out_size);

// Verify package integrity against lockfile hash
int cosmo_lock_verify_integrity(const char *package_path, const char *expected_hash);

// =============================================================================
// JSON Serialization/Deserialization
// =============================================================================

// Serialize lockfile context to JSON string
// Caller must free returned string
char* cosmo_lock_to_json(cosmo_lock_ctx_t *ctx);

// Deserialize JSON string to lockfile context
// Returns 0 on success, -1 on parse error
int cosmo_lock_from_json(cosmo_lock_ctx_t *ctx, const char *json_str);

// =============================================================================
// Package Installation Integration
// =============================================================================

// Check if dependency should be installed (based on lockfile)
// Returns 1 if should install, 0 if skip, -1 on error
int cosmo_lock_should_install(cosmo_lock_ctx_t *ctx, const char *package_name);

// Mark dependency as installed
void cosmo_lock_mark_installed(cosmo_lock_ctx_t *ctx, const char *package_name, int installed);

// Get exact version to install for a package
// Returns locked version string or NULL if not found
const char* cosmo_lock_get_install_version(cosmo_lock_ctx_t *ctx, const char *package_name);

// =============================================================================
// Utility Functions
// =============================================================================

// Print lockfile summary to stdout
void cosmo_lock_print_summary(cosmo_lock_ctx_t *ctx);

// Get last error message
const char* cosmo_lock_get_error(cosmo_lock_ctx_t *ctx);

// Clear error state
void cosmo_lock_clear_error(cosmo_lock_ctx_t *ctx);

// Validate lockfile format and content
// Returns 0 if valid, -1 if invalid
int cosmo_lock_validate(cosmo_lock_ctx_t *ctx);

// Get current ISO 8601 timestamp
void cosmo_lock_get_timestamp(char *timestamp_out, size_t out_size);

// =============================================================================
// CLI Helper Functions
// =============================================================================

// High-level functions for CLI integration

// Install dependencies from lockfile (if exists), otherwise from cosmo.json
int cosmo_lock_install_all(cosmo_lock_ctx_t *ctx);

// Generate or update lockfile
int cosmo_lock_regenerate(cosmo_lock_ctx_t *ctx);

// Show diff between lockfile and installed packages
void cosmo_lock_show_diff(cosmo_lock_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // COSMO_LOCK_H
