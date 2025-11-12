/**
 * @file cosmo_link.c
 * @brief Enhanced Dynamic Linking Support Implementation
 */

#include "cosmo_link.h"
#include "cosmo_libc.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

/* ========== Helper Functions ========== */

/**
 * Check if file exists and is readable
 */
static int file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

/**
 * Check if directory exists
 */
static int dir_exists_internal(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/**
 * Safe string copy
 */
static int safe_strncpy(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return -1;

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
    return 0;
}

/**
 * Check if string ends with suffix
 */
static int ends_with_suffix(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len) return 0;

    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

/**
 * Try to find library with various naming conventions
 * Tries in order:
 * 1. lib<name>.so (dynamic)
 * 2. lib<name>.a (static)
 * 3. <name> (exact match)
 * 4. lib<name>.so.X.Y.Z (versioned)
 */
static int find_library_in_dir(const char *dir, const char *lib_name,
                                int prefer_static, LibraryInfo *info) {
    char path[COSMO_LINK_MAX_PATH_LEN];

    /* Try versioned name first if it contains dots (e.g., ssl.1.1) */
    if (strchr(lib_name, '.') != NULL) {
        /* Try libssl.so.1.1 */
        snprintf(path, sizeof(path), "%s/lib%s", dir, lib_name);
        if (file_exists(path)) {
            safe_strncpy(info->path, path, sizeof(info->path));
            info->type = LIB_TYPE_SHARED;
            info->found = 1;
            return 0;
        }
    }

    if (prefer_static) {
        /* Try static first: libmath.a */
        snprintf(path, sizeof(path), "%s/lib%s.a", dir, lib_name);
        if (file_exists(path)) {
            safe_strncpy(info->path, path, sizeof(info->path));
            info->type = LIB_TYPE_STATIC;
            info->found = 1;
            return 0;
        }

        /* Then try dynamic: libmath.so */
        snprintf(path, sizeof(path), "%s/lib%s.so", dir, lib_name);
        if (file_exists(path)) {
            safe_strncpy(info->path, path, sizeof(info->path));
            info->type = LIB_TYPE_SHARED;
            info->found = 1;
            return 0;
        }
    } else {
        /* Try dynamic first: libmath.so */
        snprintf(path, sizeof(path), "%s/lib%s.so", dir, lib_name);
        if (file_exists(path)) {
            safe_strncpy(info->path, path, sizeof(info->path));
            info->type = LIB_TYPE_SHARED;
            info->found = 1;
            return 0;
        }

        /* Then try static: libmath.a */
        snprintf(path, sizeof(path), "%s/lib%s.a", dir, lib_name);
        if (file_exists(path)) {
            safe_strncpy(info->path, path, sizeof(info->path));
            info->type = LIB_TYPE_STATIC;
            info->found = 1;
            return 0;
        }
    }

    /* Try exact match (e.g., for custom named libraries) */
    snprintf(path, sizeof(path), "%s/%s", dir, lib_name);
    if (file_exists(path)) {
        safe_strncpy(info->path, path, sizeof(info->path));
        info->type = cosmo_link_get_library_type(path);
        info->found = 1;
        return 0;
    }

    info->found = 0;
    return -1;
}

/* ========== Public API Implementation ========== */

void cosmo_link_init_context(LibrarySearchContext *ctx) {
    if (!ctx) return;

    memset(ctx, 0, sizeof(LibrarySearchContext));
}

int cosmo_link_add_search_path(LibrarySearchContext *ctx, const char *path) {
    if (!ctx || !path) return -1;

    if (ctx->search_path_count >= COSMO_LINK_MAX_SEARCH_PATHS) {
        fprintf(stderr, "cosmo_link: Maximum search paths exceeded\n");
        return -1;
    }

    /* Verify directory exists */
    if (!dir_exists_internal(path)) {
        fprintf(stderr, "cosmo_link: Warning: search path does not exist: %s\n", path);
        /* Still add it for future use */
    }

    ctx->search_paths[ctx->search_path_count++] = path;
    return 0;
}

int cosmo_link_add_rpath(LibrarySearchContext *ctx, const char *rpath) {
    if (!ctx || !rpath) return -1;

    if (ctx->rpath_count >= COSMO_LINK_MAX_RPATHS) {
        fprintf(stderr, "cosmo_link: Maximum rpaths exceeded\n");
        return -1;
    }

    ctx->rpaths[ctx->rpath_count++] = rpath;
    return 0;
}

int cosmo_link_add_ld_library_path(LibrarySearchContext *ctx) {
    if (!ctx) return -1;

    const char *ld_path = getenv("LD_LIBRARY_PATH");
    if (!ld_path) return 0;

    /* Parse colon-separated paths */
    char *path_copy = strdup(ld_path);
    if (!path_copy) return -1;

    int added = 0;
    char *token = strtok(path_copy, ":");
    while (token != NULL) {
        if (cosmo_link_add_search_path(ctx, token) == 0) {
            added++;
        }
        token = strtok(NULL, ":");
    }

    free(path_copy);
    return added;
}

int cosmo_link_add_system_paths(LibrarySearchContext *ctx) {
    if (!ctx) return -1;

    /* Common system library paths */
    static const char *system_paths[] = {
        "/usr/local/lib",
        "/usr/lib",
        "/usr/lib64",
        "/usr/lib/x86_64-linux-gnu",
        "/usr/lib/aarch64-linux-gnu",
        "/lib",
        "/lib64",
        "/lib/x86_64-linux-gnu",
        "/lib/aarch64-linux-gnu",
        NULL
    };

    int added = 0;
    for (int i = 0; system_paths[i] != NULL; i++) {
        if (dir_exists_internal(system_paths[i])) {
            if (cosmo_link_add_search_path(ctx, system_paths[i]) == 0) {
                added++;
            }
        }
    }

    return added;
}

int cosmo_link_resolve_library(LibrarySearchContext *ctx,
                                const char *lib_name,
                                int prefer_static,
                                LibraryInfo *info) {
    if (!ctx || !lib_name || !info) return -1;

    /* Initialize output */
    memset(info, 0, sizeof(LibraryInfo));
    info->type = LIB_TYPE_UNKNOWN;
    info->found = 0;

    /* Check cache first */
    for (int i = 0; i < ctx->cache_count && i < COSMO_LINK_CACHE_SIZE; i++) {
        if (ctx->cache[i].valid && strcmp(ctx->cache[i].lib_name, lib_name) == 0) {
            safe_strncpy(info->path, ctx->cache[i].resolved_path, sizeof(info->path));
            info->type = cosmo_link_get_library_type(info->path);
            info->found = 1;
            return 0;
        }
    }

    /* Search in all paths */
    for (int i = 0; i < ctx->search_path_count; i++) {
        if (find_library_in_dir(ctx->search_paths[i], lib_name, prefer_static, info) == 0) {
            /* Add to cache */
            if (ctx->cache_count < COSMO_LINK_CACHE_SIZE) {
                safe_strncpy(ctx->cache[ctx->cache_count].lib_name, lib_name, 256);
                safe_strncpy(ctx->cache[ctx->cache_count].resolved_path, info->path,
                            sizeof(ctx->cache[ctx->cache_count].resolved_path));
                ctx->cache[ctx->cache_count].valid = 1;
                ctx->cache_count++;
            }
            return 0;
        }
    }

    /* Not found */
    fprintf(stderr, "cosmo_link: Library not found: %s\n", lib_name);
    fprintf(stderr, "cosmo_link: Searched in %d path(s)\n", ctx->search_path_count);
    return -1;
}

int cosmo_link_expand_rpath(const char *rpath,
                             const char *executable_dir,
                             char *output,
                             size_t output_size) {
    if (!rpath || !executable_dir || !output || output_size == 0) return -1;

    /* Check if rpath contains $ORIGIN */
    const char *origin = strstr(rpath, "$ORIGIN");
    if (!origin) {
        /* No $ORIGIN, just copy */
        return safe_strncpy(output, rpath, output_size);
    }

    /* Replace $ORIGIN with executable_dir */
    size_t prefix_len = origin - rpath;
    const char *suffix = origin + strlen("$ORIGIN");

    if (prefix_len + strlen(executable_dir) + strlen(suffix) + 1 > output_size) {
        fprintf(stderr, "cosmo_link: Expanded rpath too long\n");
        return -1;
    }

    /* Build expanded path */
    char *p = output;
    if (prefix_len > 0) {
        memcpy(p, rpath, prefix_len);
        p += prefix_len;
    }
    strcpy(p, executable_dir);
    p += strlen(executable_dir);
    strcpy(p, suffix);

    return 0;
}

void cosmo_link_clear_cache(LibrarySearchContext *ctx) {
    if (!ctx) return;

    for (int i = 0; i < COSMO_LINK_CACHE_SIZE; i++) {
        ctx->cache[i].valid = 0;
    }
    ctx->cache_count = 0;
}

int cosmo_link_parse_wl_option(const char *wl_arg, LibrarySearchContext *ctx) {
    if (!wl_arg || !ctx) return -1;

    /* Must start with -Wl, */
    if (strncmp(wl_arg, "-Wl,", 4) != 0) {
        return -1;
    }

    const char *options = wl_arg + 4;

    /* Parse comma-separated options */
    char *opts_copy = strdup(options);
    if (!opts_copy) return -1;

    int ret = 0;
    char *token = strtok(opts_copy, ",");
    char *pending_option = NULL;

    while (token != NULL) {
        /* Check for -rpath */
        if (strcmp(token, "-rpath") == 0) {
            pending_option = "rpath";
        } else if (strncmp(token, "-rpath=", 7) == 0) {
            /* -rpath=/path format */
            const char *path = token + 7;
            if (cosmo_link_add_rpath(ctx, path) != 0) {
                ret = -1;
                break;
            }
        } else if (pending_option) {
            /* This is the value for the pending option */
            if (strcmp(pending_option, "rpath") == 0) {
                if (cosmo_link_add_rpath(ctx, token) != 0) {
                    ret = -1;
                    break;
                }
            }
            pending_option = NULL;
        }

        token = strtok(NULL, ",");
    }

    free(opts_copy);
    return ret;
}

int cosmo_link_library_exists(const char *path) {
    return file_exists(path);
}

LibraryType cosmo_link_get_library_type(const char *path) {
    if (!path) return LIB_TYPE_UNKNOWN;

    if (ends_with_suffix(path, ".a")) {
        return LIB_TYPE_STATIC;
    } else if (ends_with_suffix(path, ".so") || strstr(path, ".so.") != NULL) {
        return LIB_TYPE_SHARED;
    }

    return LIB_TYPE_UNKNOWN;
}

int cosmo_link_normalize_library_name(const char *lib_name,
                                       char *output,
                                       size_t output_size) {
    if (!lib_name || !output || output_size == 0) return -1;

    /* Remove lib prefix if present */
    const char *name = lib_name;
    if (strncmp(name, "lib", 3) == 0) {
        name += 3;
    }

    /* Remove .so suffix if present */
    char temp[256];
    safe_strncpy(temp, name, sizeof(temp));

    char *dot_so = strstr(temp, ".so");
    if (dot_so) {
        *dot_so = '\0';
    }

    /* Remove .a suffix if present */
    size_t len = strlen(temp);
    if (len > 2 && strcmp(temp + len - 2, ".a") == 0) {
        temp[len - 2] = '\0';
    }

    return safe_strncpy(output, temp, output_size);
}

void cosmo_link_print_search_paths(const LibrarySearchContext *ctx) {
    if (!ctx) return;

    printf("Library Search Paths (%d):\n", ctx->search_path_count);
    for (int i = 0; i < ctx->search_path_count; i++) {
        printf("  [%d] %s\n", i, ctx->search_paths[i]);
    }

    printf("\nRuntime Paths (%d):\n", ctx->rpath_count);
    for (int i = 0; i < ctx->rpath_count; i++) {
        printf("  [%d] %s\n", i, ctx->rpaths[i]);
    }
}

void cosmo_link_print_stats(const LibrarySearchContext *ctx) {
    if (!ctx) return;

    printf("Library Search Statistics:\n");
    printf("  Search paths: %d\n", ctx->search_path_count);
    printf("  Runtime paths: %d\n", ctx->rpath_count);
    printf("  Cache entries: %d\n", ctx->cache_count);
}
