/* cosmo_publish.c - Package Publishing System Implementation */

#include "cosmo_publish.h"
#include <ctype.h>

// Simple JSON parsing helpers (minimal implementation)
static const char* skip_whitespace(const char *p) {
    while (*p && isspace(*p)) p++;
    return p;
}

static const char* parse_string(const char *p, char *out, size_t max_len) {
    p = skip_whitespace(p);
    if (*p != '"') return NULL;
    p++;

    size_t i = 0;
    while (*p && *p != '"' && i < max_len - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++;  // Skip escape char, just copy next char
        }
        out[i++] = *p++;
    }
    out[i] = '\0';

    if (*p != '"') return NULL;
    return p + 1;
}

static const char* find_key(const char *json, const char *key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    return strstr(json, search);
}

// ========== Manifest Management ==========

cosmo_pkg_manifest_t* cosmo_pkg_manifest_create(void) {
    cosmo_pkg_manifest_t *m = (cosmo_pkg_manifest_t*)calloc(1, sizeof(cosmo_pkg_manifest_t));
    if (!m) return NULL;

    m->dep_capacity = 16;
    m->dependencies = (cosmo_pkg_dep_t*)calloc(m->dep_capacity, sizeof(cosmo_pkg_dep_t));

    m->file_capacity = 32;
    m->files = (char**)calloc(m->file_capacity, sizeof(char*));

    if (!m->dependencies || !m->files) {
        cosmo_pkg_manifest_destroy(m);
        return NULL;
    }

    return m;
}

void cosmo_pkg_manifest_destroy(cosmo_pkg_manifest_t *manifest) {
    if (!manifest) return;

    if (manifest->dependencies) {
        free(manifest->dependencies);
    }

    if (manifest->files) {
        for (int i = 0; i < manifest->file_count; i++) {
            free(manifest->files[i]);
        }
        free(manifest->files);
    }

    free(manifest);
}

int cosmo_pkg_manifest_read(cosmo_pkg_manifest_t *manifest, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return COSMO_PKG_ERR_IO;

    // Read entire file
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *json = (char*)malloc(size + 1);
    if (!json) {
        fclose(f);
        return COSMO_PKG_ERR_NOMEM;
    }

    size_t read = fread(json, 1, size, f);
    json[read] = '\0';
    fclose(f);

    // Parse JSON manually (simple key-value extraction)
    const char *p;

    // Name
    if ((p = find_key(json, "name"))) {
        p = strchr(p, ':');
        if (p && !(p = parse_string(p + 1, manifest->name, sizeof(manifest->name)))) {
            free(json);
            return COSMO_PKG_ERR_JSON;
        }
    }

    // Version
    if ((p = find_key(json, "version"))) {
        p = strchr(p, ':');
        if (p && !(p = parse_string(p + 1, manifest->version, sizeof(manifest->version)))) {
            free(json);
            return COSMO_PKG_ERR_JSON;
        }
    }

    // Description
    if ((p = find_key(json, "description"))) {
        p = strchr(p, ':');
        if (p) parse_string(p + 1, manifest->description, sizeof(manifest->description));
    }

    // Author
    if ((p = find_key(json, "author"))) {
        p = strchr(p, ':');
        if (p) parse_string(p + 1, manifest->author, sizeof(manifest->author));
    }

    // License
    if ((p = find_key(json, "license"))) {
        p = strchr(p, ':');
        if (p) parse_string(p + 1, manifest->license, sizeof(manifest->license));
    }

    // Main
    if ((p = find_key(json, "main"))) {
        p = strchr(p, ':');
        if (p) parse_string(p + 1, manifest->main, sizeof(manifest->main));
    }

    // Parse dependencies object
    if ((p = find_key(json, "dependencies"))) {
        p = strchr(p, '{');
        if (p) {
            p++;
            while (*p && *p != '}') {
                char dep_name[COSMO_PKG_MAX_NAME_LEN] = {0};
                char dep_ver[COSMO_PKG_MAX_VERSION_LEN] = {0};

                p = skip_whitespace(p);
                if (*p == '}') break;
                if (*p == ',') { p++; continue; }

                const char *next = parse_string(p, dep_name, sizeof(dep_name));
                if (!next) break;
                p = next;

                p = skip_whitespace(p);
                if (*p != ':') break;
                p++;

                next = parse_string(p, dep_ver, sizeof(dep_ver));
                if (!next) break;
                p = next;

                if (dep_name[0] && dep_ver[0]) {
                    cosmo_pkg_manifest_add_dep(manifest, dep_name, dep_ver);
                }
            }
        }
    }

    // Parse files array
    if ((p = find_key(json, "files"))) {
        p = strchr(p, '[');
        if (p) {
            p++;
            while (*p && *p != ']') {
                char file_pattern[COSMO_PKG_MAX_PATH_LEN] = {0};

                p = skip_whitespace(p);
                if (*p == ']') break;
                if (*p == ',') { p++; continue; }

                const char *next = parse_string(p, file_pattern, sizeof(file_pattern));
                if (!next) break;
                p = next;

                if (file_pattern[0]) {
                    cosmo_pkg_manifest_add_file(manifest, file_pattern);
                }
            }
        }
    }

    free(json);

    // Parse version
    if (manifest->version[0]) {
        cosmo_pkg_version_parse(&manifest->pkg_version, manifest->version);
    }

    return COSMO_PKG_OK;
}

