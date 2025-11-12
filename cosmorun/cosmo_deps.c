/* cosmo_deps.c - Dependency Generation Implementation
 * GCC-compatible dependency generation for Make/Ninja builds
 */

#include "cosmo_deps.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Create dependency context
cosmo_deps_ctx_t* cosmo_deps_create(void) {
    cosmo_deps_ctx_t *ctx = calloc(1, sizeof(cosmo_deps_ctx_t));
    if (!ctx) return NULL;

    ctx->include_capacity = 64;  // Start with 64 slots
    ctx->includes = calloc((size_t)ctx->include_capacity, sizeof(char*));
    if (!ctx->includes) {
        free(ctx);
        return NULL;
    }

    ctx->include_count = 0;
    ctx->exclude_system = 0;
    ctx->gen_phony = 0;
    ctx->target = NULL;
    ctx->dep_file = NULL;
    ctx->source_file = NULL;
    ctx->output_file = NULL;

    return ctx;
}

// Destroy dependency context
void cosmo_deps_destroy(cosmo_deps_ctx_t *ctx) {
    if (!ctx) return;

    if (ctx->includes) {
        for (int i = 0; i < ctx->include_count; i++) {
            free(ctx->includes[i]);
        }
        free(ctx->includes);
    }

    free(ctx);
}

// Configure exclude system headers
void cosmo_deps_set_exclude_system(cosmo_deps_ctx_t *ctx, int exclude) {
    if (ctx) ctx->exclude_system = exclude;
}

// Configure phony targets
void cosmo_deps_set_phony_targets(cosmo_deps_ctx_t *ctx, int enable) {
    if (ctx) ctx->gen_phony = enable;
}

// Set custom target name
void cosmo_deps_set_target(cosmo_deps_ctx_t *ctx, const char *target) {
    if (ctx) ctx->target = target;
}

// Set dependency output file
void cosmo_deps_set_dep_file(cosmo_deps_ctx_t *ctx, const char *dep_file) {
    if (ctx) ctx->dep_file = dep_file;
}

// Set source and output file info
void cosmo_deps_set_source(cosmo_deps_ctx_t *ctx, const char *source, const char *output) {
    if (!ctx) return;
    ctx->source_file = source;
    ctx->output_file = output;
}

// Check if path is a system header
int cosmo_deps_is_system_path(const char *path) {
    if (!path) return 0;

    // Common system include directories
    const char *system_prefixes[] = {
        "/usr/include/",
        "/usr/local/include/",
        "/opt/homebrew/include/",
        "/Library/Developer/CommandLineTools/SDKs/",
        "/Applications/Xcode.app/",
        "C:\\Program Files\\",
        "C:\\Windows\\",
        NULL
    };

    for (int i = 0; system_prefixes[i]; i++) {
        if (strstr(path, system_prefixes[i]) == path) {
            return 1;
        }
    }

    return 0;
}

// Add included file to tracking list
int cosmo_deps_add_include(cosmo_deps_ctx_t *ctx, const char *path, int is_system) {
    if (!ctx || !path) return -1;

    // Skip system headers if configured
    if (ctx->exclude_system && is_system) {
        return 0;
    }

    // Check if already added (avoid duplicates)
    for (int i = 0; i < ctx->include_count; i++) {
        if (strcmp(ctx->includes[i], path) == 0) {
            return 0;  // Already tracked
        }
    }

    // Expand array if needed
    if (ctx->include_count >= ctx->include_capacity) {
        int new_capacity = ctx->include_capacity * 2;
        char **new_includes = realloc(ctx->includes, (size_t)new_capacity * sizeof(char*));
        if (!new_includes) return -1;
        ctx->includes = new_includes;
        ctx->include_capacity = new_capacity;
    }

    // Add new include
    ctx->includes[ctx->include_count] = strdup(path);
    if (!ctx->includes[ctx->include_count]) return -1;
    ctx->include_count++;

    return 0;
}

