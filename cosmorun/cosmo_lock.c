/* cosmo_lock.c - Dependency Lockfile System Implementation
 * Implements reproducible builds through version locking
 */

#include "cosmo_lock.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

// =============================================================================
// Context Management
// =============================================================================

cosmo_lock_ctx_t* cosmo_lock_create(void) {
    cosmo_lock_ctx_t *ctx = calloc(1, sizeof(cosmo_lock_ctx_t));
    if (!ctx) return NULL;

    // Initialize with default values
    strncpy(ctx->lockfile_version, COSMO_LOCK_VERSION, sizeof(ctx->lockfile_version) - 1);
    cosmo_lock_get_timestamp(ctx->timestamp, sizeof(ctx->timestamp));

    ctx->dep_capacity = 32;  // Start with 32 slots
    ctx->dependencies = calloc((size_t)ctx->dep_capacity, sizeof(cosmo_lock_dep_t));
    if (!ctx->dependencies) {
        free(ctx);
        return NULL;
    }

    ctx->dep_count = 0;
    ctx->lockfile_path = NULL;
    ctx->package_json_path = NULL;
    ctx->error_message[0] = '\0';

    return ctx;
}

void cosmo_lock_destroy(cosmo_lock_ctx_t *ctx) {
    if (!ctx) return;
    free(ctx->dependencies);
    free(ctx);
}

void cosmo_lock_set_lockfile_path(cosmo_lock_ctx_t *ctx, const char *path) {
    if (ctx) ctx->lockfile_path = path;
}

void cosmo_lock_set_package_json_path(cosmo_lock_ctx_t *ctx, const char *path) {
    if (ctx) ctx->package_json_path = path;
}

// =============================================================================
// Utility Functions
// =============================================================================

void cosmo_lock_get_timestamp(char *timestamp_out, size_t out_size) {
    if (!timestamp_out || out_size == 0) return;

    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);

    if (tm_info) {
        // ISO 8601 format: 2025-10-25T12:00:00Z
        strftime(timestamp_out, out_size, "%Y-%m-%dT%H:%M:%SZ", tm_info);
    } else {
        strncpy(timestamp_out, "1970-01-01T00:00:00Z", out_size - 1);
    }
}

const char* cosmo_lock_get_error(cosmo_lock_ctx_t *ctx) {
    return ctx ? ctx->error_message : "No context";
}

void cosmo_lock_clear_error(cosmo_lock_ctx_t *ctx) {
    if (ctx) ctx->error_message[0] = '\0';
}

static void cosmo_lock_set_error(cosmo_lock_ctx_t *ctx, const char *fmt, ...) {
    if (!ctx) return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->error_message, sizeof(ctx->error_message), fmt, args);
    va_end(args);
}

// =============================================================================
// Dependency Management
// =============================================================================

int cosmo_lock_add_dependency(cosmo_lock_ctx_t *ctx,
                              const char *name,
                              const char *version,
                              const char *resolved,
                              const char *integrity,
                              const char *dependencies) {
    if (!ctx || !name || !version) return -1;

    // Check if already exists
    if (cosmo_lock_find_dependency(ctx, name)) {
        cosmo_lock_set_error(ctx, "Dependency '%s' already exists", name);
        return -1;
    }

    // Expand array if needed
    if (ctx->dep_count >= ctx->dep_capacity) {
        int new_capacity = ctx->dep_capacity * 2;
        cosmo_lock_dep_t *new_deps = realloc(ctx->dependencies,
                                              (size_t)new_capacity * sizeof(cosmo_lock_dep_t));
        if (!new_deps) {
            cosmo_lock_set_error(ctx, "Memory allocation failed");
            return -1;
        }
        ctx->dependencies = new_deps;
        ctx->dep_capacity = new_capacity;
    }

    // Add new dependency
    cosmo_lock_dep_t *dep = &ctx->dependencies[ctx->dep_count];
    memset(dep, 0, sizeof(cosmo_lock_dep_t));

    strncpy(dep->name, name, sizeof(dep->name) - 1);
    strncpy(dep->version, version, sizeof(dep->version) - 1);

    if (resolved) {
        strncpy(dep->resolved, resolved, sizeof(dep->resolved) - 1);
    } else {
        snprintf(dep->resolved, sizeof(dep->resolved), "registry://%s@%s", name, version);
    }

    if (integrity) {
        strncpy(dep->integrity, integrity, sizeof(dep->integrity) - 1);
    }

    if (dependencies) {
        strncpy(dep->dependencies, dependencies, sizeof(dep->dependencies) - 1);
    }

    dep->installed = 0;
    ctx->dep_count++;

    return 0;
}