int cosmo_pkg_manifest_write(const cosmo_pkg_manifest_t *manifest, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return COSMO_PKG_ERR_IO;

    fprintf(f, "{\n");
    fprintf(f, "  \"name\": \"%s\",\n", manifest->name);
    fprintf(f, "  \"version\": \"%s\",\n", manifest->version);
    fprintf(f, "  \"description\": \"%s\",\n", manifest->description);
    fprintf(f, "  \"author\": \"%s\",\n", manifest->author);
    fprintf(f, "  \"license\": \"%s\",\n", manifest->license);
    fprintf(f, "  \"main\": \"%s\",\n", manifest->main);

    // Dependencies
    fprintf(f, "  \"dependencies\": {\n");
    for (int i = 0; i < manifest->dep_count; i++) {
        fprintf(f, "    \"%s\": \"%s\"%s\n",
                manifest->dependencies[i].name,
                manifest->dependencies[i].constraint,
                i < manifest->dep_count - 1 ? "," : "");
    }
    fprintf(f, "  },\n");

    // Files
    fprintf(f, "  \"files\": [\n");
    for (int i = 0; i < manifest->file_count; i++) {
        fprintf(f, "    \"%s\"%s\n",
                manifest->files[i],
                i < manifest->file_count - 1 ? "," : "");
    }
    fprintf(f, "  ]\n");

    fprintf(f, "}\n");

    fclose(f);
    return COSMO_PKG_OK;
}

int cosmo_pkg_manifest_create_template(const char *path, const char *name) {
    cosmo_pkg_manifest_t *m = cosmo_pkg_manifest_create();
    if (!m) return COSMO_PKG_ERR_NOMEM;

    snprintf(m->name, sizeof(m->name), "%s", name ? name : "my-package");
    snprintf(m->version, sizeof(m->version), "1.0.0");
    snprintf(m->description, sizeof(m->description), "Package description");
    snprintf(m->author, sizeof(m->author), "Author Name");
    snprintf(m->license, sizeof(m->license), "MIT");
    snprintf(m->main, sizeof(m->main), "src/main.c");

    // Add default files
    cosmo_pkg_manifest_add_file(m, "src/");
    cosmo_pkg_manifest_add_file(m, "include/");
    cosmo_pkg_manifest_add_file(m, "README.md");

    int result = cosmo_pkg_manifest_write(m, path);
    cosmo_pkg_manifest_destroy(m);

    return result;
}

