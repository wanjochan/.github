/* cosmo_pkg.h - Package Management System for CosmoRun
 * Implements package installation, search, listing, updating, and removal
 * for distributed C libraries and tools
 */

#ifndef COSMO_PKG_H
#define COSMO_PKG_H

#include "cosmo_libc.h"
#include "cosmo_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

// Package registry constants
#define PKG_MAX_NAME_LEN        128
#define PKG_MAX_VERSION_LEN     32
#define PKG_MAX_DESC_LEN        512
#define PKG_MAX_AUTHOR_LEN      128
#define PKG_MAX_DEPS            32
#define PKG_MAX_FILES           256
#define PKG_MAX_PACKAGES        1024
#define PKG_MAX_PATH_LEN        4096

// Package registry directories (relative to ~/.cosmorun/)
#define PKG_HOME_DIR            ".cosmorun"
#define PKG_PACKAGES_DIR        "packages"
#define PKG_CACHE_DIR           "cache"
#define PKG_REGISTRY_FILE       "registry.json"

// Package installation states
typedef enum {
    PKG_STATE_NOT_INSTALLED = 0,
    PKG_STATE_INSTALLED,
    PKG_STATE_UPDATE_AVAILABLE,
    PKG_STATE_BROKEN
} pkg_state_t;

// Package file types
typedef enum {
    PKG_FILE_HEADER = 0,    // .h files
    PKG_FILE_LIBRARY,       // .a or .so files
    PKG_FILE_BINARY,        // executable files
    PKG_FILE_DOCS,          // documentation
    PKG_FILE_OTHER          // other files
} pkg_file_type_t;

// Package dependency descriptor
typedef struct {
    char name[PKG_MAX_NAME_LEN];
    char version[PKG_MAX_VERSION_LEN];  // Min version required (e.g., ">=1.2.0")
} pkg_dependency_t;

// Package file descriptor
typedef struct {
    char path[PKG_MAX_PATH_LEN];        // File path in package
    char install_path[PKG_MAX_PATH_LEN]; // Installation target path
    pkg_file_type_t type;
    size_t size;
} pkg_file_t;

// Package metadata structure
typedef struct {
    char name[PKG_MAX_NAME_LEN];
    char version[PKG_MAX_VERSION_LEN];
    char description[PKG_MAX_DESC_LEN];
    char author[PKG_MAX_AUTHOR_LEN];

    // Dependencies
    pkg_dependency_t dependencies[PKG_MAX_DEPS];
    int dep_count;

    // Files in package
    pkg_file_t files[PKG_MAX_FILES];
    int file_count;

    // Installation state
    pkg_state_t state;
    char install_date[32];

    // Download info (for mock registry)
    char download_url[PKG_MAX_PATH_LEN];
    char checksum[64];
} pkg_metadata_t;

// Package registry context
typedef struct {
    // Available packages in registry
    pkg_metadata_t *available_packages;
    int available_count;
    int available_capacity;

    // Installed packages
    pkg_metadata_t *installed_packages;
    int installed_count;
    int installed_capacity;

    // Configuration
    char home_dir[PKG_MAX_PATH_LEN];       // ~/.cosmorun/
    char packages_dir[PKG_MAX_PATH_LEN];    // ~/.cosmorun/packages/
    char cache_dir[PKG_MAX_PATH_LEN];       // ~/.cosmorun/cache/
    char registry_file[PKG_MAX_PATH_LEN];   // ~/.cosmorun/registry.json

    // Registry URL (for future HTTP API)
    char registry_url[PKG_MAX_PATH_LEN];
} pkg_registry_t;

// Package operation result
typedef struct {
    int success;
    char error_msg[512];
    int packages_affected;
} pkg_result_t;

// ============================================================================
// Core API Functions
// ============================================================================

// Registry management
pkg_registry_t* pkg_registry_create(void);
void pkg_registry_destroy(pkg_registry_t *registry);
int pkg_registry_init(pkg_registry_t *registry);
int pkg_registry_load(pkg_registry_t *registry);
int pkg_registry_save(pkg_registry_t *registry);

// Package operations
pkg_result_t pkg_install(pkg_registry_t *registry, const char *name, const char *version);
pkg_result_t pkg_remove(pkg_registry_t *registry, const char *name);
pkg_result_t pkg_update(pkg_registry_t *registry, const char *name);
pkg_result_t pkg_update_all(pkg_registry_t *registry);

// Package queries
pkg_result_t pkg_search(pkg_registry_t *registry, const char *query);
pkg_result_t pkg_list(pkg_registry_t *registry);
pkg_result_t pkg_info(pkg_registry_t *registry, const char *name);

// Package metadata management
pkg_metadata_t* pkg_find_available(pkg_registry_t *registry, const char *name);
pkg_metadata_t* pkg_find_installed(pkg_registry_t *registry, const char *name);
int pkg_is_installed(pkg_registry_t *registry, const char *name);

// Dependency resolution
int pkg_resolve_dependencies(pkg_registry_t *registry, pkg_metadata_t *pkg,
                             pkg_metadata_t **resolved, int *count);
int pkg_check_conflicts(pkg_registry_t *registry, pkg_metadata_t *pkg);

// Version comparison
int pkg_version_compare(const char *v1, const char *v2);
int pkg_version_satisfies(const char *version, const char *requirement);

// ============================================================================
// Utility Functions
// ============================================================================

// Directory and path management
int pkg_ensure_directories(pkg_registry_t *registry);
int pkg_get_home_dir(char *buf, size_t size);
int pkg_make_package_dir(pkg_registry_t *registry, const char *name);

// File operations
int pkg_download_file(const char *url, const char *dest_path);
int pkg_extract_package(const char *archive_path, const char *dest_dir);
int pkg_verify_checksum(const char *file_path, const char *expected_checksum);

// Mock registry (for testing without network)
int pkg_load_mock_registry(pkg_registry_t *registry);
pkg_metadata_t pkg_create_mock_package(const char *name, const char *version,
                                       const char *description);

// String utilities
void pkg_trim(char *str);
int pkg_parse_version_requirement(const char *req, char *op, char *version);

#ifdef __cplusplus
}
#endif

#endif // COSMO_PKG_H
