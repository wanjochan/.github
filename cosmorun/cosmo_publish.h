/* cosmo_publish.h - Package Publishing System for CosmoRun
 * Allows developers to publish packages to the registry
 */

#ifndef COSMO_PUBLISH_H
#define COSMO_PUBLISH_H

#include "cosmo_libc.h"
#include "cosmo_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

// Maximum sizes
#define COSMO_PKG_MAX_NAME_LEN      64
#define COSMO_PKG_MAX_VERSION_LEN   32
#define COSMO_PKG_MAX_DESC_LEN      512
#define COSMO_PKG_MAX_AUTHOR_LEN    128
#define COSMO_PKG_MAX_LICENSE_LEN   32
#define COSMO_PKG_MAX_MAIN_LEN      256
#define COSMO_PKG_MAX_DEPS          64
#define COSMO_PKG_MAX_FILES         256
#define COSMO_PKG_MAX_PATH_LEN      512

// Error codes
#define COSMO_PKG_OK                0
#define COSMO_PKG_ERR_INVALID       -1
#define COSMO_PKG_ERR_IO            -2
#define COSMO_PKG_ERR_JSON          -3
#define COSMO_PKG_ERR_VALIDATION    -4
#define COSMO_PKG_ERR_EXISTS        -5
#define COSMO_PKG_ERR_NETWORK       -6
#define COSMO_PKG_ERR_NOMEM         -7

// Dependency constraint operators
typedef enum {
    COSMO_PKG_OP_EQ,        // =1.0.0
    COSMO_PKG_OP_GT,        // >1.0.0
    COSMO_PKG_OP_GTE,       // >=1.0.0
    COSMO_PKG_OP_LT,        // <2.0.0
    COSMO_PKG_OP_LTE,       // <=2.0.0
    COSMO_PKG_OP_CARET,     // ^1.5.0 (compatible, <2.0.0)
    COSMO_PKG_OP_TILDE,     // ~1.5.0 (minor updates, <1.6.0)
    COSMO_PKG_OP_ANY        // * (any version)
} cosmo_pkg_op_t;

// Version structure
typedef struct {
    int major;
    int minor;
    int patch;
} cosmo_pkg_version_t;

// Dependency structure
typedef struct {
    char name[COSMO_PKG_MAX_NAME_LEN];
    char constraint[COSMO_PKG_MAX_VERSION_LEN];
    cosmo_pkg_op_t op;
    cosmo_pkg_version_t version;
} cosmo_pkg_dep_t;

// Package manifest structure
typedef struct {
    char name[COSMO_PKG_MAX_NAME_LEN];
    char version[COSMO_PKG_MAX_VERSION_LEN];
    char description[COSMO_PKG_MAX_DESC_LEN];
    char author[COSMO_PKG_MAX_AUTHOR_LEN];
    char license[COSMO_PKG_MAX_LICENSE_LEN];
    char main[COSMO_PKG_MAX_MAIN_LEN];

    // Dependencies
    cosmo_pkg_dep_t *dependencies;
    int dep_count;
    int dep_capacity;

    // Files to include in package
    char **files;
    int file_count;
    int file_capacity;

    // Parsed version
    cosmo_pkg_version_t pkg_version;
} cosmo_pkg_manifest_t;

// Tarball context
typedef struct {
    char *buffer;
    size_t size;
    size_t capacity;
    const char *output_path;
} cosmo_pkg_tarball_t;

// Package publishing context
typedef struct {
    cosmo_pkg_manifest_t *manifest;
    cosmo_pkg_tarball_t *tarball;
    char checksum[65];  // SHA-256 hex string
    const char *registry_url;
    int verbose;
} cosmo_pkg_publish_ctx_t;

// Initialize/cleanup package manifest
cosmo_pkg_manifest_t* cosmo_pkg_manifest_create(void);
void cosmo_pkg_manifest_destroy(cosmo_pkg_manifest_t *manifest);

// Parse manifest from file (cosmo.json)
int cosmo_pkg_manifest_read(cosmo_pkg_manifest_t *manifest, const char *path);

// Write manifest to file
int cosmo_pkg_manifest_write(const cosmo_pkg_manifest_t *manifest, const char *path);

// Create template manifest
int cosmo_pkg_manifest_create_template(const char *path, const char *name);

// Validate manifest
int cosmo_pkg_manifest_validate(const cosmo_pkg_manifest_t *manifest, char *error_buf, size_t error_len);

// Add dependency
int cosmo_pkg_manifest_add_dep(cosmo_pkg_manifest_t *manifest, const char *name, const char *constraint);

// Add file pattern
int cosmo_pkg_manifest_add_file(cosmo_pkg_manifest_t *manifest, const char *pattern);

// Version parsing and comparison
int cosmo_pkg_version_parse(cosmo_pkg_version_t *ver, const char *str);
int cosmo_pkg_version_compare(const cosmo_pkg_version_t *a, const cosmo_pkg_version_t *b);
int cosmo_pkg_version_satisfies(const cosmo_pkg_version_t *ver, cosmo_pkg_op_t op, const cosmo_pkg_version_t *constraint);
const char* cosmo_pkg_version_to_string(const cosmo_pkg_version_t *ver, char *buf, size_t len);

// Parse dependency constraint (e.g., ">=2.0.0", "^1.5.0")
int cosmo_pkg_dep_parse(cosmo_pkg_dep_t *dep, const char *name, const char *constraint);

// Tarball creation
cosmo_pkg_tarball_t* cosmo_pkg_tarball_create(const char *output_path);
void cosmo_pkg_tarball_destroy(cosmo_pkg_tarball_t *tarball);
int cosmo_pkg_tarball_add_file(cosmo_pkg_tarball_t *tarball, const char *path, const char *archive_path);
int cosmo_pkg_tarball_finalize(cosmo_pkg_tarball_t *tarball);

// Create package tarball from manifest
int cosmo_pkg_create_tarball(const cosmo_pkg_manifest_t *manifest, const char *output_path, char *checksum_out);

// Checksum calculation
int cosmo_pkg_calc_checksum(const char *file_path, char *checksum_out, size_t len);

// Package publishing
cosmo_pkg_publish_ctx_t* cosmo_pkg_publish_create(void);
void cosmo_pkg_publish_destroy(cosmo_pkg_publish_ctx_t *ctx);
int cosmo_pkg_publish_init(cosmo_pkg_publish_ctx_t *ctx, const char *manifest_path);
int cosmo_pkg_publish_check_version_exists(cosmo_pkg_publish_ctx_t *ctx);
int cosmo_pkg_publish_upload(cosmo_pkg_publish_ctx_t *ctx);
int cosmo_pkg_publish_run(cosmo_pkg_publish_ctx_t *ctx);

// CLI commands
int cosmo_pkg_cmd_init(int argc, char **argv);
int cosmo_pkg_cmd_validate(int argc, char **argv);
int cosmo_pkg_cmd_pack(int argc, char **argv);
int cosmo_pkg_cmd_publish(int argc, char **argv);

// Helper functions
int cosmo_pkg_is_valid_name(const char *name);
int cosmo_pkg_is_valid_version(const char *version);
void cosmo_pkg_print_manifest(const cosmo_pkg_manifest_t *manifest);

#ifdef __cplusplus
}
#endif

#endif // COSMO_PUBLISH_H