int cosmo_pkg_manifest_validate(const cosmo_pkg_manifest_t *manifest, char *error_buf, size_t error_len) {
    if (!manifest->name[0]) {
        snprintf(error_buf, error_len, "Package name is required");
        return COSMO_PKG_ERR_VALIDATION;
    }

    if (!cosmo_pkg_is_valid_name(manifest->name)) {
        snprintf(error_buf, error_len, "Invalid package name: %s", manifest->name);
        return COSMO_PKG_ERR_VALIDATION;
    }

    if (!manifest->version[0]) {
        snprintf(error_buf, error_len, "Package version is required");
        return COSMO_PKG_ERR_VALIDATION;
    }

    if (!cosmo_pkg_is_valid_version(manifest->version)) {
        snprintf(error_buf, error_len, "Invalid version format: %s", manifest->version);
        return COSMO_PKG_ERR_VALIDATION;
    }

    if (manifest->file_count == 0) {
        snprintf(error_buf, error_len, "At least one file pattern is required");
        return COSMO_PKG_ERR_VALIDATION;
    }

    // Validate dependencies
    for (int i = 0; i < manifest->dep_count; i++) {
        if (!cosmo_pkg_is_valid_name(manifest->dependencies[i].name)) {
            snprintf(error_buf, error_len, "Invalid dependency name: %s",
                     manifest->dependencies[i].name);
            return COSMO_PKG_ERR_VALIDATION;
        }
    }

    return COSMO_PKG_OK;
}

int cosmo_pkg_manifest_add_dep(cosmo_pkg_manifest_t *manifest, const char *name, const char *constraint) {
    if (manifest->dep_count >= manifest->dep_capacity) {
        return COSMO_PKG_ERR_NOMEM;
    }

    cosmo_pkg_dep_t *dep = &manifest->dependencies[manifest->dep_count++];
    cosmo_pkg_dep_parse(dep, name, constraint);

    return COSMO_PKG_OK;
}

int cosmo_pkg_manifest_add_file(cosmo_pkg_manifest_t *manifest, const char *pattern) {
    if (manifest->file_count >= manifest->file_capacity) {
        return COSMO_PKG_ERR_NOMEM;
    }

    manifest->files[manifest->file_count++] = strdup(pattern);
    return COSMO_PKG_OK;
}

// ========== Version Management ==========

int cosmo_pkg_version_parse(cosmo_pkg_version_t *ver, const char *str) {
    memset(ver, 0, sizeof(*ver));

    if (sscanf(str, "%d.%d.%d", &ver->major, &ver->minor, &ver->patch) != 3) {
        return COSMO_PKG_ERR_VALIDATION;
    }

    return COSMO_PKG_OK;
}

int cosmo_pkg_version_compare(const cosmo_pkg_version_t *a, const cosmo_pkg_version_t *b) {
    if (a->major != b->major) return a->major - b->major;
    if (a->minor != b->minor) return a->minor - b->minor;
    return a->patch - b->patch;
}

int cosmo_pkg_version_satisfies(const cosmo_pkg_version_t *ver, cosmo_pkg_op_t op,
                                 const cosmo_pkg_version_t *constraint) {
    int cmp = cosmo_pkg_version_compare(ver, constraint);

    switch (op) {
        case COSMO_PKG_OP_EQ: return cmp == 0;
        case COSMO_PKG_OP_GT: return cmp > 0;
        case COSMO_PKG_OP_GTE: return cmp >= 0;
        case COSMO_PKG_OP_LT: return cmp < 0;
        case COSMO_PKG_OP_LTE: return cmp <= 0;
        case COSMO_PKG_OP_CARET:  // ^1.5.0 means >=1.5.0 <2.0.0
            return cmp >= 0 && ver->major == constraint->major;
        case COSMO_PKG_OP_TILDE:  // ~1.5.0 means >=1.5.0 <1.6.0
            return cmp >= 0 && ver->major == constraint->major && ver->minor == constraint->minor;
        case COSMO_PKG_OP_ANY:
            return 1;
        default:
            return 0;
    }
}

const char* cosmo_pkg_version_to_string(const cosmo_pkg_version_t *ver, char *buf, size_t len) {
    snprintf(buf, len, "%d.%d.%d", ver->major, ver->minor, ver->patch);
    return buf;
}