cosmo_lock_dep_t* cosmo_lock_find_dependency(cosmo_lock_ctx_t *ctx, const char *name) {
    if (!ctx || !name) return NULL;

    for (int i = 0; i < ctx->dep_count; i++) {
        if (strcmp(ctx->dependencies[i].name, name) == 0) {
            return &ctx->dependencies[i];
        }
    }

    return NULL;
}

int cosmo_lock_remove_dependency(cosmo_lock_ctx_t *ctx, const char *name) {
    if (!ctx || !name) return -1;

    for (int i = 0; i < ctx->dep_count; i++) {
        if (strcmp(ctx->dependencies[i].name, name) == 0) {
            // Shift remaining elements
            memmove(&ctx->dependencies[i],
                    &ctx->dependencies[i + 1],
                    (size_t)(ctx->dep_count - i - 1) * sizeof(cosmo_lock_dep_t));
            ctx->dep_count--;
            return 0;
        }
    }

    cosmo_lock_set_error(ctx, "Dependency '%s' not found", name);
    return -1;
}

// =============================================================================
// Version Comparison
// =============================================================================

int cosmo_lock_parse_semver(const char *version, int *major, int *minor, int *patch) {
    if (!version || !major || !minor || !patch) return -1;

    // Skip leading 'v' or '^' or '~' or '>=' or other operators
    while (*version && !isdigit(*version)) {
        version++;
    }

    // Parse major.minor.patch
    int parsed = sscanf(version, "%d.%d.%d", major, minor, patch);
    if (parsed < 1) return -1;

    // Set defaults for missing components
    if (parsed < 2) *minor = 0;
    if (parsed < 3) *patch = 0;

    return 0;
}

version_cmp_result_t cosmo_lock_version_compare(const char *v1, const char *v2) {
    if (!v1 || !v2) return VERSION_INCOMPATIBLE;

    int maj1, min1, patch1;
    int maj2, min2, patch2;

    if (cosmo_lock_parse_semver(v1, &maj1, &min1, &patch1) != 0) {
        return VERSION_INCOMPATIBLE;
    }
    if (cosmo_lock_parse_semver(v2, &maj2, &min2, &patch2) != 0) {
        return VERSION_INCOMPATIBLE;
    }

    // Compare major
    if (maj1 < maj2) return VERSION_LESS;
    if (maj1 > maj2) return VERSION_GREATER;

    // Compare minor
    if (min1 < min2) return VERSION_LESS;
    if (min1 > min2) return VERSION_GREATER;

    // Compare patch
    if (patch1 < patch2) return VERSION_LESS;
    if (patch1 > patch2) return VERSION_GREATER;

    return VERSION_EQUAL;
}

int cosmo_lock_version_satisfies(const char *version, const char *requirement) {
    if (!version || !requirement) return 0;

    int v_maj, v_min, v_patch;
    int r_maj, r_min, r_patch;

    if (cosmo_lock_parse_semver(version, &v_maj, &v_min, &v_patch) != 0) return 0;
    if (cosmo_lock_parse_semver(requirement, &r_maj, &r_min, &r_patch) != 0) return 0;

    // Determine operator
    const char *op = requirement;
    while (*op && !isdigit(*op) && *op != 'v') {
        op++;
    }

    if (requirement[0] == '^') {
        // Caret: ^1.2.3 allows >= 1.2.3 and < 2.0.0
        if (v_maj != r_maj) return 0;
        if (v_min < r_min) return 0;
        if (v_min == r_min && v_patch < r_patch) return 0;
        return 1;
    } else if (requirement[0] == '~') {
        // Tilde: ~1.2.3 allows >= 1.2.3 and < 1.3.0
        if (v_maj != r_maj || v_min != r_min) return 0;
        return v_patch >= r_patch;
    } else if (strncmp(requirement, ">=", 2) == 0) {
        // Greater or equal
        version_cmp_result_t cmp = cosmo_lock_version_compare(version, requirement);
        return cmp == VERSION_GREATER || cmp == VERSION_EQUAL;
    } else if (requirement[0] == '>') {
        // Greater than
        return cosmo_lock_version_compare(version, requirement) == VERSION_GREATER;
    } else if (strncmp(requirement, "<=", 2) == 0) {
        // Less or equal
        version_cmp_result_t cmp = cosmo_lock_version_compare(version, requirement);
        return cmp == VERSION_LESS || cmp == VERSION_EQUAL;
    } else if (requirement[0] == '<') {
        // Less than
        return cosmo_lock_version_compare(version, requirement) == VERSION_LESS;
    } else {
        // Exact match
        return cosmo_lock_version_compare(version, requirement) == VERSION_EQUAL;
    }
}

