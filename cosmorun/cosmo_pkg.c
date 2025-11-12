/* cosmo_pkg.c - Package Management Implementation
 * Implements package registry, installation, and management for CosmoRun
 */

#include "cosmo_pkg.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

// Platform-specific directory creation
#ifdef _WIN32
#include <direct.h>
#define mkdir_portable(path) _mkdir(path)
#else
#define mkdir_portable(path) mkdir(path, 0755)
#endif

// ============================================================================
// Helper Functions
// ============================================================================

// Trim whitespace from string
void pkg_trim(char *str) {
    if (!str) return;

    // Trim leading whitespace
    char *start = str;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n')) {
        start++;
    }

    // Trim trailing whitespace
    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n')) {
        *end = '\0';
        end--;
    }

    // Move trimmed string to beginning
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

// Get home directory path
int pkg_get_home_dir(char *buf, size_t size) {
    const char *home = getenv("HOME");
    if (!home) {
        home = getenv("USERPROFILE");  // Windows fallback
    }
    if (!home) {
        return -1;
    }

    snprintf(buf, size, "%s", home);
    return 0;
}

// Parse version requirement (e.g., ">=1.2.0" -> op=">=", version="1.2.0")
int pkg_parse_version_requirement(const char *req, char *op, char *version) {
    if (!req || !op || !version) return -1;

    const char *p = req;
    char *op_end = op;

    // Parse operator
    while (*p && (*p == '>' || *p == '<' || *p == '=' || *p == '!')) {
        *op_end++ = *p++;
    }
    *op_end = '\0';

    // If no operator, assume "="
    if (op[0] == '\0') {
        strcpy(op, "=");
        p = req;
    }

    // Parse version
    snprintf(version, PKG_MAX_VERSION_LEN, "%s", p);
    pkg_trim(version);

    return 0;
}

// Compare semantic versions (e.g., "1.2.3" vs "1.3.0")
// Returns: -1 if v1 < v2, 0 if equal, 1 if v1 > v2
int pkg_version_compare(const char *v1, const char *v2) {
    if (!v1 || !v2) return 0;

    int v1_major = 0, v1_minor = 0, v1_patch = 0;
    int v2_major = 0, v2_minor = 0, v2_patch = 0;

    sscanf(v1, "%d.%d.%d", &v1_major, &v1_minor, &v1_patch);
    sscanf(v2, "%d.%d.%d", &v2_major, &v2_minor, &v2_patch);

    if (v1_major != v2_major) return v1_major > v2_major ? 1 : -1;
    if (v1_minor != v2_minor) return v1_minor > v2_minor ? 1 : -1;
    if (v1_patch != v2_patch) return v1_patch > v2_patch ? 1 : -1;

    return 0;
}

// Check if version satisfies requirement
int pkg_version_satisfies(const char *version, const char *requirement) {
    char op[8];
    char req_version[PKG_MAX_VERSION_LEN];

    if (pkg_parse_version_requirement(requirement, op, req_version) != 0) {
        return 0;
    }

    int cmp = pkg_version_compare(version, req_version);

    if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) {
        return cmp == 0;
    } else if (strcmp(op, ">=") == 0) {
        return cmp >= 0;
    } else if (strcmp(op, ">") == 0) {
        return cmp > 0;
    } else if (strcmp(op, "<=") == 0) {
        return cmp <= 0;
    } else if (strcmp(op, "<") == 0) {
        return cmp < 0;
    } else if (strcmp(op, "!=") == 0) {
        return cmp != 0;
    }

    return 0;  // Unknown operator
}

// ============================================================================
// Registry Management
// ============================================================================

// Create package registry
pkg_registry_t* pkg_registry_create(void) {
    pkg_registry_t *registry = calloc(1, sizeof(pkg_registry_t));
    if (!registry) return NULL;

    // Allocate available packages array
    registry->available_capacity = 64;
    registry->available_packages = calloc((size_t)registry->available_capacity, sizeof(pkg_metadata_t));
    if (!registry->available_packages) {
        free(registry);
        return NULL;
    }

    // Allocate installed packages array
    registry->installed_capacity = 32;
    registry->installed_packages = calloc((size_t)registry->installed_capacity, sizeof(pkg_metadata_t));
    if (!registry->installed_packages) {
        free(registry->available_packages);
        free(registry);
        return NULL;
    }

    registry->available_count = 0;
    registry->installed_count = 0;

    return registry;
}