int cosmo_pkg_dep_parse(cosmo_pkg_dep_t *dep, const char *name, const char *constraint) {
    memset(dep, 0, sizeof(*dep));

    snprintf(dep->name, sizeof(dep->name), "%s", name);
    snprintf(dep->constraint, sizeof(dep->constraint), "%s", constraint);

    // Parse operator and version
    const char *ver_str = constraint;

    if (constraint[0] == '^') {
        dep->op = COSMO_PKG_OP_CARET;
        ver_str = constraint + 1;
    } else if (constraint[0] == '~') {
        dep->op = COSMO_PKG_OP_TILDE;
        ver_str = constraint + 1;
    } else if (constraint[0] == '*') {
        dep->op = COSMO_PKG_OP_ANY;
        return COSMO_PKG_OK;
    } else if (constraint[0] == '>' && constraint[1] == '=') {
        dep->op = COSMO_PKG_OP_GTE;
        ver_str = constraint + 2;
    } else if (constraint[0] == '>') {
        dep->op = COSMO_PKG_OP_GT;
        ver_str = constraint + 1;
    } else if (constraint[0] == '<' && constraint[1] == '=') {
        dep->op = COSMO_PKG_OP_LTE;
        ver_str = constraint + 2;
    } else if (constraint[0] == '<') {
        dep->op = COSMO_PKG_OP_LT;
        ver_str = constraint + 1;
    } else {
        dep->op = COSMO_PKG_OP_EQ;
    }

    return cosmo_pkg_version_parse(&dep->version, ver_str);
}

// ========== Validation Helpers ==========

int cosmo_pkg_is_valid_name(const char *name) {
    if (!name || !*name) return 0;

    // Must start with letter or underscore
    if (!isalpha(name[0]) && name[0] != '_') return 0;

    // Can only contain alphanumeric, underscore, hyphen
    for (const char *p = name; *p; p++) {
        if (!isalnum(*p) && *p != '_' && *p != '-') {
            return 0;
        }
    }

    return 1;
}

int cosmo_pkg_is_valid_version(const char *version) {
    cosmo_pkg_version_t v;
    return cosmo_pkg_version_parse(&v, version) == COSMO_PKG_OK;
}

void cosmo_pkg_print_manifest(const cosmo_pkg_manifest_t *manifest) {
    printf("Package: %s@%s\n", manifest->name, manifest->version);
    if (manifest->description[0]) {
        printf("Description: %s\n", manifest->description);
    }
    if (manifest->author[0]) {
        printf("Author: %s\n", manifest->author);
    }
    if (manifest->license[0]) {
        printf("License: %s\n", manifest->license);
    }
    if (manifest->main[0]) {
        printf("Main: %s\n", manifest->main);
    }

    if (manifest->dep_count > 0) {
        printf("\nDependencies:\n");
        for (int i = 0; i < manifest->dep_count; i++) {
            printf("  %s: %s\n",
                   manifest->dependencies[i].name,
                   manifest->dependencies[i].constraint);
        }
    }

    if (manifest->file_count > 0) {
        printf("\nFiles:\n");
        for (int i = 0; i < manifest->file_count; i++) {
            printf("  %s\n", manifest->files[i]);
        }
    }
}

// ========== Publishing Context ==========

cosmo_pkg_publish_ctx_t* cosmo_pkg_publish_create(void) {
    cosmo_pkg_publish_ctx_t *ctx = (cosmo_pkg_publish_ctx_t*)calloc(1, sizeof(cosmo_pkg_publish_ctx_t));
    if (!ctx) return NULL;

    ctx->registry_url = "https://registry.cosmorun.dev";  // Mock registry
    ctx->verbose = 0;

    return ctx;
}

void cosmo_pkg_publish_destroy(cosmo_pkg_publish_ctx_t *ctx) {
    if (!ctx) return;

    if (ctx->manifest) {
        cosmo_pkg_manifest_destroy(ctx->manifest);
    }

    free(ctx);
}