// Get default target name (source.c -> source.o)
const char* cosmo_deps_get_default_target(const char *source_file, const char *output_file) {
    static char target_buf[COSMO_DEPS_MAX_PATH_LEN];

    // Use output file if specified
    if (output_file && output_file[0]) {
        return output_file;
    }

    // Otherwise derive from source: foo.c -> foo.o
    if (source_file) {
        const char *base = strrchr(source_file, '/');
        if (!base) base = strrchr(source_file, '\\');
        if (!base) base = source_file;
        else base++;  // Skip separator

        // Copy and replace extension
        snprintf(target_buf, sizeof(target_buf), "%s", base);
        char *ext = strrchr(target_buf, '.');
        if (ext) {
            strcpy(ext, ".o");
        } else {
            strcat(target_buf, ".o");
        }
        return target_buf;
    }

    return "output.o";
}

// Extract dependencies from preprocessed output
// Parses #line directives to find included files
int cosmo_deps_extract_from_preprocess(cosmo_deps_ctx_t *ctx, const char *preprocess_output) {
    if (!ctx || !preprocess_output) return -1;

    const char *line_start = preprocess_output;
    const char *line_end;

    while (*line_start) {
        // Find end of line
        line_end = strchr(line_start, '\n');
        if (!line_end) line_end = line_start + strlen(line_start);

        // Check for #line directive (format: # linenum "filename" flags)
        if (line_start[0] == '#' && (line_start[1] == ' ' || line_start[1] == '\t')) {
            const char *p = line_start + 1;
            while (*p == ' ' || *p == '\t') p++;

            // Skip line number
            while (*p >= '0' && *p <= '9') p++;
            while (*p == ' ' || *p == '\t') p++;

            // Extract filename
            if (*p == '"') {
                p++;  // Skip opening quote
                const char *filename_start = p;
                while (*p && *p != '"' && p < line_end) p++;

                if (*p == '"') {
                    // Found complete filename
                    int filename_len = (int)(p - filename_start);
                    if (filename_len > 0 && filename_len < COSMO_DEPS_MAX_PATH_LEN) {
                        char filename[COSMO_DEPS_MAX_PATH_LEN];
                        memcpy(filename, filename_start, (size_t)filename_len);
                        filename[filename_len] = '\0';

                        // Check if it's a system header
                        int is_system = cosmo_deps_is_system_path(filename);

                        // Skip the source file itself
                        if (ctx->source_file && strcmp(filename, ctx->source_file) != 0) {
                            cosmo_deps_add_include(ctx, filename, is_system);
                        }
                    }
                }
            }
        }

        // Move to next line
        if (*line_end == '\n') {
            line_start = line_end + 1;
        } else {
            break;  // End of string
        }
    }

    return 0;
}

// Generate dependency output in Makefile format
int cosmo_deps_generate(cosmo_deps_ctx_t *ctx) {
    if (!ctx) return -1;

    FILE *out = stdout;
    int close_file = 0;

    // Open output file if specified
    if (ctx->dep_file) {
        out = fopen(ctx->dep_file, "w");
        if (!out) {
            perror("fopen");
            return -1;
        }
        close_file = 1;
    }

    // Determine target name
    const char *target = ctx->target;
    if (!target || !target[0]) {
        target = cosmo_deps_get_default_target(ctx->source_file, ctx->output_file);
    }

    // Write main dependency rule: target: source deps...
    fprintf(out, "%s:", target);

    // Add source file as first dependency
    if (ctx->source_file) {
        fprintf(out, " %s", ctx->source_file);
    }

    // Add all included files
    for (int i = 0; i < ctx->include_count; i++) {
        // Use backslash continuation for long lines (Makefile convention)
        if ((i + 1) % 3 == 0) {
            fprintf(out, " \\\n ");
        }
        fprintf(out, " %s", ctx->includes[i]);
    }
    fprintf(out, "\n");

    // Generate phony targets if requested (-MP flag)
    // This prevents errors if header files are deleted
    if (ctx->gen_phony) {
        fprintf(out, "\n");
        for (int i = 0; i < ctx->include_count; i++) {
            fprintf(out, "%s:\n", ctx->includes[i]);
        }
    }

    if (close_file) {
        fclose(out);
    }

    return 0;
}