// Destroy package registry
void pkg_registry_destroy(pkg_registry_t *registry) {
    if (!registry) return;

    if (registry->available_packages) {
        free(registry->available_packages);
    }
    if (registry->installed_packages) {
        free(registry->installed_packages);
    }

    free(registry);
}

// Ensure package directories exist
int pkg_ensure_directories(pkg_registry_t *registry) {
    if (!registry) return -1;

    struct stat st;

    // Create home directory if needed
    if (stat(registry->home_dir, &st) != 0) {
        if (mkdir_portable(registry->home_dir) != 0 && errno != EEXIST) {
            return -1;
        }
    }

    // Create packages directory
    if (stat(registry->packages_dir, &st) != 0) {
        if (mkdir_portable(registry->packages_dir) != 0 && errno != EEXIST) {
            return -1;
        }
    }

    // Create cache directory
    if (stat(registry->cache_dir, &st) != 0) {
        if (mkdir_portable(registry->cache_dir) != 0 && errno != EEXIST) {
            return -1;
        }
    }

    return 0;
}

// Initialize package registry
int pkg_registry_init(pkg_registry_t *registry) {
    if (!registry) return -1;

    char home[PKG_MAX_PATH_LEN];
    if (pkg_get_home_dir(home, sizeof(home)) != 0) {
        return -1;
    }

    // Set up directory paths
    snprintf(registry->home_dir, sizeof(registry->home_dir), "%s/%s", home, PKG_HOME_DIR);
    snprintf(registry->packages_dir, sizeof(registry->packages_dir), "%s/%s", registry->home_dir, PKG_PACKAGES_DIR);
    snprintf(registry->cache_dir, sizeof(registry->cache_dir), "%s/%s", registry->home_dir, PKG_CACHE_DIR);
    snprintf(registry->registry_file, sizeof(registry->registry_file), "%s/%s", registry->home_dir, PKG_REGISTRY_FILE);

    // Create directories
    if (pkg_ensure_directories(registry) != 0) {
        return -1;
    }

    // Load mock registry data
    pkg_load_mock_registry(registry);

    return 0;
}

// Make package installation directory
int pkg_make_package_dir(pkg_registry_t *registry, const char *name) {
    if (!registry || !name) return -1;

    char pkg_dir[PKG_MAX_PATH_LEN];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s/%s", registry->packages_dir, name);

    struct stat st;
    if (stat(pkg_dir, &st) != 0) {
        if (mkdir_portable(pkg_dir) != 0 && errno != EEXIST) {
            return -1;
        }
    }

    return 0;
}

// ============================================================================
// Mock Registry (for testing without network)
// ============================================================================

// Create mock package
pkg_metadata_t pkg_create_mock_package(const char *name, const char *version,
                                       const char *description) {
    pkg_metadata_t pkg = {0};

    snprintf(pkg.name, sizeof(pkg.name), "%s", name);
    snprintf(pkg.version, sizeof(pkg.version), "%s", version);
    snprintf(pkg.description, sizeof(pkg.description), "%s", description);
    snprintf(pkg.author, sizeof(pkg.author), "CosmoRun Community");
    snprintf(pkg.download_url, sizeof(pkg.download_url), "https://packages.cosmorun.dev/%s-%s.tar.gz", name, version);

    pkg.state = PKG_STATE_NOT_INSTALLED;
    pkg.dep_count = 0;
    pkg.file_count = 0;

    return pkg;
}

// Load mock registry data
int pkg_load_mock_registry(pkg_registry_t *registry) {
    if (!registry) return -1;

    // Add mock packages to registry
    registry->available_packages[registry->available_count++] =
        pkg_create_mock_package("test-package", "1.0.0", "A test package for demonstration");

    registry->available_packages[registry->available_count++] =
        pkg_create_mock_package("libhttp", "2.1.3", "HTTP client library for C");

    registry->available_packages[registry->available_count++] =
        pkg_create_mock_package("libjson", "1.5.0", "JSON parser and serializer");

    registry->available_packages[registry->available_count++] =
        pkg_create_mock_package("libcrypto", "3.0.1", "Cryptography library with common algorithms");

    registry->available_packages[registry->available_count++] =
        pkg_create_mock_package("libmath", "1.2.0", "Advanced mathematical functions");

    registry->available_packages[registry->available_count++] =
        pkg_create_mock_package("libnet", "2.0.0", "Network utilities and protocols");

    registry->available_packages[registry->available_count++] =
        pkg_create_mock_package("libutil", "1.1.0", "Common utility functions");

    registry->available_packages[registry->available_count++] =
        pkg_create_mock_package("cosmo-test", "1.0.0", "Testing framework for CosmoRun");

    return 0;
}