int cosmo_pkg_publish_init(cosmo_pkg_publish_ctx_t *ctx, const char *manifest_path) {
    ctx->manifest = cosmo_pkg_manifest_create();
    if (!ctx->manifest) return COSMO_PKG_ERR_NOMEM;

    return cosmo_pkg_manifest_read(ctx->manifest, manifest_path);
}

int cosmo_pkg_publish_check_version_exists(cosmo_pkg_publish_ctx_t *ctx) {
    // Mock implementation - in real version would check registry
    if (ctx->verbose) {
        printf("Checking if %s@%s exists at %s...\n",
               ctx->manifest->name,
               ctx->manifest->version,
               ctx->registry_url);
    }

    // For now, always return OK (version doesn't exist)
    return COSMO_PKG_OK;
}

int cosmo_pkg_publish_upload(cosmo_pkg_publish_ctx_t *ctx) {
    // Mock implementation - in real version would upload to registry
    if (ctx->verbose) {
        printf("Uploading package to %s...\n", ctx->registry_url);
    }

    printf("✓ Package published successfully (mock)\n");
    printf("  Name: %s\n", ctx->manifest->name);
    printf("  Version: %s\n", ctx->manifest->version);
    printf("  Checksum: %s\n", ctx->checksum);

    return COSMO_PKG_OK;
}

int cosmo_pkg_publish_run(cosmo_pkg_publish_ctx_t *ctx) {
    int result;

    // Validate manifest
    char error_buf[256];
    if ((result = cosmo_pkg_manifest_validate(ctx->manifest, error_buf, sizeof(error_buf))) != COSMO_PKG_OK) {
        fprintf(stderr, "Validation error: %s\n", error_buf);
        return result;
    }

    // Check version doesn't exist
    if ((result = cosmo_pkg_publish_check_version_exists(ctx)) != COSMO_PKG_OK) {
        fprintf(stderr, "Error: Version %s already exists\n", ctx->manifest->version);
        return COSMO_PKG_ERR_EXISTS;
    }

    // Create tarball
    char tarball_name[256];
    snprintf(tarball_name, sizeof(tarball_name), "%s-%s.tar.gz",
             ctx->manifest->name, ctx->manifest->version);

    if ((result = cosmo_pkg_create_tarball(ctx->manifest, tarball_name, ctx->checksum)) != COSMO_PKG_OK) {
        fprintf(stderr, "Error creating tarball\n");
        return result;
    }

    // Upload to registry
    return cosmo_pkg_publish_upload(ctx);
}

// ========== Tarball Creation ==========

int cosmo_pkg_create_tarball(const cosmo_pkg_manifest_t *manifest, const char *output_path, char *checksum_out) {
    // Simple mock implementation - creates empty file
    FILE *f = fopen(output_path, "wb");
    if (!f) return COSMO_PKG_ERR_IO;

    // Write manifest as mock content
    fprintf(f, "# Package: %s@%s\n", manifest->name, manifest->version);
    fprintf(f, "# Files to include:\n");
    for (int i = 0; i < manifest->file_count; i++) {
        fprintf(f, "#   %s\n", manifest->files[i]);
    }

    fclose(f);

    // Calculate checksum
    return cosmo_pkg_calc_checksum(output_path, checksum_out, 65);
}

int cosmo_pkg_calc_checksum(const char *file_path, char *checksum_out, size_t len) {
    // Mock SHA-256 checksum
    snprintf(checksum_out, len, "a1b2c3d4e5f6789012345678901234567890123456789012345678901234abcd");
    return COSMO_PKG_OK;
}

// ========== CLI Commands ==========

int cosmo_pkg_cmd_init(int argc, char **argv) {
    const char *name = NULL;

    // Parse args
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            name = argv[++i];
        }
    }

    printf("Creating cosmo.json...\n");

    int result = cosmo_pkg_manifest_create_template("cosmo.json", name);
    if (result != COSMO_PKG_OK) {
        fprintf(stderr, "Error creating cosmo.json\n");
        return 1;
    }

    printf("✓ Created cosmo.json template\n");
    printf("  Edit the file and run: cosmorun publish\n");

    return 0;
}

