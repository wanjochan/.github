/* cosmo_ldd.c - Dynamic Library Dependency Viewer Implementation */

#include "cosmo_ldd.h"
#include "cosmo_elf_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ========== Library Management ========== */

static ldd_library_t* ldd_library_create(const char *name) {
    ldd_library_t *lib = calloc(1, sizeof(ldd_library_t));
    if (!lib) return NULL;

    lib->name = strdup(name);
    if (!lib->name) {
        free(lib);
        return NULL;
    }

    lib->deps_capacity = 8;
    lib->deps = malloc(lib->deps_capacity * sizeof(ldd_library_t *));
    if (!lib->deps) {
        free(lib->name);
        free(lib);
        return NULL;
    }

    return lib;
}

static void ldd_library_free(ldd_library_t *lib) {
    if (!lib) return;

    free(lib->name);
    free(lib->path);

    if (lib->deps) {
        free(lib->deps);
    }

    free(lib);
}

static int ldd_library_add_dep(ldd_library_t *lib, ldd_library_t *dep) {
    if (!lib || !dep) return -1;

    /* Resize if needed */
    if (lib->num_deps >= lib->deps_capacity) {
        int new_capacity = lib->deps_capacity * 2;
        ldd_library_t **new_deps = realloc(lib->deps, new_capacity * sizeof(ldd_library_t *));
        if (!new_deps) return -1;

        lib->deps = new_deps;
        lib->deps_capacity = new_capacity;
    }

    lib->deps[lib->num_deps++] = dep;
    return 0;
}

/* ========== Context Management ========== */

ldd_context_t* ldd_context_create(void) {
    ldd_context_t *ctx = calloc(1, sizeof(ldd_context_t));
    if (!ctx) return NULL;

    ctx->libs_capacity = 32;
    ctx->all_libs = malloc(ctx->libs_capacity * sizeof(ldd_library_t *));
    if (!ctx->all_libs) {
        free(ctx);
        return NULL;
    }

    ctx->format = LDD_FORMAT_STANDARD;
    ctx->max_depth = -1;  /* No limit */

    /* Initialize search paths */
    ctx->search_paths = malloc(16 * sizeof(char *));
    if (!ctx->search_paths) {
        free(ctx->all_libs);
        free(ctx);
        return NULL;
    }

    return ctx;
}

void ldd_context_free(ldd_context_t *ctx) {
    if (!ctx) return;

    /* Free all libraries */
    for (int i = 0; i < ctx->num_libs; i++) {
        ldd_library_free(ctx->all_libs[i]);
    }
    free(ctx->all_libs);

    /* Free search paths */
    for (int i = 0; i < ctx->num_search_paths; i++) {
        free(ctx->search_paths[i]);
    }
    free(ctx->search_paths);

    free(ctx);
}

void ldd_set_format(ldd_context_t *ctx, ldd_output_format_t format) {
    if (ctx) {
        ctx->format = format;
    }
}

int ldd_add_search_path(ldd_context_t *ctx, const char *path) {
    if (!ctx || !path) return -1;

    char *path_copy = strdup(path);
    if (!path_copy) return -1;

    ctx->search_paths[ctx->num_search_paths++] = path_copy;
    return 0;
}

/* ========== Library Search ========== */

int ldd_init_default_paths(ldd_context_t *ctx) {
    if (!ctx) return -1;

    /* Standard library paths */
    const char *default_paths[] = {
        "/lib",
        "/lib64",
        "/usr/lib",
        "/usr/lib64",
        "/lib/x86_64-linux-gnu",
        "/usr/lib/x86_64-linux-gnu",
        "/lib/aarch64-linux-gnu",
        "/usr/lib/aarch64-linux-gnu",
        NULL
    };

    for (int i = 0; default_paths[i]; i++) {
        ldd_add_search_path(ctx, default_paths[i]);
    }

    /* Add LD_LIBRARY_PATH */
    const char *ld_path = getenv("LD_LIBRARY_PATH");
    if (ld_path) {
        char *path_copy = strdup(ld_path);
        if (path_copy) {
            char *token = strtok(path_copy, ":");
            while (token) {
                ldd_add_search_path(ctx, token);
                token = strtok(NULL, ":");
            }
            free(path_copy);
        }
    }

    return 0;
}