// =============================================================================
// JSON Serialization
// =============================================================================

char* cosmo_lock_to_json(cosmo_lock_ctx_t *ctx) {
    if (!ctx) return NULL;

    // Estimate size: ~500 bytes per dep + overhead
    size_t estimated_size = 1024 + (size_t)ctx->dep_count * 512;
    char *json = malloc(estimated_size);
    if (!json) return NULL;

    char *p = json;
    size_t remaining = estimated_size;

    // Write header
    int written = snprintf(p, remaining,
        "{\n"
        "  \"lockfileVersion\": \"%s\",\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"dependencies\": {\n",
        ctx->lockfile_version,
        ctx->timestamp);

    if (written < 0 || (size_t)written >= remaining) {
        free(json);
        return NULL;
    }
    p += written;
    remaining -= (size_t)written;

    // Write dependencies
    for (int i = 0; i < ctx->dep_count; i++) {
        cosmo_lock_dep_t *dep = &ctx->dependencies[i];

        written = snprintf(p, remaining,
            "    \"%s\": {\n"
            "      \"version\": \"%s\",\n"
            "      \"resolved\": \"%s\",\n"
            "      \"integrity\": \"%s\"",
            dep->name,
            dep->version,
            dep->resolved,
            dep->integrity);

        if (written < 0 || (size_t)written >= remaining) {
            free(json);
            return NULL;
        }
        p += written;
        remaining -= (size_t)written;

        // Add nested dependencies if present
        if (dep->dependencies[0] != '\0') {
            written = snprintf(p, remaining,
                ",\n      \"dependencies\": \"%s\"",
                dep->dependencies);

            if (written < 0 || (size_t)written >= remaining) {
                free(json);
                return NULL;
            }
            p += written;
            remaining -= (size_t)written;
        }

        // Close dependency object
        const char *suffix = (i < ctx->dep_count - 1) ? "\n    },\n" : "\n    }\n";
        written = snprintf(p, remaining, "%s", suffix);

        if (written < 0 || (size_t)written >= remaining) {
            free(json);
            return NULL;
        }
        p += written;
        remaining -= (size_t)written;
    }

    // Close JSON
    written = snprintf(p, remaining, "  }\n}\n");
    if (written < 0 || (size_t)written >= remaining) {
        free(json);
        return NULL;
    }

    return json;
}