// ============================================================================
// Package Queries
// ============================================================================

// Find package in available packages
pkg_metadata_t* pkg_find_available(pkg_registry_t *registry, const char *name) {
    if (!registry || !name) return NULL;

    for (int i = 0; i < registry->available_count; i++) {
        if (strcmp(registry->available_packages[i].name, name) == 0) {
            return &registry->available_packages[i];
        }
    }

    return NULL;
}

// Find package in installed packages
pkg_metadata_t* pkg_find_installed(pkg_registry_t *registry, const char *name) {
    if (!registry || !name) return NULL;

    for (int i = 0; i < registry->installed_count; i++) {
        if (strcmp(registry->installed_packages[i].name, name) == 0) {
            return &registry->installed_packages[i];
        }
    }

    return NULL;
}

// Check if package is installed
int pkg_is_installed(pkg_registry_t *registry, const char *name) {
    return pkg_find_installed(registry, name) != NULL;
}

// ============================================================================
// Package Operations
// ============================================================================

// Install package
pkg_result_t pkg_install(pkg_registry_t *registry, const char *name, const char *version) {
    pkg_result_t result = {0};

    if (!registry || !name) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Invalid arguments");
        return result;
    }

    // Check if already installed
    if (pkg_is_installed(registry, name)) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Package '%s' is already installed", name);
        return result;
    }

    // Find package in available packages
    pkg_metadata_t *pkg = pkg_find_available(registry, name);
    if (!pkg) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Package '%s' not found in registry", name);
        return result;
    }

    // Check version compatibility if specified
    if (version && !pkg_version_satisfies(pkg->version, version)) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Package version %s does not satisfy requirement %s", pkg->version, version);
        return result;
    }

    // Create package directory
    if (pkg_make_package_dir(registry, name) != 0) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Failed to create package directory");
        return result;
    }

    // Add to installed packages
    if (registry->installed_count >= registry->installed_capacity) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Maximum installed packages limit reached");
        return result;
    }

    // Copy package metadata to installed list
    memcpy(&registry->installed_packages[registry->installed_count], pkg, sizeof(pkg_metadata_t));
    registry->installed_packages[registry->installed_count].state = PKG_STATE_INSTALLED;

    // Set installation date
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(registry->installed_packages[registry->installed_count].install_date,
            sizeof(registry->installed_packages[registry->installed_count].install_date),
            "%Y-%m-%d", tm_info);

    registry->installed_count++;
    result.success = 1;
    result.packages_affected = 1;

    printf("✓ Successfully installed %s %s\n", pkg->name, pkg->version);

    return result;
}

// Remove package
pkg_result_t pkg_remove(pkg_registry_t *registry, const char *name) {
    pkg_result_t result = {0};

    if (!registry || !name) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Invalid arguments");
        return result;
    }

    // Find package in installed packages
    int found_idx = -1;
    for (int i = 0; i < registry->installed_count; i++) {
        if (strcmp(registry->installed_packages[i].name, name) == 0) {
            found_idx = i;
            break;
        }
    }

    if (found_idx == -1) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Package '%s' is not installed", name);
        return result;
    }

    // Remove package by shifting array
    for (int i = found_idx; i < registry->installed_count - 1; i++) {
        memcpy(&registry->installed_packages[i], &registry->installed_packages[i + 1], sizeof(pkg_metadata_t));
    }
    registry->installed_count--;

    result.success = 1;
    result.packages_affected = 1;

    printf("✓ Successfully removed %s\n", name);

    return result;
}

// Update package
pkg_result_t pkg_update(pkg_registry_t *registry, const char *name) {
    pkg_result_t result = {0};

    if (!registry || !name) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Invalid arguments");
        return result;
    }

    // Find installed package
    pkg_metadata_t *installed = pkg_find_installed(registry, name);
    if (!installed) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Package '%s' is not installed", name);
        return result;
    }

    // Find available package
    pkg_metadata_t *available = pkg_find_available(registry, name);
    if (!available) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Package '%s' not found in registry", name);
        return result;
    }

    // Compare versions
    int cmp = pkg_version_compare(available->version, installed->version);
    if (cmp <= 0) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Package '%s' is already at latest version %s", name, installed->version);
        return result;
    }

    // Update package metadata
    char old_version[PKG_MAX_VERSION_LEN];
    snprintf(old_version, sizeof(old_version), "%s", installed->version);

    memcpy(installed, available, sizeof(pkg_metadata_t));
    installed->state = PKG_STATE_INSTALLED;

    // Update installation date
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(installed->install_date, sizeof(installed->install_date), "%Y-%m-%d", tm_info);

    result.success = 1;
    result.packages_affected = 1;

    printf("✓ Updated %s from %s to %s\n", name, old_version, available->version);

    return result;
}