char* ldd_find_library(const ldd_context_t *ctx, const char *lib_name) {
    if (!ctx || !lib_name) return NULL;

    struct stat st;

    /* Try each search path */
    for (int i = 0; i < ctx->num_search_paths; i++) {
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", ctx->search_paths[i], lib_name);

        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            return strdup(path);
        }
    }

    return NULL;
}

/* ========== Circular Dependency Detection ========== */

int ldd_is_circular(const ldd_library_t *lib, const ldd_library_t *ancestor) {
    if (!lib || !ancestor) return 0;

    /* Check direct match */
    if (lib == ancestor) return 1;

    /* Check if ancestor appears in lib's dependency chain */
    for (int i = 0; i < lib->num_deps; i++) {
        if (ldd_is_circular(lib->deps[i], ancestor)) {
            return 1;
        }
    }

    return 0;
}

/* ========== Dependency Analysis ========== */

static ldd_library_t* ldd_find_or_create_lib(ldd_context_t *ctx, const char *name) {
    /* Check if already exists */
    for (int i = 0; i < ctx->num_libs; i++) {
        if (strcmp(ctx->all_libs[i]->name, name) == 0) {
            return ctx->all_libs[i];
        }
    }

    /* Create new */
    ldd_library_t *lib = ldd_library_create(name);
    if (!lib) return NULL;

    /* Resize if needed */
    if (ctx->num_libs >= ctx->libs_capacity) {
        int new_capacity = ctx->libs_capacity * 2;
        ldd_library_t **new_libs = realloc(ctx->all_libs, new_capacity * sizeof(ldd_library_t *));
        if (!new_libs) {
            ldd_library_free(lib);
            return NULL;
        }
        ctx->all_libs = new_libs;
        ctx->libs_capacity = new_capacity;
    }

    ctx->all_libs[ctx->num_libs++] = lib;
    return lib;
}