// Simple JSON parser for lockfile (basic implementation)
int cosmo_lock_from_json(cosmo_lock_ctx_t *ctx, const char *json_str) {
    if (!ctx || !json_str) return -1;

    // Reset context
    ctx->dep_count = 0;

    // This is a simplified parser - in production, use a proper JSON library
    // For now, we'll just parse the basic structure

    const char *p = json_str;

    // Find "lockfileVersion"
    const char *version_start = strstr(p, "\"lockfileVersion\":");
    if (version_start) {
        version_start = strchr(version_start + 17, '"');  // Skip past field name
        if (version_start) {
            version_start++;
            const char *version_end = strchr(version_start, '"');
            if (version_end) {
                size_t len = (size_t)(version_end - version_start);
                if (len < sizeof(ctx->lockfile_version)) {
                    memcpy(ctx->lockfile_version, version_start, len);
                    ctx->lockfile_version[len] = '\0';
                }
            }
        }
    }

    // Find "timestamp"
    const char *ts_start = strstr(p, "\"timestamp\":");
    if (ts_start) {
        ts_start = strchr(ts_start + 11, '"');  // Skip past field name
        if (ts_start) {
            ts_start++;
            const char *ts_end = strchr(ts_start, '"');
            if (ts_end) {
                size_t len = (size_t)(ts_end - ts_start);
                if (len < sizeof(ctx->timestamp)) {
                    memcpy(ctx->timestamp, ts_start, len);
                    ctx->timestamp[len] = '\0';
                }
            }
        }
    }

    // Find "dependencies" object
    const char *deps_start = strstr(p, "\"dependencies\":");
    if (!deps_start) return 0;  // No dependencies

    deps_start = strchr(deps_start, '{');
    if (!deps_start) return -1;
    deps_start++;

    // Parse each dependency (simplified)
    p = deps_start;
    while (*p && *p != '}') {
        // Skip whitespace
        while (*p && isspace(*p)) p++;
        if (*p == '}') break;

        // Find package name
        if (*p == '"') {
            p++;
            const char *name_start = p;
            const char *name_end = strchr(p, '"');
            if (!name_end) break;

            char name[COSMO_LOCK_MAX_NAME_LEN] = {0};
            size_t name_len = (size_t)(name_end - name_start);
            if (name_len >= sizeof(name)) break;
            memcpy(name, name_start, name_len);

            p = name_end + 1;

            // Find dependency object
            const char *obj_start = strchr(p, '{');
            if (!obj_start) break;
            p = obj_start + 1;

            // Extract fields
            char version[COSMO_LOCK_MAX_VERSION_LEN] = {0};
            char resolved[COSMO_LOCK_MAX_URL_LEN] = {0};
            char integrity[COSMO_LOCK_MAX_HASH_LEN] = {0};
            char dependencies[COSMO_LOCK_MAX_PATH_LEN] = {0};

            // Parse version
            const char *v = strstr(p, "\"version\":");
            if (v) {
                v = strchr(v + 9, '"');  // Skip past field name
                if (v) {
                    v++;
                    const char *v_end = strchr(v, '"');
                    if (v_end && (size_t)(v_end - v) < sizeof(version)) {
                        memcpy(version, v, (size_t)(v_end - v));
                    }
                }
            }

            // Parse resolved
            const char *r = strstr(p, "\"resolved\":");
            if (r) {
                r = strchr(r + 10, '"');  // Skip past field name
                if (r) {
                    r++;
                    const char *r_end = strchr(r, '"');
                    if (r_end && (size_t)(r_end - r) < sizeof(resolved)) {
                        memcpy(resolved, r, (size_t)(r_end - r));
                    }
                }
            }

            // Parse integrity
            const char *i = strstr(p, "\"integrity\":");
            if (i) {
                i = strchr(i + 11, '"');  // Skip past field name
                if (i) {
                    i++;
                    const char *i_end = strchr(i, '"');
                    if (i_end && (size_t)(i_end - i) < sizeof(integrity)) {
                        memcpy(integrity, i, (size_t)(i_end - i));
                    }
                }
            }

            // Add dependency
            cosmo_lock_add_dependency(ctx, name, version, resolved, integrity, dependencies);

            // Find end of object
            const char *obj_end = strchr(p, '}');
            if (!obj_end) break;
            p = obj_end + 1;

            // Skip comma
            while (*p && (*p == ',' || isspace(*p))) p++;
        } else {
            p++;
        }
    }

    return 0;
}

// =============================================================================
// File I/O
// =============================================================================

int cosmo_lock_load(cosmo_lock_ctx_t *ctx) {
    if (!ctx) return -1;

    const char *path = ctx->lockfile_path ? ctx->lockfile_path : COSMO_LOCK_FILENAME;

    FILE *f = fopen(path, "r");
    if (!f) {
        cosmo_lock_set_error(ctx, "Failed to open lockfile: %s", path);
        return -1;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 10 * 1024 * 1024) {  // Max 10MB
        fclose(f);
        cosmo_lock_set_error(ctx, "Invalid lockfile size");
        return -1;
    }

    // Read file
    char *json_str = malloc((size_t)size + 1);
    if (!json_str) {
        fclose(f);
        cosmo_lock_set_error(ctx, "Memory allocation failed");
        return -1;
    }

    size_t read_size = fread(json_str, 1, (size_t)size, f);
    fclose(f);

    if ((long)read_size != size) {
        free(json_str);
        cosmo_lock_set_error(ctx, "Failed to read lockfile");
        return -1;
    }

    json_str[size] = '\0';

    // Parse JSON
    int result = cosmo_lock_from_json(ctx, json_str);
    free(json_str);

    if (result != 0) {
        cosmo_lock_set_error(ctx, "Failed to parse lockfile");
    }

    return result;
}