int cosmo_pkg_cmd_validate(int argc, char **argv) {
    const char *manifest_path = "cosmo.json";

    // Parse args
    for (int i = 2; i < argc; i++) {
        if (argv[i][0] != '-') {
            manifest_path = argv[i];
            break;
        }
    }

    cosmo_pkg_manifest_t *m = cosmo_pkg_manifest_create();
    if (!m) {
        fprintf(stderr, "Error: Out of memory\n");
        return 1;
    }

    int result = cosmo_pkg_manifest_read(m, manifest_path);
    if (result != COSMO_PKG_OK) {
        fprintf(stderr, "Error reading %s\n", manifest_path);
        cosmo_pkg_manifest_destroy(m);
        return 1;
    }

    char error_buf[256];
    result = cosmo_pkg_manifest_validate(m, error_buf, sizeof(error_buf));

    if (result == COSMO_PKG_OK) {
        printf("✓ Manifest is valid\n");
        cosmo_pkg_print_manifest(m);
    } else {
        fprintf(stderr, "✗ Validation failed: %s\n", error_buf);
    }

    cosmo_pkg_manifest_destroy(m);
    return result == COSMO_PKG_OK ? 0 : 1;
}

int cosmo_pkg_cmd_pack(int argc, char **argv) {
    const char *manifest_path = "cosmo.json";
    const char *output = NULL;

    // Parse args
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (argv[i][0] != '-') {
            manifest_path = argv[i];
        }
    }

    cosmo_pkg_manifest_t *m = cosmo_pkg_manifest_create();
    if (!m) {
        fprintf(stderr, "Error: Out of memory\n");
        return 1;
    }

    int result = cosmo_pkg_manifest_read(m, manifest_path);
    if (result != COSMO_PKG_OK) {
        fprintf(stderr, "Error reading %s\n", manifest_path);
        cosmo_pkg_manifest_destroy(m);
        return 1;
    }

    char error_buf[256];
    result = cosmo_pkg_manifest_validate(m, error_buf, sizeof(error_buf));
    if (result != COSMO_PKG_OK) {
        fprintf(stderr, "Validation error: %s\n", error_buf);
        cosmo_pkg_manifest_destroy(m);
        return 1;
    }

    char tarball_name[256];
    if (output) {
        snprintf(tarball_name, sizeof(tarball_name), "%s", output);
    } else {
        snprintf(tarball_name, sizeof(tarball_name), "%s-%s.tar.gz", m->name, m->version);
    }

    char checksum[65];
    result = cosmo_pkg_create_tarball(m, tarball_name, checksum);

    if (result == COSMO_PKG_OK) {
        printf("✓ Created %s\n", tarball_name);
        printf("  Checksum: %s\n", checksum);
    } else {
        fprintf(stderr, "Error creating tarball\n");
    }

    cosmo_pkg_manifest_destroy(m);
    return result == COSMO_PKG_OK ? 0 : 1;
}

int cosmo_pkg_cmd_publish(int argc, char **argv) {
    const char *manifest_path = "cosmo.json";
    int verbose = 0;

    // Parse args
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (argv[i][0] != '-') {
            manifest_path = argv[i];
        }
    }

    cosmo_pkg_publish_ctx_t *ctx = cosmo_pkg_publish_create();
    if (!ctx) {
        fprintf(stderr, "Error: Out of memory\n");
        return 1;
    }

    ctx->verbose = verbose;

    int result = cosmo_pkg_publish_init(ctx, manifest_path);
    if (result != COSMO_PKG_OK) {
        fprintf(stderr, "Error reading %s\n", manifest_path);
        cosmo_pkg_publish_destroy(ctx);
        return 1;
    }

    printf("Publishing %s@%s...\n", ctx->manifest->name, ctx->manifest->version);

    result = cosmo_pkg_publish_run(ctx);

    cosmo_pkg_publish_destroy(ctx);
    return result == COSMO_PKG_OK ? 0 : 1;
}