static int ldd_analyze_library(ldd_context_t *ctx, ldd_library_t *lib, int depth) {
    if (!ctx || !lib) return -1;

    /* Check depth limit */
    if (ctx->max_depth >= 0 && depth > ctx->max_depth) {
        return 0;
    }

    /* Already processed? */
    if (lib->processed) return 0;
    lib->processed = 1;

    /* Find library path if not already set */
    if (!lib->path && !lib->found) {
        lib->path = ldd_find_library(ctx, lib->name);
        lib->found = (lib->path != NULL);
    }

    /* If library not found, can't analyze dependencies */
    if (!lib->found) return 0;

    /* Load and parse ELF file */
    FILE *f = fopen(lib->path, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *data = malloc(size);
    if (!data) {
        fclose(f);
        return -1;
    }

    if (fread(data, 1, size, f) != size) {
        free(data);
        fclose(f);
        return -1;
    }
    fclose(f);

    /* Parse ELF */
    elf_parser_t parser;
    if (elf_parser_init(&parser, data, size) != 0) {
        free(data);
        return -1;
    }

    if (elf_parse_dynamic(&parser) != 0) {
        elf_parser_free(&parser);
        free(data);
        return -1;
    }

    /* Get needed libraries */
    char **needed_libs = NULL;
    int num_needed = 0;

    if (elf_get_needed_libs(&parser, &needed_libs, &num_needed) == 0) {
        for (int i = 0; i < num_needed; i++) {
            ldd_library_t *dep = ldd_find_or_create_lib(ctx, needed_libs[i]);
            if (dep) {
                ldd_library_add_dep(lib, dep);

                /* Recursively analyze dependency */
                if (!ldd_is_circular(dep, lib)) {
                    ldd_analyze_library(ctx, dep, depth + 1);
                }
            }
            free(needed_libs[i]);
        }
        free(needed_libs);
    }

    elf_parser_free(&parser);
    free(data);

    return 0;
}

int ldd_analyze(ldd_context_t *ctx, const char *library_path) {
    if (!ctx || !library_path) return -1;

    /* Check if file exists */
    struct stat st;
    if (stat(library_path, &st) != 0) {
        fprintf(stderr, "Error: Cannot access '%s'\n", library_path);
        return -1;
    }

    /* Extract library name from path */
    const char *name = strrchr(library_path, '/');
    name = name ? name + 1 : library_path;

    /* Create root library */
    ctx->root = ldd_library_create(name);
    if (!ctx->root) return -1;

    /* Set path for root */
    ctx->root->path = strdup(library_path);
    ctx->root->found = 1;

    /* Add to all_libs */
    if (ctx->num_libs >= ctx->libs_capacity) {
        int new_capacity = ctx->libs_capacity * 2;
        ldd_library_t **new_libs = realloc(ctx->all_libs, new_capacity * sizeof(ldd_library_t *));
        if (!new_libs) {
            ldd_library_free(ctx->root);
            ctx->root = NULL;
            return -1;
        }
        ctx->all_libs = new_libs;
        ctx->libs_capacity = new_capacity;
    }
    ctx->all_libs[ctx->num_libs++] = ctx->root;

    /* Analyze dependencies */
    return ldd_analyze_library(ctx, ctx->root, 0);
}

/* ========== Output Formatting ========== */

static void ldd_print_standard(const ldd_context_t *ctx) {
    if (!ctx || !ctx->root) return;

    /* Print dependencies recursively */
    for (int i = 0; i < ctx->root->num_deps; i++) {
        ldd_library_t *dep = ctx->root->deps[i];

        printf("\t%s => ", dep->name);
        if (dep->found && dep->path) {
            printf("%s", dep->path);
            if (dep->load_addr) {
                printf(" (0x%016lx)", dep->load_addr);
            }
        } else {
            printf("not found");
        }
        printf("\n");
    }

    /* Print vdso if on Linux */
#ifdef __linux__
    printf("\tlinux-vdso.so.1 (0x00007ffce3d9c000)\n");
#endif
}

static void ldd_print_tree_recursive(const ldd_library_t *lib, int depth, int is_last) {
    if (!lib) return;

    /* Print indentation */
    for (int i = 0; i < depth; i++) {
        printf("│   ");
    }

    /* Print branch */
    if (depth > 0) {
        printf("%s── ", is_last ? "└" : "├");
    }

    /* Print library info */
    printf("%s", lib->name);
    if (lib->found && lib->path) {
        printf(" => %s", lib->path);
    } else if (!lib->found) {
        printf(" => not found");
    }
    printf("\n");

    /* Print dependencies */
    for (int i = 0; i < lib->num_deps; i++) {
        ldd_print_tree_recursive(lib->deps[i], depth + 1, i == lib->num_deps - 1);
    }
}

static void ldd_print_tree(const ldd_context_t *ctx) {
    if (!ctx || !ctx->root) return;

    printf("%s\n", ctx->root->path ? ctx->root->path : ctx->root->name);
    for (int i = 0; i < ctx->root->num_deps; i++) {
        ldd_print_tree_recursive(ctx->root->deps[i], 0, i == ctx->root->num_deps - 1);
    }
}

void ldd_print(const ldd_context_t *ctx) {
    if (!ctx) return;

    switch (ctx->format) {
        case LDD_FORMAT_TREE:
            ldd_print_tree(ctx);
            break;

        case LDD_FORMAT_STANDARD:
        default:
            ldd_print_standard(ctx);
            break;
    }
}