int cosmo_lock_save(cosmo_lock_ctx_t *ctx) {
    if (!ctx) return -1;

    const char *path = ctx->lockfile_path ? ctx->lockfile_path : COSMO_LOCK_FILENAME;

    // Update timestamp
    cosmo_lock_get_timestamp(ctx->timestamp, sizeof(ctx->timestamp));

    // Serialize to JSON
    char *json = cosmo_lock_to_json(ctx);
    if (!json) {
        cosmo_lock_set_error(ctx, "Failed to serialize lockfile");
        return -1;
    }

    // Write to file
    FILE *f = fopen(path, "w");
    if (!f) {
        free(json);
        cosmo_lock_set_error(ctx, "Failed to create lockfile: %s", path);
        return -1;
    }

    size_t len = strlen(json);
    size_t written = fwrite(json, 1, len, f);
    fclose(f);
    free(json);

    if (written != len) {
        cosmo_lock_set_error(ctx, "Failed to write lockfile");
        return -1;
    }

    return 0;
}

// =============================================================================
// High-Level Operations
// =============================================================================

int cosmo_lock_generate(cosmo_lock_ctx_t *ctx) {
    if (!ctx) return -1;

    // For MVP, we'll create a simple lockfile structure
    // In production, this would read cosmo.json and resolve all dependencies

    const char *pkg_path = ctx->package_json_path ? ctx->package_json_path : "cosmo.json";

    // Check if cosmo.json exists
    FILE *f = fopen(pkg_path, "r");
    if (!f) {
        cosmo_lock_set_error(ctx, "cosmo.json not found");
        return -1;
    }
    fclose(f);

    // TODO: Parse cosmo.json and resolve dependencies
    // For now, this is a placeholder that would be integrated with the package manager

    cosmo_lock_set_error(ctx, "lockfile generation not yet implemented - needs package manager integration");
    return 0;
}

int cosmo_lock_verify(cosmo_lock_ctx_t *ctx) {
    if (!ctx) return -1;

    // Load lockfile
    if (cosmo_lock_load(ctx) != 0) {
        return -1;
    }

    // Verify each dependency
    int mismatches = 0;
    for (int i = 0; i < ctx->dep_count; i++) {
        cosmo_lock_dep_t *dep = &ctx->dependencies[i];

        // TODO: Check if package is installed with correct version
        // This would integrate with the package manager's installation directory

        // For now, just mark as not verified
        if (!dep->installed) {
            printf("! %s@%s not installed\n", dep->name, dep->version);
            mismatches++;
        }
    }

    if (mismatches > 0) {
        cosmo_lock_set_error(ctx, "%d dependencies not installed", mismatches);
        return -1;
    }

    return 0;
}

int cosmo_lock_update_dependency(cosmo_lock_ctx_t *ctx, const char *package_name) {
    if (!ctx || !package_name) return -1;

    // Find dependency
    cosmo_lock_dep_t *dep = cosmo_lock_find_dependency(ctx, package_name);
    if (!dep) {
        cosmo_lock_set_error(ctx, "Dependency '%s' not found in lockfile", package_name);
        return -1;
    }

    // TODO: Fetch latest compatible version from registry
    // For now, just update timestamp
    cosmo_lock_get_timestamp(ctx->timestamp, sizeof(ctx->timestamp));

    return cosmo_lock_save(ctx);
}

int cosmo_lock_resolve_conflicts(cosmo_lock_ctx_t *ctx) {
    if (!ctx) return -1;

    // TODO: Implement dependency conflict resolution algorithm
    // This would analyze the dependency tree and find compatible versions

    cosmo_lock_set_error(ctx, "Conflict resolution not yet implemented");
    return 0;
}

// =============================================================================
// Package Installation Helpers
// =============================================================================

int cosmo_lock_should_install(cosmo_lock_ctx_t *ctx, const char *package_name) {
    if (!ctx || !package_name) return -1;

    cosmo_lock_dep_t *dep = cosmo_lock_find_dependency(ctx, package_name);
    if (!dep) return 0;  // Not in lockfile, skip

    return !dep->installed ? 1 : 0;
}