// Update all packages
pkg_result_t pkg_update_all(pkg_registry_t *registry) {
    pkg_result_t result = {0};

    if (!registry) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Invalid arguments");
        return result;
    }

    int updated = 0;
    for (int i = 0; i < registry->installed_count; i++) {
        pkg_result_t update_result = pkg_update(registry, registry->installed_packages[i].name);
        if (update_result.success) {
            updated++;
        }
    }

    result.success = 1;
    result.packages_affected = updated;

    if (updated == 0) {
        printf("✓ All packages are up to date\n");
    } else {
        printf("✓ Updated %d package(s)\n", updated);
    }

    return result;
}

// Search packages
pkg_result_t pkg_search(pkg_registry_t *registry, const char *query) {
    pkg_result_t result = {0};

    if (!registry || !query) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Invalid arguments");
        return result;
    }

    printf("Searching for '%s'...\n\n", query);

    int found = 0;
    for (int i = 0; i < registry->available_count; i++) {
        pkg_metadata_t *pkg = &registry->available_packages[i];

        // Simple substring search in name and description
        if (strstr(pkg->name, query) || strstr(pkg->description, query)) {
            printf("  %s (%s) - %s\n", pkg->name, pkg->version, pkg->description);
            found++;
        }
    }

    if (found == 0) {
        printf("No packages found matching '%s'\n", query);
    } else {
        printf("\nFound %d package(s)\n", found);
    }

    result.success = 1;
    result.packages_affected = found;

    return result;
}

// List installed packages
pkg_result_t pkg_list(pkg_registry_t *registry) {
    pkg_result_t result = {0};

    if (!registry) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Invalid arguments");
        return result;
    }

    if (registry->installed_count == 0) {
        printf("No packages installed\n");
        result.success = 1;
        return result;
    }

    printf("Installed packages:\n\n");
    for (int i = 0; i < registry->installed_count; i++) {
        pkg_metadata_t *pkg = &registry->installed_packages[i];
        printf("  %s (%s) - installed %s\n", pkg->name, pkg->version, pkg->install_date);
    }

    printf("\nTotal: %d package(s)\n", registry->installed_count);

    result.success = 1;
    result.packages_affected = registry->installed_count;

    return result;
}

// Show package info
pkg_result_t pkg_info(pkg_registry_t *registry, const char *name) {
    pkg_result_t result = {0};

    if (!registry || !name) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Invalid arguments");
        return result;
    }

    // Try to find in available packages first
    pkg_metadata_t *pkg = pkg_find_available(registry, name);
    int is_installed = pkg_is_installed(registry, name);

    if (!pkg) {
        // Try installed packages
        pkg = pkg_find_installed(registry, name);
    }

    if (!pkg) {
        result.success = 0;
        snprintf(result.error_msg, sizeof(result.error_msg), "Package '%s' not found", name);
        return result;
    }

    // Display package information
    printf("Package: %s\n", pkg->name);
    printf("Version: %s\n", pkg->version);
    printf("Description: %s\n", pkg->description);
    printf("Author: %s\n", pkg->author);
    printf("Status: %s\n", is_installed ? "Installed" : "Not installed");

    if (is_installed) {
        pkg_metadata_t *installed_pkg = pkg_find_installed(registry, name);
        if (installed_pkg && installed_pkg->install_date[0]) {
            printf("Install Date: %s\n", installed_pkg->install_date);
        }
    }

    if (pkg->dep_count > 0) {
        printf("Dependencies:\n");
        for (int i = 0; i < pkg->dep_count; i++) {
            printf("  - %s %s\n", pkg->dependencies[i].name, pkg->dependencies[i].version);
        }
    }

    result.success = 1;
    result.packages_affected = 1;

    return result;
}

// ============================================================================
// Registry Persistence (simplified for MVP)
// ============================================================================

int pkg_registry_load(pkg_registry_t *registry) {
    // TODO: Load from JSON file
    // For now, mock registry is loaded in pkg_registry_init
    return 0;
}

int pkg_registry_save(pkg_registry_t *registry) {
    // TODO: Save to JSON file
    // For now, changes are only in memory
    return 0;
}