void cosmo_lock_mark_installed(cosmo_lock_ctx_t *ctx, const char *package_name, int installed) {
    if (!ctx || !package_name) return;

    cosmo_lock_dep_t *dep = cosmo_lock_find_dependency(ctx, package_name);
    if (dep) {
        dep->installed = installed;
    }
}

const char* cosmo_lock_get_install_version(cosmo_lock_ctx_t *ctx, const char *package_name) {
    if (!ctx || !package_name) return NULL;

    cosmo_lock_dep_t *dep = cosmo_lock_find_dependency(ctx, package_name);
    return dep ? dep->version : NULL;
}

// =============================================================================
// Display and Debugging
// =============================================================================

void cosmo_lock_print_summary(cosmo_lock_ctx_t *ctx) {
    if (!ctx) return;

    printf("=== Lockfile Summary ===\n");
    printf("Version:   %s\n", ctx->lockfile_version);
    printf("Timestamp: %s\n", ctx->timestamp);
    printf("Dependencies: %d\n", ctx->dep_count);
    printf("\n");

    for (int i = 0; i < ctx->dep_count; i++) {
        cosmo_lock_dep_t *dep = &ctx->dependencies[i];
        printf("  %s@%s %s\n",
               dep->name,
               dep->version,
               dep->installed ? "[installed]" : "");

        if (dep->resolved[0]) {
            printf("    resolved: %s\n", dep->resolved);
        }
        if (dep->integrity[0]) {
            printf("    integrity: %s\n", dep->integrity);
        }
    }
}

int cosmo_lock_validate(cosmo_lock_ctx_t *ctx) {
    if (!ctx) return -1;

    // Check version format
    if (strcmp(ctx->lockfile_version, COSMO_LOCK_VERSION) != 0) {
        cosmo_lock_set_error(ctx, "Incompatible lockfile version: %s", ctx->lockfile_version);
        return -1;
    }

    // Validate dependencies
    for (int i = 0; i < ctx->dep_count; i++) {
        cosmo_lock_dep_t *dep = &ctx->dependencies[i];

        if (dep->name[0] == '\0') {
            cosmo_lock_set_error(ctx, "Empty package name at index %d", i);
            return -1;
        }

        if (dep->version[0] == '\0') {
            cosmo_lock_set_error(ctx, "Empty version for package '%s'", dep->name);
            return -1;
        }
    }

    return 0;
}

void cosmo_lock_show_diff(cosmo_lock_ctx_t *ctx) {
    if (!ctx) return;

    printf("=== Lockfile Diff ===\n");
    for (int i = 0; i < ctx->dep_count; i++) {
        cosmo_lock_dep_t *dep = &ctx->dependencies[i];
        if (dep->installed) {
            printf("  ✓ %s@%s\n", dep->name, dep->version);
        } else {
            printf("  ✗ %s@%s (missing)\n", dep->name, dep->version);
        }
    }
}

int cosmo_lock_install_all(cosmo_lock_ctx_t *ctx) {
    if (!ctx) return -1;

    // Try to load lockfile first
    if (cosmo_lock_load(ctx) == 0) {
        printf("Installing from lockfile...\n");
        // TODO: Install each dependency from lockfile
        return 0;
    } else {
        printf("No lockfile found, installing from cosmo.json...\n");
        // TODO: Install from cosmo.json and generate lockfile
        return cosmo_lock_generate(ctx);
    }
}

int cosmo_lock_regenerate(cosmo_lock_ctx_t *ctx) {
    if (!ctx) return -1;
    return cosmo_lock_generate(ctx);
}

// =============================================================================
// Integrity Verification (Placeholder)
// =============================================================================

int cosmo_lock_calculate_integrity(const char *package_path, char *integrity_out, size_t out_size) {
    if (!package_path || !integrity_out || out_size == 0) return -1;

    // TODO: Implement SHA-256 hash calculation
    // For now, just create a placeholder
    snprintf(integrity_out, out_size, "sha256:placeholder_%s", package_path);
    return 0;
}

int cosmo_lock_verify_integrity(const char *package_path, const char *expected_hash) {
    if (!package_path || !expected_hash) return -1;

    char actual_hash[COSMO_LOCK_MAX_HASH_LEN];
    if (cosmo_lock_calculate_integrity(package_path, actual_hash, sizeof(actual_hash)) != 0) {
        return -1;
    }

    return strcmp(actual_hash, expected_hash) == 0 ? 0 : -1;
}
