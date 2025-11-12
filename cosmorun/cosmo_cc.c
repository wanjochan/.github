/**
 * @file cosmo_cc.c
 * @brief C Compiler Toolchain Implementation
 */

#include "cosmo_cc.h"
#include "cosmo_libc.h"
#include "cosmo_parallel_link.h"
#include "cosmo_got_plt_reloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "../third_party/tinycc.hack/elf.h"

/* Define CLOCK_MONOTONIC if not defined (Cosmopolitan compatibility) */
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

/* Simple time tracking for Cosmopolitan compatibility */
static inline int safe_clock_gettime(int clk_id, struct timespec *tp) {
    (void)clk_id;  /* Unused */
    time_t t = time(NULL);
    tp->tv_sec = t;
    tp->tv_nsec = 0;
    return 0;
}
#define clock_gettime(clk, tp) safe_clock_gettime(clk, tp)

/* ========== Performance Profiling Infrastructure ========== */

/* Timing utilities */
typedef struct {
    const char *name;
    double elapsed_ms;
} TimerEvent;

static struct timeval timer_start;
static TimerEvent timers[256];  /* Increased from 64 to 256 for detailed profiling */
static int timer_count = 0;

static void timer_reset(void) {
    gettimeofday(&timer_start, NULL);
    timer_count = 0;
}

static void timer_record(const char *name) {
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = (now.tv_sec - timer_start.tv_sec) * 1000.0 +
                     (now.tv_usec - timer_start.tv_usec) / 1000.0;
    if (timer_count < 256) {
        timers[timer_count].name = name;
        timers[timer_count].elapsed_ms = elapsed;
        timer_count++;
    }
}

static void timer_print(void) {
    fprintf(stderr, "\n=== PERFORMANCE PROFILE ===\n");
    for (int i = 0; i < timer_count; i++) {
        double phase_time = (i > 0) ? timers[i].elapsed_ms - timers[i-1].elapsed_ms : timers[i].elapsed_ms;
        fprintf(stderr, "%-40s: %7.1fms (total: %7.1fms)\n",
                timers[i].name, phase_time, timers[i].elapsed_ms);
    }
    fprintf(stderr, "=== TOTAL: %.1fms ===\n\n", timers[timer_count-1].elapsed_ms);
}

/* ========== Memory Pool System ========== */

/* Memory pool for small fixed-size allocations (LinkerSymbol, etc.)
 * Reduces malloc/free overhead by allocating in bulk and freeing all at once */
typedef struct MemoryPool {
    void *arena;              /* Current arena block */
    size_t offset;            /* Current offset in arena */
    size_t arena_size;        /* Size of each arena */
    void **arenas;            /* List of all allocated arenas */
    int arena_count;          /* Number of arenas */
    int arena_capacity;       /* Capacity of arenas array */
} MemoryPool;

/* Initialize memory pool with specified arena size */
static MemoryPool* init_memory_pool(size_t arena_size) {
    MemoryPool *pool = (MemoryPool*)calloc(1, sizeof(MemoryPool));
    if (!pool) {
        fprintf(stderr, "linker: failed to allocate memory pool\n");
        return NULL;
    }

    pool->arena_size = arena_size;
    pool->arena_capacity = 16;  /* Start with capacity for 16 arenas */
    pool->arenas = (void**)malloc(pool->arena_capacity * sizeof(void*));
    if (!pool->arenas) {
        free(pool);
        fprintf(stderr, "linker: failed to allocate arena list\n");
        return NULL;
    }

    /* Allocate first arena */
    pool->arena = malloc(arena_size);
    if (!pool->arena) {
        free(pool->arenas);
        free(pool);
        fprintf(stderr, "linker: failed to allocate initial arena\n");
        return NULL;
    }

    pool->arenas[0] = pool->arena;
    pool->arena_count = 1;
    pool->offset = 0;

    return pool;
}

/* Allocate memory from pool */
static void* pool_alloc(MemoryPool *pool, size_t size) {
    if (!pool) return NULL;

    /* Align to 8-byte boundary */
    size = (size + 7) & ~7;

    /* Check if current arena has space */
    if (pool->offset + size > pool->arena_size) {
        /* Need new arena */
        void *new_arena = malloc(pool->arena_size);
        if (!new_arena) {
            fprintf(stderr, "linker: failed to allocate new arena\n");
            return NULL;
        }

        /* Expand arena list if needed */
        if (pool->arena_count >= pool->arena_capacity) {
            int new_cap = pool->arena_capacity * 2;
            void **new_arenas = (void**)realloc(pool->arenas, new_cap * sizeof(void*));
            if (!new_arenas) {
                free(new_arena);
                fprintf(stderr, "linker: failed to expand arena list\n");
                return NULL;
            }
            pool->arenas = new_arenas;
            pool->arena_capacity = new_cap;
        }

        pool->arenas[pool->arena_count++] = new_arena;
        pool->arena = new_arena;
        pool->offset = 0;
    }

    /* Allocate from current arena */
    void *ptr = (char*)pool->arena + pool->offset;
    pool->offset += size;
    return ptr;
}

/* Free entire memory pool */
static void destroy_memory_pool(MemoryPool *pool) {
    if (!pool) return;

    for (int i = 0; i < pool->arena_count; i++) {
        free(pool->arenas[i]);
    }
    free(pool->arenas);
    free(pool);
}

/* ========== Linker Diagnostics System ========== */

/* Log levels */
typedef enum {
    LOG_ERROR = 0,    /* Always show: critical errors */
    LOG_WARN = 1,     /* Show by default: warnings */
    LOG_INFO = 2,     /* Show with -v: informational messages */
    LOG_DEBUG = 3     /* Show with -vv: detailed debug output */
} LogLevel;

/* Global verbosity level */
static LogLevel g_log_level = LOG_WARN;

/* Debug flags for diagnostic output */
static int g_dump_symbols = 0;
static int g_dump_relocations = 0;
static int g_trace_resolve = 0;

/* Linker statistics */
typedef struct {
    int input_objects;
    int archive_objects_extracted;
    int runtime_objects_added;
    int total_symbols;
    int undefined_symbols;
    int weak_symbols;
    int total_relocations;
    int failed_relocations;
    int sections_merged;
    size_t total_code_size;
    size_t total_data_size;
    double link_time_sec;
} LinkerStats;

/* Global statistics */
static LinkerStats g_stats = {0};

/* Logging macros */
#define LOG_ERROR_MSG(...) do { \
    if (g_log_level >= LOG_ERROR) { \
        fprintf(stderr, "linker: error: "); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } \
} while(0)

#define LOG_WARN_MSG(...) do { \
    if (g_log_level >= LOG_WARN) { \
        fprintf(stderr, "linker: warning: "); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } \
} while(0)

#define LOG_INFO_MSG(...) do { \
    if (g_log_level >= LOG_INFO) { \
        fprintf(stderr, "linker: "); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } \
} while(0)

#define LOG_DEBUG_MSG(...) do { \
    if (g_log_level >= LOG_DEBUG) { \
        fprintf(stderr, "linker: debug: "); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } \
} while(0)

/* Progress indicator for long operations */
static void show_progress(const char *operation, int current, int total) {
    if (g_log_level < LOG_INFO) return;

    if (total <= 0) return;
    int percent = (current * 100) / total;
    fprintf(stderr, "\r%s: %d/%d (%d%%)  ", operation, current, total, percent);
    if (current >= total) {
        fprintf(stderr, "\n");
    }
    fflush(stderr);
}

/* Print linker summary statistics */
static void print_linker_summary(void) {
    if (g_log_level < LOG_WARN) return;  /* Only suppress in quiet mode */

    fprintf(stderr, "\n=== Linker Summary ===\n");
    fprintf(stderr, "Input objects:      %d\n", g_stats.input_objects);
    if (g_stats.archive_objects_extracted > 0) {
        fprintf(stderr, "Archive objects:    %d\n", g_stats.archive_objects_extracted);
    }
    if (g_stats.runtime_objects_added > 0) {
        fprintf(stderr, "Runtime objects:    %d\n", g_stats.runtime_objects_added);
    }
    fprintf(stderr, "Total symbols:      %d\n", g_stats.total_symbols);
    if (g_stats.undefined_symbols > 0) {
        fprintf(stderr, "Undefined symbols:  %d\n", g_stats.undefined_symbols);
    }
    if (g_stats.weak_symbols > 0) {
        fprintf(stderr, "Weak symbols:       %d\n", g_stats.weak_symbols);
    }
    if (g_stats.total_relocations > 0) {
        fprintf(stderr, "Relocations:        %d", g_stats.total_relocations);
        if (g_stats.failed_relocations > 0) {
            fprintf(stderr, " (%d failed)", g_stats.failed_relocations);
        }
        fprintf(stderr, "\n");
    }
    if (g_stats.sections_merged > 0) {
        fprintf(stderr, "Sections merged:    %d\n", g_stats.sections_merged);
    }
    if (g_stats.total_code_size > 0) {
        fprintf(stderr, "Code size:          %zu bytes\n", g_stats.total_code_size);
    }
    if (g_stats.total_data_size > 0) {
        fprintf(stderr, "Data size:          %zu bytes\n", g_stats.total_data_size);
    }
    if (g_stats.link_time_sec > 0) {
        fprintf(stderr, "Link time:          %.3fs\n", g_stats.link_time_sec);
    }
    fprintf(stderr, "=====================\n");
}

/* Set linker verbosity level */
static void set_linker_verbosity(LogLevel level) {
    g_log_level = level;
}

/* Public API for setting verbosity */
void cosmo_linker_set_verbosity(int level) {
    if (level < 0) level = 0;
    if (level > 3) level = 3;
    g_log_level = (LogLevel)level;
}

/* Public API for setting debug flags */
void cosmo_linker_set_dump_symbols(int enable) {
    g_dump_symbols = enable;
}

void cosmo_linker_set_dump_relocations(int enable) {
    g_dump_relocations = enable;
}

void cosmo_linker_set_trace_resolve(int enable) {
    g_trace_resolve = enable;
}

/* Reset statistics */
static void reset_linker_stats(void) {
    memset(&g_stats, 0, sizeof(g_stats));
}

/* ========== AR Archive Tool Implementation ========== */

/* AR file format constants */
#define AR_MAGIC "!<arch>\n"
#define AR_MAGIC_LEN 8
#define AR_FMAG "`\n"
#define AR_FMAG_LEN 2

/* AR header structure (60 bytes) */
struct ar_hdr {
    char ar_name[16];    /* File name */
    char ar_date[12];    /* Modification date (decimal) */
    char ar_uid[6];      /* User ID (decimal) */
    char ar_gid[6];      /* Group ID (decimal) */
    char ar_mode[8];     /* File mode (octal) */
    char ar_size[10];    /* File size (decimal) */
    char ar_fmag[2];     /* Magic string ("`\n") */
};

/**
 * Helper: Parse AR header and extract member info
 * @return member size on success, -1 on error
 */
static long parse_ar_header(FILE *fp, char *name_out, long *mtime_out) {
    struct ar_hdr hdr;

    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) {
        return -1;
    }

    /* Verify magic trailer */
    if (memcmp(hdr.ar_fmag, AR_FMAG, AR_FMAG_LEN) != 0) {
        fprintf(stderr, "ar: invalid header magic\n");
        return -1;
    }

    /* Extract name (remove trailing spaces) */
    int name_len = 0;
    for (int i = 0; i < 16 && hdr.ar_name[i] != ' ' && hdr.ar_name[i] != '/'; i++) {
        name_out[name_len++] = hdr.ar_name[i];
    }
    name_out[name_len] = '\0';

    /* Parse size (ASCII decimal) */
    char size_str[11];
    memcpy(size_str, hdr.ar_size, 10);
    size_str[10] = '\0';
    long size = atol(size_str);

    /* Parse timestamp if requested */
    if (mtime_out) {
        char time_str[13];
        memcpy(time_str, hdr.ar_date, 12);
        time_str[12] = '\0';
        *mtime_out = atol(time_str);
    }

    return size;
}

/**
 * Helper: Write AR file header for a member
 * @return 0 on success, -1 on error
 */
static int write_ar_header(FILE *fp, const char *name, long size, long mtime) {
    struct ar_hdr hdr;
    memset(&hdr, ' ', sizeof(hdr));

    /* Write name (truncate if > 16 chars) */
    int name_len = strlen(name);
    if (name_len > 16) name_len = 16;
    memcpy(hdr.ar_name, name, name_len);

    /* Write fields as ASCII strings (pad with spaces, no null terminator) */
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%-12ld", mtime);
    memcpy(hdr.ar_date, tmp, 12);
    snprintf(tmp, sizeof(tmp), "%-6d", 0);
    memcpy(hdr.ar_uid, tmp, 6);
    snprintf(tmp, sizeof(tmp), "%-6d", 0);
    memcpy(hdr.ar_gid, tmp, 6);
    snprintf(tmp, sizeof(tmp), "%-8o", 0644);
    memcpy(hdr.ar_mode, tmp, 8);
    snprintf(tmp, sizeof(tmp), "%-10ld", size);
    memcpy(hdr.ar_size, tmp, 10);
    memcpy(hdr.ar_fmag, AR_FMAG, AR_FMAG_LEN);

    if (fwrite(&hdr, sizeof(hdr), 1, fp) != 1) {
        return -1;
    }
    return 0;
}

int cosmo_ar_create(const char *archive, const char **objects, int count, int verbose) {
    FILE *ar_fp = NULL;
    FILE *obj_fp = NULL;
    int ret = -1;

    /* Open archive for writing */
    ar_fp = fopen(archive, "wb");
    if (!ar_fp) {
        fprintf(stderr, "ar: cannot create '%s': %s\n", archive, strerror(errno));
        return -1;
    }

    /* Write AR magic header */
    if (fwrite(AR_MAGIC, AR_MAGIC_LEN, 1, ar_fp) != 1) {
        fprintf(stderr, "ar: failed to write magic header\n");
        goto cleanup;
    }

    /* Add each object file */
    for (int i = 0; i < count; i++) {
        const char *obj = objects[i];
        struct stat st;

        /* Get file info */
        if (stat(obj, &st) != 0) {
            fprintf(stderr, "ar: '%s': %s\n", obj, strerror(errno));
            goto cleanup;
        }

        /* Extract basename for archive member name */
        const char *basename = strrchr(obj, '/');
        basename = basename ? basename + 1 : obj;

        if (verbose) {
            printf("a - %s\n", basename);
        }

        /* Write AR header */
        if (write_ar_header(ar_fp, basename, st.st_size, st.st_mtime) != 0) {
            fprintf(stderr, "ar: failed to write header for '%s'\n", obj);
            goto cleanup;
        }

        /* Copy file contents */
        obj_fp = fopen(obj, "rb");
        if (!obj_fp) {
            fprintf(stderr, "ar: cannot open '%s': %s\n", obj, strerror(errno));
            goto cleanup;
        }

        char buffer[8192];
        size_t bytes_written = 0;
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), obj_fp)) > 0) {
            if (fwrite(buffer, 1, n, ar_fp) != n) {
                fprintf(stderr, "ar: write error for '%s'\n", obj);
                fclose(obj_fp);
                goto cleanup;
            }
            bytes_written += n;
        }
        fclose(obj_fp);
        obj_fp = NULL;

        /* Add padding byte if size is odd (AR requires 2-byte alignment) */
        if (bytes_written & 1) {
            char pad = '\n';
            if (fwrite(&pad, 1, 1, ar_fp) != 1) {
                fprintf(stderr, "ar: failed to write padding\n");
                goto cleanup;
            }
        }
    }

    ret = 0;

cleanup:
    if (obj_fp) fclose(obj_fp);
    if (ar_fp) fclose(ar_fp);
    return ret;
}

int cosmo_ar_extract(const char *archive, const char *member, int verbose) {
    FILE *ar_fp = fopen(archive, "rb");
    if (!ar_fp) {
        fprintf(stderr, "ar: cannot open '%s': %s\n", archive, strerror(errno));
        return -1;
    }

    /* Read and verify magic header */
    char magic[AR_MAGIC_LEN];
    if (fread(magic, AR_MAGIC_LEN, 1, ar_fp) != 1 ||
        memcmp(magic, AR_MAGIC, AR_MAGIC_LEN) != 0) {
        fprintf(stderr, "ar: '%s': not an archive\n", archive);
        fclose(ar_fp);
        return -1;
    }

    int found = 0;
    int ret = 0;

    /* Extract member(s) */
    while (!feof(ar_fp)) {
        char name[17];
        long size = parse_ar_header(ar_fp, name, NULL);

        if (size < 0) {
            if (feof(ar_fp)) break;
            fclose(ar_fp);
            return -1;
        }

        /* Check if we should extract this member */
        int should_extract = (member == NULL || strcmp(name, member) == 0);

        if (should_extract) {
            found = 1;
            if (verbose) {
                printf("x - %s\n", name);
            }

            /* Extract to file */
            FILE *out_fp = fopen(name, "wb");
            if (!out_fp) {
                fprintf(stderr, "ar: cannot create '%s': %s\n", name, strerror(errno));
                fclose(ar_fp);
                return -1;
            }

            /* Copy data */
            char buffer[8192];
            long remaining = size;
            while (remaining > 0) {
                size_t to_read = (remaining > (long)sizeof(buffer)) ? sizeof(buffer) : (size_t)remaining;
                size_t n = fread(buffer, 1, to_read, ar_fp);
                if (n == 0) break;
                if (fwrite(buffer, 1, n, out_fp) != n) {
                    fprintf(stderr, "ar: write error for '%s'\n", name);
                    fclose(out_fp);
                    fclose(ar_fp);
                    return -1;
                }
                remaining -= n;
            }
            fclose(out_fp);

            /* Skip padding if needed */
            if (size & 1) {
                fseek(ar_fp, 1, SEEK_CUR);
            }

            /* If extracting specific member, we're done */
            if (member != NULL) break;
        } else {
            /* Skip this member */
            long skip = size;
            if (skip & 1) skip++; /* Add padding byte if odd size */
            if (fseek(ar_fp, skip, SEEK_CUR) != 0) {
                break;
            }
        }
    }

    fclose(ar_fp);

    if (member != NULL && !found) {
        fprintf(stderr, "ar: member '%s' not found\n", member);
        return -1;
    }

    return ret;
}

int cosmo_ar_list(const char *archive, int verbose) {
    FILE *fp = fopen(archive, "rb");
    if (!fp) {
        fprintf(stderr, "ar: cannot open '%s': %s\n", archive, strerror(errno));
        return -1;
    }

    /* Read and verify magic header */
    char magic[AR_MAGIC_LEN];
    if (fread(magic, AR_MAGIC_LEN, 1, fp) != 1 ||
        memcmp(magic, AR_MAGIC, AR_MAGIC_LEN) != 0) {
        fprintf(stderr, "ar: '%s': not an archive\n", archive);
        fclose(fp);
        return -1;
    }

    /* List each member */
    while (!feof(fp)) {
        char name[17];
        long mtime = 0;
        long size = parse_ar_header(fp, name, verbose ? &mtime : NULL);

        if (size < 0) {
            if (feof(fp)) break;
            fclose(fp);
            return -1;
        }

        if (verbose) {
            /* Format: rw-r--r-- uid/gid size date time name */
            time_t t = (time_t)mtime;
            struct tm *tm = localtime(&t);
            char time_str[32];
            strftime(time_str, sizeof(time_str), "%b %d %H:%M %Y", tm);
            printf("rw-r--r-- %d/%d %7ld %s %s\n", 0, 0, size, time_str, name);
        } else {
            printf("%s\n", name);
        }

        /* Skip to next member (handle 2-byte alignment) */
        long skip = size;
        if (skip & 1) skip++; /* Add padding byte if odd size */
        if (fseek(fp, skip, SEEK_CUR) != 0) {
            break;
        }
    }

    fclose(fp);
    return 0;
}

int cosmo_ar_delete(const char *archive, const char *member) {
    FILE *in_fp = NULL;
    FILE *out_fp = NULL;
    char temp_file[256];
    int ret = -1;
    int found = 0;

    /* Open input archive */
    in_fp = fopen(archive, "rb");
    if (!in_fp) {
        fprintf(stderr, "ar: cannot open '%s': %s\n", archive, strerror(errno));
        return -1;
    }

    /* Create temporary output file */
    snprintf(temp_file, sizeof(temp_file), "%s.tmp", archive);
    out_fp = fopen(temp_file, "wb");
    if (!out_fp) {
        fprintf(stderr, "ar: cannot create temporary file: %s\n", strerror(errno));
        fclose(in_fp);
        return -1;
    }

    /* Read and verify magic header */
    char magic[AR_MAGIC_LEN];
    if (fread(magic, AR_MAGIC_LEN, 1, in_fp) != 1 ||
        memcmp(magic, AR_MAGIC, AR_MAGIC_LEN) != 0) {
        fprintf(stderr, "ar: '%s': not an archive\n", archive);
        goto cleanup;
    }

    /* Write magic to output */
    if (fwrite(AR_MAGIC, AR_MAGIC_LEN, 1, out_fp) != 1) {
        fprintf(stderr, "ar: failed to write magic header\n");
        goto cleanup;
    }

    /* Copy members except the one to delete */
    while (!feof(in_fp)) {
        long pos = ftell(in_fp);
        char name[17];
        long size = parse_ar_header(in_fp, name, NULL);

        if (size < 0) {
            if (feof(in_fp)) break;
            goto cleanup;
        }

        /* Check if this is the member to delete */
        if (strcmp(name, member) == 0) {
            found = 1;
            /* Skip this member */
            long skip = size;
            if (skip & 1) skip++; /* Add padding byte if odd size */
            if (fseek(in_fp, skip, SEEK_CUR) != 0) {
                goto cleanup;
            }
            continue;
        }

        /* Copy this member to output */
        /* First, rewind to read the header again */
        if (fseek(in_fp, pos, SEEK_SET) != 0) {
            goto cleanup;
        }

        /* Copy header (60 bytes) */
        struct ar_hdr hdr;
        if (fread(&hdr, sizeof(hdr), 1, in_fp) != 1) {
            goto cleanup;
        }
        if (fwrite(&hdr, sizeof(hdr), 1, out_fp) != 1) {
            fprintf(stderr, "ar: write error\n");
            goto cleanup;
        }

        /* Copy data */
        char buffer[8192];
        long remaining = size;
        while (remaining > 0) {
            size_t to_read = (remaining > (long)sizeof(buffer)) ? sizeof(buffer) : (size_t)remaining;
            size_t n = fread(buffer, 1, to_read, in_fp);
            if (n == 0) break;
            if (fwrite(buffer, 1, n, out_fp) != n) {
                fprintf(stderr, "ar: write error\n");
                goto cleanup;
            }
            remaining -= n;
        }

        /* Copy padding if needed */
        if (size & 1) {
            char pad;
            if (fread(&pad, 1, 1, in_fp) != 1) {
                /* End of file, no padding */
                break;
            }
            if (fwrite(&pad, 1, 1, out_fp) != 1) {
                fprintf(stderr, "ar: write error\n");
                goto cleanup;
            }
        }
    }

    if (!found) {
        fprintf(stderr, "ar: member '%s' not found\n", member);
        goto cleanup;
    }

    ret = 0;

cleanup:
    if (in_fp) fclose(in_fp);
    if (out_fp) fclose(out_fp);

    if (ret == 0) {
        /* Replace original with temporary */
        if (unlink(archive) != 0 || rename(temp_file, archive) != 0) {
            fprintf(stderr, "ar: failed to update archive: %s\n", strerror(errno));
            ret = -1;
        }
    } else {
        /* Remove temporary file */
        unlink(temp_file);
    }

    return ret;
}


/* ========== Standalone Linker Implementation ========== */

#include "../third_party/tinycc.hack/libtcc.h"

/* Forward declaration for complete linker pipeline (defined at end of file) */
static int linker_pipeline_full(const char **objects, int count, const char *output,
                                const char **lib_paths, int lib_count,
                                const char **libs, int libs_count,
                                LibcBackend libc_backend, int gc_sections);

int parse_libc_option(const char *arg) {
    if (!arg) return -1;

    if (strcmp(arg, "cosmo") == 0) {
        return LIBC_COSMO;
    } else if (strcmp(arg, "system") == 0) {
        return LIBC_SYSTEM;
    } else if (strcmp(arg, "mini") == 0) {
        return LIBC_MINI;
    } else {
        return -1;
    }
}

int cosmo_link(const char **objects, int count, const char *output,
               const char **lib_paths, int lib_count,
               const char **libs, int libs_count,
               LibcBackend libc_backend, int gc_sections) {
    if (!objects || count <= 0 || !output) {
        fprintf(stderr, "cosmo_link: Invalid arguments\n");
        return -1;
    }

    /* Use custom linker pipeline (7-phase implementation at end of file) */
    return linker_pipeline_full(objects, count, output, lib_paths, lib_count, libs, libs_count, libc_backend, gc_sections);
}


/* ========== nm - Symbol Table Tool Implementation ========== */

/**
 * Get symbol type character for nm output
 */
static char get_symbol_type(Elf64_Sym *sym, Elf64_Shdr *sections) {
    unsigned char bind = ELF64_ST_BIND(sym->st_info);
    unsigned char type = ELF64_ST_TYPE(sym->st_info);
    uint16_t shndx = sym->st_shndx;

    /* Undefined symbol */
    if (shndx == SHN_UNDEF) {
        return 'U';
    }

    /* Absolute symbol */
    if (shndx == SHN_ABS) {
        return (bind == STB_LOCAL) ? 'a' : 'A';
    }

    /* Common symbol */
    if (shndx == SHN_COMMON) {
        return (bind == STB_LOCAL) ? 'c' : 'C';
    }

    /* Get section flags */
    if (shndx < SHN_LORESERVE) {
        Elf64_Shdr *sec = &sections[shndx];
        uint64_t flags = sec->sh_flags;
        uint32_t sec_type = sec->sh_type;

        /* BSS section */
        if (sec_type == SHT_NOBITS && (flags & SHF_ALLOC)) {
            return (bind == STB_LOCAL) ? 'b' : 'B';
        }

        /* Text (code) section */
        if ((flags & SHF_EXECINSTR) && (flags & SHF_ALLOC)) {
            return (bind == STB_LOCAL) ? 't' : 'T';
        }

        /* Read-only data section */
        if ((flags & SHF_ALLOC) && !(flags & SHF_WRITE)) {
            return (bind == STB_LOCAL) ? 'r' : 'R';
        }

        /* Writable data section */
        if ((flags & SHF_ALLOC) && (flags & SHF_WRITE)) {
            return (bind == STB_LOCAL) ? 'd' : 'D';
        }
    }

    /* Default: other types */
    return (bind == STB_LOCAL) ? '?' : '?';
}

/**
 * Compare symbols for sorting (by address, then name)
 */
typedef struct {
    uint64_t addr;
    char type;
    const char *name;
} SymbolEntry;

static int compare_symbols(const void *a, const void *b) {
    const SymbolEntry *sa = (const SymbolEntry *)a;
    const SymbolEntry *sb = (const SymbolEntry *)b;

    /* Sort by address first */
    if (sa->addr < sb->addr) return -1;
    if (sa->addr > sb->addr) return 1;

    /* Then by name */
    return strcmp(sa->name, sb->name);
}

int cosmo_nm(const char *file, int format, int flags) {
    FILE *f = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Shdr *sections = NULL;
    Elf64_Sym *symbols = NULL;
    char *strtab = NULL;
    SymbolEntry *entries = NULL;
    int result = -1;

    /* Open ELF file */
    f = fopen(file, "rb");
    if (!f) {
        fprintf(stderr, "nm: cannot open '%s': No such file\n", file);
        return -1;
    }

    /* Read ELF header */
    if (fread(&ehdr, sizeof(ehdr), 1, f) != 1) {
        fprintf(stderr, "nm: '%s': Failed to read ELF header\n", file);
        goto cleanup;
    }

    /* Verify ELF magic */
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "nm: '%s': Not an ELF file\n", file);
        goto cleanup;
    }

    /* Verify 64-bit ELF */
    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "nm: '%s': Not a 64-bit ELF file\n", file);
        goto cleanup;
    }

    /* Read section headers */
    sections = (Elf64_Shdr *)malloc(ehdr.e_shnum * sizeof(Elf64_Shdr));
    if (!sections) {
        fprintf(stderr, "nm: Out of memory\n");
        goto cleanup;
    }

    if (fseek(f, ehdr.e_shoff, SEEK_SET) != 0) {
        fprintf(stderr, "nm: '%s': Failed to seek to section headers\n", file);
        goto cleanup;
    }

    if (fread(sections, sizeof(Elf64_Shdr), ehdr.e_shnum, f) != ehdr.e_shnum) {
        fprintf(stderr, "nm: '%s': Failed to read section headers\n", file);
        goto cleanup;
    }

    /* Find symbol table and string table */
    Elf64_Shdr *symtab_hdr = NULL;
    Elf64_Shdr *strtab_hdr = NULL;

    for (int i = 0; i < ehdr.e_shnum; i++) {
        if (sections[i].sh_type == SHT_SYMTAB) {
            symtab_hdr = &sections[i];
            /* String table is linked section */
            if (sections[i].sh_link < ehdr.e_shnum) {
                strtab_hdr = &sections[sections[i].sh_link];
            }
            break;
        }
    }

    if (!symtab_hdr || !strtab_hdr) {
        fprintf(stderr, "nm: '%s': No symbols\n", file);
        result = 0;
        goto cleanup;
    }

    /* Read string table */
    strtab = (char *)malloc(strtab_hdr->sh_size);
    if (!strtab) {
        fprintf(stderr, "nm: Out of memory\n");
        goto cleanup;
    }

    if (fseek(f, strtab_hdr->sh_offset, SEEK_SET) != 0) {
        fprintf(stderr, "nm: '%s': Failed to seek to string table\n", file);
        goto cleanup;
    }

    if (fread(strtab, 1, strtab_hdr->sh_size, f) != strtab_hdr->sh_size) {
        fprintf(stderr, "nm: '%s': Failed to read string table\n", file);
        goto cleanup;
    }

    /* Read symbol table */
    size_t num_symbols = symtab_hdr->sh_size / sizeof(Elf64_Sym);
    symbols = (Elf64_Sym *)malloc(symtab_hdr->sh_size);
    if (!symbols) {
        fprintf(stderr, "nm: Out of memory\n");
        goto cleanup;
    }

    if (fseek(f, symtab_hdr->sh_offset, SEEK_SET) != 0) {
        fprintf(stderr, "nm: '%s': Failed to seek to symbol table\n", file);
        goto cleanup;
    }

    if (fread(symbols, sizeof(Elf64_Sym), num_symbols, f) != num_symbols) {
        fprintf(stderr, "nm: '%s': Failed to read symbol table\n", file);
        goto cleanup;
    }

    /* Build symbol entries array */
    entries = (SymbolEntry *)malloc(num_symbols * sizeof(SymbolEntry));
    if (!entries) {
        fprintf(stderr, "nm: Out of memory\n");
        goto cleanup;
    }

    size_t entry_count = 0;
    for (size_t i = 0; i < num_symbols; i++) {
        Elf64_Sym *sym = &symbols[i];

        /* Skip symbols without names */
        if (sym->st_name == 0 || sym->st_name >= strtab_hdr->sh_size) {
            continue;
        }

        /* Skip section symbols and file symbols */
        unsigned char type = ELF64_ST_TYPE(sym->st_info);
        if (type == STT_SECTION || type == STT_FILE) {
            continue;
        }

        /* Apply filters */
        if (flags & NM_FILTER_UNDEF) {
            if (sym->st_shndx != SHN_UNDEF) continue;
        }

        if (flags & NM_FILTER_EXTERN) {
            unsigned char bind = ELF64_ST_BIND(sym->st_info);
            if (bind == STB_LOCAL) continue;
        }

        entries[entry_count].addr = sym->st_value;
        entries[entry_count].type = get_symbol_type(sym, sections);
        entries[entry_count].name = &strtab[sym->st_name];
        entry_count++;
    }

    /* Sort symbols */
    qsort(entries, entry_count, sizeof(SymbolEntry), compare_symbols);

    /* Output symbols */
    for (size_t i = 0; i < entry_count; i++) {
        SymbolEntry *e = &entries[i];

        switch (format) {
        case NM_FORMAT_BSD:
            /* BSD format: "address type name" */
            if (e->type == 'U') {
                printf("                 %c %s\n", e->type, e->name);
            } else {
                printf("%016lx %c %s\n", e->addr, e->type, e->name);
            }
            break;

        case NM_FORMAT_POSIX:
            /* POSIX format: "name type value [size]" */
            if (e->type == 'U') {
                printf("%s %c\n", e->name, e->type);
            } else {
                printf("%s %c %016lx\n", e->name, e->type, e->addr);
            }
            break;

        case NM_FORMAT_SYSV:
            /* System V format (table) */
            if (i == 0) {
                printf("\nSymbols from %s:\n\n", file);
                printf("%-40s|%-8s|%-18s|%-8s\n",
                       "Name", "Type", "Value", "Size");
                printf("----------------------------------------------------------------\n");
            }
            printf("%-40s|%-8c|0x%016lx|%-8s\n",
                   e->name, e->type, e->addr, "");
            break;
        }
    }

    result = 0;

cleanup:
    if (f) fclose(f);
    if (sections) free(sections);
    if (symbols) free(symbols);
    if (strtab) free(strtab);
    if (entries) free(entries);

    return result;
}


/* ========== objdump - Object File Disassembler Implementation ========== */

/* Helper: Get section name from string table (bounds-checked) */
static const char* get_section_name(const char *strtab, int name_idx) {
    if (!strtab || name_idx < 0) return "";
    /* Note: Caller should verify name_idx < strtab_size before calling */
    return strtab + name_idx;
}

/* Helper: Get section type name */
static const char* get_section_type(uint32_t type) {
    switch (type) {
        case SHT_NULL: return "NULL";
        case SHT_PROGBITS: return "PROGBITS";
        case SHT_SYMTAB: return "SYMTAB";
        case SHT_STRTAB: return "STRTAB";
        case SHT_RELA: return "RELA";
        case SHT_HASH: return "HASH";
        case SHT_DYNAMIC: return "DYNAMIC";
        case SHT_NOTE: return "NOTE";
        case SHT_NOBITS: return "NOBITS";
        case SHT_REL: return "REL";
        case SHT_DYNSYM: return "DYNSYM";
        case SHT_INIT_ARRAY: return "INIT_ARRAY";
        case SHT_FINI_ARRAY: return "FINI_ARRAY";
        default: return "UNKNOWN";
    }
}

/* Helper: Get section flags string */
static void get_section_flags(uint64_t flags, char *buf, size_t size) {
    buf[0] = '\0';
    if (flags & SHF_WRITE) strcat(buf, "W");
    if (flags & SHF_ALLOC) strcat(buf, "A");
    if (flags & SHF_EXECINSTR) strcat(buf, "X");
    if (flags & SHF_MERGE) strcat(buf, "M");
    if (flags & SHF_STRINGS) strcat(buf, "S");
}

/* Helper: Display section headers for ELF64 */
static int display_section_headers_64(FILE *fp, Elf64_Ehdr *ehdr) {
    Elf64_Shdr *sections;
    char *shstrtab;
    int i;

    /* Read section headers */
    sections = (Elf64_Shdr*)malloc(ehdr->e_shnum * sizeof(Elf64_Shdr));
    if (!sections) {
        fprintf(stderr, "Memory allocation failed\n");
        return -1;
    }

    fseek(fp, ehdr->e_shoff, SEEK_SET);
    if (fread(sections, sizeof(Elf64_Shdr), ehdr->e_shnum, fp) != ehdr->e_shnum) {
        fprintf(stderr, "Failed to read section headers\n");
        free(sections);
        return -1;
    }

    /* Read section header string table */
    if (ehdr->e_shstrndx >= ehdr->e_shnum) {
        fprintf(stderr, "Invalid section header string table index\n");
        free(sections);
        return -1;
    }

    Elf64_Shdr *shstrtab_hdr = &sections[ehdr->e_shstrndx];
    shstrtab = (char*)malloc(shstrtab_hdr->sh_size);
    if (!shstrtab) {
        fprintf(stderr, "Memory allocation failed\n");
        free(sections);
        return -1;
    }

    fseek(fp, shstrtab_hdr->sh_offset, SEEK_SET);
    if (fread(shstrtab, 1, shstrtab_hdr->sh_size, fp) != shstrtab_hdr->sh_size) {
        fprintf(stderr, "Failed to read section header string table\n");
        free(shstrtab);
        free(sections);
        return -1;
    }

    /* Display section headers */
    printf("\nSections:\n");
    printf("Idx Name              Type            Address          Off    Size   Flags\n");

    for (i = 0; i < ehdr->e_shnum; i++) {
        const char *name = get_section_name(shstrtab, sections[i].sh_name);
        const char *type = get_section_type(sections[i].sh_type);
        char flags[16];
        get_section_flags(sections[i].sh_flags, flags, sizeof(flags));

        printf("%3d %-17s %-15s %016lx %06lx %06lx %-5s\n",
               i, name, type,
               (unsigned long)sections[i].sh_addr,
               (unsigned long)sections[i].sh_offset,
               (unsigned long)sections[i].sh_size,
               flags);
    }

    free(shstrtab);
    free(sections);
    return 0;
}

/* Helper: Display symbol table for ELF64 */
static int display_symbols_64(FILE *fp, Elf64_Ehdr *ehdr) {
    Elf64_Shdr *sections;
    char *shstrtab, *strtab = NULL;
    int i, j;
    Elf64_Sym *symtab = NULL;
    int symcount = 0;

    /* Read section headers */
    sections = (Elf64_Shdr*)malloc(ehdr->e_shnum * sizeof(Elf64_Shdr));
    if (!sections) return -1;

    fseek(fp, ehdr->e_shoff, SEEK_SET);
    if (fread(sections, sizeof(Elf64_Shdr), ehdr->e_shnum, fp) != ehdr->e_shnum) {
        free(sections);
        return -1;
    }

    /* Find symbol table and string table */
    for (i = 0; i < ehdr->e_shnum; i++) {
        if (sections[i].sh_type == SHT_SYMTAB) {
            symcount = sections[i].sh_size / sizeof(Elf64_Sym);
            symtab = (Elf64_Sym*)malloc(sections[i].sh_size);
            fseek(fp, sections[i].sh_offset, SEEK_SET);
            fread(symtab, sections[i].sh_size, 1, fp);

            /* Get associated string table */
            if (sections[i].sh_link < ehdr->e_shnum) {
                strtab = (char*)malloc(sections[sections[i].sh_link].sh_size);
                fseek(fp, sections[sections[i].sh_link].sh_offset, SEEK_SET);
                fread(strtab, sections[sections[i].sh_link].sh_size, 1, fp);
            }
            break;
        }
    }

    if (!symtab) {
        printf("No symbol table found\n");
        free(sections);
        return 0;
    }

    printf("\nSYMBOL TABLE:\n");
    for (j = 0; j < symcount; j++) {
        const char *name = (strtab && symtab[j].st_name) ?
                          strtab + symtab[j].st_name : "";
        printf("%016lx  %c  %s\n",
               (unsigned long)symtab[j].st_value,
               (ELF64_ST_BIND(symtab[j].st_info) == STB_GLOBAL) ? 'g' : 'l',
               name);
    }

    free(symtab);
    if (strtab) free(strtab);
    free(sections);
    return 0;
}

/* Helper: Display relocations for ELF64 */
static int display_relocations_64(FILE *fp, Elf64_Ehdr *ehdr) {
    Elf64_Shdr *sections;
    char *shstrtab;
    int i, j;

    sections = (Elf64_Shdr*)malloc(ehdr->e_shnum * sizeof(Elf64_Shdr));
    if (!sections) return -1;

    fseek(fp, ehdr->e_shoff, SEEK_SET);
    fread(sections, sizeof(Elf64_Shdr), ehdr->e_shnum, fp);

    /* Read section name string table */
    Elf64_Shdr *shstrtab_hdr = &sections[ehdr->e_shstrndx];
    shstrtab = (char*)malloc(shstrtab_hdr->sh_size);
    fseek(fp, shstrtab_hdr->sh_offset, SEEK_SET);
    fread(shstrtab, 1, shstrtab_hdr->sh_size, fp);

    printf("\nRELOCATION TABLES:\n");

    for (i = 0; i < ehdr->e_shnum; i++) {
        if (sections[i].sh_type == SHT_RELA) {
            const char *name = get_section_name(shstrtab, sections[i].sh_name);
            printf("\n%s:\n", name);
            printf("Offset           Type             Symbol\n");

            Elf64_Rela *rela = (Elf64_Rela*)malloc(sections[i].sh_size);
            fseek(fp, sections[i].sh_offset, SEEK_SET);
            fread(rela, sections[i].sh_size, 1, fp);

            int rela_count = sections[i].sh_size / sizeof(Elf64_Rela);
            for (j = 0; j < rela_count; j++) {
                printf("%016lx  R_X86_64_%-8lu  %lu\n",
                       (unsigned long)rela[j].r_offset,
                       (unsigned long)ELF64_R_TYPE(rela[j].r_info),
                       (unsigned long)ELF64_R_SYM(rela[j].r_info));
            }
            free(rela);
        }
    }

    free(shstrtab);
    free(sections);
    return 0;
}

/* Helper: Disassemble code sections (hex dump) */
static int disassemble_code_64(FILE *fp, Elf64_Ehdr *ehdr) {
    Elf64_Shdr *sections;
    char *shstrtab;
    int i;

    sections = (Elf64_Shdr*)malloc(ehdr->e_shnum * sizeof(Elf64_Shdr));
    if (!sections) return -1;

    fseek(fp, ehdr->e_shoff, SEEK_SET);
    fread(sections, sizeof(Elf64_Shdr), ehdr->e_shnum, fp);

    Elf64_Shdr *shstrtab_hdr = &sections[ehdr->e_shstrndx];
    shstrtab = (char*)malloc(shstrtab_hdr->sh_size);
    fseek(fp, shstrtab_hdr->sh_offset, SEEK_SET);
    fread(shstrtab, 1, shstrtab_hdr->sh_size, fp);

    printf("\nDISASSEMBLY OF EXECUTABLE SECTIONS:\n");

    for (i = 0; i < ehdr->e_shnum; i++) {
        if (sections[i].sh_flags & SHF_EXECINSTR) {
            const char *name = get_section_name(shstrtab, sections[i].sh_name);
            printf("\n%s:\n", name);

            unsigned char *code = (unsigned char*)malloc(sections[i].sh_size);
            fseek(fp, sections[i].sh_offset, SEEK_SET);
            fread(code, sections[i].sh_size, 1, fp);

            /* Simple hex dump (real disassembly would need capstone/etc) */
            size_t j;
            for (j = 0; j < sections[i].sh_size; j += 16) {
                printf("%08lx: ", (unsigned long)(sections[i].sh_addr + j));
                size_t k;
                for (k = 0; k < 16 && j + k < sections[i].sh_size; k++) {
                    printf("%02x ", code[j + k]);
                }
                printf("\n");
            }
            free(code);
        }
    }

    free(shstrtab);
    free(sections);
    return 0;
}

int cosmo_objdump(const char *file, int flags) {
    FILE *fp;
    Elf64_Ehdr ehdr;

    /* Open file */
    fp = fopen(file, "rb");
    if (!fp) {
        fprintf(stderr, "Cannot open file: %s\n", file);
        return -1;
    }

    /* Read ELF header */
    if (fread(&ehdr, sizeof(ehdr), 1, fp) != 1) {
        fprintf(stderr, "Failed to read ELF header\n");
        fclose(fp);
        return -1;
    }

    /* Validate ELF magic */
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "Not an ELF file\n");
        fclose(fp);
        return -1;
    }

    /* Check if 64-bit ELF */
    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "Only 64-bit ELF files are supported\n");
        fclose(fp);
        return -1;
    }

    printf("%s:     file format elf64-x86-64\n", file);

    /* Display requested information */
    if (flags & OBJDUMP_HEADERS) {
        display_section_headers_64(fp, &ehdr);
    }

    if (flags & OBJDUMP_SYMBOLS) {
        display_symbols_64(fp, &ehdr);
    }

    if (flags & OBJDUMP_RELOC) {
        display_relocations_64(fp, &ehdr);
    }

    if (flags & OBJDUMP_DISASM) {
        disassemble_code_64(fp, &ehdr);
    }

    fclose(fp);
    return 0;
}


/* ========== strip - Symbol Removal Tool Implementation ========== */

/* Helper: Check if section should be kept */
static int should_keep_section(const char *section_name, Elf64_Shdr *shdr, int flags) {
    /* Always keep NULL section */
    if (shdr->sh_type == SHT_NULL) {
        return 1;
    }

    /* STRIP_ALL: Remove all symbol/string tables */
    if (flags & STRIP_ALL) {
        if (shdr->sh_type == SHT_SYMTAB || shdr->sh_type == SHT_STRTAB) {
            /* Keep .shstrtab (section name string table) */
            if (section_name && strcmp(section_name, ".shstrtab") == 0) {
                return 1;
            }
            return 0;
        }
    }

    /* STRIP_DEBUG: Remove debug sections and debug symbols */
    if (flags & STRIP_DEBUG) {
        if (section_name && strncmp(section_name, ".debug", 6) == 0) {
            return 0;
        }
        if (section_name && strncmp(section_name, ".gnu.debuglto", 13) == 0) {
            return 0;
        }
        if (section_name && strcmp(section_name, ".stab") == 0) {
            return 0;
        }
        if (section_name && strcmp(section_name, ".stabstr") == 0) {
            return 0;
        }
    }

    return 1;
}

int cosmo_strip(const char *input, const char *output, int flags) {
    FILE *in = NULL, *out = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Shdr *sections = NULL;
    char *shstrtab = NULL;
    unsigned char *file_data = NULL;
    long file_size = 0;
    int result = -1;
    int i;

    /* Open input file */
    in = fopen(input, "rb");
    if (!in) {
        fprintf(stderr, "strip: Cannot open input file '%s'\n", input);
        return -1;
    }

    /* Get file size */
    fseek(in, 0, SEEK_END);
    file_size = ftell(in);
    fseek(in, 0, SEEK_SET);

    /* Read ELF header */
    if (fread(&ehdr, sizeof(Elf64_Ehdr), 1, in) != 1) {
        fprintf(stderr, "strip: Failed to read ELF header\n");
        goto cleanup;
    }

    /* Verify ELF magic */
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "strip: Not an ELF file\n");
        goto cleanup;
    }

    /* Only support 64-bit ELF for now */
    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "strip: Only 64-bit ELF supported\n");
        goto cleanup;
    }

    /* Read entire file into memory */
    fseek(in, 0, SEEK_SET);
    file_data = (unsigned char *)malloc(file_size);
    if (!file_data) {
        fprintf(stderr, "strip: Out of memory\n");
        goto cleanup;
    }

    if (fread(file_data, 1, file_size, in) != (size_t)file_size) {
        fprintf(stderr, "strip: Failed to read file\n");
        goto cleanup;
    }

    fclose(in);
    in = NULL;

    /* Parse ELF */
    memcpy(&ehdr, file_data, sizeof(Elf64_Ehdr));

    /* Allocate section headers */
    sections = (Elf64_Shdr *)malloc(ehdr.e_shnum * sizeof(Elf64_Shdr));
    if (!sections) {
        fprintf(stderr, "strip: Out of memory\n");
        goto cleanup;
    }

    /* Read section headers */
    memcpy(sections, file_data + ehdr.e_shoff, ehdr.e_shnum * sizeof(Elf64_Shdr));

    /* Read section name string table */
    if (ehdr.e_shstrndx < ehdr.e_shnum) {
        Elf64_Shdr *shstrtab_shdr = &sections[ehdr.e_shstrndx];
        shstrtab = (char *)malloc(shstrtab_shdr->sh_size);
        if (!shstrtab) {
            fprintf(stderr, "strip: Out of memory\n");
            goto cleanup;
        }
        memcpy(shstrtab, file_data + shstrtab_shdr->sh_offset, shstrtab_shdr->sh_size);
    }

    /* Count sections to keep */
    int new_shnum = 0;
    int *section_map = (int *)calloc(ehdr.e_shnum, sizeof(int));
    if (!section_map) {
        fprintf(stderr, "strip: Out of memory\n");
        goto cleanup;
    }

    for (i = 0; i < ehdr.e_shnum; i++) {
        const char *name = NULL;
        if (shstrtab && sections[i].sh_name < sections[ehdr.e_shstrndx].sh_size) {
            name = shstrtab + sections[i].sh_name;
        }

        if (should_keep_section(name, &sections[i], flags)) {
            section_map[i] = new_shnum++;
        } else {
            section_map[i] = -1;
        }
    }

    /* Open output file */
    out = fopen(output, "wb");
    if (!out) {
        fprintf(stderr, "strip: Cannot open output file '%s'\n", output);
        free(section_map);
        goto cleanup;
    }

    /* Write ELF header with updated section count */
    Elf64_Ehdr new_ehdr = ehdr;
    new_ehdr.e_shnum = new_shnum;

    /* Update section header string table index */
    if (ehdr.e_shstrndx < ehdr.e_shnum && section_map[ehdr.e_shstrndx] >= 0) {
        new_ehdr.e_shstrndx = section_map[ehdr.e_shstrndx];
    }

    fwrite(&new_ehdr, sizeof(Elf64_Ehdr), 1, out);

    /* Calculate new section offsets */
    Elf64_Off current_offset = sizeof(Elf64_Ehdr);
    Elf64_Shdr *new_sections = (Elf64_Shdr *)calloc(new_shnum, sizeof(Elf64_Shdr));
    if (!new_sections) {
        fprintf(stderr, "strip: Out of memory\n");
        free(section_map);
        goto cleanup;
    }

    /* Copy sections and update offsets */
    int new_idx = 0;
    for (i = 0; i < ehdr.e_shnum; i++) {
        if (section_map[i] < 0) continue;

        new_sections[new_idx] = sections[i];

        /* Update section data offset (skip NULL section) */
        if (sections[i].sh_type != SHT_NULL && sections[i].sh_size > 0) {
            new_sections[new_idx].sh_offset = current_offset;
            current_offset += sections[i].sh_size;
        }

        /* Update sh_link and sh_info if they reference sections */
        if (sections[i].sh_link < ehdr.e_shnum && section_map[sections[i].sh_link] >= 0) {
            new_sections[new_idx].sh_link = section_map[sections[i].sh_link];
        } else {
            new_sections[new_idx].sh_link = 0;
        }

        if ((sections[i].sh_flags & SHF_INFO_LINK) &&
            sections[i].sh_info < ehdr.e_shnum &&
            section_map[sections[i].sh_info] >= 0) {
            new_sections[new_idx].sh_info = section_map[sections[i].sh_info];
        }

        new_idx++;
    }

    /* Write section data */
    for (i = 0; i < ehdr.e_shnum; i++) {
        if (section_map[i] < 0) continue;
        if (sections[i].sh_type == SHT_NULL) continue;
        if (sections[i].sh_size == 0) continue;

        fwrite(file_data + sections[i].sh_offset, 1, sections[i].sh_size, out);
    }

    /* Update section header offset */
    new_ehdr.e_shoff = current_offset;

    /* Write section headers */
    fwrite(new_sections, sizeof(Elf64_Shdr), new_shnum, out);

    /* Update ELF header with correct section offset */
    fseek(out, 0, SEEK_SET);
    fwrite(&new_ehdr, sizeof(Elf64_Ehdr), 1, out);

    result = 0;

    free(new_sections);
    free(section_map);

cleanup:
    if (in) fclose(in);
    if (out) fclose(out);
    if (sections) free(sections);
    if (shstrtab) free(shstrtab);
    if (file_data) free(file_data);

    return result;
}


/* ========== Custom Static Linker - Module 1: ELF Parser ========== */

/**
 * Data structures for object file representation
 */

/* Architecture enumeration - use GotPltArch from cosmo_got_plt_reloc.h */
typedef GotPltArch LinkerArch;

/* Section representation */
typedef struct LinkerSection {
    char *name;                 /* Section name */
    uint32_t type;              /* Section type (SHT_*) */
    uint64_t flags;             /* Section flags (SHF_*) */
    uint64_t addr;              /* Virtual address */
    uint64_t size;              /* Section size */
    uint64_t alignment;         /* Section alignment */
    unsigned char *data;        /* Section data (NULL for SHT_NOBITS) */
    uint32_t shndx;             /* Original section index */
} LinkerSection;

/* Symbol representation */
typedef struct LinkerSymbol {
    char *name;                 /* Symbol name */
    uint64_t value;             /* Symbol value */
    uint64_t size;              /* Symbol size */
    uint16_t shndx;             /* Section index */
    unsigned char bind;         /* Symbol binding (STB_*) */
    unsigned char type;         /* Symbol type (STT_*) */
    unsigned char visibility;   /* Symbol visibility (STV_*) */
} LinkerSymbol;

/* Relocation entry */
typedef struct LinkerRelocation {
    uint64_t offset;            /* Relocation offset */
    uint32_t type;              /* Relocation type (R_X86_64_*) */
    uint32_t symbol;            /* Symbol index */
    int64_t addend;             /* Relocation addend */
} LinkerRelocation;

/* Relocation section */
typedef struct LinkerRelaSection {
    uint32_t target_shndx;      /* Target section index */
    uint32_t count;             /* Number of relocations */
    LinkerRelocation *relas;    /* Relocation entries */
} LinkerRelaSection;

/* Object file representation */
typedef struct ObjectFile {
    char *filename;             /* Source file name */
    LinkerArch arch;            /* Architecture type */

    /* Sections */
    uint32_t section_count;
    LinkerSection *sections;

    /* Symbols */
    uint32_t symbol_count;
    LinkerSymbol *symbols;

    /* Relocations */
    uint32_t rela_count;
    LinkerRelaSection *rela_sections;

    /* String tables */
    char *strtab;               /* Symbol string table */
    uint64_t strtab_size;
    char *shstrtab;             /* Section string table */
    uint64_t shstrtab_size;

    /* Garbage collection support */
    int used;                   /* 1 if object is used (reachable from entry point), 0 otherwise */
} ObjectFile;


/**
 * Helper: Read string from string table
 */
static const char* linker_get_string(const char *strtab, uint64_t strtab_size, uint32_t offset) {
    if (!strtab || offset >= strtab_size) {
        return "";
    }
    return strtab + offset;
}

/**
 * Helper: Free object file structure
 */
static void free_object_file(ObjectFile *obj) {
    if (!obj) return;

    if (obj->filename) free(obj->filename);

    /* Free sections */
    if (obj->sections) {
        for (uint32_t i = 0; i < obj->section_count; i++) {
            if (obj->sections[i].name) free(obj->sections[i].name);
            if (obj->sections[i].data) free(obj->sections[i].data);
        }
        free(obj->sections);
    }

    /* Free symbols */
    if (obj->symbols) {
        for (uint32_t i = 0; i < obj->symbol_count; i++) {
            if (obj->symbols[i].name) free(obj->symbols[i].name);
        }
        free(obj->symbols);
    }

    /* Free relocations */
    if (obj->rela_sections) {
        for (uint32_t i = 0; i < obj->rela_count; i++) {
            if (obj->rela_sections[i].relas) free(obj->rela_sections[i].relas);
        }
        free(obj->rela_sections);
    }

    if (obj->strtab) free(obj->strtab);
    if (obj->shstrtab) free(obj->shstrtab);

    free(obj);
}

/**
 * Parse ELF64 from memory buffer (R30: memory-based parsing to skip temp file I/O)
 * @param data Pointer to ELF data in memory
 * @param size Size of ELF data
 * @param name Object name for error messages
 * @return ObjectFile structure or NULL on error
 */
static ObjectFile* parse_elf64_from_memory(const unsigned char *data, size_t size, const char *name) {
    ObjectFile *obj = NULL;
    Elf64_Ehdr *ehdr;
    Elf64_Shdr *shdrs = NULL;
    int result = -1;
    const char *path = name;  /* Use name for error messages */

    /* Verify minimum size for ELF header */
    if (size < sizeof(Elf64_Ehdr)) {
        fprintf(stderr, "linker: '%s': file too small for ELF header\n", path);
        return NULL;
    }

    /* Read ELF header from memory */
    ehdr = (Elf64_Ehdr *)data;

    /* Verify ELF magic */
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "linker: '%s': not an ELF file (magic: %02x %02x %02x %02x)\n",
                path, ehdr->e_ident[0], ehdr->e_ident[1], ehdr->e_ident[2], ehdr->e_ident[3]);
        return NULL;
    }

    /* Verify 64-bit ELF */
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "linker: '%s': not a 64-bit ELF file\n", path);
        return NULL;
    }

    /* Check architecture support */
    if (ehdr->e_machine == EM_X86_64) {
        /* x86-64 architecture */
    } else if (ehdr->e_machine == EM_AARCH64) {
        /* ARM64/AArch64 architecture */
    } else {
        fprintf(stderr, "linker: '%s': unsupported architecture (got %d, expected x86-64 or ARM64)\n",
                path, ehdr->e_machine);
        return NULL;
    }

    /* Verify object file type */
    if (ehdr->e_type != ET_REL) {
        fprintf(stderr, "linker: '%s': not a relocatable object file (type: %d)\n",
                path, ehdr->e_type);
        return NULL;
    }

    /* Allocate object structure */
    obj = (ObjectFile *)calloc(1, sizeof(ObjectFile));
    if (!obj) {
        fprintf(stderr, "linker: out of memory\n");
        return NULL;
    }

    obj->filename = strdup(path);

    /* Set architecture */
    if (ehdr->e_machine == EM_X86_64) {
        obj->arch = ARCH_X86_64;
    } else if (ehdr->e_machine == EM_AARCH64) {
        obj->arch = ARCH_ARM64;
    }

    /* Verify section header table is within bounds */
    if (ehdr->e_shoff + ehdr->e_shnum * sizeof(Elf64_Shdr) > size) {
        fprintf(stderr, "linker: '%s': section headers beyond file size\n", path);
        goto cleanup;
    }

    /* Get section headers from memory */
    shdrs = (Elf64_Shdr *)(data + ehdr->e_shoff);

    /* Read section header string table */
    if (ehdr->e_shstrndx >= ehdr->e_shnum) {
        fprintf(stderr, "linker: '%s': invalid section header string table index\n", path);
        goto cleanup;
    }

    Elf64_Shdr *shstrtab_hdr = &shdrs[ehdr->e_shstrndx];
    if (shstrtab_hdr->sh_offset + shstrtab_hdr->sh_size > size) {
        fprintf(stderr, "linker: '%s': shstrtab beyond file size\n", path);
        goto cleanup;
    }

    obj->shstrtab_size = shstrtab_hdr->sh_size;
    obj->shstrtab = (char *)malloc(obj->shstrtab_size);
    if (!obj->shstrtab) {
        fprintf(stderr, "linker: out of memory\n");
        goto cleanup;
    }
    memcpy(obj->shstrtab, data + shstrtab_hdr->sh_offset, obj->shstrtab_size);

    /* Find symbol table and string table */
    Elf64_Shdr *symtab_hdr = NULL;
    Elf64_Shdr *strtab_hdr = NULL;

    for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_SYMTAB) {
            symtab_hdr = &shdrs[i];
            if (shdrs[i].sh_link < ehdr->e_shnum) {
                strtab_hdr = &shdrs[shdrs[i].sh_link];
            }
            break;
        }
    }

    /* Read string table */
    if (strtab_hdr) {
        if (strtab_hdr->sh_offset + strtab_hdr->sh_size > size) {
            fprintf(stderr, "linker: '%s': strtab beyond file size\n", path);
            goto cleanup;
        }

        obj->strtab_size = strtab_hdr->sh_size;
        obj->strtab = (char *)malloc(obj->strtab_size);
        if (!obj->strtab) {
            fprintf(stderr, "linker: out of memory\n");
            goto cleanup;
        }
        memcpy(obj->strtab, data + strtab_hdr->sh_offset, obj->strtab_size);
    }

    /* Parse sections */
    obj->section_count = ehdr->e_shnum;
    obj->sections = (LinkerSection *)calloc(obj->section_count, sizeof(LinkerSection));
    if (!obj->sections) {
        fprintf(stderr, "linker: out of memory\n");
        goto cleanup;
    }

    for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *shdr = &shdrs[i];
        LinkerSection *sec = &obj->sections[i];

        /* Get section name */
        const char *sec_name = linker_get_string(obj->shstrtab, obj->shstrtab_size, shdr->sh_name);
        sec->name = strdup(sec_name);
        sec->type = shdr->sh_type;
        sec->flags = shdr->sh_flags;
        sec->addr = shdr->sh_addr;
        sec->size = shdr->sh_size;
        sec->alignment = shdr->sh_addralign;
        sec->shndx = i;

        /* Read section data (skip NOBITS sections like .bss) */
        if (shdr->sh_type != SHT_NOBITS && shdr->sh_size > 0) {
            if (shdr->sh_offset + shdr->sh_size > size) {
                fprintf(stderr, "linker: '%s': section '%s' beyond file size\n", path, sec_name);
                goto cleanup;
            }

            sec->data = (unsigned char *)malloc(shdr->sh_size);
            if (!sec->data) {
                fprintf(stderr, "linker: out of memory\n");
                goto cleanup;
            }
            memcpy(sec->data, data + shdr->sh_offset, shdr->sh_size);
        }
    }

    /* Parse symbols */
    if (symtab_hdr) {
        if (symtab_hdr->sh_offset + symtab_hdr->sh_size > size) {
            fprintf(stderr, "linker: '%s': symtab beyond file size\n", path);
            goto cleanup;
        }

        size_t nsyms = symtab_hdr->sh_size / sizeof(Elf64_Sym);
        Elf64_Sym *elf_syms = (Elf64_Sym *)(data + symtab_hdr->sh_offset);

        obj->symbol_count = nsyms;
        obj->symbols = (LinkerSymbol *)calloc(nsyms, sizeof(LinkerSymbol));
        if (!obj->symbols) {
            fprintf(stderr, "linker: out of memory\n");
            goto cleanup;
        }

        for (size_t i = 0; i < nsyms; i++) {
            Elf64_Sym *esym = &elf_syms[i];
            LinkerSymbol *sym = &obj->symbols[i];

            const char *sym_name = linker_get_string(obj->strtab, obj->strtab_size, esym->st_name);
            sym->name = strdup(sym_name);
            sym->value = esym->st_value;
            sym->size = esym->st_size;
            sym->shndx = esym->st_shndx;
            sym->bind = ELF64_ST_BIND(esym->st_info);
            sym->type = ELF64_ST_TYPE(esym->st_info);
            sym->visibility = ELF64_ST_VISIBILITY(esym->st_other);
        }
    }

    /* Parse relocation sections */
    uint32_t rela_count = 0;
    for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_RELA) {
            rela_count++;
        }
    }

    if (rela_count > 0) {
        obj->rela_count = rela_count;
        obj->rela_sections = (LinkerRelaSection *)calloc(rela_count, sizeof(LinkerRelaSection));
        if (!obj->rela_sections) {
            fprintf(stderr, "linker: out of memory\n");
            goto cleanup;
        }

        uint32_t rela_idx = 0;
        for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
            if (shdrs[i].sh_type != SHT_RELA) continue;

            Elf64_Shdr *rela_hdr = &shdrs[i];

            if (rela_hdr->sh_offset + rela_hdr->sh_size > size) {
                fprintf(stderr, "linker: '%s': rela section beyond file size\n", path);
                goto cleanup;
            }

            LinkerRelaSection *rela_sec = &obj->rela_sections[rela_idx++];

            rela_sec->target_shndx = rela_hdr->sh_info;
            rela_sec->count = rela_hdr->sh_size / sizeof(Elf64_Rela);

            /* Read relocations from memory */
            Elf64_Rela *elf_relas = (Elf64_Rela *)(data + rela_hdr->sh_offset);

            rela_sec->relas = (LinkerRelocation *)calloc(rela_sec->count, sizeof(LinkerRelocation));
            if (!rela_sec->relas) {
                fprintf(stderr, "linker: out of memory\n");
                goto cleanup;
            }

            for (uint32_t j = 0; j < rela_sec->count; j++) {
                Elf64_Rela *erela = &elf_relas[j];
                LinkerRelocation *rela = &rela_sec->relas[j];

                rela->offset = erela->r_offset;
                rela->type = ELF64_R_TYPE(erela->r_info);
                rela->symbol = ELF64_R_SYM(erela->r_info);
                rela->addend = erela->r_addend;
            }
        }
    }

    result = 0;

cleanup:
    if (result != 0 && obj) {
        free_object_file(obj);
        obj = NULL;
    }

    return obj;
}

/**
 * Parse ELF64 object file
 * @param path Path to .o file
 * @return ObjectFile structure or NULL on error
 * Note: Made non-static for parallel parsing support
 */
ObjectFile* parse_elf64_object(const char *path) {
    FILE *f = NULL;
    ObjectFile *obj = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Shdr *shdrs = NULL;
    int result = -1;

    /* Open file */
    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "linker: cannot open '%s': %s\n", path, strerror(errno));
        return NULL;
    }

    /* Read ELF header */
    if (fread(&ehdr, sizeof(ehdr), 1, f) != 1) {
        fprintf(stderr, "linker: '%s': failed to read ELF header\n", path);
        goto cleanup;
    }

    /* Verify ELF magic */
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "linker: '%s': not an ELF file (magic: %02x %02x %02x %02x)\n",
                path, ehdr.e_ident[0], ehdr.e_ident[1], ehdr.e_ident[2], ehdr.e_ident[3]);
        goto cleanup;
    }

    /* Verify 64-bit ELF */
    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "linker: '%s': not a 64-bit ELF file\n", path);
        goto cleanup;
    }

    /* Check architecture support */
    if (ehdr.e_machine == EM_X86_64) {
        /* x86-64 architecture */
    } else if (ehdr.e_machine == EM_AARCH64) {
        /* ARM64/AArch64 architecture */
    } else {
        fprintf(stderr, "linker: '%s': unsupported architecture (got %d, expected x86-64 or ARM64)\n",
                path, ehdr.e_machine);
        goto cleanup;
    }

    /* Verify object file type */
    if (ehdr.e_type != ET_REL) {
        fprintf(stderr, "linker: '%s': not a relocatable object file (type: %d)\n",
                path, ehdr.e_type);
        goto cleanup;
    }

    /* Allocate object structure */
    obj = (ObjectFile *)calloc(1, sizeof(ObjectFile));
    if (!obj) {
        fprintf(stderr, "linker: out of memory\n");
        goto cleanup;
    }

    obj->filename = strdup(path);

    /* Set architecture */
    if (ehdr.e_machine == EM_X86_64) {
        obj->arch = ARCH_X86_64;
    } else if (ehdr.e_machine == EM_AARCH64) {
        obj->arch = ARCH_ARM64;
    }

    /* Read section headers */
    shdrs = (Elf64_Shdr *)malloc(ehdr.e_shnum * sizeof(Elf64_Shdr));
    if (!shdrs) {
        fprintf(stderr, "linker: out of memory\n");
        goto cleanup;
    }

    if (fseek(f, ehdr.e_shoff, SEEK_SET) != 0) {
        fprintf(stderr, "linker: '%s': failed to seek to section headers\n", path);
        goto cleanup;
    }

    if (fread(shdrs, sizeof(Elf64_Shdr), ehdr.e_shnum, f) != ehdr.e_shnum) {
        fprintf(stderr, "linker: '%s': failed to read section headers\n", path);
        goto cleanup;
    }

    /* Read section header string table */
    if (ehdr.e_shstrndx >= ehdr.e_shnum) {
        fprintf(stderr, "linker: '%s': invalid section header string table index\n", path);
        goto cleanup;
    }

    Elf64_Shdr *shstrtab_hdr = &shdrs[ehdr.e_shstrndx];
    obj->shstrtab_size = shstrtab_hdr->sh_size;
    obj->shstrtab = (char *)malloc(obj->shstrtab_size);
    if (!obj->shstrtab) {
        fprintf(stderr, "linker: out of memory\n");
        goto cleanup;
    }

    if (fseek(f, shstrtab_hdr->sh_offset, SEEK_SET) != 0 ||
        fread(obj->shstrtab, 1, obj->shstrtab_size, f) != obj->shstrtab_size) {
        fprintf(stderr, "linker: '%s': failed to read section header string table\n", path);
        goto cleanup;
    }

    /* Find symbol table and string table */
    Elf64_Shdr *symtab_hdr = NULL;
    Elf64_Shdr *strtab_hdr = NULL;

    for (uint16_t i = 0; i < ehdr.e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_SYMTAB) {
            symtab_hdr = &shdrs[i];
            if (shdrs[i].sh_link < ehdr.e_shnum) {
                strtab_hdr = &shdrs[shdrs[i].sh_link];
            }
            break;
        }
    }

    /* Read string table */
    if (strtab_hdr) {
        obj->strtab_size = strtab_hdr->sh_size;
        obj->strtab = (char *)malloc(obj->strtab_size);
        if (!obj->strtab) {
            fprintf(stderr, "linker: out of memory\n");
            goto cleanup;
        }

        if (fseek(f, strtab_hdr->sh_offset, SEEK_SET) != 0 ||
            fread(obj->strtab, 1, obj->strtab_size, f) != obj->strtab_size) {
            fprintf(stderr, "linker: '%s': failed to read string table\n", path);
            goto cleanup;
        }
    }

    /* Parse sections */
    obj->section_count = ehdr.e_shnum;
    obj->sections = (LinkerSection *)calloc(obj->section_count, sizeof(LinkerSection));
    if (!obj->sections) {
        fprintf(stderr, "linker: out of memory\n");
        goto cleanup;
    }

    for (uint16_t i = 0; i < ehdr.e_shnum; i++) {
        Elf64_Shdr *shdr = &shdrs[i];
        LinkerSection *sec = &obj->sections[i];

        /* Get section name */
        const char *name = linker_get_string(obj->shstrtab, obj->shstrtab_size, shdr->sh_name);
        sec->name = strdup(name);
        sec->type = shdr->sh_type;
        sec->flags = shdr->sh_flags;
        sec->addr = shdr->sh_addr;
        sec->size = shdr->sh_size;
        sec->alignment = shdr->sh_addralign;
        sec->shndx = i;

        /* Read section data (skip NOBITS sections like .bss) */
        if (shdr->sh_type != SHT_NOBITS && shdr->sh_size > 0) {
            sec->data = (unsigned char *)malloc(shdr->sh_size);
            if (!sec->data) {
                fprintf(stderr, "linker: out of memory\n");
                goto cleanup;
            }

            if (fseek(f, shdr->sh_offset, SEEK_SET) != 0 ||
                fread(sec->data, 1, shdr->sh_size, f) != shdr->sh_size) {
                fprintf(stderr, "linker: '%s': failed to read section '%s'\n", path, name);
                goto cleanup;
            }
        }
    }

    /* Parse symbols */
    if (symtab_hdr) {
        size_t nsyms = symtab_hdr->sh_size / sizeof(Elf64_Sym);
        Elf64_Sym *elf_syms = (Elf64_Sym *)malloc(symtab_hdr->sh_size);
        if (!elf_syms) {
            fprintf(stderr, "linker: out of memory\n");
            goto cleanup;
        }

        if (fseek(f, symtab_hdr->sh_offset, SEEK_SET) != 0 ||
            fread(elf_syms, sizeof(Elf64_Sym), nsyms, f) != nsyms) {
            fprintf(stderr, "linker: '%s': failed to read symbol table\n", path);
            free(elf_syms);
            goto cleanup;
        }

        obj->symbol_count = nsyms;
        obj->symbols = (LinkerSymbol *)calloc(nsyms, sizeof(LinkerSymbol));
        if (!obj->symbols) {
            fprintf(stderr, "linker: out of memory\n");
            free(elf_syms);
            goto cleanup;
        }

        for (size_t i = 0; i < nsyms; i++) {
            Elf64_Sym *esym = &elf_syms[i];
            LinkerSymbol *sym = &obj->symbols[i];

            const char *name = linker_get_string(obj->strtab, obj->strtab_size, esym->st_name);
            sym->name = strdup(name);
            sym->value = esym->st_value;
            sym->size = esym->st_size;
            sym->shndx = esym->st_shndx;
            sym->bind = ELF64_ST_BIND(esym->st_info);
            sym->type = ELF64_ST_TYPE(esym->st_info);
            sym->visibility = ELF64_ST_VISIBILITY(esym->st_other);
        }

        free(elf_syms);
    }

    /* Parse relocation sections */
    uint32_t rela_count = 0;
    for (uint16_t i = 0; i < ehdr.e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_RELA) {
            rela_count++;
        }
    }

    if (rela_count > 0) {
        obj->rela_count = rela_count;
        obj->rela_sections = (LinkerRelaSection *)calloc(rela_count, sizeof(LinkerRelaSection));
        if (!obj->rela_sections) {
            fprintf(stderr, "linker: out of memory\n");
            goto cleanup;
        }

        uint32_t rela_idx = 0;
        for (uint16_t i = 0; i < ehdr.e_shnum; i++) {
            if (shdrs[i].sh_type != SHT_RELA) continue;

            Elf64_Shdr *rela_hdr = &shdrs[i];
            LinkerRelaSection *rela_sec = &obj->rela_sections[rela_idx++];

            rela_sec->target_shndx = rela_hdr->sh_info;
            rela_sec->count = rela_hdr->sh_size / sizeof(Elf64_Rela);

            /* Read relocations */
            Elf64_Rela *elf_relas = (Elf64_Rela *)malloc(rela_hdr->sh_size);
            if (!elf_relas) {
                fprintf(stderr, "linker: out of memory\n");
                goto cleanup;
            }

            if (fseek(f, rela_hdr->sh_offset, SEEK_SET) != 0 ||
                fread(elf_relas, sizeof(Elf64_Rela), rela_sec->count, f) != rela_sec->count) {
                fprintf(stderr, "linker: '%s': failed to read relocation section\n", path);
                free(elf_relas);
                goto cleanup;
            }

            rela_sec->relas = (LinkerRelocation *)calloc(rela_sec->count, sizeof(LinkerRelocation));
            if (!rela_sec->relas) {
                fprintf(stderr, "linker: out of memory\n");
                free(elf_relas);
                goto cleanup;
            }

            for (uint32_t j = 0; j < rela_sec->count; j++) {
                Elf64_Rela *erela = &elf_relas[j];
                LinkerRelocation *rela = &rela_sec->relas[j];

                rela->offset = erela->r_offset;
                rela->type = ELF64_R_TYPE(erela->r_info);
                rela->symbol = ELF64_R_SYM(erela->r_info);
                rela->addend = erela->r_addend;
            }

            free(elf_relas);
        }
    }

    result = 0;

cleanup:
    if (shdrs) free(shdrs);
    if (f) fclose(f);

    if (result != 0 && obj) {
        free_object_file(obj);
        obj = NULL;
    }

    return obj;
}


/**
 * Extract object file from AR archive
 * @param ar_path Path to .a archive
 * @param member_name Member name to extract
 * @return ObjectFile structure or NULL on error
 */
static ObjectFile* extract_ar_member(const char *ar_path, const char *member_name) {
    FILE *ar_fp = NULL;
    FILE *temp_fp = NULL;
    ObjectFile *obj = NULL;
    char temp_path[256];

    /* Open archive */
    ar_fp = fopen(ar_path, "rb");
    if (!ar_fp) {
        fprintf(stderr, "linker: cannot open archive '%s': %s\n", ar_path, strerror(errno));
        return NULL;
    }

    /* Verify AR magic */
    char magic[AR_MAGIC_LEN];
    if (fread(magic, AR_MAGIC_LEN, 1, ar_fp) != 1 ||
        memcmp(magic, AR_MAGIC, AR_MAGIC_LEN) != 0) {
        fprintf(stderr, "linker: '%s': not an archive\n", ar_path);
        fclose(ar_fp);
        return NULL;
    }

    /* Search for member */
    int found = 0;
    while (!feof(ar_fp)) {
        char name[17];
        long size = parse_ar_header(ar_fp, name, NULL);

        if (size < 0) {
            if (feof(ar_fp)) break;
            fclose(ar_fp);
            return NULL;
        }

        /* Check if this is the requested member */
        if (strcmp(name, member_name) == 0) {
            found = 1;

            /* Create temporary file */
            snprintf(temp_path, sizeof(temp_path), "/tmp/linker_ar_%d_%s", getpid(), member_name);
            temp_fp = fopen(temp_path, "wb");
            if (!temp_fp) {
                fprintf(stderr, "linker: cannot create temp file: %s\n", strerror(errno));
                fclose(ar_fp);
                return NULL;
            }

            /* Extract member to temp file */
            char buffer[8192];
            long remaining = size;
            while (remaining > 0) {
                size_t to_read = (remaining > (long)sizeof(buffer)) ? sizeof(buffer) : (size_t)remaining;
                size_t n = fread(buffer, 1, to_read, ar_fp);
                if (n == 0) break;
                if (fwrite(buffer, 1, n, temp_fp) != n) {
                    fprintf(stderr, "linker: write error\n");
                    fclose(temp_fp);
                    fclose(ar_fp);
                    unlink(temp_path);
                    return NULL;
                }
                remaining -= n;
            }
            fclose(temp_fp);
            break;
        }

        /* Skip this member */
        long skip = size;
        if (skip & 1) skip++; /* AR alignment */
        if (fseek(ar_fp, skip, SEEK_CUR) != 0) {
            break;
        }
    }

    fclose(ar_fp);

    if (!found) {
        fprintf(stderr, "linker: member '%s' not found in archive '%s'\n", member_name, ar_path);
        return NULL;
    }

    /* Parse extracted object file */
    obj = parse_elf64_object(temp_path);

    /* Clean up temp file */
    unlink(temp_path);

    return obj;
}


/* ========== Custom Static Linker - Module 2: Section Merging and Memory Layout ========== */

/**
 * Merged section structure representing combined sections from multiple object files
 */
typedef struct {
    char *name;              /* Section name (.text, .data, .rodata, .bss) */
    uint8_t *data;          /* Section data (NULL for .bss) */
    size_t size;            /* Total section size */
    uint64_t vma;           /* Virtual memory address */
    uint32_t flags;         /* Section flags (SHF_WRITE, SHF_ALLOC, SHF_EXECINSTR) */
    uint32_t alignment;     /* Alignment requirement (typically 16) */
} MergedSection;

/* Performance optimization: Section lookup cache */
#define SECTION_CACHE_SIZE 16

typedef struct {
    const char *name;
    MergedSection *section;
} SectionCacheEntry;

static SectionCacheEntry g_section_cache[SECTION_CACHE_SIZE];
static int g_section_cache_size = 0;

/**
 * Find merged section with cache optimization
 */
static MergedSection* find_merged_section_cached(const char *name,
                                                  MergedSection *sections,
                                                  int count) {
    /* Check cache first */
    for (int i = 0; i < g_section_cache_size; i++) {
        if (strcmp(g_section_cache[i].name, name) == 0) {
            return g_section_cache[i].section;
        }
    }

    /* Not in cache, do linear search */
    for (int i = 0; i < count; i++) {
        if (strcmp(sections[i].name, name) == 0) {
            /* Add to cache (LRU replacement if full) */
            if (g_section_cache_size < SECTION_CACHE_SIZE) {
                g_section_cache[g_section_cache_size].name = sections[i].name;
                g_section_cache[g_section_cache_size].section = &sections[i];
                g_section_cache_size++;
            }
            return &sections[i];
        }
    }
    return NULL;
}

/**
 * Clear section cache (call when section array is reallocated)
 */
static void clear_section_cache(void) {
    g_section_cache_size = 0;
}

/**
 * Object file structure for Module 2 (alternative representation)
 * Note: This is Module 2's original design, kept for future integration
 */
typedef struct {
    const char *filename;
    Elf64_Ehdr *ehdr;
    Elf64_Shdr *sections;
    char *shstrtab;
    uint8_t *file_data;
    size_t file_size;
} ObjectFile_Module2;

/**
 * Symbol structure for symbol table
 */
typedef struct {
    char *name;
    uint64_t value;
    uint64_t size;
    uint16_t shndx;
    uint8_t bind;
    uint8_t type;
} Symbol;

/* Standard ELF memory layout constants */
#define LINKER_BASE_ADDR    0x400000UL      /* Standard x86-64 base address */
#define LINKER_PAGE_SIZE    4096            /* Page size for alignment */
#define LINKER_SECTION_ALIGN 16             /* Section alignment within segments */

/**
 * Align value to specified alignment
 */
static uint64_t align_up(uint64_t value, uint64_t alignment) {
    if (alignment == 0) return value;
    return (value + alignment - 1) & ~(alignment - 1);
}

/**
 * Extract ELF section flags from section header
 */
static uint32_t extract_section_flags(Elf64_Shdr *shdr) {
    uint32_t flags = 0;
    if (shdr->sh_flags & SHF_WRITE)     flags |= SHF_WRITE;
    if (shdr->sh_flags & SHF_ALLOC)     flags |= SHF_ALLOC;
    if (shdr->sh_flags & SHF_EXECINSTR) flags |= SHF_EXECINSTR;
    return flags;
}

/**
 * Check if section should be merged
 */
static int should_merge_section(const char *name, Elf64_Shdr *shdr) {
    if (!name || !shdr) return 0;

    /* Only merge sections with SHF_ALLOC flag */
    if (!(shdr->sh_flags & SHF_ALLOC)) return 0;

    /* Merge standard sections */
    if (strcmp(name, ".text") == 0) return 1;
    if (strcmp(name, ".data") == 0) return 1;
    if (strcmp(name, ".rodata") == 0) return 1;
    if (strcmp(name, ".bss") == 0) return 1;

    /* Merge .text.* and .data.* sections (function/data sections) */
    if (strncmp(name, ".text.", 6) == 0) return 1;
    if (strncmp(name, ".data.", 6) == 0) return 1;
    if (strncmp(name, ".rodata.", 8) == 0) return 1;

    return 0;
}

/**
 * Get merged section name for a given section
 */
static const char* get_merged_section_name(const char *name) {
    if (!name) return NULL;

    if (strcmp(name, ".text") == 0 || strncmp(name, ".text.", 6) == 0) {
        return ".text";
    }
    if (strcmp(name, ".data") == 0 || strncmp(name, ".data.", 6) == 0) {
        return ".data";
    }
    if (strcmp(name, ".rodata") == 0 || strncmp(name, ".rodata.", 8) == 0) {
        return ".rodata";
    }
    if (strcmp(name, ".bss") == 0) {
        return ".bss";
    }

    return name;
}

/**
 * Simple hash table for fast section name lookup during merging
 * Reduces O(M) linear search to O(1) hash lookup
 */
typedef struct {
    const char *name;
    int index;  /* Index into merged sections array */
} SectionNameEntry;

#define SECTION_MERGE_HASH_SIZE 64
typedef struct {
    SectionNameEntry *entries[SECTION_MERGE_HASH_SIZE];
    int counts[SECTION_MERGE_HASH_SIZE];
    int capacities[SECTION_MERGE_HASH_SIZE];
} SectionMergeHash;

static SectionMergeHash* create_section_merge_hash(void) {
    SectionMergeHash *hash = calloc(1, sizeof(SectionMergeHash));
    if (!hash) return NULL;

    for (int i = 0; i < SECTION_MERGE_HASH_SIZE; i++) {
        hash->capacities[i] = 4;  /* Initial capacity per bucket */
        hash->entries[i] = malloc(4 * sizeof(SectionNameEntry));
        if (!hash->entries[i]) {
            for (int j = 0; j < i; j++) free(hash->entries[j]);
            free(hash);
            return NULL;
        }
    }
    return hash;
}

static void free_section_merge_hash(SectionMergeHash *hash) {
    if (!hash) return;
    for (int i = 0; i < SECTION_MERGE_HASH_SIZE; i++) {
        free(hash->entries[i]);
    }
    free(hash);
}

static unsigned int hash_section_name(const char *name) {
    unsigned int hash = 5381;
    while (*name) {
        hash = ((hash << 5) + hash) + (unsigned char)*name++;
    }
    return hash % SECTION_MERGE_HASH_SIZE;
}

static int section_merge_hash_lookup(SectionMergeHash *hash, const char *name) {
    unsigned int h = hash_section_name(name);
    for (int i = 0; i < hash->counts[h]; i++) {
        if (strcmp(hash->entries[h][i].name, name) == 0) {
            return hash->entries[h][i].index;
        }
    }
    return -1;  /* Not found */
}

static void section_merge_hash_insert(SectionMergeHash *hash, const char *name, int index) {
    unsigned int h = hash_section_name(name);

    /* Expand bucket if needed */
    if (hash->counts[h] >= hash->capacities[h]) {
        int new_cap = hash->capacities[h] * 2;
        SectionNameEntry *new_entries = realloc(hash->entries[h],
                                                new_cap * sizeof(SectionNameEntry));
        if (!new_entries) return;  /* Fail silently - will fallback to linear search */
        hash->entries[h] = new_entries;
        hash->capacities[h] = new_cap;
    }

    hash->entries[h][hash->counts[h]].name = name;
    hash->entries[h][hash->counts[h]].index = index;
    hash->counts[h]++;
}

/**
 * Find or create merged section (optimized with hash table)
 */
static MergedSection* find_or_create_section(MergedSection **sections, int *count,
                                             const char *name, uint32_t flags,
                                             SectionMergeHash *hash) {
    /* Fast hash lookup first */
    if (hash) {
        int idx = section_merge_hash_lookup(hash, name);
        if (idx >= 0) {
            return &(*sections)[idx];
        }
    }

    /* Create new section */
    *sections = (MergedSection*)realloc(*sections, (*count + 1) * sizeof(MergedSection));
    if (!*sections) return NULL;

    MergedSection *sec = &(*sections)[*count];
    memset(sec, 0, sizeof(MergedSection));

    sec->name = strdup(name);
    sec->flags = flags;
    sec->alignment = LINKER_SECTION_ALIGN;
    sec->data = NULL;
    sec->size = 0;
    sec->vma = 0;

    /* Add to hash table */
    if (hash) {
        section_merge_hash_insert(hash, sec->name, *count);
    }

    (*count)++;
    return sec;
}

/**
 * Merge sections from multiple object files
 * @param objs Array of object files
 * @param obj_count Number of object files
 * @param section_count Output: number of merged sections
 * @return Array of merged sections (NULL on error)
 */
static MergedSection* merge_sections(ObjectFile **objs, int obj_count, int *section_count) {
    MergedSection *merged = NULL;
    int merged_count = 0;

    if (!objs || obj_count <= 0 || !section_count) {
        fprintf(stderr, "linker: Invalid arguments to merge_sections\n");
        return NULL;
    }

    timer_record("Phase 3.0: Start merge");

    /* Create hash table for O(1) section lookup */
    SectionMergeHash *section_hash = create_section_merge_hash();
    if (!section_hash) {
        fprintf(stderr, "linker: Failed to create section merge hash table\n");
        return NULL;
    }

    /* Process each object file */
    for (int i = 0; i < obj_count; i++) {
        ObjectFile *obj = objs[i];
        if (!obj || !obj->sections) {
            continue;
        }

        /* Process each section in object file */
        for (uint32_t j = 0; j < obj->section_count; j++) {
            LinkerSection *sec = &obj->sections[j];

            /* Get section name */
            const char *sec_name = sec->name;
            if (!sec_name) continue;

            /* Skip debug sections */
            if (sec_name && strncmp(sec_name, ".debug", 6) == 0) {
                continue;
            }

            /* Check if we should merge this section (simple check for now) */
            if (sec->type == SHT_NULL || sec->type == SHT_SYMTAB ||
                sec->type == SHT_STRTAB || sec->type == SHT_RELA) {
                continue;
            }

            /* Get merged section name */
            const char *merged_name = get_merged_section_name(sec_name);
            if (!merged_name) continue;

            /* Find or create merged section using hash table */
            uint32_t flags = (uint32_t)sec->flags;
            MergedSection *msec = find_or_create_section(&merged, &merged_count,
                                                          merged_name, flags, section_hash);
            if (!msec) {
                fprintf(stderr, "linker: Failed to create merged section\n");
                free_section_merge_hash(section_hash);
                goto error;
            }

            /* Skip empty sections */
            if (sec->size == 0) continue;

            /* Align current size */
            size_t align = (sec->alignment > 0) ? sec->alignment : LINKER_SECTION_ALIGN;
            if (align > msec->alignment) {
                msec->alignment = align;
            }
            msec->size = align_up(msec->size, align);

            /* For .bss sections, no data to copy (zero-initialized) */
            if (sec->type == SHT_NOBITS) {
                msec->size += sec->size;
                continue;
            }

            /* Allocate or expand data buffer */
            size_t new_size = msec->size + sec->size;
            uint8_t *new_data = (uint8_t*)realloc(msec->data, new_size);
            if (!new_data) {
                fprintf(stderr, "linker: Out of memory while merging sections\n");
                goto error;
            }
            msec->data = new_data;

            /* Copy section data */
            if (sec->data) {
                memcpy(msec->data + msec->size, sec->data, sec->size);
            } else {
                /* Zero-fill if no data */
                memset(msec->data + msec->size, 0, sec->size);
            }

            msec->size = new_size;
        }
    }

    timer_record("Phase 3.1: Collect and merge sections");

    /* Free hash table */
    free_section_merge_hash(section_hash);

    *section_count = merged_count;
    return merged;

error:
    /* Cleanup on error */
    for (int i = 0; i < merged_count; i++) {
        if (merged[i].name) free(merged[i].name);
        if (merged[i].data) free(merged[i].data);
    }
    free(merged);
    return NULL;
}


/* ========== Module 3: Symbol Table Construction and Resolution ========== */

/**
 * Performance Optimizations (Module D):
 *
 * 1. Hash Table for Symbol Lookup:
 *    - Replaces O(n) linear search with O(1) hash table
 *    - Uses djb2 hash algorithm with 1024 buckets
 *    - Measured speedup: ~1700x for 10k symbols
 *
 * 2. Section Lookup Cache:
 *    - Small fixed cache (16 entries) for frequently accessed sections
 *    - Reduces repeated section lookups during relocation processing
 *
 * 3. Memory Allocation Optimization:
 *    - Pre-allocate hash buckets to reduce malloc/free calls
 *    - Dynamic bucket expansion only when needed
 *
 * 4. Benchmark Support:
 *    - Compile with -DLINKER_BENCHMARK to enable timing measurements
 *    - Use BENCHMARK_START/END macros around critical sections
 */

/* Performance optimization: Benchmark macros */
#ifdef LINKER_BENCHMARK
#include <time.h>
#define BENCHMARK_START(name) \
    struct timespec __start_##name; \
    clock_gettime(CLOCK_MONOTONIC, &__start_##name);

#define BENCHMARK_END(name) \
    struct timespec __end_##name; \
    clock_gettime(CLOCK_MONOTONIC, &__end_##name); \
    double __elapsed_##name = (__end_##name.tv_sec - __start_##name.tv_sec) + \
                              (__end_##name.tv_nsec - __start_##name.tv_nsec) / 1e9; \
    fprintf(stderr, "[BENCHMARK] %s: %.3f ms\n", #name, __elapsed_##name * 1000);
#else
#define BENCHMARK_START(name)
#define BENCHMARK_END(name)
#endif

/* Performance optimization: Hash table for symbol lookup */
#define SYMBOL_HASH_SIZE 1024
#define SECTION_HASH_SIZE 1024  /* Hash table size for section name lookups */

/**
 * Simple hash function (djb2 algorithm)
 */
static uint32_t hash_symbol_name(const char *name) {
    uint32_t hash = 5381;
    while (*name) {
        hash = ((hash << 5) + hash) + (*name++);  /* hash * 33 + c */
    }
    return hash;
}

/**
 * Symbol table for the linker (extends LinkerSymbol from Module 1)
 * With hash table optimization for O(1) symbol lookup
 */
typedef struct LinkerSymbolTable {
    LinkerSymbol **symbols;     /* Array of all symbols (for iteration) */
    int count;                  /* Total number of symbols */
    int capacity;               /* Allocated capacity */
    LinkerSymbol **undefined;   /* Undefined symbols needing resolution */
    int undef_count;            /* Number of undefined symbols */
    int undef_capacity;         /* Allocated capacity for undefined */

    /* Hash table optimization */
    LinkerSymbol **buckets[SYMBOL_HASH_SIZE];  /* Hash buckets */
    int bucket_counts[SYMBOL_HASH_SIZE];       /* Symbols per bucket */
    int bucket_capacities[SYMBOL_HASH_SIZE];   /* Allocated capacity per bucket */

    /* String interning pool (avoid duplicate symbol names) */
    char **string_pool;         /* Deduplicated string storage */
    int string_count;           /* Number of unique strings */
    int string_capacity;        /* Allocated capacity */

    /* Memory pool for LinkerSymbol allocations */
    MemoryPool *symbol_pool;    /* Pool for allocating LinkerSymbol structs */
} LinkerSymbolTable;

/**
 * String interning: find or insert string into pool, return deduplicated pointer
 * This avoids duplicate memory allocation for repeated symbol names
 */
static const char* intern_string(LinkerSymbolTable *st, const char *str) {
    /* Search for existing string in pool */
    for (int i = 0; i < st->string_count; i++) {
        if (strcmp(st->string_pool[i], str) == 0) {
            return st->string_pool[i];  /* Return existing string */
        }
    }

    /* Not found - add to pool */
    if (st->string_count >= st->string_capacity) {
        int new_cap = (st->string_capacity == 0) ? 256 : st->string_capacity * 2;
        char **new_pool = (char**)realloc(st->string_pool, new_cap * sizeof(char*));
        if (!new_pool) {
            fprintf(stderr, "linker: out of memory expanding string pool\n");
            return NULL;
        }
        st->string_pool = new_pool;
        st->string_capacity = new_cap;
    }

    /* Duplicate string and add to pool */
    char *new_str = strdup(str);
    if (!new_str) {
        fprintf(stderr, "linker: out of memory duplicating string\n");
        return NULL;
    }

    st->string_pool[st->string_count++] = new_str;
    return new_str;
}

/**
 * Create a new symbol table with hash table optimization
 */
static LinkerSymbolTable* create_symbol_table(void) {
    LinkerSymbolTable *st = (LinkerSymbolTable*)calloc(1, sizeof(LinkerSymbolTable));
    if (!st) {
        fprintf(stderr, "linker: out of memory allocating symbol table\n");
        return NULL;
    }

    st->capacity = 256;
    st->symbols = (LinkerSymbol**)calloc(st->capacity, sizeof(LinkerSymbol*));
    if (!st->symbols) {
        fprintf(stderr, "linker: out of memory allocating symbols array\n");
        free(st);
        return NULL;
    }

    st->undef_capacity = 64;
    st->undefined = (LinkerSymbol**)calloc(st->undef_capacity, sizeof(LinkerSymbol*));
    if (!st->undefined) {
        fprintf(stderr, "linker: out of memory allocating undefined array\n");
        free(st->symbols);
        free(st);
        return NULL;
    }

    /* Initialize string pool */
    st->string_capacity = 256;
    st->string_pool = (char**)malloc(st->string_capacity * sizeof(char*));
    if (!st->string_pool) {
        fprintf(stderr, "linker: out of memory allocating string pool\n");
        free(st->undefined);
        free(st->symbols);
        free(st);
        return NULL;
    }
    st->string_count = 0;

    /* Initialize hash buckets with small initial capacity */
    for (int i = 0; i < SYMBOL_HASH_SIZE; i++) {
        st->bucket_capacities[i] = 4;  /* Start small, will grow if needed */
        st->buckets[i] = (LinkerSymbol**)malloc(4 * sizeof(LinkerSymbol*));
        if (!st->buckets[i]) {
            /* Cleanup on error */
            for (int j = 0; j < i; j++) {
                free(st->buckets[j]);
            }
            free(st->string_pool);
            free(st->undefined);
            free(st->symbols);
            free(st);
            fprintf(stderr, "linker: out of memory allocating hash buckets\n");
            return NULL;
        }
        st->bucket_counts[i] = 0;
    }

    /* Initialize memory pool for LinkerSymbol allocations
     * 2MB arena size: enough for ~50K symbols per arena (40 bytes each) */
    st->symbol_pool = init_memory_pool(2 * 1024 * 1024);
    if (!st->symbol_pool) {
        fprintf(stderr, "linker: out of memory allocating symbol pool\n");
        for (int i = 0; i < SYMBOL_HASH_SIZE; i++) {
            free(st->buckets[i]);
        }
        free(st->string_pool);
        free(st->undefined);
        free(st->symbols);
        free(st);
        return NULL;
    }

    return st;
}

/**
 * Free symbol table and all symbols
 */
static void free_symbol_table(LinkerSymbolTable *st) {
    if (!st) return;

    /* Free symbol pool (this frees all LinkerSymbol structs at once)
     * No need to free individual symbols since they're allocated from the pool */
    if (st->symbol_pool) {
        /* Log pool statistics if debug enabled */
        if (getenv("LINKER_DEBUG")) {
            fprintf(stderr, "linker: Symbol pool stats: %d arenas, %.1f MB total\n",
                    st->symbol_pool->arena_count,
                    (st->symbol_pool->arena_count * st->symbol_pool->arena_size) / (1024.0 * 1024.0));
        }
        destroy_memory_pool(st->symbol_pool);
    }

    free(st->symbols);
    free(st->undefined);

    /* Free string pool */
    for (int i = 0; i < st->string_count; i++) {
        free(st->string_pool[i]);
    }
    free(st->string_pool);

    /* Free hash buckets */
    for (int i = 0; i < SYMBOL_HASH_SIZE; i++) {
        free(st->buckets[i]);
    }

    free(st);
}

/**
 * Find symbol by name using hash table (O(1) average case)
 * Returns NULL if not found
 */
static LinkerSymbol* find_symbol(LinkerSymbolTable *st, const char *name) {
    uint32_t hash = hash_symbol_name(name) % SYMBOL_HASH_SIZE;

    /* Search in hash bucket */
    for (int i = 0; i < st->bucket_counts[hash]; i++) {
        if (strcmp(st->buckets[hash][i]->name, name) == 0) {
            return st->buckets[hash][i];
        }
    }
    return NULL;
}

/**
 * Dump symbol table for debugging (--dump-symbols)
 */
static void dump_symbol_table(LinkerSymbolTable *symtab, MergedSection *sections, int section_count) {
    if (!g_dump_symbols || !symtab) return;

    printf("\n=== Symbol Table Dump ===\n");
    printf("%-18s %-6s %-7s %-12s %s\n", "Address", "Type", "Bind", "Section", "Name");
    printf("--------------------------------------------------------------------------------\n");

    int defined = 0, undefined = 0;

    for (int i = 0; i < symtab->count; i++) {
        LinkerSymbol *sym = symtab->symbols[i];

        const char *type_str = (sym->type == STT_FUNC) ? "FUNC" :
                               (sym->type == STT_OBJECT) ? "OBJECT" :
                               (sym->type == STT_NOTYPE) ? "NOTYPE" :
                               (sym->type == STT_SECTION) ? "SECTION" : "OTHER";
        const char *bind_str = (sym->bind == STB_GLOBAL) ? "GLOBAL" :
                               (sym->bind == STB_WEAK) ? "WEAK" :
                               (sym->bind == STB_LOCAL) ? "LOCAL" : "OTHER";

        /* Try to find section name */
        const char *section_name = "(none)";
        if (sym->shndx != SHN_UNDEF && sym->shndx != 0) {
            /* Search in merged sections by matching address range */
            for (int j = 0; j < section_count; j++) {
                if (sym->value >= sections[j].vma &&
                    sym->value < sections[j].vma + sections[j].size) {
                    section_name = sections[j].name;
                    break;
                }
            }
        }

        if (sym->shndx == SHN_UNDEF || sym->shndx == 0) {
            printf("%-18s %-6s %-7s %-12s %s\n",
                   "UNDEF", type_str, bind_str, section_name, sym->name);
            undefined++;
        } else {
            printf("0x%-16lx %-6s %-7s %-12s %s\n",
                   sym->value, type_str, bind_str, section_name, sym->name);
            defined++;
        }
    }

    printf("\nTotal: %d symbols (%d defined, %d undefined)\n",
           symtab->count, defined, undefined);
    printf("================================================================================\n\n");
}

/**
 * Assign virtual memory addresses to merged sections
 * Optimized layout: .text -> .rodata -> .data -> .bss
 * .rodata placed immediately after .text to minimize PC-relative relocation distances
 * @param sections Array of merged sections
 * @param count Number of sections
 * @return 0 on success, -1 on error
 */
static int assign_addresses(MergedSection *sections, int count) {
    if (!sections || count <= 0) {
        fprintf(stderr, "linker: Invalid arguments to assign_addresses\n");
        return -1;
    }

    timer_record("Phase 3.2: Start address assignment");

    uint64_t current_addr = LINKER_BASE_ADDR;

    /* Optimized section ordering to minimize PC-relative relocation distances
     * .rodata is placed right after .text to keep code and read-only data close together
     * This reduces the chance of PC32/PLT32 relocation overflows */
    const char *section_order[] = {".text", ".rodata", ".data", ".bss"};
    int order_count = sizeof(section_order) / sizeof(section_order[0]);

    /* Process sections in standard order */
    for (int i = 0; i < order_count; i++) {
        const char *name = section_order[i];

        /* Find section by name */
        MergedSection *sec = NULL;
        for (int j = 0; j < count; j++) {
            if (strcmp(sections[j].name, name) == 0) {
                sec = &sections[j];
                break;
            }
        }

        if (!sec || sec->size == 0) continue;

        /* Optimized alignment strategy:
         * - .text: Page aligned (required for executable segment)
         * - .rodata: Keep close to .text (minimal alignment) to reduce PC32 overflow risk
         * - .data/.bss: Page aligned (separate writable segment) */
        if (i == 0) {
            /* .text: Always page-aligned */
            current_addr = align_up(current_addr, LINKER_PAGE_SIZE);
        } else if (strcmp(name, ".rodata") == 0) {
            /* .rodata: Keep close to .text, use minimal alignment (16-byte for efficiency) */
            current_addr = align_up(current_addr, 16);
        } else if (strcmp(name, ".data") == 0) {
            /* .data: Page-aligned (starts new writable segment) */
            current_addr = align_up(current_addr, LINKER_PAGE_SIZE);
        } else {
            /* Other sections: Use their natural alignment */
            current_addr = align_up(current_addr, sec->alignment > 0 ? sec->alignment : 16);
        }

        /* Assign virtual memory address */
        sec->vma = current_addr;
        current_addr += sec->size;

        if (getenv("LINKER_DEBUG")) {
            fprintf(stderr, "linker: Assigned %s at 0x%lx, size 0x%lx\n",
                    sec->name, sec->vma, sec->size);
        }
    }

    /* Process other sections not in standard order */
    for (int j = 0; j < count; j++) {
        MergedSection *sec = &sections[j];
        if (sec->vma != 0) continue;  /* Already assigned */

        current_addr = align_up(current_addr, sec->alignment);
        sec->vma = current_addr;
        current_addr += sec->size;

        if (getenv("LINKER_DEBUG")) {
            fprintf(stderr, "linker: Assigned %s at 0x%lx, size 0x%lx\n",
                    sec->name, sec->vma, sec->size);
        }
    }

    timer_record("Phase 3.3: Finish address assignment");

    return 0;
}

/**
 * Add symbol to symbol table (Module 3 extension)
 * Handles duplicates according to symbol binding rules
 */
typedef struct SymbolExt {
    LinkerSymbol base;
    int obj_index;              /* Index of object file defining this symbol */
    int defined;                /* 1 if defined, 0 if undefined */
} SymbolExt;

static int add_symbol(LinkerSymbolTable *st, const char *name, uint64_t value,
                     uint64_t size, uint16_t shndx, unsigned char bind,
                     unsigned char type, unsigned char visibility,
                     int obj_index, int defined) {
    /* Check for existing symbol */
    LinkerSymbol *existing = find_symbol(st, name);

    if (existing) {
        /* Handle symbol resolution rules */

        /* Strong symbol overrides weak symbol */
        if (existing->bind == STB_WEAK && bind == STB_GLOBAL && defined) {
            existing->value = value;
            existing->size = size;
            existing->shndx = shndx;
            existing->bind = bind;
            existing->type = type;
            return 0;
        }

        /* Weak symbol doesn't override strong symbol */
        if (existing->bind == STB_GLOBAL && bind == STB_WEAK) {
            return 0;  /* Keep existing */
        }

        /* Multiple strong definitions - keep first (common for archives) */
        if (existing->bind == STB_GLOBAL && bind == STB_GLOBAL &&
            defined && existing->shndx != SHN_UNDEF) {
            /* Keep existing symbol (first wins) */
            return 0;
        }

        /* Common symbol handling - take larger size */
        if (shndx == SHN_COMMON && existing->shndx == SHN_COMMON) {
            if (size > existing->size) {
                existing->size = size;
                existing->value = value;
            }
            return 0;
        }

        /* Undefined symbol can be updated with definition */
        if (existing->shndx == SHN_UNDEF && defined) {
            existing->value = value;
            existing->size = size;
            existing->shndx = shndx;
            existing->bind = bind;
            existing->type = type;
            return 0;
        }

        return 0;  /* Symbol already handled */
    }

    /* Create new symbol from memory pool */
    LinkerSymbol *sym = (LinkerSymbol*)pool_alloc(st->symbol_pool, sizeof(LinkerSymbol));
    if (!sym) {
        fprintf(stderr, "linker: out of memory allocating symbol\n");
        return -1;
    }

    /* Zero-initialize the symbol */
    memset(sym, 0, sizeof(LinkerSymbol));

    /* Use string interning to avoid duplicate name allocations */
    const char *interned_name = intern_string(st, name);
    if (!interned_name) {
        /* Note: Can't free pool-allocated memory individually */
        return -1;
    }

    sym->name = interned_name;
    sym->value = value;
    sym->size = size;
    sym->shndx = shndx;
    sym->bind = bind;
    sym->type = type;
    sym->visibility = visibility;

    /* Expand array if needed */
    if (st->count >= st->capacity) {
        st->capacity *= 2;
        LinkerSymbol **new_symbols = (LinkerSymbol**)realloc(st->symbols,
                                                 st->capacity * sizeof(LinkerSymbol*));
        if (!new_symbols) {
            fprintf(stderr, "linker: out of memory expanding symbol table\n");
            /* Note: sym->name is in string_pool, don't free individually */
            free(sym);
            return -1;
        }
        st->symbols = new_symbols;
    }

    st->symbols[st->count++] = sym;

    /* Add to hash table */
    uint32_t hash = hash_symbol_name(name) % SYMBOL_HASH_SIZE;

    /* Expand bucket if needed */
    if (st->bucket_counts[hash] >= st->bucket_capacities[hash]) {
        int new_cap = st->bucket_capacities[hash] * 2;
        LinkerSymbol **new_bucket = (LinkerSymbol**)realloc(st->buckets[hash],
                                                            new_cap * sizeof(LinkerSymbol*));
        if (!new_bucket) {
            fprintf(stderr, "linker: out of memory expanding hash bucket\n");
            st->count--;  /* Rollback symbol addition */
            /* Note: sym->name is in string_pool, don't free individually */
            free(sym);
            return -1;
        }
        st->buckets[hash] = new_bucket;
        st->bucket_capacities[hash] = new_cap;
    }

    st->buckets[hash][st->bucket_counts[hash]++] = sym;

    /* Track undefined symbols */
    if (!defined && bind == STB_GLOBAL) {
        if (st->undef_count >= st->undef_capacity) {
            st->undef_capacity *= 2;
            LinkerSymbol **new_undef = (LinkerSymbol**)realloc(st->undefined,
                                                   st->undef_capacity * sizeof(LinkerSymbol*));
            if (!new_undef) {
                fprintf(stderr, "linker: out of memory expanding undefined array\n");
                return -1;
            }
            st->undefined = new_undef;
        }
        st->undefined[st->undef_count++] = sym;
    }

    return 0;
}

/**
 * Build unified symbol table from all object files
 */
static LinkerSymbolTable* build_symbol_table(ObjectFile **objs, int count) {
    timer_record("Phase 3.5.0: Start symbol table");

    /* Count total symbols first for pre-allocation */
    int total_symbols = 0;
    for (int i = 0; i < count; i++) {
        if (objs[i]->symbols && objs[i]->symbol_count > 0) {
            total_symbols += objs[i]->symbol_count;
        }
    }
    timer_record("Phase 3.5.1: Count symbols");

    LinkerSymbolTable *st = create_symbol_table();
    if (!st) return NULL;

    /* Pre-allocate symbol table to avoid reallocation during insertion */
    if (total_symbols > st->capacity) {
        st->capacity = total_symbols + 64;  /* Add some headroom */
        LinkerSymbol **new_symbols = (LinkerSymbol**)realloc(st->symbols,
                                                 st->capacity * sizeof(LinkerSymbol*));
        if (!new_symbols) {
            fprintf(stderr, "linker: out of memory pre-allocating symbol table\n");
            free_symbol_table(st);
            return NULL;
        }
        st->symbols = new_symbols;
    }
    timer_record("Phase 3.5.2: Allocate table");

    /* Collect symbols from all object files */
    for (int i = 0; i < count; i++) {
        ObjectFile *obj = objs[i];

        if (!obj->symbols || obj->symbol_count == 0) {
            continue;
        }

        /* Process each symbol in the object file */
        for (int j = 0; j < obj->symbol_count; j++) {
            LinkerSymbol *sym = &obj->symbols[j];

            /* Skip null symbol and local symbols without names */
            if (j == 0 || !sym->name || sym->name[0] == '\0') {
                continue;
            }

            /* Skip section and file symbols */
            if (sym->type == STT_SECTION || sym->type == STT_FILE) {
                continue;
            }

            /* Only add global/weak symbols, or local symbols if debugging */
            if (sym->bind != STB_GLOBAL && sym->bind != STB_WEAK) {
                continue;  /* Skip local symbols for now */
            }

            int defined = (sym->shndx != SHN_UNDEF);

            /* Add symbol to table */
            if (add_symbol(st, sym->name, sym->value, sym->size,
                          sym->shndx, sym->bind, sym->type, sym->visibility, i, defined) < 0) {
                free_symbol_table(st);
                return NULL;
            }
        }
    }
    timer_record("Phase 3.5.3: Add symbols");

    /* Create synthetic linker-provided symbols
     * These are special symbols that the linker itself must provide for runtime */

    /* _GLOBAL_OFFSET_TABLE_ - GOT base for position-independent code
     * Set to address 0 initially, will be updated during address assignment
     * if a .got section exists. If no .got section, this remains defined but unused. */
    if (add_symbol(st, "_GLOBAL_OFFSET_TABLE_", 0, 0,
                  1 /* non-UNDEF section */, STB_GLOBAL, STT_NOTYPE,
                  STV_DEFAULT, -1 /* synthetic */, 1 /* defined */) < 0) {
        free_symbol_table(st);
        return NULL;
    }
    timer_record("Phase 3.5.4: Create synthetic symbols");

    return st;
}

/**
 * Parse .a archive file header
 */
static int parse_archive_member(FILE *fp, char *name_out, size_t *size_out) {
    struct ar_hdr {
        char ar_name[16];
        char ar_date[12];
        char ar_uid[6];
        char ar_gid[6];
        char ar_mode[8];
        char ar_size[10];
        char ar_fmag[2];
    } hdr;

    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) {
        return -1;
    }

    /* Verify magic */
    if (memcmp(hdr.ar_fmag, "`\n", 2) != 0) {
        return -1;
    }

    /* Extract name */
    int len = 0;
    for (int i = 0; i < 16 && hdr.ar_name[i] != ' ' && hdr.ar_name[i] != '/'; i++) {
        name_out[len++] = hdr.ar_name[i];
    }
    name_out[len] = '\0';

    /* Parse size */
    char size_str[11];
    memcpy(size_str, hdr.ar_size, 10);
    size_str[10] = '\0';
    *size_out = atol(size_str);

    return 0;
}

/**
 * Check if archive member defines any needed symbols
 */
static int member_defines_needed_symbol(const char *member_data, size_t size,
                                       LinkerSymbolTable *st) {
    /* Quick check: is this an ELF object file? */
    if (size < sizeof(Elf64_Ehdr)) return 0;

    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)member_data;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        return 0;  /* Not an ELF file */
    }

    /* Find symbol table */
    if (ehdr->e_shoff + ehdr->e_shnum * sizeof(Elf64_Shdr) > size) {
        return 0;  /* Invalid */
    }

    Elf64_Shdr *sections = (Elf64_Shdr*)(member_data + ehdr->e_shoff);

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (sections[i].sh_type != SHT_SYMTAB) continue;

        /* Get symbol table and string table */
        if (sections[i].sh_offset + sections[i].sh_size > size) continue;
        if (sections[i].sh_link >= ehdr->e_shnum) continue;

        Elf64_Sym *symtab = (Elf64_Sym*)(member_data + sections[i].sh_offset);
        int symcount = sections[i].sh_size / sizeof(Elf64_Sym);

        Elf64_Shdr *strtab_hdr = &sections[sections[i].sh_link];
        if (strtab_hdr->sh_offset + strtab_hdr->sh_size > size) continue;

        char *strtab = (char*)(member_data + strtab_hdr->sh_offset);
        size_t strtab_size = strtab_hdr->sh_size;

        /* Check each symbol */
        for (int j = 0; j < symcount; j++) {
            Elf64_Sym *sym = &symtab[j];

            /* Skip undefined symbols */
            if (sym->st_shndx == SHN_UNDEF) continue;
            if (sym->st_name == 0) continue;

            unsigned char bind = ELF64_ST_BIND(sym->st_info);
            if (bind != STB_GLOBAL && bind != STB_WEAK) continue;

            /* Bounds check: Symbol name must be within string table */
            if (sym->st_name >= strtab_size) continue;

            const char *symname = strtab + sym->st_name;

            /* Bounds check: Ensure null-terminated within strtab */
            size_t max_name_len = strtab_size - sym->st_name;
            if (strnlen(symname, max_name_len) == max_name_len) continue;

            /* Check if this symbol is needed */
            for (int k = 0; k < st->undef_count; k++) {
                /* Check if symbol is still undefined (shndx == SHN_UNDEF means undefined) */
                if (st->undefined[k]->shndx != SHN_UNDEF) continue;  /* Already resolved */

                if (strcmp(st->undefined[k]->name, symname) == 0) {
                    return 1;  /* This member provides a needed symbol */
                }
            }
        }
    }

    return 0;
}

/**
 * Extract needed objects from archive
 * Returns array of object file data (caller must free)
 */
static int extract_needed_objects(const char *archive, LinkerSymbolTable *st,
                                  char ***extracted_data, size_t **extracted_sizes,
                                  int *extracted_count) {
    FILE *fp = fopen(archive, "rb");
    if (!fp) {
        fprintf(stderr, "linker: cannot open archive '%s': %s\n",
                archive, strerror(errno));
        return -1;
    }

    /* Verify archive magic */
    char magic[8];
    if (fread(magic, 8, 1, fp) != 1 || memcmp(magic, "!<arch>\n", 8) != 0) {
        fprintf(stderr, "linker: '%s' is not a valid archive\n", archive);
        fclose(fp);
        return -1;
    }

    int capacity = 16;
    *extracted_data = (char**)calloc(capacity, sizeof(char*));
    *extracted_sizes = (size_t*)calloc(capacity, sizeof(size_t));
    *extracted_count = 0;

    /* Iterate through archive members */
    while (!feof(fp)) {
        char name[17];
        size_t member_size;
        long member_offset = ftell(fp);

        if (parse_archive_member(fp, name, &member_size) < 0) {
            if (feof(fp)) break;
            fprintf(stderr, "linker: error parsing archive member\n");
            fclose(fp);
            return -1;
        }

        /* Skip special members */
        if (strcmp(name, "") == 0 || strcmp(name, "/") == 0 ||
            strcmp(name, "//") == 0) {
            long skip = member_size;
            if (skip & 1) skip++;  /* Archive members are 2-byte aligned */
            fseek(fp, skip, SEEK_CUR);
            continue;
        }

        /* Read member data */
        char *member_data = (char*)malloc(member_size);
        if (!member_data) {
            fprintf(stderr, "linker: out of memory\n");
            fclose(fp);
            return -1;
        }

        if (fread(member_data, 1, member_size, fp) != member_size) {
            fprintf(stderr, "linker: error reading archive member\n");
            free(member_data);
            fclose(fp);
            return -1;
        }

        /* Check if this member provides needed symbols */
        if (member_defines_needed_symbol(member_data, member_size, st)) {
            /* Expand array if needed */
            if (*extracted_count >= capacity) {
                capacity *= 2;
                *extracted_data = (char**)realloc(*extracted_data,
                                                  capacity * sizeof(char*));
                *extracted_sizes = (size_t*)realloc(*extracted_sizes,
                                                    capacity * sizeof(size_t));
            }

            (*extracted_data)[*extracted_count] = member_data;
            (*extracted_sizes)[*extracted_count] = member_size;
            (*extracted_count)++;
        } else {
            free(member_data);
        }

        /* Skip padding byte if needed */
        if (member_size & 1) {
            fseek(fp, 1, SEEK_CUR);
        }
    }

    fclose(fp);
    return 0;
}

/**
 * Relocate symbol addresses based on merged sections
 * @param syms Array of symbol pointers
 * @param sym_count Number of symbols
 * @param sections Merged sections with assigned addresses
 * @param section_count Number of sections
 * @return 0 on success, -1 on error
 */
static int relocate_symbols(LinkerSymbol **syms, int sym_count,
                            MergedSection *sections, int section_count) {
    if (!syms || sym_count <= 0 || !sections || section_count <= 0) {
        return 0; /* Nothing to do */
    }

    /* Update each symbol's address */
    for (int i = 0; i < sym_count; i++) {
        LinkerSymbol *sym = syms[i];
        if (!sym) continue;

        /* Skip undefined symbols */
        if (sym->shndx == SHN_UNDEF) continue;

        /* Skip absolute symbols */
        if (sym->shndx == SHN_ABS) continue;

        /* Find corresponding merged section */
        /* Note: This assumes shndx maps to section index in merged sections */
        /* In a full implementation, we'd need a mapping table from original to merged sections */
        if (sym->shndx < section_count) {
            MergedSection *sec = &sections[sym->shndx];
            sym->value = sec->vma + sym->value;

            if (getenv("LINKER_DEBUG")) {
                fprintf(stderr, "linker: Relocated symbol %s to 0x%lx\n",
                        sym->name ? sym->name : "(unnamed)", sym->value);
            }
        }
    }

    return 0;
}

/**
 * Create ELF program headers for merged sections
 * @param sections Merged sections
 * @param count Number of sections
 * @param phdr_count Output: number of program headers created
 * @return Array of program headers (NULL on error)
 */
static Elf64_Phdr* create_program_headers(MergedSection *sections, int count,
                                         int *phdr_count) {
    if (!sections || count <= 0 || !phdr_count) {
        fprintf(stderr, "linker: Invalid arguments to create_program_headers\n");
        return NULL;
    }

    /* Count required program headers (PT_LOAD for each section type) */
    int text_exists = 0, data_exists = 0;

    for (int i = 0; i < count; i++) {
        if (sections[i].size == 0) continue;

        if (strcmp(sections[i].name, ".text") == 0) {
            text_exists = 1;
        } else if (strcmp(sections[i].name, ".rodata") == 0 ||
                   strcmp(sections[i].name, ".data") == 0 ||
                   strcmp(sections[i].name, ".bss") == 0) {
            data_exists = 1;
        }
    }

    int num_phdrs = text_exists + data_exists;
    if (num_phdrs == 0) {
        *phdr_count = 0;
        return NULL;
    }

    Elf64_Phdr *phdrs = (Elf64_Phdr*)calloc(num_phdrs, sizeof(Elf64_Phdr));
    if (!phdrs) {
        fprintf(stderr, "linker: Out of memory allocating program headers\n");
        return NULL;
    }

    int phdr_idx = 0;

    /* Create PT_LOAD for .text (R+X) */
    if (text_exists) {
        MergedSection *text_sec = NULL;
        for (int i = 0; i < count; i++) {
            if (strcmp(sections[i].name, ".text") == 0) {
                text_sec = &sections[i];
                break;
            }
        }

        if (text_sec && text_sec->size > 0) {
            Elf64_Phdr *phdr = &phdrs[phdr_idx++];
            phdr->p_type = PT_LOAD;
            phdr->p_flags = PF_R | PF_X;
            phdr->p_offset = 0; /* To be filled by output writer */
            phdr->p_vaddr = text_sec->vma;
            phdr->p_paddr = text_sec->vma;
            phdr->p_filesz = text_sec->size;
            phdr->p_memsz = text_sec->size;
            phdr->p_align = LINKER_PAGE_SIZE;
        }
    }

    /* Create PT_LOAD for .rodata + .data + .bss (R+W) */
    if (data_exists) {
        uint64_t min_vma = UINT64_MAX;
        uint64_t max_vma = 0;
        size_t total_filesz = 0;
        size_t total_memsz = 0;

        /* Find bounds of data sections */
        for (int i = 0; i < count; i++) {
            if (sections[i].size == 0) continue;

            if (strcmp(sections[i].name, ".rodata") == 0 ||
                strcmp(sections[i].name, ".data") == 0 ||
                strcmp(sections[i].name, ".bss") == 0) {

                if (sections[i].vma < min_vma) {
                    min_vma = sections[i].vma;
                }
                uint64_t end = sections[i].vma + sections[i].size;
                if (end > max_vma) {
                    max_vma = end;
                }

                /* .bss has no file size */
                if (strcmp(sections[i].name, ".bss") != 0) {
                    total_filesz += sections[i].size;
                }
                total_memsz += sections[i].size;
            }
        }

        if (min_vma != UINT64_MAX && max_vma > min_vma) {
            Elf64_Phdr *phdr = &phdrs[phdr_idx++];
            phdr->p_type = PT_LOAD;
            phdr->p_flags = PF_R | PF_W;
            phdr->p_offset = 0; /* To be filled by output writer */
            phdr->p_vaddr = min_vma;
            phdr->p_paddr = min_vma;
            phdr->p_filesz = total_filesz;
            phdr->p_memsz = total_memsz;
            phdr->p_align = LINKER_PAGE_SIZE;
        }
    }

    *phdr_count = phdr_idx;
    return phdrs;
}

/**
 * Free merged sections
 */
static void free_merged_sections(MergedSection *sections, int count) {
    if (!sections) return;

    for (int i = 0; i < count; i++) {
        if (sections[i].name) free(sections[i].name);
        if (sections[i].data) free(sections[i].data);
    }
    free(sections);
}

/**
 * Search for library file in library paths (Module 3)
 * Returns allocated string with full path or NULL if not found
 */
static char* find_library(const char *libname, const char **lib_paths, int lib_count) {
    /* Convert -l name to lib{name}.a */
    char filename[256];
    snprintf(filename, sizeof(filename), "lib%s.a", libname);

    /* Search in provided paths */
    for (int i = 0; i < lib_count; i++) {
        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", lib_paths[i], filename);

        if (access(fullpath, R_OK) == 0) {
            return strdup(fullpath);
        }
    }

    /* Search in standard paths */
    const char *std_paths[] = {"/lib", "/usr/lib", "/usr/local/lib"};
    for (int i = 0; i < 3; i++) {
        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", std_paths[i], filename);

        if (access(fullpath, R_OK) == 0) {
            return strdup(fullpath);
        }
    }

    return NULL;
}

/**
 * Resolve all undefined symbols by searching archives
 */
static int resolve_symbols(LinkerSymbolTable *symtab, const char **lib_paths,
                          int lib_count, const char **libs, int libs_count) {
    /* First pass: check if symbols are already defined in loaded objects */
    int unresolved = 0;
    for (int i = 0; i < symtab->undef_count; i++) {
        if (symtab->undefined[i]->shndx == SHN_UNDEF) {
            unresolved++;
        }
    }

    if (unresolved == 0) {
        return 0;  /* All symbols resolved */
    }

    /* Log undefined symbols at INFO level */
    if (g_log_level >= LOG_INFO) {
        LOG_INFO_MSG("Found %d defined symbols, %d undefined symbols",
                     symtab->count - unresolved, unresolved);
        LOG_INFO_MSG("Undefined symbols needing resolution (showing first 20):");
        int show_count = (unresolved < 20) ? unresolved : 20;
        for (int i = 0; i < show_count; i++) {
            if (symtab->undefined[i]->shndx == SHN_UNDEF) {
                fprintf(stderr, "  - %s\n", symtab->undefined[i]->name);
            }
        }
        if (unresolved > 20) {
            fprintf(stderr, "  ... and %d more\n", unresolved - 20);
        }
    }

    /* Search libraries for undefined symbols */
    for (int i = 0; i < libs_count; i++) {
        char *archive_path = find_library(libs[i], lib_paths, lib_count);
        if (!archive_path) {
            fprintf(stderr, "linker: warning: library '%s' not found\n", libs[i]);
            continue;
        }

        if (g_trace_resolve) {
            LOG_INFO_MSG("[RESOLVE] Searching archive '%s'...", libs[i]);
        }

        /* Directly add all objects from archive to global object list */
        /* This is a simplified approach - extract and add all archive members */
        FILE *ar_fp = fopen(archive_path, "rb");
        if (!ar_fp) {
            free(archive_path);
            continue;
        }

        /* Skip AR magic */
        char magic[8];
        if (fread(magic, 8, 1, ar_fp) != 1 || memcmp(magic, "!<arch>\n", 8) != 0) {
            fclose(ar_fp);
            free(archive_path);
            continue;
        }

        /* Process each member */
        while (!feof(ar_fp)) {
            struct ar_hdr {
                char ar_name[16];
                char ar_date[12];
                char ar_uid[6];
                char ar_gid[6];
                char ar_mode[8];
                char ar_size[10];
                char ar_fmag[2];
            } hdr;

            long member_start = ftell(ar_fp);
            if (fread(&hdr, sizeof(hdr), 1, ar_fp) != 1) break;

            /* Check magic */
            if (memcmp(hdr.ar_fmag, "`\n", 2) != 0) break;

            /* Parse size */
            char size_str[11];
            memcpy(size_str, hdr.ar_size, 10);
            size_str[10] = '\0';
            size_t member_size = atol(size_str);

            /* Skip special members (symbol table, string table) */
            if (hdr.ar_name[0] == '/' || hdr.ar_name[0] == ' ') {
                fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
                continue;
            }

            /* Read member data */
            char *member_data = malloc(member_size);
            if (!member_data) {
                fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
                continue;
            }

            if (fread(member_data, member_size, 1, ar_fp) != 1) {
                free(member_data);
                fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
                continue;
            }

            /* Check if this is an ELF object file */
            const unsigned char elf_magic[4] = {0x7f, 'E', 'L', 'F'};
            if (member_size > 4 && memcmp(member_data, elf_magic, 4) == 0) {
                /* Write to temp file and parse */
                char temp_path[] = "/tmp/cosmo_ar_XXXXXX";
                int temp_fd = mkstemp(temp_path);
                if (temp_fd >= 0) {
                    write(temp_fd, member_data, member_size);
                    close(temp_fd);

                    /* Parse the object file */
                    ObjectFile *ar_obj = parse_elf64_object(temp_path);
                    if (ar_obj) {
                        /* Get member name */
                        char member_name[17];
                        int name_len = 0;
                        for (int n = 0; n < 16 && hdr.ar_name[n] != ' ' && hdr.ar_name[n] != '/'; n++) {
                            member_name[name_len++] = hdr.ar_name[n];
                        }
                        member_name[name_len] = '\0';

                        /* Add symbols from this object to symbol table */
                        for (int s = 0; s < ar_obj->symbol_count; s++) {
                            LinkerSymbol *sym = &ar_obj->symbols[s];
                            if (sym->bind == STB_GLOBAL || sym->bind == STB_WEAK) {
                                int defined = (sym->shndx != SHN_UNDEF);
                                if (defined && g_trace_resolve) {
                                    LOG_INFO_MSG("[RESOLVE]   Found '%s' in %s at 0x%lx",
                                               sym->name, member_name, sym->value);
                                }
                                add_symbol(symtab, sym->name, sym->value, sym->size,
                                         sym->shndx, sym->bind, sym->type, sym->visibility, -1, defined);
                            }
                        }
                        /* Note: We're leaking ar_obj here, but it's needed for relocation later */
                        /* TODO: Track these objects properly */
                    }
                    unlink(temp_path);
                }
            }

            free(member_data);

            /* Align to 2-byte boundary */
            fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
        }

        fclose(ar_fp);
        free(archive_path);
    }

    /* Synthesize APE/Cosmopolitan-specific symbols */
    LOG_DEBUG_MSG("Synthesizing APE/Cosmopolitan runtime symbols");

    /* Define linker-generated symbols as weak/zero-valued stubs */
    struct {
        const char *name;
        uint64_t value;
    } ape_symbols[] = {
        /* APE PE header symbols */
        {"ape_pe_shnum", 0},
        {"ape_pe_optsz", 0},
        {"ape_pe_entry", 0},
        {"ape_pe_base", 0x400000},  /* Typical Windows PE base */
        {"ape_pe_sectionalignment", 0x1000},
        {"ape_pe_filealignment", 0x200},
        {"ape_pe_sizeofheaders", 0},
        {"ape_pe_sections", 0},
        {"ape_pe_sections_end", 0},

        /* APE version symbols */
        {"v_ntsubversion", 0},
        {"v_ntsubsystem", 3},  /* CUI subsystem */
        {"v_ntdllchar", 0},
        {"v_ape_realdwords", 0},
        {"v_ape_allbytes", 0},

        /* APE data/import symbols */
        {"ape_idata", 0},
        {"ape_idata_idtsize", 0},

        /* APE text section symbols */
        {"ape_text_memsz", 0},
        {"ape_text_rva", 0x1000},
        {"ape_text_filesz", 0},
        {"ape_text_offset", 0},
        {"ape_text_nops", 0},

        /* APE ROM/RAM symbols */
        {"ape_rom_rva", 0},
        {"ape_ram_rva", 0},

        /* APE stack symbols */
        {"ape_stack_vaddr", 0},
        {"ape_stack_memsz", 0x100000},  /* 1MB stack */

        /* APE Mach-O symbols */
        {"ape_macho_end", 0},

        /* APE note section symbols */
        {"ape_note", 0},
        {"ape_note_end", 0},
        {"ape_note_vaddr", 0},

        /* Cosmopolitan main symbol */
        {"cosmo", 0},

        /* BSS end marker */
        {"_edata", 0},
        {"_end", 0},
        {"__bss_start", 0},

        /* Test and init symbols */
        {"__test_end", 0},
        {"__init_program_executable_name", 0},

        /* Program name symbols */
        {"GetProgramExecutableName", 0},
        {"program_invocation_name", 0},
        {"program_invocation_short_name", 0},

        {NULL, 0}
    };

    for (int i = 0; ape_symbols[i].name; i++) {
        LinkerSymbol *existing = find_symbol(symtab, ape_symbols[i].name);
        if (existing && existing->shndx == SHN_UNDEF) {
            /* Symbol is undefined, provide weak stub */
            LOG_DEBUG_MSG("Synthesizing APE symbol: %s = 0x%lx",
                         ape_symbols[i].name, ape_symbols[i].value);
            existing->value = ape_symbols[i].value;
            existing->shndx = SHN_ABS;  /* Absolute symbol */
            existing->bind = STB_WEAK;  /* Weak binding so it can be overridden */
            existing->type = STT_NOTYPE;
        }
    }

    /* Check for remaining undefined symbols */
    int errors = 0;
    for (int i = 0; i < symtab->undef_count; i++) {
        if (symtab->undefined[i]->shndx == SHN_UNDEF) {
            /* Treat undefined symbols as warnings, not errors.
             * In a full linker implementation, these would be resolved with weak symbols
             * or eliminated by --gc-sections. For now, we allow the link to proceed
             * since unused code paths (C++/OpenMP/etc.) may have unresolved dependencies. */
            fprintf(stderr, "linker: warning: undefined reference to '%s'\n",
                   symtab->undefined[i]->name);
            errors++;
        }
    }

    if (errors > 0) {
        fprintf(stderr, "linker: %d undefined symbols (treated as weak/ignored)\n", errors);
    }

    return 0;  /* Continue despite undefined symbols */
}


/**
 * Extract specific object files by name from an archive
 * Returns array of ObjectFile pointers and updates *extracted_count
 */
static ObjectFile** extract_specific_objects(const char *archive_path,
                                             const char **object_names, int name_count,
                                             int *extracted_count) {
    ObjectFile **extracted = NULL;
    int count = 0;
    int allocated_capacity = 0;

    if (!archive_path || !object_names || name_count == 0) {
        *extracted_count = 0;
        return NULL;
    }

    LOG_DEBUG_MSG("extract_specific_objects: Opening archive '%s'", archive_path);
    LOG_DEBUG_MSG("extract_specific_objects: Looking for %d objects:", name_count);
    for (int i = 0; i < name_count; i++) {
        LOG_DEBUG_MSG("  - %s", object_names[i]);
    }

    /* Allocate with 2x capacity to handle potential overruns (wildcards, duplicates, etc.) */
    allocated_capacity = name_count * 2;

    /* Track extracted member names to avoid duplicates */
    char **extracted_names = malloc(allocated_capacity * sizeof(char*));
    int extracted_names_count = 0;
    if (!extracted_names) {
        *extracted_count = 0;
        return NULL;
    }
    extracted = malloc(allocated_capacity * sizeof(ObjectFile*));
    if (!extracted) {
        free(extracted_names);
        *extracted_count = 0;
        return NULL;
    }
    LOG_DEBUG_MSG("extract_specific_objects: Allocated buffer with capacity %d (requested %d)",
                  allocated_capacity, name_count);

    FILE *ar_fp = fopen(archive_path, "rb");
    if (!ar_fp) {
        LOG_DEBUG_MSG("extract_specific_objects: Failed to open archive: %s", strerror(errno));
        free(extracted);
        free(extracted_names);
        *extracted_count = 0;
        return NULL;
    }
    LOG_DEBUG_MSG("extract_specific_objects: Archive opened successfully");

    /* Check AR magic */
    char magic[8];
    if (fread(magic, 8, 1, ar_fp) != 1 || memcmp(magic, "!<arch>\n", 8) != 0) {
        LOG_DEBUG_MSG("extract_specific_objects: Invalid or missing AR magic");
        fclose(ar_fp);
        free(extracted);
        free(extracted_names);
        *extracted_count = 0;
        return NULL;
    }
    LOG_DEBUG_MSG("extract_specific_objects: Valid AR magic found");

    /* Read string table for long filenames (if present) */
    char *string_table = NULL;
    size_t string_table_size = 0;

    /* Process each member - scan entire archive to find all requested objects */
    int member_index = 0;
    while (!feof(ar_fp)) {
        struct ar_hdr {
            char ar_name[16];
            char ar_date[12];
            char ar_uid[6];
            char ar_gid[6];
            char ar_mode[8];
            char ar_size[10];
            char ar_fmag[2];
        } hdr;

        long member_start = ftell(ar_fp);
        if (fread(&hdr, sizeof(hdr), 1, ar_fp) != 1) break;

        /* Check magic */
        if (memcmp(hdr.ar_fmag, "`\n", 2) != 0) {
            LOG_DEBUG_MSG("extract_specific_objects: Invalid member magic at index %d", member_index);
            break;
        }

        /* Parse size */
        char size_str[11];
        memcpy(size_str, hdr.ar_size, 10);
        size_str[10] = '\0';
        size_t member_size = atol(size_str);

        /* Extract member name */
        char member_name[256];  /* Larger buffer for long names */
        memcpy(member_name, hdr.ar_name, 16);
        member_name[16] = '\0';

        /* Log raw member name for debugging */
        char raw_name[17];
        memcpy(raw_name, hdr.ar_name, 16);
        raw_name[16] = '\0';
        LOG_DEBUG_MSG("extract_specific_objects: Member %d raw name: '%s' (size: %zu)",
                      member_index, raw_name, member_size);

        /* Check for special members */
        if (member_name[0] == '/' && member_name[1] == '/' && member_name[2] == ' ') {
            /* This is the GNU ar string table - read it */
            LOG_DEBUG_MSG("extract_specific_objects: Found string table member (size: %zu)", member_size);
            if (!string_table) {
                string_table = malloc(member_size);
                if (string_table && fread(string_table, member_size, 1, ar_fp) == 1) {
                    string_table_size = member_size;
                    LOG_DEBUG_MSG("extract_specific_objects: String table loaded (%zu bytes)", string_table_size);
                } else {
                    LOG_DEBUG_MSG("extract_specific_objects: Failed to read string table");
                    free(string_table);
                    string_table = NULL;
                }
            }
            fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
            member_index++;
            continue;
        }

        /* Handle long filenames (/N format - offset into string table) */
        if (member_name[0] == '/' && member_name[1] >= '0' && member_name[1] <= '9') {
            if (string_table) {
                /* Parse offset */
                long offset = atol(member_name + 1);
                LOG_DEBUG_MSG("extract_specific_objects: Long name reference, offset: %ld", offset);
                if (offset >= 0 && offset < (long)string_table_size) {
                    /* Copy name from string table */
                    const char *name_in_table = string_table + offset;
                    size_t name_len = 0;
                    while (name_len < sizeof(member_name) - 1 &&
                           offset + name_len < string_table_size &&
                           name_in_table[name_len] != '/' &&
                           name_in_table[name_len] != '\n') {
                        member_name[name_len] = name_in_table[name_len];
                        name_len++;
                    }
                    member_name[name_len] = '\0';
                    LOG_DEBUG_MSG("extract_specific_objects: Resolved long name: '%s'", member_name);
                }
            } else {
                LOG_DEBUG_MSG("extract_specific_objects: Long name but no string table available");
            }
        } else {
            /* Trim trailing spaces and slashes for short names */
            for (int i = 15; i >= 0; i--) {
                if (member_name[i] == ' ' || member_name[i] == '/') {
                    member_name[i] = '\0';
                } else {
                    break;
                }
            }
            LOG_DEBUG_MSG("extract_specific_objects: Processed short name: '%s'", member_name);
        }

        /* Check if this is one of the objects we want */
        int should_extract = 0;
        for (int i = 0; i < name_count; i++) {
            /* Exact match or prefix match (for truncated long names in ar format) */
            size_t member_len = strlen(member_name);
            size_t target_len = strlen(object_names[i]);

            if (strcmp(member_name, object_names[i]) == 0) {
                /* Exact match */
                should_extract = 1;
                LOG_DEBUG_MSG("extract_specific_objects: EXACT MATCH! Will extract '%s'", member_name);
                break;
            } else if (member_len == 16 && target_len > 16 &&
                       strncmp(member_name, object_names[i], 16) == 0) {
                /* Truncated name match - ar format only stores first 16 chars */
                should_extract = 1;
                LOG_DEBUG_MSG("extract_specific_objects: TRUNCATED MATCH! '%s' matches '%s'",
                              member_name, object_names[i]);
                break;
            }
        }

        /* Extract object if matched */
        if (should_extract && member_size > 0) {
            /* Check if already extracted by member name */
            int already_extracted = 0;
            for (int i = 0; i < extracted_names_count; i++) {
                if (strcmp(extracted_names[i], member_name) == 0) {
                    already_extracted = 1;
                    LOG_DEBUG_MSG("extract_specific_objects: Skipping duplicate member '%s'", member_name);
                    break;
                }
            }
            if (already_extracted) {
                continue;  /* Skip to next member */
            }

            /* Dynamically expand buffer if needed */
            if (count >= allocated_capacity) {
                int new_capacity = allocated_capacity * 2;
                LOG_DEBUG_MSG("extract_specific_objects: Buffer full (%d/%d), expanding to %d",
                              count, allocated_capacity, new_capacity);
                ObjectFile **new_extracted = realloc(extracted, new_capacity * sizeof(ObjectFile*));
                char **new_extracted_names = realloc(extracted_names, new_capacity * sizeof(char*));
                if (!new_extracted || !new_extracted_names) {
                    LOG_ERROR_MSG("extract_specific_objects: Failed to expand buffer from %d to %d",
                                  allocated_capacity, new_capacity);
                    /* Continue with current buffer - will skip this object */
                } else {
                    extracted = new_extracted;
                    extracted_names = new_extracted_names;
                    allocated_capacity = new_capacity;
                }
            }

            /* Only extract if we have space */
            if (count < allocated_capacity) {
                LOG_DEBUG_MSG("extract_specific_objects: Extracting member '%s' (%zu bytes)", member_name, member_size);
                /* Read member data */
                char *member_data = malloc(member_size);
                if (member_data && fread(member_data, member_size, 1, ar_fp) == 1) {
                    /* Check if ELF object */
                    const unsigned char elf_magic[4] = {0x7f, 'E', 'L', 'F'};
                    if (member_size > 4 && memcmp(member_data, elf_magic, 4) == 0) {
                        LOG_DEBUG_MSG("extract_specific_objects: Valid ELF object, writing to temp file");
                        /* Write to temp file */
                        char temp_path[] = "/tmp/cosmo_rt_XXXXXX";
                        int temp_fd = mkstemp(temp_path);
                        if (temp_fd >= 0) {
                            write(temp_fd, member_data, member_size);
                            close(temp_fd);

                            /* Parse object */
                            ObjectFile *ar_obj = parse_elf64_object(temp_path);
                            if (ar_obj) {
                                extracted[count++] = ar_obj;
                                LOG_DEBUG_MSG("extract_specific_objects: Successfully extracted '%s' (%d, capacity %d)",
                                              member_name, count, allocated_capacity);

                                /* Track this extraction */
                                extracted_names[extracted_names_count++] = strdup(member_name);
                            } else {
                                LOG_DEBUG_MSG("extract_specific_objects: Failed to parse ELF object '%s'", member_name);
                            }
                            unlink(temp_path);
                        } else {
                            LOG_DEBUG_MSG("extract_specific_objects: Failed to create temp file: %s", strerror(errno));
                        }
                    } else {
                        LOG_DEBUG_MSG("extract_specific_objects: Not an ELF object (invalid magic)");
                    }
                    free(member_data);
                } else if (member_data) {
                    LOG_DEBUG_MSG("extract_specific_objects: Failed to read member data");
                    free(member_data);
                } else {
                    LOG_DEBUG_MSG("extract_specific_objects: Failed to allocate memory for member data");
                }
            }
        }

        /* Seek to next member (align to even byte boundary) */
        fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
        member_index++;
    }

    fclose(ar_fp);
    if (string_table) free(string_table);

    /* Free extracted names tracking */
    for (int i = 0; i < extracted_names_count; i++) {
        free(extracted_names[i]);
    }
    free(extracted_names);

    *extracted_count = count;
    LOG_DEBUG_MSG("extract_specific_objects: Extraction complete. Extracted %d/%d objects", count, name_count);
    return extracted;
}


/**
 * Archive symbol index entry: maps symbol name to object file name and offset
 */
typedef struct {
    char *symbol_name;
    char *object_name;
    long file_offset;  /* Direct offset to member header in archive (for fast seek) */
} ArchiveSymbolEntry;

/**
 * Archive symbol index: complete index of all symbols in an archive
 */
typedef struct {
    ArchiveSymbolEntry *entries;
    int count;
    int capacity;
} ArchiveSymbolIndex;

/**
 * Build symbol index from archive file
 * Scans all .o files in archive and builds symbol_name -> object_name mapping
 */
static ArchiveSymbolIndex* build_archive_index(const char *archive_path) {
    ArchiveSymbolIndex *index = calloc(1, sizeof(ArchiveSymbolIndex));
    if (!index) return NULL;

    index->capacity = 1024;  /* Initial capacity for ~1000 symbols */
    index->entries = malloc(index->capacity * sizeof(ArchiveSymbolEntry));
    if (!index->entries) {
        free(index);
        return NULL;
    }

    FILE *ar_fp = fopen(archive_path, "rb");
    if (!ar_fp) {
        free(index->entries);
        free(index);
        return NULL;
    }

    /* Check AR magic */
    char magic[8];
    if (fread(magic, 8, 1, ar_fp) != 1 || memcmp(magic, "!<arch>\n", 8) != 0) {
        fclose(ar_fp);
        free(index->entries);
        free(index);
        return NULL;
    }

    /* Read string table for long filenames (if present) */
    char *string_table = NULL;
    size_t string_table_size = 0;

    /* Process each archive member */
    while (!feof(ar_fp)) {
        struct ar_hdr {
            char ar_name[16];
            char ar_date[12];
            char ar_uid[6];
            char ar_gid[6];
            char ar_mode[8];
            char ar_size[10];
            char ar_fmag[2];
        } hdr;

        long member_start = ftell(ar_fp);
        if (fread(&hdr, sizeof(hdr), 1, ar_fp) != 1) break;

        /* Check magic */
        if (memcmp(hdr.ar_fmag, "`\n", 2) != 0) break;

        /* Parse size */
        char size_str[11];
        memcpy(size_str, hdr.ar_size, 10);
        size_str[10] = '\0';
        size_t member_size = atol(size_str);

        /* Parse member name */
        char member_name[256];
        memcpy(member_name, hdr.ar_name, 16);
        member_name[16] = '\0';

        /* Handle special members (/ = symbol table, // = string table) */
        if (strncmp(member_name, "//", 2) == 0) {
            /* GNU-style long filename string table */
            string_table_size = member_size;
            string_table = malloc(string_table_size);
            if (string_table) {
                fread(string_table, string_table_size, 1, ar_fp);
            }
            fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
            continue;
        } else if (member_name[0] == '/' && member_name[1] != '/') {
            /* Skip symbol table and other special members */
            fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
            continue;
        }

        /* Resolve long name if needed */
        if (member_name[0] == '/' && member_name[1] >= '0' && member_name[1] <= '9') {
            /* GNU-style long name reference */
            if (string_table) {
                long offset = atol(member_name + 1);
                if (offset < string_table_size) {
                    const char *long_name = string_table + offset;
                    size_t name_len = strcspn(long_name, "/\n");
                    if (name_len > 0 && name_len < sizeof(member_name)) {
                        memcpy(member_name, long_name, name_len);
                        member_name[name_len] = '\0';
                    }
                }
            }
        } else {
            /* Trim trailing spaces and slashes for short names */
            for (int i = 15; i >= 0; i--) {
                if (member_name[i] == ' ' || member_name[i] == '/') {
                    member_name[i] = '\0';
                } else {
                    break;
                }
            }
        }

        /* Only process ELF objects */
        if (member_size < 4) {
            fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
            continue;
        }

        /* Check ELF magic */
        unsigned char elf_magic[4];
        long data_start = ftell(ar_fp);
        if (fread(elf_magic, 4, 1, ar_fp) != 1 ||
            memcmp(elf_magic, "\x7f""ELF", 4) != 0) {
            fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
            continue;
        }

        /* Read entire object into memory for symbol extraction */
        fseek(ar_fp, data_start, SEEK_SET);
        char *member_data = malloc(member_size);
        if (!member_data || fread(member_data, member_size, 1, ar_fp) != 1) {
            free(member_data);
            fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
            continue;
        }

        /* Parse ELF to extract symbols */
        Elf64_Ehdr *ehdr = (Elf64_Ehdr*)member_data;

        /* Bounds check: ELF header must fit in member data */
        if (member_size < sizeof(Elf64_Ehdr)) {
            free(member_data);
            fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
            continue;
        }

        /* Bounds check: Section headers must be within member data */
        if (ehdr->e_shoff > member_size ||
            ehdr->e_shoff + (uint64_t)ehdr->e_shnum * sizeof(Elf64_Shdr) > member_size) {
            free(member_data);
            fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
            continue;
        }

        Elf64_Shdr *shdrs = (Elf64_Shdr*)(member_data + ehdr->e_shoff);

        /* Find symbol table */
        for (int i = 0; i < ehdr->e_shnum; i++) {
            if (shdrs[i].sh_type != SHT_SYMTAB) continue;

            /* Bounds check: Symbol table must be within member data */
            if (shdrs[i].sh_offset > member_size ||
                shdrs[i].sh_offset + shdrs[i].sh_size > member_size) {
                continue;
            }

            /* Bounds check: sh_link must be valid section index */
            if (shdrs[i].sh_link >= ehdr->e_shnum) {
                continue;
            }

            Elf64_Sym *symtab = (Elf64_Sym*)(member_data + shdrs[i].sh_offset);
            int sym_count = shdrs[i].sh_size / sizeof(Elf64_Sym);

            /* Bounds check: Validate symbol count */
            if (sym_count < 0 || (size_t)sym_count > shdrs[i].sh_size / sizeof(Elf64_Sym)) {
                continue;
            }

            /* Get string table */
            Elf64_Shdr *strtab_shdr = &shdrs[shdrs[i].sh_link];

            /* Bounds check: String table must be within member data */
            if (strtab_shdr->sh_offset > member_size ||
                strtab_shdr->sh_offset + strtab_shdr->sh_size > member_size) {
                continue;
            }

            char *strtab = member_data + strtab_shdr->sh_offset;
            size_t strtab_size = strtab_shdr->sh_size;

            /* Extract defined global symbols */
            for (int j = 0; j < sym_count; j++) {
                Elf64_Sym *sym = &symtab[j];
                unsigned char bind = ELF64_ST_BIND(sym->st_info);

                /* Only index defined global/weak symbols */
                if (sym->st_shndx == SHN_UNDEF) continue;
                if (bind != STB_GLOBAL && bind != STB_WEAK) continue;
                if (sym->st_name == 0) continue;

                /* Bounds check: Symbol name offset must be within string table */
                if (sym->st_name >= strtab_size) {
                    continue;
                }

                char *sym_name = strtab + sym->st_name;

                /* Bounds check: Ensure null-terminated string within strtab */
                size_t max_name_len = strtab_size - sym->st_name;
                if (strnlen(sym_name, max_name_len) == max_name_len) {
                    continue;  /* String not null-terminated within bounds */
                }

                if (!sym_name || sym_name[0] == '\0') continue;

                /* Expand index if needed */
                if (index->count >= index->capacity) {
                    /* Bounds check: Prevent integer overflow on capacity expansion */
                    int new_capacity = index->capacity * 2;
                    if (new_capacity < index->capacity || new_capacity > 1000000) {
                        fprintf(stderr, "linker: symbol index overflow (capacity %d)\n", index->capacity);
                        free(member_data);
                        goto cleanup;
                    }

                    ArchiveSymbolEntry *new_entries = realloc(index->entries,
                        new_capacity * sizeof(ArchiveSymbolEntry));
                    if (!new_entries) {
                        free(member_data);
                        goto cleanup;
                    }
                    index->capacity = new_capacity;
                    index->entries = new_entries;

                    LOG_DEBUG_MSG("Expanded symbol index to capacity %d", new_capacity);
                }

                /* Add to index with file offset for direct seek */
                index->entries[index->count].symbol_name = strdup(sym_name);
                index->entries[index->count].object_name = strdup(member_name);
                index->entries[index->count].file_offset = member_start;
                index->count++;
            }

            break;  /* Only process first symbol table */
        }

        free(member_data);

        /* Seek to next member (align to even byte boundary) */
        fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
    }

cleanup:
    if (string_table) free(string_table);
    fclose(ar_fp);

    LOG_DEBUG_MSG("build_archive_index: Indexed %d symbols from '%s'",
                  index->count, archive_path);

    return index;
}

/**
 * Free archive symbol index
 */
static void free_archive_index(ArchiveSymbolIndex *index) {
    if (!index) return;

    /* Bounds check: Validate count before freeing */
    if (index->count < 0 || index->count > index->capacity) {
        fprintf(stderr, "linker: warning: corrupted index during free (count=%d, capacity=%d)\n",
                index->count, index->capacity);
        /* Still free what we can */
        if (index->entries) free(index->entries);
        free(index);
        return;
    }

    if (index->entries) {
        for (int i = 0; i < index->count; i++) {
            /* Bounds check: Ensure we're within array bounds */
            assert(i >= 0 && i < index->capacity);

            free(index->entries[i].symbol_name);
            free(index->entries[i].object_name);
        }
        free(index->entries);
    }
    free(index);
}

/**
 * Helper structure for fast object extraction by file offset
 */
typedef struct {
    const char *object_name;
    long file_offset;
} ObjectRequest;

/**
 * Archive mmap context (R31-A: reuse across iterations)
 */
typedef struct {
    const char *path;
    int fd;
    void *map;
    size_t size;
} ArchiveMmapContext;

/**
 * R31-A: Open archive with mmap (reusable across iterations)
 */
static ArchiveMmapContext* open_archive_mmap(const char *archive_path) {
    ArchiveMmapContext *ctx = calloc(1, sizeof(ArchiveMmapContext));
    if (!ctx) return NULL;

    ctx->path = archive_path;
    ctx->fd = open(archive_path, O_RDONLY);
    if (ctx->fd < 0) {
        free(ctx);
        return NULL;
    }

    struct stat st;
    if (fstat(ctx->fd, &st) != 0) {
        close(ctx->fd);
        free(ctx);
        return NULL;
    }
    ctx->size = st.st_size;

    ctx->map = mmap(NULL, ctx->size, PROT_READ, MAP_PRIVATE, ctx->fd, 0);
    if (ctx->map == MAP_FAILED) {
        close(ctx->fd);
        free(ctx);
        return NULL;
    }

    return ctx;
}

/**
 * R31-A: Close archive mmap context
 */
static void close_archive_mmap(ArchiveMmapContext *ctx) {
    if (!ctx) return;
    if (ctx->map && ctx->map != MAP_FAILED) {
        munmap(ctx->map, ctx->size);
    }
    if (ctx->fd >= 0) {
        close(ctx->fd);
    }
    free(ctx);
}

/**
 * R31-A: Extract from mmap'd archive (zero-copy, context reuse)
 */
static ObjectFile** extract_objects_from_mmap(ArchiveMmapContext *ctx,
                                                ObjectRequest *requests,
                                                int request_count,
                                                int *extracted_count) {
    if (!ctx || !requests || request_count == 0) {
        *extracted_count = 0;
        return NULL;
    }

    /* Track extracted object names to avoid duplicates */
    char **extracted_names = malloc(request_count * sizeof(char*));
    if (!extracted_names) {
        *extracted_count = 0;
        return NULL;
    }
    int extracted_names_count = 0;

    ObjectFile **extracted = malloc(request_count * sizeof(ObjectFile*));
    if (!extracted) {
        free(extracted_names);
        *extracted_count = 0;
        return NULL;
    }
    int count = 0;

    /* Extract each object by parsing directly from mmap'd memory */
    for (int i = 0; i < request_count; i++) {
        /* Check if already extracted by object name */
        int already_extracted = 0;
        for (int j = 0; j < extracted_names_count; j++) {
            if (strcmp(extracted_names[j], requests[i].object_name) == 0) {
                already_extracted = 1;
                break;
            }
        }
        if (already_extracted) {
            continue;  /* Skip duplicate */
        }

        /* Validate offset bounds */
        long file_offset = requests[i].file_offset;
        if (file_offset < 0 || (size_t)file_offset >= ctx->size) {
            continue;  /* Invalid offset */
        }

        /* Archive member header structure */
        struct ar_hdr {
            char ar_name[16];
            char ar_date[12];
            char ar_uid[6];
            char ar_gid[6];
            char ar_mode[8];
            char ar_size[10];
            char ar_fmag[2];
        };

        /* Check if we have enough space for header */
        if ((size_t)file_offset + sizeof(struct ar_hdr) > ctx->size) {
            continue;  /* Not enough space for header */
        }

        /* Read header directly from mmap'd memory */
        struct ar_hdr *hdr = (struct ar_hdr*)((char*)ctx->map + file_offset);

        /* Verify magic */
        if (memcmp(hdr->ar_fmag, "`\n", 2) != 0) {
            continue;  /* Invalid header */
        }

        /* Parse member size */
        char size_str[11];
        memcpy(size_str, hdr->ar_size, 10);
        size_str[10] = '\0';
        size_t member_size = atol(size_str);

        /* Calculate member data offset (after header) */
        size_t member_offset = file_offset + sizeof(struct ar_hdr);

        /* Validate member bounds */
        if (member_offset + member_size > ctx->size) {
            continue;  /* Member extends beyond archive */
        }

        /* R31-A: Parse directly from mmap'd memory (zero-copy!)
         * Note: parse_elf64_from_memory will make its own copies of needed data */
        const unsigned char *member_data = (const unsigned char*)ctx->map + member_offset;
        ObjectFile *obj = parse_elf64_from_memory(member_data, member_size, requests[i].object_name);

        if (obj) {
            /* Filename already set by parse_elf64_from_memory */
            extracted[count++] = obj;
            /* Track this extraction */
            extracted_names[extracted_names_count++] = strdup(requests[i].object_name);
        }
    }

    /* Free extracted names tracking */
    for (int i = 0; i < extracted_names_count; i++) {
        free(extracted_names[i]);
    }
    free(extracted_names);

    *extracted_count = count;
    return extracted;
}

/**
 * Extract objects from archive based on undefined symbols (R31-A: mmap context version)
 * Uses symbol index for fast lookup
 */
static ObjectFile** extract_objects_for_symbols_mmap(ArchiveMmapContext *ctx,
                                                ArchiveSymbolIndex *index,
                                                const char **undef_symbols,
                                                int undef_count,
                                                int *extracted_count) {
    if (!ctx || !index || !undef_symbols || undef_count == 0) {
        if (extracted_count) *extracted_count = 0;
        return NULL;
    }

    /* Bounds check: Validate index structure */
    if (index->count < 0 || index->count > index->capacity) {
        fprintf(stderr, "linker: internal error: invalid index count %d (capacity %d)\n",
                index->count, index->capacity);
        *extracted_count = 0;
        return NULL;
    }

    /* Build list of unique object requests with file offsets */
    ObjectRequest *requests = malloc(undef_count * sizeof(ObjectRequest));
    if (!requests) {
        *extracted_count = 0;
        return NULL;
    }
    int request_count = 0;

    for (int i = 0; i < undef_count; i++) {
        const char *sym_name = undef_symbols[i];
        if (!sym_name) continue;  /* Skip null symbol names */

        /* Search index for this symbol */
        for (int j = 0; j < index->count; j++) {
            /* Bounds check: Ensure we're within index array */
            assert(j >= 0 && j < index->capacity);

            if (!index->entries[j].symbol_name || !index->entries[j].object_name) {
                continue;  /* Skip malformed entries */
            }

            if (strcmp(index->entries[j].symbol_name, sym_name) == 0) {
                const char *obj_name = index->entries[j].object_name;
                long file_offset = index->entries[j].file_offset;

                /* Check if already in request list */
                int already_added = 0;
                for (int k = 0; k < request_count; k++) {
                    if (strcmp(requests[k].object_name, obj_name) == 0) {
                        already_added = 1;
                        break;
                    }
                }

                if (!already_added) {
                    requests[request_count].object_name = obj_name;
                    requests[request_count].file_offset = file_offset;
                    request_count++;
                }

                break;  /* Found symbol, move to next */
            }
        }
    }

    LOG_DEBUG_MSG("extract_objects_for_symbols: Need %d objects for %d symbols",
                  request_count, undef_count);

    /* R31-A: Extract using mmap context (zero-copy, context reuse) */
    ObjectFile **result = extract_objects_from_mmap(ctx,
                                                      requests,
                                                      request_count, extracted_count);

    free(requests);
    return result;
}

/**
 * Extract all object files from archive libraries (OLD implementation - kept for reference)
 * Returns array of ObjectFile pointers and updates *extracted_count
 */
static ObjectFile** extract_archive_objects_all(const char **libs, int libs_count,
                                            const char **lib_paths, int lib_count,
                                            int *extracted_count) {
    ObjectFile **extracted = NULL;
    int capacity = 100;  /* Initial capacity */
    int count = 0;

    extracted = malloc(capacity * sizeof(ObjectFile*));
    if (!extracted) return NULL;

    for (int i = 0; i < libs_count; i++) {
        char *archive_path = find_library(libs[i], lib_paths, lib_count);
        if (!archive_path) {
            fprintf(stderr, "linker: warning: library '%s' not found\n", libs[i]);
            continue;
        }

        FILE *ar_fp = fopen(archive_path, "rb");
        if (!ar_fp) {
            free(archive_path);
            continue;
        }

        /* Check AR magic */
        char magic[8];
        if (fread(magic, 8, 1, ar_fp) != 1 || memcmp(magic, "!<arch>\n", 8) != 0) {
            fclose(ar_fp);
            free(archive_path);
            continue;
        }

        /* Process each member */
        while (!feof(ar_fp)) {
            struct ar_hdr {
                char ar_name[16];
                char ar_date[12];
                char ar_uid[6];
                char ar_gid[6];
                char ar_mode[8];
                char ar_size[10];
                char ar_fmag[2];
            } hdr;

            long member_start = ftell(ar_fp);
            if (fread(&hdr, sizeof(hdr), 1, ar_fp) != 1) break;

            /* Check magic */
            if (memcmp(hdr.ar_fmag, "`\n", 2) != 0) break;

            /* Parse size */
            char size_str[11];
            memcpy(size_str, hdr.ar_size, 10);
            size_str[10] = '\0';
            size_t member_size = atol(size_str);

            /* Skip special members */
            if (hdr.ar_name[0] == '/' || hdr.ar_name[0] == ' ') {
                fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
                continue;
            }

            /* Read member data */
            char *member_data = malloc(member_size);
            if (!member_data) {
                fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
                continue;
            }

            if (fread(member_data, member_size, 1, ar_fp) != 1) {
                free(member_data);
                fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
                continue;
            }

            /* Check if ELF object */
            const unsigned char elf_magic[4] = {0x7f, 'E', 'L', 'F'};
            if (member_size > 4 && memcmp(member_data, elf_magic, 4) == 0) {
                /* Write to temp file */
                char temp_path[] = "/tmp/cosmo_ar_XXXXXX";
                int temp_fd = mkstemp(temp_path);
                if (temp_fd >= 0) {
                    write(temp_fd, member_data, member_size);
                    close(temp_fd);

                    /* Parse object */
                    ObjectFile *ar_obj = parse_elf64_object(temp_path);
                    if (ar_obj) {
                        /* Log extracted object and its defined symbols at INFO level */
                        if (g_log_level >= LOG_INFO) {
                            /* Extract member name */
                            char member_name[17];
                            memcpy(member_name, hdr.ar_name, 16);
                            member_name[16] = '\0';
                            /* Remove trailing spaces and '/' */
                            for (int k = 15; k >= 0 && (member_name[k] == ' ' || member_name[k] == '/'); k--) {
                                member_name[k] = '\0';
                            }

                            /* Count defined symbols */
                            int defined_count = 0;
                            for (int k = 0; k < ar_obj->symbol_count; k++) {
                                if (ar_obj->symbols[k].shndx != SHN_UNDEF && ar_obj->symbols[k].name[0] != '\0') {
                                    defined_count++;
                                }
                            }

                            LOG_INFO_MSG("Extracted %s from %s (provides %d symbols)",
                                        member_name, libs[i], defined_count);

                            /* Show first 5 defined symbols at DEBUG level */
                            if (g_log_level >= LOG_DEBUG && defined_count > 0) {
                                int shown = 0;
                                for (int k = 0; k < ar_obj->symbol_count && shown < 5; k++) {
                                    if (ar_obj->symbols[k].shndx != SHN_UNDEF && ar_obj->symbols[k].name[0] != '\0') {
                                        fprintf(stderr, "    + %s\n", ar_obj->symbols[k].name);
                                        shown++;
                                    }
                                }
                                if (defined_count > 5) {
                                    fprintf(stderr, "    ... and %d more\n", defined_count - 5);
                                }
                            }
                        }

                        /* Expand array if needed */
                        if (count >= capacity) {
                            capacity *= 2;
                            ObjectFile **new_extracted = realloc(extracted, capacity * sizeof(ObjectFile*));
                            if (!new_extracted) {
                                free(member_data);
                                fclose(ar_fp);
                                free(archive_path);
                                *extracted_count = count;
                                return extracted;
                            }
                            extracted = new_extracted;
                        }
                        extracted[count++] = ar_obj;
                    }
                    unlink(temp_path);
                }
            }

            free(member_data);
            fseek(ar_fp, member_start + sizeof(hdr) + member_size + (member_size & 1), SEEK_SET);
        }

        fclose(ar_fp);
        free(archive_path);
    }

    *extracted_count = count;
    return extracted;
}

/**
 * Extract archive objects with lazy symbol resolution (NEW implementation)
 * Iteratively extracts objects from archives based on undefined symbols
 * Returns array of ObjectFile pointers and updates *extracted_count
 */
static ObjectFile** extract_archive_objects(const char **libs, int libs_count,
                                            const char **lib_paths, int lib_count,
                                            ObjectFile **obj_files, int obj_count,
                                            int *extracted_count) {
    ObjectFile **all_extracted = NULL;
    int total_extracted = 0;
    int capacity = 100;

    all_extracted = malloc(capacity * sizeof(ObjectFile*));
    if (!all_extracted) {
        *extracted_count = 0;
        return NULL;
    }

    /* CRITICAL FIX: Create local working copy of obj_files to avoid use-after-free
     * The function needs to extend obj_files for iterative symbol resolution,
     * but must not corrupt the caller's pointer by doing realloc on it.
     */
    ObjectFile **working_obj_files = malloc(obj_count * sizeof(ObjectFile*));
    if (!working_obj_files) {
        free(all_extracted);
        *extracted_count = 0;
        return NULL;
    }
    memcpy(working_obj_files, obj_files, obj_count * sizeof(ObjectFile*));
    int working_obj_count = obj_count;

    /* R31-A: Build symbol indexes and mmap contexts for all archives (one-time cost) */
    ArchiveSymbolIndex **indexes = calloc(libs_count, sizeof(ArchiveSymbolIndex*));
    char **archive_paths = calloc(libs_count, sizeof(char*));
    ArchiveMmapContext **mmap_ctxs = calloc(libs_count, sizeof(ArchiveMmapContext*));
    int valid_archive_count = 0;

    timer_record("Phase 2.0.1: Start archive indexing");
    for (int i = 0; i < libs_count; i++) {
        char *archive_path = find_library(libs[i], lib_paths, lib_count);
        if (!archive_path) {
            LOG_DEBUG_MSG("Library '%s' not found", libs[i]);
            continue;
        }

        LOG_INFO_MSG("Building symbol index for %s...", archive_path);
        ArchiveSymbolIndex *index = build_archive_index(archive_path);
        if (index) {
            /* R31-A: Open mmap context once for reuse across iterations */
            ArchiveMmapContext *mmap_ctx = open_archive_mmap(archive_path);
            if (mmap_ctx) {
                indexes[valid_archive_count] = index;
                archive_paths[valid_archive_count] = archive_path;
                mmap_ctxs[valid_archive_count] = mmap_ctx;
                valid_archive_count++;
                LOG_INFO_MSG("  Indexed %d symbols, mmap'd %zu bytes", index->count, mmap_ctx->size);
            } else {
                LOG_DEBUG_MSG("Failed to mmap %s", archive_path);
                free_archive_index(index);
                free(archive_path);
            }
        } else {
            LOG_DEBUG_MSG("Failed to build index for %s", archive_path);
            free(archive_path);
        }
    }
    timer_record("Phase 2.0.2: Finish archive indexing");

    if (valid_archive_count == 0) {
        LOG_DEBUG_MSG("No valid archives found");
        free(mmap_ctxs);
        free(indexes);
        free(archive_paths);
        *extracted_count = 0;
        return all_extracted;
    }

    /* Iterative symbol resolution */
    int iteration = 0;
    int max_iterations = 10;  /* Prevent infinite loops */
    int prev_undef_count = -1;

    LOG_INFO_MSG("Starting lazy symbol extraction (max %d iterations)...", max_iterations);

    while (iteration < max_iterations) {
        iteration++;
        timer_record("Phase 2.1: Start iteration");

        /* Build temporary symbol table from current objects */
        LinkerSymbolTable *temp_symtab = build_symbol_table(working_obj_files, working_obj_count);
        if (!temp_symtab) {
            LOG_ERROR_MSG("Failed to build symbol table in iteration %d", iteration);
            break;
        }
        timer_record("Phase 2.2: Build temp symtab");

        int undef_count = temp_symtab->undef_count;
        LOG_INFO_MSG("  Iteration %d: %d undefined symbols", iteration, undef_count);

        /* Check termination conditions */
        if (undef_count == 0) {
            LOG_INFO_MSG("  All symbols resolved!");
            free_symbol_table(temp_symtab);
            break;
        }

        if (undef_count == prev_undef_count) {
            LOG_INFO_MSG("  No progress, stopping (stuck at %d undefined)", undef_count);
            free_symbol_table(temp_symtab);
            break;
        }

        prev_undef_count = undef_count;

        /* Collect undefined symbol names */
        const char **undef_symbols = malloc(undef_count * sizeof(char*));
        for (int i = 0; i < undef_count; i++) {
            undef_symbols[i] = temp_symtab->undefined[i]->name;
        }

        /* Try to extract objects from each archive */
        int extracted_this_round = 0;

        for (int i = 0; i < valid_archive_count; i++) {
            int extracted = 0;
            /* R31-A: Use mmap context for zero-copy extraction */
            ObjectFile **new_objs = extract_objects_for_symbols_mmap(
                mmap_ctxs[i], indexes[i],
                undef_symbols, undef_count, &extracted);

            if (extracted > 0) {
                LOG_INFO_MSG("  Extracted %d objects from %s",
                            extracted, archive_paths[i]);

                /* Add to global list */
                for (int j = 0; j < extracted; j++) {
                    if (total_extracted >= capacity) {
                        capacity *= 2;
                        ObjectFile **new_all = realloc(all_extracted,
                                                      capacity * sizeof(ObjectFile*));
                        if (!new_all) {
                            free(undef_symbols);
                            free_symbol_table(temp_symtab);
                            goto cleanup;
                        }
                        all_extracted = new_all;
                    }
                    all_extracted[total_extracted++] = new_objs[j];
                }

                /* Add to working set for next iteration */
                ObjectFile **new_working_files = realloc(working_obj_files,
                    (working_obj_count + extracted) * sizeof(ObjectFile*));
                if (!new_working_files) {
                    free(new_objs);
                    free(undef_symbols);
                    free_symbol_table(temp_symtab);
                    goto cleanup;
                }
                working_obj_files = new_working_files;

                for (int j = 0; j < extracted; j++) {
                    working_obj_files[working_obj_count++] = new_objs[j];
                }

                free(new_objs);
                extracted_this_round += extracted;
            }
        }

        free(undef_symbols);
        free_symbol_table(temp_symtab);
        timer_record("Phase 2.3: Extract objects");

        if (extracted_this_round == 0) {
            LOG_INFO_MSG("  No objects extracted this round, stopping");
            break;
        }
    }

    LOG_INFO_MSG("Lazy extraction complete: %d iterations, %d objects extracted",
                iteration, total_extracted);

cleanup:
    /* R31-A: Close mmap contexts */
    for (int i = 0; i < valid_archive_count; i++) {
        close_archive_mmap(mmap_ctxs[i]);
    }
    free(mmap_ctxs);

    /* Free indexes and paths */
    for (int i = 0; i < valid_archive_count; i++) {
        free_archive_index(indexes[i]);
        free(archive_paths[i]);
    }
    free(indexes);
    free(archive_paths);

    /* Free working copy of obj_files */
    if (working_obj_files) {
        free(working_obj_files);
    }

    *extracted_count = total_extracted;
    return all_extracted;
}

/* ========== Module 3.5: Section Hash Table for Fast Relocation Processing ========== */

/**
 * Section hash table for O(1) section lookup during relocation processing
 * Eliminates O(M) linear searches through merged sections
 */
typedef struct {
    MergedSection **buckets[SECTION_HASH_SIZE];  /* Hash buckets */
    int bucket_counts[SECTION_HASH_SIZE];        /* Sections per bucket */
    int bucket_capacities[SECTION_HASH_SIZE];    /* Allocated capacity per bucket */
} SectionHashTable;

/**
 * Relocation batch entry for sorting relocations by target section
 * Improves cache locality by processing relocations in target-section order
 */
typedef struct {
    const char *target_name;    /* Target section name (for sorting) */
    ObjectFile *obj;            /* Source object file */
    LinkerRelaSection *rela_sec;/* Relocation section */
    uint32_t rel_index;         /* Index within rela_sec->relas */
} RelocBatchEntry;

/**
 * Comparison function for sorting relocations by target section name
 */
static int compare_reloc_batch(const void *a, const void *b) {
    const RelocBatchEntry *ea = (const RelocBatchEntry *)a;
    const RelocBatchEntry *eb = (const RelocBatchEntry *)b;
    if (!ea->target_name) return eb->target_name ? 1 : 0;
    if (!eb->target_name) return -1;
    return strcmp(ea->target_name, eb->target_name);
}

/**
 * Create and build section hash table from merged sections
 */
static SectionHashTable* build_section_hash_table(MergedSection *sections, int section_count) {
    SectionHashTable *ht = (SectionHashTable*)calloc(1, sizeof(SectionHashTable));
    if (!ht) {
        fprintf(stderr, "linker: out of memory allocating section hash table\n");
        return NULL;
    }

    /* Initialize hash buckets with small initial capacity */
    for (int i = 0; i < SECTION_HASH_SIZE; i++) {
        ht->bucket_capacities[i] = 4;  /* Start small, will grow if needed */
        ht->buckets[i] = (MergedSection**)malloc(4 * sizeof(MergedSection*));
        if (!ht->buckets[i]) {
            /* Cleanup on error */
            for (int j = 0; j < i; j++) {
                free(ht->buckets[j]);
            }
            free(ht);
            fprintf(stderr, "linker: out of memory allocating section hash buckets\n");
            return NULL;
        }
        ht->bucket_counts[i] = 0;
    }

    /* Build hash table from sections */
    for (int i = 0; i < section_count; i++) {
        MergedSection *sec = &sections[i];
        if (!sec->name) continue;

        /* Calculate hash */
        uint32_t hash = hash_symbol_name(sec->name) % SECTION_HASH_SIZE;

        /* Expand bucket if needed */
        if (ht->bucket_counts[hash] >= ht->bucket_capacities[hash]) {
            int new_cap = ht->bucket_capacities[hash] * 2;
            MergedSection **new_bucket = (MergedSection**)realloc(ht->buckets[hash],
                                                                  new_cap * sizeof(MergedSection*));
            if (!new_bucket) {
                fprintf(stderr, "linker: out of memory expanding section hash bucket\n");
                /* Cleanup and return NULL */
                for (int j = 0; j < SECTION_HASH_SIZE; j++) {
                    free(ht->buckets[j]);
                }
                free(ht);
                return NULL;
            }
            ht->buckets[hash] = new_bucket;
            ht->bucket_capacities[hash] = new_cap;
        }

        /* Add section to bucket */
        ht->buckets[hash][ht->bucket_counts[hash]++] = sec;
    }

    return ht;
}

/**
 * Find section by name using hash table (O(1) average case)
 * Returns NULL if not found
 */
static MergedSection* find_section_in_hash(SectionHashTable *ht, const char *name) {
    if (!ht || !name) return NULL;

    uint32_t hash = hash_symbol_name(name) % SECTION_HASH_SIZE;

    /* Search in hash bucket */
    for (int i = 0; i < ht->bucket_counts[hash]; i++) {
        if (strcmp(ht->buckets[hash][i]->name, name) == 0) {
            return ht->buckets[hash][i];
        }
    }
    return NULL;
}

/**
 * Free section hash table
 */
static void free_section_hash_table(SectionHashTable *ht) {
    if (!ht) return;

    /* Free hash buckets (sections themselves are owned by caller) */
    for (int i = 0; i < SECTION_HASH_SIZE; i++) {
        free(ht->buckets[i]);
    }

    free(ht);
}


/* ========== Module 4: Relocation Processing and ELF Executable Writer ========== */

/**
 * Relocation type definitions for x86-64
 */
#ifndef R_X86_64_64
#define R_X86_64_64       1  /* Direct 64 bit  */
#define R_X86_64_PC32     2  /* PC relative 32 bit signed */
#define R_X86_64_GOT32    3  /* GOT entry offset */
#define R_X86_64_PLT32    4  /* 32 bit PLT address */
#define R_X86_64_COPY     5  /* Copy symbol at runtime (DSO only) */
#define R_X86_64_GLOB_DAT 6  /* GOT entry for symbol */
#define R_X86_64_JUMP_SLOT 7 /* PLT entry for symbol */
#define R_X86_64_RELATIVE 8  /* Relative to load address */
#define R_X86_64_GOTPCREL 9  /* GOT entry PC-relative */
#define R_X86_64_32       10 /* Direct 32 bit zero extended */
#define R_X86_64_32S      11 /* Direct 32 bit sign extended */
#define R_X86_64_16       12 /* Direct 16 bit zero extended */
#define R_X86_64_PC16     13 /* 16-bit PC relative */
#define R_X86_64_8        14 /* Direct 8-bit */
#define R_X86_64_PC8      15 /* 8-bit PC relative */
#define R_X86_64_DTPMOD64 16 /* Module ID (64-bit) */
#define R_X86_64_DTPOFF64 17 /* Offset in TLS block (64-bit) */
#define R_X86_64_TPOFF64  18 /* Offset in initial TLS block (64-bit) */
#define R_X86_64_TLSGD    19 /* TLS General Dynamic */
#define R_X86_64_TLSLD    20 /* TLS Local Dynamic */
#define R_X86_64_DTPOFF32 21 /* Offset in TLS block (32-bit) */
#define R_X86_64_GOTTPOFF 22 /* GOT entry for TLS IE */
#define R_X86_64_TPOFF32  23 /* Offset in initial TLS block (32-bit) */
#define R_X86_64_PC64     24 /* 64-bit PC relative */
#define R_X86_64_GOTOFF64 25 /* Offset from GOT base */
#define R_X86_64_GOTPC32  26 /* 32-bit PC relative to GOT */
#define R_X86_64_SIZE32   32 /* Symbol size (32-bit) */
#define R_X86_64_SIZE64   33 /* Symbol size (64-bit) */
#define R_X86_64_GOTPCRELX 41 /* Load from 32 bit signed PC relative offset to GOT (relaxable) */
#define R_X86_64_REX_GOTPCRELX 42 /* Load from 32 bit signed PC relative offset to GOT (REX relaxable) */
#endif

/**
 * ARM64 (AArch64) relocation type definitions
 */
#ifndef R_AARCH64_NONE
#define R_AARCH64_NONE                    0    /* No relocation */
#define R_AARCH64_ABS64                   257  /* Direct 64 bit */
#define R_AARCH64_ABS32                   258  /* Direct 32 bit */
#define R_AARCH64_ABS16                   259  /* Direct 16 bit */
#define R_AARCH64_PREL64                  260  /* PC-relative 64 bit */
#define R_AARCH64_PREL32                  261  /* PC-relative 32 bit */
#define R_AARCH64_PREL16                  262  /* PC-relative 16 bit */
#define R_AARCH64_CALL26                  283  /* PC-relative call (26-bit) */
#define R_AARCH64_JUMP26                  282  /* PC-relative jump (26-bit) */
#define R_AARCH64_CONDBR19                280  /* PC-relative conditional branch (19-bit) */
#define R_AARCH64_ADR_PREL_LO21           274  /* ADR instruction (21-bit PC-relative) */
#define R_AARCH64_ADR_PREL_PG_HI21        275  /* ADRP instruction (page PC-relative) */
#define R_AARCH64_ADD_ABS_LO12_NC         277  /* ADD immediate (low 12 bits) */
#define R_AARCH64_LDST8_ABS_LO12_NC       278  /* LDR/STR 8-bit (low 12 bits) */
#define R_AARCH64_LDST16_ABS_LO12_NC      284  /* LDR/STR 16-bit (low 12 bits) */
#define R_AARCH64_LDST32_ABS_LO12_NC      285  /* LDR/STR 32-bit (low 12 bits) */
#define R_AARCH64_LDST64_ABS_LO12_NC      286  /* LDR/STR 64-bit (low 12 bits) */
#define R_AARCH64_LDST128_ABS_LO12_NC     299  /* LDR/STR 128-bit (low 12 bits) */
#endif

/**
 * Relocation type definitions for ARM64/AArch64
 */
#ifndef R_AARCH64_ABS64
#define R_AARCH64_ABS64             257  /* Direct 64-bit */
#define R_AARCH64_PREL32            261  /* PC-relative 32-bit */
#define R_AARCH64_CALL26            283  /* Call/jump 26-bit */
#define R_AARCH64_ADR_PREL_PG_HI21  275  /* Page PC-relative */
#define R_AARCH64_ADD_ABS_LO12_NC   277  /* Low 12 bits */
#endif

/**
 * Symbol table structure for Module 4 (adapted to use LinkerSymbolTable from Module 3)
 */
typedef struct LinkerSymbolEntry {
    char *name;
    uint64_t value;      /* Final resolved address */
    uint64_t size;
    uint8_t type;        /* STT_FUNC, STT_OBJECT, etc. */
    uint8_t binding;     /* STB_GLOBAL, STB_WEAK, etc. */
    uint16_t shndx;      /* Section index in merged sections */
} LinkerSymbolEntry;

typedef struct LinkerSymbolTableM4 {
    LinkerSymbolEntry *entries;
    size_t count;
    size_t capacity;
} LinkerSymbolTableM4;

/**
 * Look up symbol in resolved symbol table (Module 4)
 * @return symbol value on success, 0 with error message on failure
 */
static int lookup_symbol_m4(LinkerSymbolTableM4 *symtab, const char *name, uint64_t *value_out) {
    size_t i;

    if (!symtab || !name || !value_out) {
        fprintf(stderr, "linker: internal error: null parameter to lookup_symbol\n");
        return -1;
    }

    for (i = 0; i < symtab->count; i++) {
        if (symtab->entries[i].name && strcmp(symtab->entries[i].name, name) == 0) {
            *value_out = symtab->entries[i].value;
            return 0;
        }
    }

    fprintf(stderr, "linker: undefined reference to '%s'\n", name);
    return -1;
}

/**
 * Check if a signed value fits in 32-bit signed integer
 * Returns: 0 if fits, 1 if overflow (warning), -1 for critical error
 *
 * @param value The computed relocation value
 * @param reloc_type Relocation type name (for diagnostics)
 * @param overflow_list Optional overflow list to record candidates (pass 1)
 * @param symbol_name Symbol name (for overflow tracking)
 * @param symbol_addr Target symbol address
 * @param source_addr Source address (P)
 * @param reloc_offset Relocation offset
 * @param rel_type Relocation type constant
 * @param addend Relocation addend
 * @param target_section Target section pointer
 */
static int check_signed_32bit_with_overflow(int64_t value, const char *reloc_type,
                                           OverflowList *overflow_list,
                                           const char *symbol_name,
                                           uint64_t symbol_addr,
                                           uint64_t source_addr,
                                           uint64_t reloc_offset,
                                           uint32_t rel_type,
                                           int64_t addend,
                                           void *target_section) {
    if (value < INT32_MIN || value > INT32_MAX) {
        /* Calculate overflow amount to help diagnose the issue */
        int64_t overflow_amount = (value > INT32_MAX) ?
                                  (value - INT32_MAX) : (INT32_MIN - value);

        /* If overflow list provided, record this candidate for GOT/PLT */
        if (overflow_list && symbol_name && (rel_type == R_X86_64_PC32 || rel_type == R_X86_64_PLT32)) {
            add_overflow_candidate(overflow_list, symbol_name, symbol_addr,
                                 reloc_offset, rel_type, addend, source_addr,
                                 overflow_amount, target_section);
            fprintf(stderr, "linker: detected %s overflow for '%s' by %ld bytes (will redirect through GOT/PLT)\n",
                    reloc_type, symbol_name, (long)overflow_amount);
        } else {
            fprintf(stderr, "linker: warning: %s overflow by %ld bytes (value 0x%lx, range [0x%x, 0x%x])\n",
                    reloc_type, (long)overflow_amount, (long)value, INT32_MIN, INT32_MAX);
        }
        return 1;  /* Overflow warning, not critical error */
    }
    return 0;
}

/**
 * Check if a signed value fits in 32-bit signed integer (simple version)
 * Returns: 0 if fits, 1 if overflow (warning), -1 for critical error
 */
static int check_signed_32bit(int64_t value, const char *reloc_type) {
    return check_signed_32bit_with_overflow(value, reloc_type, NULL, NULL, 0, 0, 0, 0, 0, NULL);
}

/**
 * Check if an unsigned value fits in 32-bit unsigned integer
 * Returns: 0 if fits, 1 if overflow (warning), -1 for critical error
 */
static int check_unsigned_32bit(uint64_t value, const char *reloc_type) {
    if (value > UINT32_MAX) {
        fprintf(stderr, "linker: warning: relocation %s overflow: value 0x%lx doesn't fit in 32 bits (skipping)\n",
                reloc_type, (unsigned long)value);
        return 1;  /* Overflow warning, not critical error */
    }
    return 0;
}

/**
 * Check if a signed value fits in 16-bit signed integer
 * Returns: 0 if fits, 1 if overflow (warning), -1 for critical error
 */
static int check_signed_16bit(int64_t value, const char *reloc_type) {
    if (value < INT16_MIN || value > INT16_MAX) {
        fprintf(stderr, "linker: warning: relocation %s overflow: value 0x%lx doesn't fit in 16 bits (skipping)\n",
                reloc_type, (long)value);
        return 1;  /* Overflow warning, not critical error */
    }
    return 0;
}

/**
 * Check if an unsigned value fits in 16-bit unsigned integer
 * Returns: 0 if fits, 1 if overflow (warning), -1 for critical error
 */
static int check_unsigned_16bit(uint64_t value, const char *reloc_type) {
    if (value > UINT16_MAX) {
        fprintf(stderr, "linker: warning: relocation %s overflow: value 0x%lx doesn't fit in 16 bits (skipping)\n",
                reloc_type, (unsigned long)value);
        return 1;  /* Overflow warning, not critical error */
    }
    return 0;
}

/**
 * Check if a signed value fits in 8-bit signed integer
 */
static int check_signed_8bit(int64_t value, const char *reloc_type) {
    if (value < INT8_MIN || value > INT8_MAX) {
        fprintf(stderr, "linker: relocation %s overflow: value %ld doesn't fit in 8 bits\n",
                reloc_type, (long)value);
        return -1;
    }
    return 0;
}

/**
 * Check if an unsigned value fits in 8-bit unsigned integer
 */
static int check_unsigned_8bit(uint64_t value, const char *reloc_type) {
    if (value > UINT8_MAX) {
        fprintf(stderr, "linker: relocation %s overflow: value %lu doesn't fit in 8 bits\n",
                reloc_type, (unsigned long)value);
        return -1;
    }
    return 0;
}

/**
 * Get relocation type name for debugging
 * @param type Relocation type
 * @param arch Target architecture
 * @return String name of relocation type
 */
static const char* reloc_type_name(uint32_t type, LinkerArch arch) {
    if (arch == ARCH_X86_64) {
        switch(type) {
            case R_X86_64_64: return "R_X86_64_64";
            case R_X86_64_PC32: return "R_X86_64_PC32";
            case R_X86_64_GOT32: return "R_X86_64_GOT32";
            case R_X86_64_PLT32: return "R_X86_64_PLT32";
            case R_X86_64_COPY: return "R_X86_64_COPY";
            case R_X86_64_GLOB_DAT: return "R_X86_64_GLOB_DAT";
            case R_X86_64_JUMP_SLOT: return "R_X86_64_JUMP_SLOT";
            case R_X86_64_RELATIVE: return "R_X86_64_RELATIVE";
            case R_X86_64_GOTPCREL: return "R_X86_64_GOTPCREL";
            case R_X86_64_32: return "R_X86_64_32";
            case R_X86_64_32S: return "R_X86_64_32S";
            case R_X86_64_16: return "R_X86_64_16";
            case R_X86_64_PC16: return "R_X86_64_PC16";
            case R_X86_64_8: return "R_X86_64_8";
            case R_X86_64_PC8: return "R_X86_64_PC8";
            case R_X86_64_DTPMOD64: return "R_X86_64_DTPMOD64";
            case R_X86_64_DTPOFF64: return "R_X86_64_DTPOFF64";
            case R_X86_64_TPOFF64: return "R_X86_64_TPOFF64";
            case R_X86_64_TLSGD: return "R_X86_64_TLSGD";
            case R_X86_64_TLSLD: return "R_X86_64_TLSLD";
            case R_X86_64_DTPOFF32: return "R_X86_64_DTPOFF32";
            case R_X86_64_GOTTPOFF: return "R_X86_64_GOTTPOFF";
            case R_X86_64_TPOFF32: return "R_X86_64_TPOFF32";
            case R_X86_64_PC64: return "R_X86_64_PC64";
            case R_X86_64_GOTOFF64: return "R_X86_64_GOTOFF64";
            case R_X86_64_GOTPC32: return "R_X86_64_GOTPC32";
            case R_X86_64_SIZE32: return "R_X86_64_SIZE32";
            case R_X86_64_SIZE64: return "R_X86_64_SIZE64";
            case R_X86_64_GOTPCRELX: return "R_X86_64_GOTPCRELX";
            case R_X86_64_REX_GOTPCRELX: return "R_X86_64_REX_GOTPCRELX";
            default: return "R_X86_64_UNKNOWN";
        }
    } else if (arch == ARCH_ARM64) {
        switch(type) {
            case R_AARCH64_NONE: return "R_AARCH64_NONE";
            case R_AARCH64_ABS64: return "R_AARCH64_ABS64";
            case R_AARCH64_ABS32: return "R_AARCH64_ABS32";
            case R_AARCH64_ABS16: return "R_AARCH64_ABS16";
            case R_AARCH64_PREL64: return "R_AARCH64_PREL64";
            case R_AARCH64_PREL32: return "R_AARCH64_PREL32";
            case R_AARCH64_PREL16: return "R_AARCH64_PREL16";
            case R_AARCH64_CALL26: return "R_AARCH64_CALL26";
            case R_AARCH64_JUMP26: return "R_AARCH64_JUMP26";
            case R_AARCH64_CONDBR19: return "R_AARCH64_CONDBR19";
            case R_AARCH64_ADR_PREL_LO21: return "R_AARCH64_ADR_PREL_LO21";
            case R_AARCH64_ADR_PREL_PG_HI21: return "R_AARCH64_ADR_PREL_PG_HI21";
            case R_AARCH64_ADD_ABS_LO12_NC: return "R_AARCH64_ADD_ABS_LO12_NC";
            case R_AARCH64_LDST8_ABS_LO12_NC: return "R_AARCH64_LDST8_ABS_LO12_NC";
            case R_AARCH64_LDST16_ABS_LO12_NC: return "R_AARCH64_LDST16_ABS_LO12_NC";
            case R_AARCH64_LDST32_ABS_LO12_NC: return "R_AARCH64_LDST32_ABS_LO12_NC";
            case R_AARCH64_LDST64_ABS_LO12_NC: return "R_AARCH64_LDST64_ABS_LO12_NC";
            case R_AARCH64_LDST128_ABS_LO12_NC: return "R_AARCH64_LDST128_ABS_LO12_NC";
            default: return "R_AARCH64_UNKNOWN";
        }
    }
    return "UNKNOWN_ARCH";
}

/**
 * Apply x86-64 relocation
 * @param target Pointer to relocation target in section data
 * @param section_size Size of section data buffer
 * @param offset Offset into section data where relocation applies
 * @param rel_type Relocation type (R_X86_64_*)
 * @param symbol_value Value of referenced symbol (S)
 * @param symbol_size Size of referenced symbol
 * @param addend Addend from relocation entry (A)
 * @param target_addr Final virtual address of target location (P)
 * @param base_addr Base load address (B)
 * @param overflow_list Optional overflow list for tracking (Pass 1)
 * @param symbol_name Symbol name (for overflow tracking)
 * @param target_section Target section pointer (for overflow tracking)
 * @return 0 on success, -1 on error, 1 if skipped due to overflow
 */
static int apply_x86_64_relocation(void *target, size_t section_size, uint64_t offset,
                                   uint32_t rel_type, uint64_t symbol_value, uint64_t symbol_size,
                                   int64_t addend, uint64_t target_addr, uint64_t base_addr,
                                   OverflowList *overflow_list, const char *symbol_name,
                                   void *target_section) {
    int64_t result;

    switch (rel_type) {
        case R_X86_64_64:
            /* S + A: Direct 64-bit absolute address */
            if (offset + 8 > section_size) {
                fprintf(stderr, "linker: R_X86_64_64 relocation exceeds section bounds\n");
                return -1;
            }
            result = symbol_value + addend;
            *(uint64_t *)target = (uint64_t)result;
            break;

        case R_X86_64_PC32:
            /* S + A - P: PC-relative 32-bit signed */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_X86_64_PC32 relocation exceeds section bounds\n");
                return -1;
            }
            result = (int64_t)symbol_value + addend - (int64_t)target_addr;
            {
                int check_result = check_signed_32bit_with_overflow(result, "R_X86_64_PC32",
                                                                    overflow_list, symbol_name,
                                                                    symbol_value, target_addr,
                                                                    offset, R_X86_64_PC32,
                                                                    addend, target_section);
                if (check_result > 0) {
                    /* Overflow - skip this relocation (will be handled by GOT/PLT in pass 2) */
                    return 1;  /* Return 1 to indicate skipped relocation */
                } else if (check_result < 0) {
                    return -1;  /* Critical error */
                }
            }
            *(int32_t *)target = (int32_t)result;
            break;

        case R_X86_64_GOT32:
            /* GOT entry offset - for static linking, treat as direct reference */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_X86_64_GOT32 relocation exceeds section bounds\n");
                return -1;
            }
            *(int32_t *)target = (int32_t)(symbol_value + addend);
            break;

        case R_X86_64_PLT32:
            /* S + A - P: Treat as PC32 for static linking (no PLT needed) */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_X86_64_PLT32 relocation exceeds section bounds\n");
                return -1;
            }
            result = (int64_t)symbol_value + addend - (int64_t)target_addr;
            {
                int check_result = check_signed_32bit_with_overflow(result, "R_X86_64_PLT32",
                                                                    overflow_list, symbol_name,
                                                                    symbol_value, target_addr,
                                                                    offset, R_X86_64_PLT32,
                                                                    addend, target_section);
                if (check_result > 0) {
                    /* Overflow - skip this relocation (will be handled by GOT/PLT in pass 2) */
                    return 1;  /* Return 1 to indicate skipped relocation */
                } else if (check_result < 0) {
                    return -1;  /* Critical error */
                }
            }
            *(int32_t *)target = (int32_t)result;
            break;

        case R_X86_64_COPY:
            /* Copy symbol at runtime (DSO only) - skip for static linking */
            break;

        case R_X86_64_GLOB_DAT:
        case R_X86_64_JUMP_SLOT:
            /* GOT/PLT entries - for static linking, treat as direct 64-bit */
            if (offset + 8 > section_size) {
                fprintf(stderr, "linker: R_X86_64_GLOB_DAT/JUMP_SLOT relocation exceeds section bounds\n");
                return -1;
            }
            *(uint64_t *)target = symbol_value;
            break;

        case R_X86_64_RELATIVE:
            /* B + A: Relative to load address */
            if (offset + 8 > section_size) {
                fprintf(stderr, "linker: R_X86_64_RELATIVE relocation exceeds section bounds\n");
                return -1;
            }
            *(uint64_t *)target = base_addr + addend;
            break;

        case R_X86_64_GOTPCREL:
            /* GOT entry PC-relative - for static linking, convert to PC-relative */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_X86_64_GOTPCREL relocation exceeds section bounds\n");
                return -1;
            }
            result = (int64_t)(symbol_value + addend) - (int64_t)target_addr;
            {
                int check_result = check_signed_32bit(result, "R_X86_64_GOTPCREL");
                if (check_result > 0) {
                    /* Overflow - skip this relocation with warning */
                    return 1;  /* Return 1 to indicate skipped relocation */
                } else if (check_result < 0) {
                    return -1;  /* Critical error */
                }
            }
            *(int32_t *)target = (int32_t)result;
            break;

        case R_X86_64_32:
            /* S + A: Direct 32-bit zero-extended
             * NOTE: For Cosmopolitan compatibility, we also accept signed 32-bit values
             * that can be represented in 32 bits when sign-extended.
             */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_X86_64_32 relocation exceeds section bounds\n");
                return -1;
            }
            result = symbol_value + addend;
            /* Check if value fits in unsigned 32-bit */
            if ((uint64_t)result > UINT32_MAX) {
                /* Doesn't fit as unsigned - try signed 32-bit (Cosmopolitan compatibility) */
                if (result >= INT32_MIN && result <= INT32_MAX) {
                    /* Can represent as signed 32-bit - allow it */
                    LOG_DEBUG_MSG("R_X86_64_32: accepting signed 32-bit value 0x%lx (%ld)",
                                  (uint64_t)result, result);
                } else {
                    /* Genuine overflow - cannot fit in 32 bits either way */
                    fprintf(stderr, "linker: warning: relocation R_X86_64_32 overflow: value 0x%lx doesn't fit in 32 bits (skipping)\n",
                            (uint64_t)result);
                    return 1;  /* Skip this relocation */
                }
            }
            *(uint32_t *)target = (uint32_t)result;
            break;

        case R_X86_64_32S:
            /* S + A: Direct 32-bit sign-extended */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_X86_64_32S relocation exceeds section bounds\n");
                return -1;
            }
            result = (int64_t)symbol_value + addend;
            {
                int check_result = check_signed_32bit(result, "R_X86_64_32S");
                if (check_result > 0) {
                    /* Overflow - skip this relocation with warning */
                    return 1;  /* Return 1 to indicate skipped relocation */
                } else if (check_result < 0) {
                    return -1;  /* Critical error */
                }
            }
            *(int32_t *)target = (int32_t)result;
            break;

        case R_X86_64_16:
            /* Direct 16-bit zero extended */
            if (offset + 2 > section_size) {
                fprintf(stderr, "linker: R_X86_64_16 relocation exceeds section bounds\n");
                return -1;
            }
            result = symbol_value + addend;
            {
                int check_result = check_unsigned_16bit((uint64_t)result, "R_X86_64_16");
                if (check_result > 0) {
                    /* Overflow - skip this relocation with warning */
                    return 1;  /* Return 1 to indicate skipped relocation */
                } else if (check_result < 0) {
                    return -1;  /* Critical error */
                }
            }
            *(uint16_t *)target = (uint16_t)result;
            break;

        case R_X86_64_PC16:
            /* 16-bit PC relative */
            if (offset + 2 > section_size) {
                fprintf(stderr, "linker: R_X86_64_PC16 relocation exceeds section bounds\n");
                return -1;
            }
            result = (int64_t)symbol_value + addend - (int64_t)target_addr;
            {
                int check_result = check_signed_16bit(result, "R_X86_64_PC16");
                if (check_result > 0) {
                    /* Overflow - skip this relocation with warning */
                    return 1;  /* Return 1 to indicate skipped relocation */
                } else if (check_result < 0) {
                    return -1;  /* Critical error */
                }
            }
            *(int16_t *)target = (int16_t)result;
            break;

        case R_X86_64_8:
            /* Direct 8-bit */
            if (offset + 1 > section_size) {
                fprintf(stderr, "linker: R_X86_64_8 relocation exceeds section bounds\n");
                return -1;
            }
            result = symbol_value + addend;
            if (check_unsigned_8bit((uint64_t)result, "R_X86_64_8") < 0) {
                return -1;
            }
            *(uint8_t *)target = (uint8_t)result;
            break;

        case R_X86_64_PC8:
            /* 8-bit PC relative */
            if (offset + 1 > section_size) {
                fprintf(stderr, "linker: R_X86_64_PC8 relocation exceeds section bounds\n");
                return -1;
            }
            result = (int64_t)symbol_value + addend - (int64_t)target_addr;
            if (check_signed_8bit(result, "R_X86_64_PC8") < 0) {
                return -1;
            }
            *(int8_t *)target = (int8_t)result;
            break;

        /* TLS relocations - skip for static linking */
        case R_X86_64_DTPMOD64:
        case R_X86_64_DTPOFF64:
        case R_X86_64_TPOFF64:
        case R_X86_64_TLSGD:
        case R_X86_64_TLSLD:
        case R_X86_64_DTPOFF32:
        case R_X86_64_GOTTPOFF:
        case R_X86_64_TPOFF32:
            /* TLS relocations are not supported in static linking - skip with warning */
            fprintf(stderr, "linker: warning: skipping TLS relocation %s (not supported in static linking)\n",
                    reloc_type_name(rel_type, ARCH_X86_64));
            return 1;  /* Skip this relocation */

        case R_X86_64_PC64:
            /* 64-bit PC relative */
            if (offset + 8 > section_size) {
                fprintf(stderr, "linker: R_X86_64_PC64 relocation exceeds section bounds\n");
                return -1;
            }
            *(int64_t *)target = (int64_t)(symbol_value + addend) - (int64_t)target_addr;
            break;

        case R_X86_64_GOTOFF64:
            /* Offset from GOT base - for static linking, treat as direct reference */
            if (offset + 8 > section_size) {
                fprintf(stderr, "linker: R_X86_64_GOTOFF64 relocation exceeds section bounds\n");
                return -1;
            }
            *(int64_t *)target = symbol_value + addend;
            break;

        case R_X86_64_SIZE32:
            /* Symbol size (32-bit) */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_X86_64_SIZE32 relocation exceeds section bounds\n");
                return -1;
            }
            *(uint32_t *)target = (uint32_t)symbol_size;
            break;

        case R_X86_64_SIZE64:
            /* Symbol size (64-bit) */
            if (offset + 8 > section_size) {
                fprintf(stderr, "linker: R_X86_64_SIZE64 relocation exceeds section bounds\n");
                return -1;
            }
            *(uint64_t *)target = symbol_size;
            break;

        case R_X86_64_GOTPCRELX:
        case R_X86_64_REX_GOTPCRELX:
            /* Optimized GOTPCREL - for static linking, treat as PC-relative */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_X86_64_GOTPCRELX/REX_GOTPCRELX relocation exceeds section bounds\n");
                return -1;
            }
            result = (int64_t)(symbol_value + addend) - (int64_t)target_addr;
            {
                int check_result = check_signed_32bit(result, reloc_type_name(rel_type, ARCH_X86_64));
                if (check_result > 0) {
                    /* Overflow - skip this relocation with warning */
                    return 1;  /* Return 1 to indicate skipped relocation */
                } else if (check_result < 0) {
                    return -1;  /* Critical error */
                }
            }
            *(int32_t *)target = (int32_t)result;
            break;

        default:
            fprintf(stderr, "linker: unsupported x86-64 relocation type %u (%s)\n",
                    rel_type, reloc_type_name(rel_type, ARCH_X86_64));
            return -1;
    }

    return 0;
}

/**
 * Apply ARM64 (AArch64) relocation
 * @param target Pointer to relocation target in section data
 * @param section_size Size of section data buffer
 * @param offset Offset into section data where relocation applies
 * @param rel_type Relocation type (R_AARCH64_*)
 * @param symbol_value Value of referenced symbol (S)
 * @param symbol_size Size of referenced symbol
 * @param addend Addend from relocation entry (A)
 * @param target_addr Final virtual address of target location (P)
 * @param base_addr Base load address (B)
 * @return 0 on success, -1 on error
 */
static int apply_arm64_relocation(void *target, size_t section_size, uint64_t offset,
                                  uint32_t rel_type, uint64_t symbol_value, uint64_t symbol_size,
                                  int64_t addend, uint64_t target_addr, uint64_t base_addr) {
    int64_t result;
    uint32_t insn;

    (void)symbol_size;  /* Unused for ARM64 relocations */
    (void)base_addr;

    switch (rel_type) {
        case R_AARCH64_NONE:
            /* No relocation */
            break;

        case R_AARCH64_ABS64:
            /* S + A: Direct 64-bit */
            if (offset + 8 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_ABS64 relocation exceeds section bounds\n");
                return -1;
            }
            *(uint64_t *)target = symbol_value + addend;
            break;

        case R_AARCH64_ABS32:
            /* S + A: Direct 32-bit */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_ABS32 relocation exceeds section bounds\n");
                return -1;
            }
            result = symbol_value + addend;
            if (check_unsigned_32bit((uint64_t)result, "R_AARCH64_ABS32") < 0) {
                return -1;
            }
            *(uint32_t *)target = (uint32_t)result;
            break;

        case R_AARCH64_ABS16:
            /* S + A: Direct 16-bit */
            if (offset + 2 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_ABS16 relocation exceeds section bounds\n");
                return -1;
            }
            result = symbol_value + addend;
            if (check_unsigned_16bit((uint64_t)result, "R_AARCH64_ABS16") < 0) {
                return -1;
            }
            *(uint16_t *)target = (uint16_t)result;
            break;

        case R_AARCH64_PREL64:
            /* S + A - P: PC-relative 64-bit */
            if (offset + 8 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_PREL64 relocation exceeds section bounds\n");
                return -1;
            }
            *(int64_t *)target = (int64_t)(symbol_value + addend) - (int64_t)target_addr;
            break;

        case R_AARCH64_PREL32:
            /* S + A - P: PC-relative 32-bit */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_PREL32 relocation exceeds section bounds\n");
                return -1;
            }
            result = (int64_t)(symbol_value + addend) - (int64_t)target_addr;
            if (check_signed_32bit(result, "R_AARCH64_PREL32") < 0) {
                return -1;
            }
            *(int32_t *)target = (int32_t)result;
            break;

        case R_AARCH64_PREL16:
            /* S + A - P: PC-relative 16-bit */
            if (offset + 2 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_PREL16 relocation exceeds section bounds\n");
                return -1;
            }
            result = (int64_t)(symbol_value + addend) - (int64_t)target_addr;
            if (check_signed_16bit(result, "R_AARCH64_PREL16") < 0) {
                return -1;
            }
            *(int16_t *)target = (int16_t)result;
            break;

        case R_AARCH64_CALL26:
        case R_AARCH64_JUMP26:
            /* PC-relative call/jump (26-bit): (S + A - P) >> 2 */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_CALL26/JUMP26 relocation exceeds section bounds\n");
                return -1;
            }
            result = (int64_t)(symbol_value + addend) - (int64_t)target_addr;
            /* Check 26-bit range: ±128MB */
            if (result < -0x8000000LL || result > 0x7FFFFFFLL) {
                fprintf(stderr, "linker: R_AARCH64_CALL26/JUMP26 overflow: offset %ld out of range\n", (long)result);
                return -1;
            }
            insn = *(uint32_t *)target;
            insn = (insn & 0xFC000000) | ((result >> 2) & 0x03FFFFFF);
            *(uint32_t *)target = insn;
            break;

        case R_AARCH64_CONDBR19:
            /* PC-relative conditional branch (19-bit): (S + A - P) >> 2 */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_CONDBR19 relocation exceeds section bounds\n");
                return -1;
            }
            result = (int64_t)(symbol_value + addend) - (int64_t)target_addr;
            /* Check 19-bit range: ±1MB */
            if (result < -0x100000LL || result > 0xFFFFFLL) {
                fprintf(stderr, "linker: R_AARCH64_CONDBR19 overflow: offset %ld out of range\n", (long)result);
                return -1;
            }
            insn = *(uint32_t *)target;
            insn = (insn & 0xFF00001F) | (((result >> 2) & 0x7FFFF) << 5);
            *(uint32_t *)target = insn;
            break;

        case R_AARCH64_ADR_PREL_LO21:
            /* ADR instruction (21-bit PC-relative): S + A - P */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_ADR_PREL_LO21 relocation exceeds section bounds\n");
                return -1;
            }
            result = (int64_t)(symbol_value + addend) - (int64_t)target_addr;
            /* Check 21-bit range: ±1MB */
            if (result < -0x100000LL || result > 0xFFFFFLL) {
                fprintf(stderr, "linker: R_AARCH64_ADR_PREL_LO21 overflow: offset %ld out of range\n", (long)result);
                return -1;
            }
            insn = *(uint32_t *)target;
            /* Encode immlo[1:0] in bits [30:29] and immhi[20:2] in bits [23:5] */
            insn = (insn & 0x9F00001F) | ((result & 0x3) << 29) | (((result >> 2) & 0x7FFFF) << 5);
            *(uint32_t *)target = insn;
            break;

        case R_AARCH64_ADR_PREL_PG_HI21:
            /* ADRP instruction: Page(S+A) - Page(P) */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_ADR_PREL_PG_HI21 relocation exceeds section bounds\n");
                return -1;
            }
            {
                uint64_t page_s = (symbol_value + addend) & ~0xFFFULL;
                uint64_t page_p = target_addr & ~0xFFFULL;
                int64_t delta = (int64_t)(page_s - page_p);

                /* Encode into ADRP: immhi[20:12] and immlo[1:0] */
                insn = *(uint32_t *)target;
                uint32_t imm = ((delta >> 12) & 0x1FFFFF);
                insn = (insn & 0x9F00001F) | ((imm & 0x3) << 29) | (((imm >> 2) & 0x7FFFF) << 5);
                *(uint32_t *)target = insn;
            }
            break;

        case R_AARCH64_ADD_ABS_LO12_NC:
            /* ADD immediate: Low 12 bits of (S + A) */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_ADD_ABS_LO12_NC relocation exceeds section bounds\n");
                return -1;
            }
            {
                uint32_t imm = (symbol_value + addend) & 0xFFF;
                insn = *(uint32_t *)target;
                insn = (insn & 0xFFC003FF) | (imm << 10);
                *(uint32_t *)target = insn;
            }
            break;

        case R_AARCH64_LDST8_ABS_LO12_NC:
            /* LDR/STR 8-bit: Low 12 bits */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_LDST8_ABS_LO12_NC relocation exceeds section bounds\n");
                return -1;
            }
            {
                uint32_t imm = (symbol_value + addend) & 0xFFF;
                insn = *(uint32_t *)target;
                insn = (insn & 0xFFC003FF) | (imm << 10);
                *(uint32_t *)target = insn;
            }
            break;

        case R_AARCH64_LDST16_ABS_LO12_NC:
            /* LDR/STR 16-bit: Low 12 bits >> 1 */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_LDST16_ABS_LO12_NC relocation exceeds section bounds\n");
                return -1;
            }
            {
                uint32_t imm = ((symbol_value + addend) & 0xFFF) >> 1;
                insn = *(uint32_t *)target;
                insn = (insn & 0xFFC003FF) | (imm << 10);
                *(uint32_t *)target = insn;
            }
            break;

        case R_AARCH64_LDST32_ABS_LO12_NC:
            /* LDR/STR 32-bit: Low 12 bits >> 2 */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_LDST32_ABS_LO12_NC relocation exceeds section bounds\n");
                return -1;
            }
            {
                uint32_t imm = ((symbol_value + addend) & 0xFFF) >> 2;
                insn = *(uint32_t *)target;
                insn = (insn & 0xFFC003FF) | (imm << 10);
                *(uint32_t *)target = insn;
            }
            break;

        case R_AARCH64_LDST64_ABS_LO12_NC:
            /* LDR/STR 64-bit: Low 12 bits >> 3 */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_LDST64_ABS_LO12_NC relocation exceeds section bounds\n");
                return -1;
            }
            {
                uint32_t imm = ((symbol_value + addend) & 0xFFF) >> 3;
                insn = *(uint32_t *)target;
                insn = (insn & 0xFFC003FF) | (imm << 10);
                *(uint32_t *)target = insn;
            }
            break;

        case R_AARCH64_LDST128_ABS_LO12_NC:
            /* LDR/STR 128-bit: Low 12 bits >> 4 */
            if (offset + 4 > section_size) {
                fprintf(stderr, "linker: R_AARCH64_LDST128_ABS_LO12_NC relocation exceeds section bounds\n");
                return -1;
            }
            {
                uint32_t imm = ((symbol_value + addend) & 0xFFF) >> 4;
                insn = *(uint32_t *)target;
                insn = (insn & 0xFFC003FF) | (imm << 10);
                *(uint32_t *)target = insn;
            }
            break;

        default:
            fprintf(stderr, "linker: unsupported ARM64 relocation type %u (%s)\n",
                    rel_type, reloc_type_name(rel_type, ARCH_ARM64));
            return -1;
    }

    return 0;
}

/**
 * Apply a single relocation to section data (architecture dispatcher)
 * @param section_data Pointer to section data buffer
 * @param section_size Size of section data buffer
 * @param offset Offset into section data where relocation applies
 * @param rel_type Relocation type (R_X86_64_* or R_AARCH64_*)
 * @param symbol_value Value of referenced symbol (S)
 * @param addend Addend from relocation entry (A)
 * @param target_addr Final virtual address of target location (P)
 * @param overflow_list Optional overflow list for tracking (Pass 1)
 * @param symbol_name Symbol name (for overflow tracking)
 * @param target_section Target section pointer (for overflow tracking)
 * @return 0 on success, -1 on error, 1 if skipped due to overflow
 */
static int apply_single_relocation(void *section_data, size_t section_size, uint64_t offset,
                                   uint32_t rel_type, uint64_t symbol_value, int64_t addend,
                                   uint64_t target_addr, OverflowList *overflow_list,
                                   const char *symbol_name, void *target_section) {
    /* Bounds check */
    if (offset >= section_size) {
        fprintf(stderr, "linker: Relocation offset %lu exceeds section size %lu\n",
                offset, section_size);
        return -1;
    }

    void *target = (char *)section_data + offset;

    /* Determine architecture based on relocation type range */
    LinkerArch arch;
    if (rel_type <= 50) {
        /* x86-64 relocation types are typically 0-50 */
        arch = ARCH_X86_64;
    } else if (rel_type >= 250 && rel_type <= 400) {
        /* ARM64 relocation types are typically 250-400 */
        arch = ARCH_ARM64;
    } else {
        fprintf(stderr, "linker: cannot determine architecture for relocation type %u\n", rel_type);
        return -1;
    }

    /* Default values for extended parameters */
    uint64_t symbol_size = 0;  /* Symbol size not available in this context */
    uint64_t base_addr = 0x400000;  /* Default base load address */

    /* Dispatch to architecture-specific handler */
    if (arch == ARCH_X86_64) {
        return apply_x86_64_relocation(target, section_size, offset, rel_type,
                                       symbol_value, symbol_size, addend,
                                       target_addr, base_addr, overflow_list,
                                       symbol_name, target_section);
    } else if (arch == ARCH_ARM64) {
        return apply_arm64_relocation(target, section_size, offset, rel_type,
                                      symbol_value, symbol_size, addend,
                                      target_addr, base_addr);
    }

    fprintf(stderr, "linker: unsupported architecture\n");
    return -1;
}

/**
 * Create PT_LOAD program header for a section
 */
static void create_phdr_for_section(Elf64_Phdr *phdr, MergedSection *sec) {
    phdr->p_type = PT_LOAD;
    phdr->p_flags = 0;

    /* Set flags based on section attributes */
    if (sec->flags & SHF_EXECINSTR) phdr->p_flags |= PF_X;  /* Executable */
    if (sec->flags & SHF_WRITE)     phdr->p_flags |= PF_W;  /* Writable */
    if (sec->flags & SHF_ALLOC)     phdr->p_flags |= PF_R;  /* Readable */

    phdr->p_offset = 0;  /* Will be filled by caller */
    phdr->p_vaddr = sec->vma;
    phdr->p_paddr = sec->vma;  /* Physical address = virtual address */
    phdr->p_filesz = sec->size;  /* For simplicity, treat all as having file size */
    phdr->p_memsz = sec->size;
    phdr->p_align = sec->alignment;
}

/**
 * Write ELF64 executable file
 * @param output Output file path
 * @param sections Array of merged sections
 * @param section_count Number of sections
 * @param phdrs_in Program headers (if NULL, will be created from sections)
 * @param phdr_count_in Number of program headers
 * @param entry_point Entry point address
 * @return 0 on success, -1 on error
 */
static int write_elf64_executable_m4(const char *output, MergedSection *sections,
                                     size_t section_count, Elf64_Phdr *phdrs_in,
                                     int phdr_count_in, uint64_t entry_point,
                                     LinkerArch arch) {
    FILE *out = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr *phdrs = NULL;
    size_t phdr_count = 0;
    uint64_t entry_addr = entry_point;
    int result = -1;
    size_t i;
    int allocated_phdrs = 0;

    if (!output || !sections) {
        fprintf(stderr, "linker: internal error: null parameter to write_elf64_executable\n");
        return -1;
    }

    /* Use provided program headers or create new ones */
    if (phdrs_in && phdr_count_in > 0) {
        phdrs = phdrs_in;
        phdr_count = phdr_count_in;
    } else {
        /* Allocate program headers (one per loadable section) */
        phdrs = calloc(section_count, sizeof(Elf64_Phdr));
        if (!phdrs) {
            fprintf(stderr, "linker: out of memory\n");
            return -1;
        }
        allocated_phdrs = 1;

        /* Create program headers for loadable sections */
        for (i = 0; i < section_count; i++) {
            if (sections[i].flags & SHF_ALLOC) {
                create_phdr_for_section(&phdrs[phdr_count], &sections[i]);
                phdr_count++;
            }
        }
    }

    /* Initialize ELF header */
    memset(&ehdr, 0, sizeof(ehdr));

    /* ELF magic and identification */
    ehdr.e_ident[EI_MAG0] = ELFMAG0;
    ehdr.e_ident[EI_MAG1] = ELFMAG1;
    ehdr.e_ident[EI_MAG2] = ELFMAG2;
    ehdr.e_ident[EI_MAG3] = ELFMAG3;
    ehdr.e_ident[EI_CLASS] = ELFCLASS64;
    ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
    ehdr.e_ident[EI_ABIVERSION] = 0;

    /* ELF header fields */
    ehdr.e_type = ET_EXEC;               /* Executable file */
    ehdr.e_machine = (arch == ARCH_ARM64) ? EM_AARCH64 : EM_X86_64;  /* Architecture */
    ehdr.e_version = EV_CURRENT;
    ehdr.e_entry = entry_addr;           /* Entry point address */
    ehdr.e_phoff = sizeof(Elf64_Ehdr);   /* Program header offset */
    ehdr.e_shoff = 0;                    /* Section header offset (optional, set to 0) */
    ehdr.e_flags = 0;
    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum = phdr_count;
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    ehdr.e_shnum = 0;                    /* No section headers for now */
    ehdr.e_shstrndx = 0;

    /* Validate ELF header fields */
    if (entry_addr == 0) {
        fprintf(stderr, "linker: error: entry point is NULL\n");
        goto cleanup;
    }

    if (phdr_count == 0) {
        fprintf(stderr, "linker: error: no program headers\n");
        goto cleanup;
    }

    /* Verify entry point is within a PT_LOAD segment */
    int entry_in_segment = 0;
    for (i = 0; i < phdr_count; i++) {
        if (phdrs[i].p_type == PT_LOAD &&
            entry_addr >= phdrs[i].p_vaddr &&
            entry_addr < phdrs[i].p_vaddr + phdrs[i].p_memsz) {
            entry_in_segment = 1;
            /* Verify it's in an executable segment (.text) */
            if (!(phdrs[i].p_flags & PF_X)) {
                fprintf(stderr, "linker: warning: _start is in non-executable segment\n");
            }
            break;
        }
    }

    if (!entry_in_segment) {
        fprintf(stderr, "linker: warning: _start (0x%lx) not within any PT_LOAD segment\n",
                entry_addr);
        /* Continue anyway - might work if address is valid */
    }

    /* Open output file */
    timer_record("Phase 6.1: Open file");
    out = fopen(output, "wb");
    if (!out) {
        fprintf(stderr, "linker: cannot create output file '%s': %s\n",
               output, strerror(errno));
        goto cleanup;
    }

    /* Set large buffer for improved I/O performance (1MB) */
    static char io_buffer[1024 * 1024];
    if (setvbuf(out, io_buffer, _IOFBF, sizeof(io_buffer)) != 0) {
        fprintf(stderr, "linker: warning: failed to set large buffer\n");
        /* Continue with default buffering */
    }

    /* Write ELF header */
    timer_record("Phase 6.2: Write ELF header");
    if (fwrite(&ehdr, sizeof(ehdr), 1, out) != 1) {
        fprintf(stderr, "linker: failed to write ELF header\n");
        goto cleanup;
    }

    /* Write program headers */
    timer_record("Phase 6.2.1: Write program headers");
    if (fwrite(phdrs, sizeof(Elf64_Phdr), phdr_count, out) != phdr_count) {
        fprintf(stderr, "linker: failed to write program headers\n");
        goto cleanup;
    }

    /* Write section data */
    timer_record("Phase 6.3: Write sections");
    for (i = 0; i < section_count; i++) {
        if (!(sections[i].flags & SHF_ALLOC)) {
            /* Non-loadable section: skip */
            continue;
        }

        /* Write section data if present */
        if (sections[i].data && sections[i].size > 0) {
            if (fwrite(sections[i].data, 1, sections[i].size, out) != sections[i].size) {
                fprintf(stderr, "linker: failed to write section %s\n", sections[i].name);
                goto cleanup;
            }
        }
    }

    /* Close file (flushes buffer) */
    timer_record("Phase 6.3.1: Flush and close");
    fclose(out);
    out = NULL;

    /* Set executable permissions (chmod 0755) */
    if (chmod(output, 0755) != 0) {
        fprintf(stderr, "linker: warning: failed to set executable permissions on '%s'\n", output);
        /* Not a fatal error */
    }

    result = 0;

cleanup:
    if (out) fclose(out);
    if (allocated_phdrs && phdrs) free(phdrs);

    return result;
}


/* ========== Dead Code Elimination (--gc-sections) ========== */

/**
 * Simple queue for BFS traversal of symbol dependency graph
 */
typedef struct SymbolQueue {
    int *obj_indices;      /* Queue of object file indices */
    int head;              /* Head of queue (dequeue position) */
    int tail;              /* Tail of queue (enqueue position) */
    int capacity;          /* Queue capacity */
} SymbolQueue;

/**
 * Create a new symbol queue for BFS
 */
static SymbolQueue* create_symbol_queue(int capacity) {
    SymbolQueue *q = (SymbolQueue*)malloc(sizeof(SymbolQueue));
    if (!q) return NULL;

    q->obj_indices = (int*)calloc((size_t)capacity, sizeof(int));
    if (!q->obj_indices) {
        free(q);
        return NULL;
    }

    q->head = 0;
    q->tail = 0;
    q->capacity = capacity;
    return q;
}

/**
 * Enqueue an object index
 */
static void enqueue(SymbolQueue *q, int obj_idx) {
    if (q->tail < q->capacity) {
        q->obj_indices[q->tail++] = obj_idx;
    }
}

/**
 * Dequeue an object index
 */
static int dequeue(SymbolQueue *q) {
    if (q->head >= q->tail) {
        return -1;  /* Queue empty */
    }
    return q->obj_indices[q->head++];
}

/**
 * Check if queue is empty
 */
static int queue_empty(SymbolQueue *q) {
    return (q->head >= q->tail);
}

/**
 * Free symbol queue
 */
static void free_symbol_queue(SymbolQueue *q) {
    if (!q) return;
    free(q->obj_indices);
    free(q);
}

/**
 * Find object file index that defines a given symbol
 * @return Object index if found, -1 otherwise
 */
static int find_defining_object(ObjectFile **obj_files, int count, const char *symbol_name) {
    for (int i = 0; i < count; i++) {
        ObjectFile *obj = obj_files[i];
        for (uint32_t j = 0; j < obj->symbol_count; j++) {
            LinkerSymbol *sym = &obj->symbols[j];
            /* Symbol must be defined (not UNDEF) and match name */
            if (sym->shndx != SHN_UNDEF && sym->name &&
                strcmp(sym->name, symbol_name) == 0) {
                return i;  /* Found defining object */
            }
        }
    }
    return -1;  /* Not found */
}

/**
 * Mark symbols that are actually used (reachable from entry point)
 * Uses BFS to traverse symbol dependency graph through relocations
 * @param obj_files Array of object files
 * @param count Number of object files
 * @param entry_symbol Entry point symbol name (e.g., "_start")
 * @return Number of objects marked as used
 */
static int mark_used_symbols(ObjectFile **obj_files, int count, const char *entry_symbol) {
    if (!obj_files || count <= 0) return 0;

    /* Create BFS queue */
    SymbolQueue *queue = create_symbol_queue(count * 2);
    if (!queue) {
        fprintf(stderr, "linker: gc-sections: out of memory\n");
        return count;  /* Fail-safe: mark all as used */
    }

    /* Initialize all objects as unused */
    for (int i = 0; i < count; i++) {
        obj_files[i]->used = 0;
    }

    /* Force mark critical runtime objects as used (cannot be garbage collected)
     * These provide essential runtime symbols like __hostos, __oldstack, cosmo, etc.
     * Runtime initialization depends on these even if not directly referenced in user code */
    for (int i = 0; i < count; i++) {
        ObjectFile *obj = obj_files[i];
        if (!obj || !obj->filename) continue;

        const char *basename = strrchr(obj->filename, '/');
        basename = basename ? basename + 1 : obj->filename;

        /* Critical runtime objects that must always be included */
        if (strcmp(basename, "crt.o") == 0 ||
            strcmp(basename, "ape.o") == 0 ||
            strcmp(basename, "ape-no-modify-self.o") == 0 ||
            strcmp(basename, "hostos.o") == 0 ||
            strcmp(basename, "envp.o") == 0 ||
            strcmp(basename, "oldstack.o") == 0 ||
            strcmp(basename, "kstarttsc.o") == 0 ||
            strcmp(basename, "program_executable_name.o") == 0 ||
            strcmp(basename, "program_executable_name_init.o") == 0) {
            obj_files[i]->used = 1;
            enqueue(queue, i);
        }
    }

    /* Find entry point and mark it */
    int entry_obj = find_defining_object(obj_files, count, entry_symbol);
    if (entry_obj < 0) {
        /* Try alternate entry point "main" */
        entry_obj = find_defining_object(obj_files, count, "main");
        if (entry_obj < 0) {
            fprintf(stderr, "linker: gc-sections: warning: entry point '%s' not found, keeping all objects\n",
                    entry_symbol);
            free_symbol_queue(queue);
            /* Mark all as used if no entry point found */
            for (int i = 0; i < count; i++) {
                obj_files[i]->used = 1;
            }
            return count;
        }
    }

    /* Mark entry point object and enqueue */
    obj_files[entry_obj]->used = 1;
    enqueue(queue, entry_obj);

    /* BFS traversal: process dependencies through relocations */
    while (!queue_empty(queue)) {
        int obj_idx = dequeue(queue);
        if (obj_idx < 0 || obj_idx >= count) continue;

        ObjectFile *obj = obj_files[obj_idx];

        /* Process all relocation sections in this object */
        for (uint32_t r = 0; r < obj->rela_count; r++) {
            LinkerRelaSection *rela_sec = &obj->rela_sections[r];

            /* Process each relocation */
            for (uint32_t i = 0; i < rela_sec->count; i++) {
                LinkerRelocation *rel = &rela_sec->relas[i];

                /* Get symbol referenced by this relocation */
                if (rel->symbol >= obj->symbol_count) continue;
                LinkerSymbol *ref_sym = &obj->symbols[rel->symbol];

                /* Skip if symbol is defined in same object (local reference) */
                if (ref_sym->shndx != SHN_UNDEF) continue;

                /* Find which object defines this symbol */
                int def_obj = find_defining_object(obj_files, count, ref_sym->name);
                if (def_obj >= 0 && !obj_files[def_obj]->used) {
                    /* Mark as used and enqueue for processing */
                    obj_files[def_obj]->used = 1;
                    enqueue(queue, def_obj);
                }
            }
        }
    }

    /* Count used objects */
    int used_count = 0;
    for (int i = 0; i < count; i++) {
        if (obj_files[i]->used) {
            used_count++;
        }
    }

    free_symbol_queue(queue);
    return used_count;
}


/* ========== Complete Linker Pipeline Integration ========== */

/**
 * Relocation statistics tracking
 */
typedef struct {
    int total_relocs;       /* Total relocations processed */
    int skipped_relocs;     /* Relocations skipped due to overflow */
    int failed_relocs;      /* Relocations that failed critically */
} RelocationStats;

/**
 * Relocation debug info for --dump-relocations
 */
typedef struct {
    uint64_t offset;
    int type;
    const char *symbol_name;
    uint64_t value;
    int status;  /* 0=applied, 1=skipped, -1=failed */
} RelocationDebugInfo;

static RelocationDebugInfo *g_reloc_debug = NULL;
static int g_reloc_debug_count = 0;
static int g_reloc_debug_capacity = 0;

/**
 * Record relocation for debugging
 */
static void record_relocation(uint64_t offset, int type, const char *symbol_name,
                              uint64_t value, int status) {
    if (!g_dump_relocations) return;

    /* Expand array if needed */
    if (g_reloc_debug_count >= g_reloc_debug_capacity) {
        g_reloc_debug_capacity = (g_reloc_debug_capacity == 0) ? 1024 : g_reloc_debug_capacity * 2;
        g_reloc_debug = realloc(g_reloc_debug, g_reloc_debug_capacity * sizeof(RelocationDebugInfo));
        if (!g_reloc_debug) {
            fprintf(stderr, "linker: warning: failed to allocate relocation debug buffer\n");
            return;
        }
    }

    /* Record relocation */
    g_reloc_debug[g_reloc_debug_count++] = (RelocationDebugInfo){
        .offset = offset,
        .type = type,
        .symbol_name = symbol_name ? symbol_name : "(null)",
        .value = value,
        .status = status
    };
}

/**
 * Dump relocations for debugging
 */
static void dump_relocations(void) {
    if (!g_dump_relocations || g_reloc_debug_count == 0) return;

    printf("\n=== Relocation Dump ===\n");
    printf("%-12s %-18s %-24s %-14s %s\n", "Offset", "Type", "Symbol", "Value", "Status");
    printf("--------------------------------------------------------------------------------\n");

    int applied = 0, skipped = 0, failed = 0;

    for (int i = 0; i < g_reloc_debug_count; i++) {
        RelocationDebugInfo *r = &g_reloc_debug[i];

        const char *type_str;
        switch (r->type) {
            case 1: type_str = "R_X86_64_64"; break;
            case 2: type_str = "R_X86_64_PC32"; break;
            case 10: type_str = "R_X86_64_32"; break;
            case 11: type_str = "R_X86_64_32S"; break;
            case 257: type_str = "R_AARCH64_ABS64"; break;
            case 259: type_str = "R_AARCH64_PREL32"; break;
            default: type_str = "UNKNOWN"; break;
        }

        const char *status_str;
        if (r->status == 0) {
            status_str = "APPLIED";
            applied++;
        } else if (r->status > 0) {
            status_str = "SKIPPED (overflow)";
            skipped++;
        } else {
            status_str = "FAILED";
            failed++;
        }

        printf("0x%-10lx %-18s %-24s 0x%-12lx %s\n",
               r->offset, type_str, r->symbol_name, r->value, status_str);
    }

    printf("\nTotal: %d relocations (%d applied, %d skipped, %d failed)\n",
           g_reloc_debug_count, applied, skipped, failed);
    printf("================================================================================\n\n");

    /* Free debug buffer */
    free(g_reloc_debug);
    g_reloc_debug = NULL;
    g_reloc_debug_count = 0;
    g_reloc_debug_capacity = 0;
}

/**
 * Full linker pipeline that integrates all 4 modules
 * This function is defined here (after all module definitions) so it can
 * access all structure members without forward declarations
 */
static int linker_pipeline_full(const char **objects, int count, const char *output,
                                const char **lib_paths, int lib_count,
                                const char **libs, int libs_count,
                                LibcBackend libc_backend, int gc_sections) {
    int ret = -1;
    ObjectFile **obj_files = NULL;
    MergedSection *merged = NULL;
    LinkerSymbolTable *symtab = NULL;
    SectionHashTable *section_hash = NULL;
    Elf64_Phdr *phdrs = NULL;
    int phdr_count = 0;
    int merged_count = 0;
    RelocationStats stats = {0, 0, 0};
    struct timespec start_time, end_time;
    OverflowList *overflow_list = NULL;
    GotPltTable *got_plt_table = NULL;

    /* Reset statistics and start timing */
    reset_linker_stats();
    timer_reset();  /* Start performance profiling */
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    g_stats.input_objects = count;

    LOG_INFO_MSG("Starting linker with %d input object(s)", count);
    timer_record("Start");

    /* Dispatch based on libc backend */
    switch (libc_backend) {
        case LIBC_COSMO:
            /* Cosmopolitan libc: proceed with current implementation */
            break;

        case LIBC_SYSTEM:
            fprintf(stderr, "linker: --libc=system not yet implemented\n");
            fprintf(stderr, "linker: System libc support is planned for future release\n");
            return -1;

        case LIBC_MINI:
            fprintf(stderr, "linker: --libc=mini not yet implemented\n");
            fprintf(stderr, "linker: Minimal libc support is planned for future release\n");
            return -1;

        default:
            fprintf(stderr, "linker: Unknown libc backend: %d\n", libc_backend);
            return -1;
    }

    /* Add Cosmopolitan libc (LIBC_COSMO path) */
    /* Try multiple possible paths for cosmocc */
    const char *possible_lib_paths[] = {
        "lib",  /* Local lib directory with compiled crt.o and ape.o */
        "../third_party/cosmocc/x86_64-linux-cosmo/lib",
        "/workspace/self-evolve-ai/third_party/cosmocc/x86_64-linux-cosmo/lib",
        "/workspace/r18-wt-cpp/../self-evolve-ai/third_party/cosmocc/x86_64-linux-cosmo/lib",  /* Worktree fallback */
        NULL
    };

    const char *cosmo_lib_path = NULL;
    const char *cosmo_crt = NULL;
    const char *cosmo_ape = NULL;
    char crt_path_buf[512];
    char ape_path_buf[512];

    /* Find the first existing path */
    for (int i = 0; possible_lib_paths[i] != NULL; i++) {
        snprintf(crt_path_buf, sizeof(crt_path_buf), "%s/crt.o", possible_lib_paths[i]);
        FILE *test = fopen(crt_path_buf, "rb");
        if (test) {
            fclose(test);
            cosmo_lib_path = possible_lib_paths[i];
            cosmo_crt = strdup(crt_path_buf);
            snprintf(ape_path_buf, sizeof(ape_path_buf), "%s/ape.o", possible_lib_paths[i]);
            cosmo_ape = strdup(ape_path_buf);
            break;
        }
    }

    if (!cosmo_lib_path || !cosmo_crt) {
        fprintf(stderr, "linker: Cannot find Cosmopolitan libc installation\n");
        fprintf(stderr, "linker: Searched paths:\n");
        for (int i = 0; possible_lib_paths[i] != NULL; i++) {
            fprintf(stderr, "  - %s/crt.o\n", possible_lib_paths[i]);
        }
        return -1;
    }

    /* Expand lib_paths to include cosmo path */
    const char **expanded_lib_paths = malloc((lib_count + 1) * sizeof(char*));
    if (!expanded_lib_paths) {
        fprintf(stderr, "linker: Out of memory\n");
        return -1;
    }
    for (int i = 0; i < lib_count; i++) {
        expanded_lib_paths[i] = lib_paths[i];
    }
    expanded_lib_paths[lib_count] = cosmo_lib_path;

    /* Expand libs to include libcosmo */
    const char **expanded_libs = malloc((libs_count + 1) * sizeof(char*));
    if (!expanded_libs) {
        free(expanded_lib_paths);
        fprintf(stderr, "linker: Out of memory\n");
        return -1;
    }
    for (int i = 0; i < libs_count; i++) {
        expanded_libs[i] = libs[i];
    }
    expanded_libs[libs_count] = "cosmo";

    /* Expand objects to include cosmo crt.o and ape.o */
    const char **expanded_objects = malloc((count + 2) * sizeof(char*));
    if (!expanded_objects) {
        free(expanded_lib_paths);
        free(expanded_libs);
        fprintf(stderr, "linker: Out of memory\n");
        return -1;
    }
    expanded_objects[0] = cosmo_crt;  /* crt.o goes first */
    expanded_objects[1] = cosmo_ape;  /* ape.o provides runtime support */
    for (int i = 0; i < count; i++) {
        expanded_objects[i + 2] = objects[i];
    }

    /* Update counts */
    lib_count++;
    libs_count++;
    count += 2;  /* Added crt.o and ape.o */
    objects = expanded_objects;
    lib_paths = expanded_lib_paths;
    libs = expanded_libs;

    /* Phase 1: Parse all object files (with optional parallelization) */
    LOG_INFO_MSG("[Phase 1/7] Parsing object files...");
    obj_files = malloc(count * sizeof(ObjectFile*));
    if (!obj_files) {
        LOG_ERROR_MSG("Out of memory allocating object file array");
        return -1;
    }

    /* Try parallel parsing first (falls back to sequential if not beneficial) */
    if (parallel_parse_objects(objects, count, obj_files) < 0) {
        LOG_ERROR_MSG("Failed to parse object files");
        fprintf(stderr, "  hint: check if files exist and are valid ELF objects\n");
        goto cleanup;
    }

    LOG_INFO_MSG("Parsed %d object file(s)", count);
    timer_record("Phase 1: Parse inputs");

    /* Phase 1.3: Extract required Cosmopolitan runtime objects from libcosmo.a */
    const char *runtime_objects[] = {
        "hostos.o",
        "envp.o",
        "oldstack.o",
        "kstarttsc.o",
        "program_executable_name.o",
        "program_executable_name_init.o",
        "cosmo.o",  /* Provides weak 'cosmo' symbol for runtime detection */

        /* pthread core runtime objects - force extract for threading support */
        "pthread_create.o",
        "pthread_atfork.o",
        "pthread_attr_init.o",
        "pthread_attr_destroy.o",
        "pthread_attr_getstack.o",
        "pthread_attr_setdetachstate.o",
        "pthread_attr_setstacksize.o",
        "pthread_cancel.o",
        "pthread_setcancelstate.o",
        "pthread_setcanceltype.o",
        "pthread_getspecific.o",
        "pthread_setspecific.o",
        "pthread_key_create.o",
        "pthread_key_delete.o",
        "pthread_getattr_np.o",
        "pthread_detach.o",
        "pthread_delay_np.o",
        "pthread_timedjoin_np.o",
        "pthread_yield_np.o",
        "pthread_cleanup_push.o",
        "pthread_cleanup_pop.o",
        "pthread_orphan_np.o",
        "pthread_pause_np.o",
        "pthread_sigmask.o",
        "pthread_static.o",
        "pthread_syshand.o",

        /* pthread mutex support */
        "pthread_mutex_init.o",
        "pthread_mutex_destroy.o",
        "pthread_mutex_lock.o",
        "pthread_mutex_unlock.o",
        "pthread_mutex_trylock.o",
        "pthread_mutexattr_init.o",
        "pthread_mutexattr_destroy.o",
        "pthread_mutexattr_settype.o",

        /* pthread condition variable support */
        "pthread_cond_init.o",
        "pthread_cond_destroy.o",
        "pthread_cond_wait.o",
        "pthread_cond_signal.o",
        "pthread_cond_broadcast.o",
        "pthread_cond_timedwait.o",
        "pthread_condattr_init.o",
        "pthread_condattr_destroy.o",

        /* pthread rwlock support */
        "pthread_rwlock_rdlock.o",
        "pthread_rwlock_wrlock.o",
        "pthread_rwlock_unlock.o",

        /* System runtime: clock functions */
        "clock_nanosleep.o",
        "clock_gettime.o",
        "clock_getres.o",
        "clock_settime.o",

        /* System runtime: scheduler functions */
        "sched_yield.o",
        "sched_getparam.o",
        "sched_setparam.o",
        "sched_getscheduler.o",
        "sched_setscheduler.o",
        "sched_get_priority_min.o",

        /* System runtime: memory functions */
        "posix_memalign.o"
    };
    int runtime_obj_count = 0;
    /* Construct libcosmo.a path using the detected cosmo_lib_path */
    char libcosmo_path_buf[512];
    snprintf(libcosmo_path_buf, sizeof(libcosmo_path_buf), "%s/libcosmo.a", cosmo_lib_path);
    ObjectFile **runtime_objs = extract_specific_objects(libcosmo_path_buf, runtime_objects,
                                                        sizeof(runtime_objects) / sizeof(runtime_objects[0]),
                                                        &runtime_obj_count);

    if (runtime_obj_count > 0) {
        /* Reallocate obj_files to include runtime objects */
        ObjectFile **new_obj_files = realloc(obj_files, (count + runtime_obj_count) * sizeof(ObjectFile*));
        if (!new_obj_files) {
            fprintf(stderr, "linker: Out of memory expanding object list for runtime\n");
            if (runtime_objs) free(runtime_objs);
            goto cleanup;
        }
        obj_files = new_obj_files;

        /* Copy runtime objects into obj_files */
        for (int i = 0; i < runtime_obj_count; i++) {
            obj_files[count + i] = runtime_objs[i];
        }
        count += runtime_obj_count;

        /* Free the temporary runtime_objs array (not the objects themselves) */
        free(runtime_objs);

        g_stats.runtime_objects_added = runtime_obj_count;
        LOG_INFO_MSG("Added %d runtime object(s) from libcosmo.a", runtime_obj_count);
    }

    /* Phase 1.5: Extract objects from archive libraries with lazy symbol resolution */
    LOG_DEBUG_MSG("BEFORE Phase 1.5: count=%d, obj_files=%p, obj_files[0]=%p",
                  count, (void*)obj_files, (count > 0 ? (void*)obj_files[0] : NULL));
    LOG_INFO_MSG("[Phase 1.5/7] Extracting archive objects (lazy resolution)...");
    int archive_obj_count = 0;
    ObjectFile **archive_objs = extract_archive_objects(libs, libs_count, lib_paths, lib_count,
                                                        obj_files, count, &archive_obj_count);

    if (archive_obj_count > 0) {
        /* Reallocate obj_files to include archive objects */
        ObjectFile **new_obj_files = realloc(obj_files, (count + archive_obj_count) * sizeof(ObjectFile*));
        if (!new_obj_files) {
            LOG_ERROR_MSG("Out of memory expanding object list for archives");
            if (archive_objs) free(archive_objs);
            goto cleanup;
        }
        obj_files = new_obj_files;

        /* Copy archive objects into obj_files */
        for (int i = 0; i < archive_obj_count; i++) {
            obj_files[count + i] = archive_objs[i];
        }
        count += archive_obj_count;

        /* Free the temporary archive_objs array (not the objects themselves) */
        free(archive_objs);

        g_stats.archive_objects_extracted = archive_obj_count;
        LOG_INFO_MSG("Extracted %d object(s) from archives", archive_obj_count);
    }
    timer_record("Phase 2: Extract archives");

    /* Phase 1.7: Dead code elimination (--gc-sections) */
    if (gc_sections) {
        int original_count = count;
        int used_count = mark_used_symbols(obj_files, count, "_start");

        fprintf(stderr, "linker: --gc-sections: %d/%d objects used\n", used_count, original_count);

        /* Compact obj_files array to only include used objects */
        int write_idx = 0;
        for (int i = 0; i < count; i++) {
            if (obj_files[i]->used) {
                /* Keep this object */
                if (write_idx != i) {
                    obj_files[write_idx] = obj_files[i];
                }
                write_idx++;
            } else {
                /* Free unused object */
                free_object_file(obj_files[i]);
            }
        }
        count = used_count;

        if (count == 0) {
            fprintf(stderr, "linker: gc-sections: error: no objects remaining after dead code elimination\n");
            goto cleanup;
        }
    }

    /* Determine architecture (all objects must have same architecture) */
    LOG_DEBUG_MSG("About to check architecture: count=%d, obj_files=%p, obj_files[0]=%p",
                  count, (void*)obj_files, (count > 0 ? (void*)obj_files[0] : NULL));
    if (count == 0) {
        LOG_ERROR_MSG("No objects loaded - cannot determine architecture");
        goto cleanup;
    }
    if (!obj_files || !obj_files[0]) {
        LOG_ERROR_MSG("obj_files is NULL or obj_files[0] is NULL (count=%d)", count);
        goto cleanup;
    }
    LinkerArch arch = obj_files[0]->arch;
    for (int i = 1; i < count; i++) {
        if (obj_files[i]->arch != arch) {
            fprintf(stderr, "linker: Architecture mismatch: object %d has different architecture than first object\n", i);
            goto cleanup;
        }
    }

    /* Phase 2: Merge sections and assign addresses */
    LOG_INFO_MSG("[Phase 2/7] Merging sections...");
    merged = merge_sections(obj_files, count, &merged_count);
    if (!merged) {
        LOG_ERROR_MSG("Failed to merge sections");
        goto cleanup;
    }
    g_stats.sections_merged = merged_count;
    LOG_DEBUG_MSG("Merged into %d section(s)", merged_count);

    LOG_INFO_MSG("[Phase 2.5/7] Assigning addresses...");
    if (assign_addresses(merged, merged_count) < 0) {
        LOG_ERROR_MSG("Failed to assign addresses to sections");
        goto cleanup;
    }
    timer_record("Phase 3: Merge sections");

    /* Calculate section sizes for statistics */
    for (int i = 0; i < merged_count; i++) {
        if (strstr(merged[i].name, ".text") || strstr(merged[i].name, ".rodata")) {
            g_stats.total_code_size += merged[i].size;
        } else if (strstr(merged[i].name, ".data") || strstr(merged[i].name, ".bss")) {
            g_stats.total_data_size += merged[i].size;
        }
    }

    /* Phase 3: Build symbol table and resolve symbols */
    LOG_INFO_MSG("[Phase 3/7] Building symbol table...");
    symtab = build_symbol_table(obj_files, count);
    if (!symtab) {
        LOG_ERROR_MSG("Failed to build symbol table");
        goto cleanup;
    }
    g_stats.total_symbols = symtab->count;
    LOG_DEBUG_MSG("Built symbol table with %d symbol(s)", symtab->count);

    /* Count symbol types */
    for (int i = 0; i < symtab->count; i++) {
        if (symtab->symbols[i]->shndx == 0 || symtab->symbols[i]->shndx == SHN_UNDEF) {
            g_stats.undefined_symbols++;
        }
        if (symtab->symbols[i]->bind == STB_WEAK) {
            g_stats.weak_symbols++;
        }
    }

    LOG_INFO_MSG("[Phase 3.5/7] Relocating symbols...");
    if (relocate_symbols(symtab->symbols, symtab->count, merged, merged_count) < 0) {
        LOG_ERROR_MSG("Failed to relocate symbols");
        goto cleanup;
    }

    LOG_INFO_MSG("[Phase 3.7/7] Resolving undefined symbols...");
    if (resolve_symbols(symtab, lib_paths, lib_count, libs, libs_count) < 0) {
        LOG_ERROR_MSG("Failed to resolve symbols");
        fprintf(stderr, "  hint: undefined symbols may need additional libraries (-l)\n");
        goto cleanup;
    }

    /* Dump symbol table if requested */
    dump_symbol_table(symtab, merged, merged_count);
    timer_record("Phase 3.5: Build symbol table");

    /* Phase 4: Apply relocations */
    LOG_INFO_MSG("[Phase 4/7] Applying relocations...");
    timer_record("Phase 4.0: Start relocations");

    /* Initialize overflow list for GOT/PLT generation */
    overflow_list = init_overflow_list(128);
    if (!overflow_list) {
        LOG_ERROR_MSG("Failed to initialize overflow list");
        goto cleanup;
    }
    timer_record("Phase 4.0.1: Initialize overflow list");

    /* Build section hash table for O(1) section lookups */
    section_hash = build_section_hash_table(merged, merged_count);
    if (!section_hash) {
        LOG_ERROR_MSG("Failed to build section hash table");
        goto cleanup;
    }
    timer_record("Phase 4.1: Build section hash");

    /* Count total relocations for batching */
    int total_reloc_count = 0;
    for (int i = 0; i < count; i++) {
        ObjectFile *obj = obj_files[i];
        for (uint32_t r = 0; r < obj->rela_count; r++) {
            total_reloc_count += obj->rela_sections[r].count;
        }
    }

    /* Build flat array of all relocations */
    RelocBatchEntry *reloc_batch = calloc(total_reloc_count, sizeof(RelocBatchEntry));
    if (!reloc_batch) {
        LOG_ERROR_MSG("Failed to allocate relocation batch array");
        goto cleanup;
    }

    int batch_idx = 0;
    for (int i = 0; i < count; i++) {
        ObjectFile *obj = obj_files[i];
        for (uint32_t r = 0; r < obj->rela_count; r++) {
            LinkerRelaSection *rela_sec = &obj->rela_sections[r];
            const char *target_name = NULL;
            if (rela_sec->target_shndx < obj->section_count) {
                target_name = obj->sections[rela_sec->target_shndx].name;
            }
            for (uint32_t j = 0; j < rela_sec->count; j++) {
                reloc_batch[batch_idx].target_name = target_name;
                reloc_batch[batch_idx].obj = obj;
                reloc_batch[batch_idx].rela_sec = rela_sec;
                reloc_batch[batch_idx].rel_index = j;
                batch_idx++;
            }
        }
    }
    timer_record("Phase 4.2: Build relocation batch");

    /* Sort relocations by target section name for cache locality */
    qsort(reloc_batch, total_reloc_count, sizeof(RelocBatchEntry), compare_reloc_batch);
    timer_record("Phase 4.3: Sort relocations by target");

    /* Process relocations in sorted order */
    int reloc_count = 0;
    for (int batch_i = 0; batch_i < total_reloc_count; batch_i++) {
        RelocBatchEntry *batch_entry = &reloc_batch[batch_i];
        ObjectFile *obj = batch_entry->obj;
        LinkerRelaSection *rela_sec = batch_entry->rela_sec;
        LinkerRelocation *rel = &rela_sec->relas[batch_entry->rel_index];

        stats.total_relocs++;
        reloc_count++;

                /* Find target section using hash table (O(1) instead of O(M)) */
                MergedSection *target = NULL;
                if (rela_sec->target_shndx < obj->section_count) {
                    const char *target_name = obj->sections[rela_sec->target_shndx].name;
                    if (target_name) {
                        /* Normalize section name to match merged section naming */
                        const char *normalized_name = get_merged_section_name(target_name);
                        target = find_section_in_hash(section_hash, normalized_name);
                    }
                }

                if (!target) {
                    LOG_DEBUG_MSG("Skipping relocation: target section not found");
                    continue;
                }

                /* Get symbol */
                if (rel->symbol >= obj->symbol_count) {
                    LOG_DEBUG_MSG("Skipping relocation: symbol index out of range");
                    continue;
                }
                LinkerSymbol *sym = &obj->symbols[rel->symbol];

                /* Find symbol value in symbol table */
                uint64_t symbol_value = 0;
                int found = 0;
                LinkerSymbol *resolved = find_symbol(symtab, sym->name);
                if (resolved) {
                    symbol_value = resolved->value;
                    found = 1;
                }

                if (!found) {
                    /* Symbol not in global symbol table - check if it's section-relative */
                    if (sym->shndx != SHN_UNDEF && sym->shndx < obj->section_count) {
                        /* Section-relative symbol (local label, static variable, etc.) */
                        /* Find the merged section corresponding to this symbol's original section */
                        const char *sym_section_name = obj->sections[sym->shndx].name;

                        /* Skip relocations for debug sections */
                        if (sym_section_name && strncmp(sym_section_name, ".debug", 6) == 0) {
                            continue;
                        }

                        /* Use hash table for O(1) lookup instead of O(M) linear search */
                        /* Normalize section name to match merged section naming */
                        const char *normalized_sym_name = get_merged_section_name(sym_section_name);
                        MergedSection *sym_section = find_section_in_hash(section_hash, normalized_sym_name);
                        if (sym_section) {
                            /* Calculate symbol value: section base + symbol offset */
                            symbol_value = sym_section->vma + sym->value;
                            found = 1;
                            LOG_DEBUG_MSG("Resolved section-relative symbol '%s': section=%s, vma=0x%lx, offset=0x%lx, value=0x%lx",
                                          sym->name[0] ? sym->name : "<unnamed>",
                                          sym_section_name, sym_section->vma, sym->value, symbol_value);
                        }
                    }

                    if (!found && sym->name[0] != '\0') {
                        LOG_WARN_MSG("Symbol '%s' not found in symbol table for relocation", sym->name);
                    }
                }

                /* Apply relocation (Pass 1: collect overflow candidates) */
                int reloc_result = apply_single_relocation(target->data, target->size, rel->offset,
                                                          rel->type, symbol_value, rel->addend,
                                                          target->vma + rel->offset, overflow_list,
                                                          sym->name, target);

                /* Record relocation for debugging */
                record_relocation(target->vma + rel->offset, rel->type, sym->name,
                                symbol_value, reloc_result);

                if (reloc_result < 0) {
                    /* Critical error */
                    stats.failed_relocs++;
                    g_stats.failed_relocations++;
                    LOG_ERROR_MSG("Failed to apply relocation for symbol '%s'", sym->name);
                    fprintf(stderr, "  relocation type: %d, offset: 0x%lx\n", rel->type, rel->offset);
                    fprintf(stderr, "  symbol_value: 0x%lx, addend: 0x%lx, target_vma: 0x%lx\n",
                            symbol_value, rel->addend, target->vma + rel->offset);
                    goto cleanup;
                } else if (reloc_result > 0) {
                    /* Skipped due to overflow (warning already printed) */
                    stats.skipped_relocs++;
                    /* Enhanced overflow diagnostics: show symbol and distance */
                    int64_t distance = (int64_t)symbol_value - (int64_t)(target->vma + rel->offset);
                    fprintf(stderr, "linker: skipped overflow for symbol '%s' (type=%d, distance=%ld bytes, symbol@0x%lx, target@0x%lx)\n",
                            sym->name[0] ? sym->name : "<unnamed>",
                            rel->type, (long)distance, symbol_value, target->vma + rel->offset);
                    LOG_DEBUG_MSG("Skipped relocation for symbol '%s': type=%d, value=0x%lx, addend=%ld, target=0x%lx",
                                  sym->name[0] ? sym->name : "<unnamed>", rel->type, symbol_value, rel->addend, target->vma + rel->offset);
                }
    }

    /* Free relocation batch array */
    free(reloc_batch);
    reloc_batch = NULL;

    g_stats.total_relocations = reloc_count;
    LOG_INFO_MSG("Applied %d relocation(s)", reloc_count);
    timer_record("Phase 4.4: Process relocations (Pass 1)");

    /* Phase 4.5: Generate GOT/PLT table if overflows detected */
    if (overflow_list && overflow_list->count > 0) {
        LOG_INFO_MSG("[Phase 4.5/7] Generating GOT/PLT table for %d overflow(s)...", overflow_list->count);
        fprintf(stderr, "linker: detected %d relocation overflow(s), generating GOT/PLT table\n", overflow_list->count);

        /* Find .text section end to determine GOT/PLT placement */
        uint64_t code_end = 0;
        for (int i = 0; i < merged_count; i++) {
            if (strcmp(merged[i].name, ".text") == 0) {
                code_end = merged[i].vma + merged[i].size;
                break;
            }
        }

        if (code_end == 0) {
            LOG_ERROR_MSG("Failed to find .text section for GOT/PLT placement");
            goto cleanup;
        }

        /* Generate GOT/PLT table */
        got_plt_table = create_got_plt_table(overflow_list, code_end, ARCH_X86_64);
        if (!got_plt_table) {
            LOG_ERROR_MSG("Failed to create GOT/PLT table");
            goto cleanup;
        }

        print_got_plt_stats(got_plt_table);
        timer_record("Phase 4.5: Generate GOT/PLT table");

        /* Phase 4.6: Second pass - redirect overflows to PLT */
        LOG_INFO_MSG("[Phase 4.6/7] Redirecting %d overflow(s) to PLT stubs...", overflow_list->count);
        fprintf(stderr, "linker: redirecting %d overflow relocations through PLT\n", overflow_list->count);

        int redirect_count = 0;
        int redirect_failures = 0;

        for (int i = 0; i < overflow_list->count; i++) {
            OverflowCandidate *candidate = &overflow_list->entries[i];

            /* Find PLT entry for this symbol */
            int plt_index = find_plt_entry(got_plt_table, candidate->symbol_name);
            if (plt_index < 0) {
                fprintf(stderr, "linker: error: no PLT entry for overflow symbol '%s'\n", candidate->symbol_name);
                redirect_failures++;
                continue;
            }

            /* Get PLT stub address */
            uint64_t plt_addr = get_plt_address(got_plt_table, candidate->symbol_name);
            if (plt_addr == 0) {
                fprintf(stderr, "linker: error: invalid PLT address for symbol '%s'\n", candidate->symbol_name);
                redirect_failures++;
                continue;
            }

            /* Calculate PLT-relative offset: PLT_ADDR - (source_addr + 4) */
            /* Note: source_addr already includes the relocation offset (P) */
            int64_t plt_offset = (int64_t)plt_addr - (int64_t)(candidate->source_addr + 4);

            /* Verify PLT stub is within ±2GB range */
            if (plt_offset > INT32_MAX || plt_offset < INT32_MIN) {
                fprintf(stderr, "linker: error: PLT stub itself overflows for symbol '%s' (offset: %ld)\n",
                        candidate->symbol_name, (long)plt_offset);
                fprintf(stderr, "  PLT address: 0x%lx, source: 0x%lx, offset: %ld bytes\n",
                        plt_addr, candidate->source_addr, (long)plt_offset);
                redirect_failures++;
                continue;
            }

            /* Apply PLT redirection to target section */
            if (!candidate->target_section) {
                fprintf(stderr, "linker: error: NULL target section for symbol '%s'\n", candidate->symbol_name);
                redirect_failures++;
                continue;
            }

            MergedSection *target_sec = (MergedSection*)candidate->target_section;
            if (candidate->reloc_offset >= target_sec->size) {
                fprintf(stderr, "linker: error: relocation offset out of bounds for symbol '%s'\n",
                        candidate->symbol_name);
                redirect_failures++;
                continue;
            }

            /* Write PLT offset to relocation site */
            void *reloc_site = (char*)target_sec->data + candidate->reloc_offset;
            *(int32_t*)reloc_site = (int32_t)plt_offset;

            redirect_count++;

            LOG_DEBUG_MSG("Redirected overflow: '%s' → PLT[%d] @ 0x%lx (offset: %d)",
                          candidate->symbol_name, plt_index, plt_addr, (int)plt_offset);
        }

        fprintf(stderr, "linker: redirected %d/%d overflows through PLT (%d failures)\n",
                redirect_count, overflow_list->count, redirect_failures);

        if (redirect_failures > 0) {
            LOG_ERROR_MSG("Failed to redirect %d overflow(s)", redirect_failures);
            fprintf(stderr, "linker: error: cannot continue with unresolved overflows\n");
            goto cleanup;
        }

        /* Update statistics: subtract redirected relocations from skipped count */
        stats.skipped_relocs -= redirect_count;

        timer_record("Phase 4.6: Redirect overflows to PLT");

        /* Phase 4.7: Embed GOT/PLT sections in merged sections */
        LOG_INFO_MSG("[Phase 4.7/7] Embedding GOT/PLT sections in output binary...");

        /* Create .got section */
        if (got_plt_table->got_count > 0) {
            /* Expand merged array */
            MergedSection *new_merged = (MergedSection*)realloc(merged, (merged_count + 1) * sizeof(MergedSection));
            if (!new_merged) {
                LOG_ERROR_MSG("Failed to expand merged sections for .got");
                goto cleanup;
            }
            merged = new_merged;
            MergedSection *got_section = &merged[merged_count++];
            memset(got_section, 0, sizeof(MergedSection));

            strncpy(got_section->name, ".got", sizeof(got_section->name) - 1);
            got_section->vma = got_plt_table->got_base;
            got_section->size = got_plt_table->got_size;
            got_section->alignment = 8;  /* 8-byte aligned (64-bit addresses) */
            got_section->flags = SHF_ALLOC | SHF_WRITE;  /* Writable data */

            /* Allocate and copy GOT data */
            got_section->data = malloc(got_plt_table->got_size);
            if (!got_section->data) {
                LOG_ERROR_MSG("Failed to allocate GOT section data");
                goto cleanup;
            }
            memcpy(got_section->data, got_plt_table->got_data, got_plt_table->got_size);

            fprintf(stderr, "linker: embedded .got section: base=0x%lx, size=%lu bytes (%d entries)\n",
                    got_plt_table->got_base, got_plt_table->got_size, got_plt_table->got_count);
        }

        /* Create .plt section */
        if (got_plt_table->plt_count > 0) {
            /* Expand merged array */
            MergedSection *new_merged = (MergedSection*)realloc(merged, (merged_count + 1) * sizeof(MergedSection));
            if (!new_merged) {
                LOG_ERROR_MSG("Failed to expand merged sections for .plt");
                goto cleanup;
            }
            merged = new_merged;
            MergedSection *plt_section = &merged[merged_count++];
            memset(plt_section, 0, sizeof(MergedSection));

            strncpy(plt_section->name, ".plt", sizeof(plt_section->name) - 1);
            plt_section->vma = got_plt_table->plt_base;
            plt_section->size = got_plt_table->plt_size;
            plt_section->alignment = 16;  /* 16-byte aligned (PLT stub size) */
            plt_section->flags = SHF_ALLOC | SHF_EXECINSTR;  /* Executable code */

            /* Allocate and copy PLT data */
            plt_section->data = malloc(got_plt_table->plt_size);
            if (!plt_section->data) {
                LOG_ERROR_MSG("Failed to allocate PLT section data");
                goto cleanup;
            }
            memcpy(plt_section->data, got_plt_table->plt_data, got_plt_table->plt_size);

            fprintf(stderr, "linker: embedded .plt section: base=0x%lx, size=%lu bytes (%d stubs)\n",
                    got_plt_table->plt_base, got_plt_table->plt_size, got_plt_table->plt_count);
        }

        timer_record("Phase 4.7: Embed GOT/PLT sections");

        fprintf(stderr, "linker: GOT/PLT integration complete\n");
    }

    /* Print relocation statistics */
    fprintf(stderr, "linker: Relocation summary: %d total, %d applied, %d skipped due to overflow\n",
            stats.total_relocs,
            stats.total_relocs - stats.skipped_relocs,
            stats.skipped_relocs);

    if (stats.skipped_relocs > 0) {
        fprintf(stderr, "linker: warning: %d relocations were skipped due to overflow\n", stats.skipped_relocs);
        fprintf(stderr, "linker: note: PC-relative relocations have ±2GB range limit\n");
        fprintf(stderr, "linker: note: Large distance between .text and .rodata sections can cause overflows\n");

        /* Report section layout to help diagnose overflow causes */
        for (int i = 0; i < merged_count; i++) {
            if (merged[i].size > 0 && (strcmp(merged[i].name, ".text") == 0 ||
                                       strcmp(merged[i].name, ".rodata") == 0)) {
                fprintf(stderr, "linker: section %s: vma=0x%lx, size=0x%lx (%lu KB)\n",
                        merged[i].name, merged[i].vma, merged[i].size, merged[i].size / 1024);
            }
        }

        if (stats.skipped_relocs > 10) {
            fprintf(stderr, "linker: suggestion: Try splitting large files into smaller object files\n");
            fprintf(stderr, "linker: suggestion: Consider using PIC (position independent code) for large programs\n");
        }
    }

    /* Dump relocations if requested */
    dump_relocations();

    /* Free section hash table */
    free_section_hash_table(section_hash);
    section_hash = NULL;
    timer_record("Phase 4.5: Cleanup");

    /* Phase 5: Create program headers */
    timer_record("Phase 5.0: Start headers");
    LOG_INFO_MSG("[Phase 5/7] Creating program headers...");
    timer_record("Phase 5.1: Create ELF header prep");
    phdrs = create_program_headers(merged, merged_count, &phdr_count);
    if (!phdrs) {
        LOG_ERROR_MSG("Failed to create program headers");
        goto cleanup;
    }
    LOG_DEBUG_MSG("Created %d program header(s)", phdr_count);
    timer_record("Phase 5.2: Create program headers");

    /* Phase 6: Find entry point (_start symbol) using O(1) hash lookup */
    LOG_INFO_MSG("[Phase 6/7] Finding entry point...");
    uint64_t entry_point = 0;  /* Must find _start symbol */

    /* Use hash table lookup instead of linear search (O(1) vs O(n)) */
    LinkerSymbol *start_sym = find_symbol(symtab, "_start");

    /* Validate _start symbol was found */
    if (!start_sym) {
        fprintf(stderr, "linker: error: _start symbol not found\n");
        fprintf(stderr, "  hint: ensure crt.o is linked (contains _start entry point)\n");
        fprintf(stderr, "  hint: crt.o should be first object file in link command\n");
        return -1;
    }

    entry_point = start_sym->value;
    if (entry_point == 0) {
        fprintf(stderr, "linker: error: _start symbol has NULL address\n");
        return -1;
    }

    LOG_DEBUG_MSG("Entry point found: _start @ 0x%lx", entry_point);
    timer_record("Phase 5.3: Find entry point");

    /* Phase 7: Write ELF executable */
    timer_record("Phase 6.0: Start write");
    LOG_INFO_MSG("[Phase 7/7] Writing ELF executable: %s", output);
    if (write_elf64_executable_m4(output, merged, merged_count, phdrs, phdr_count, entry_point, arch) < 0) {
        LOG_ERROR_MSG("Failed to write output file: %s", output);
        fprintf(stderr, "  hint: check disk space and write permissions\n");
        goto cleanup;
    }

    /* Calculate and record link time */
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    g_stats.link_time_sec = (end_time.tv_sec - start_time.tv_sec) +
                            (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    LOG_INFO_MSG("Link successful: %s", output);
    timer_record("Phase 6.4: Write complete");
    ret = 0;  /* Success */

cleanup:
    /* Print summary if linking succeeded */
    if (ret == 0) {
        print_linker_summary();
        timer_print();  /* Print performance profile */
    }

    /* Free expanded arrays */
    if (cosmo_crt && cosmo_crt != crt_path_buf) free((void*)cosmo_crt);
    if (expanded_lib_paths) free((void*)expanded_lib_paths);
    if (expanded_libs) free((void*)expanded_libs);
    if (expanded_objects) free((void*)expanded_objects);

    if (obj_files) {
        for (int i = 0; i < count; i++) {
            if (obj_files[i]) {
                free_object_file(obj_files[i]);
            }
        }
        free(obj_files);
    }
    if (merged) {
        free_merged_sections(merged, merged_count);
    }
    if (symtab) {
        free_symbol_table(symtab);
    }
    if (section_hash) {
        free_section_hash_table(section_hash);
    }
    if (phdrs) {
        free(phdrs);
    }
    if (overflow_list) {
        free_overflow_list(overflow_list);
    }
    if (got_plt_table) {
        free_got_plt_table(got_plt_table);
    }

    return ret;
}


/* ========== CRT Syscall Wrappers ========== */

/**
 * @section CRT syscall wrappers for standalone executables
 *
 * These functions provide minimal libc functionality for programs
 * linked with our custom CRT. They make direct Linux syscalls
 * without depending on external libraries.
 */

#ifdef BUILD_CRT_WRAPPERS

/**
 * Exit process with status code
 * @param status Exit code (0 = success)
 */
void _exit(int status) {
    asm volatile(
        "mov %0, %%edi\n"
        "mov $60, %%eax\n"  /* __NR_exit */
        "syscall"
        :: "r"(status) : "rax", "rdi"
    );
    __builtin_unreachable();
}

/**
 * Write data to file descriptor
 * @param fd File descriptor (1 = stdout, 2 = stderr)
 * @param buf Data buffer
 * @param count Number of bytes to write
 * @return Number of bytes written, or -1 on error
 */
ssize_t _write(int fd, const void *buf, size_t count) {
    ssize_t ret;
    asm volatile(
        "mov $1, %%eax\n"   /* __NR_write */
        "syscall"
        : "=a"(ret)
        : "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * Read data from file descriptor
 * @param fd File descriptor (0 = stdin)
 * @param buf Buffer to read into
 * @param count Maximum bytes to read
 * @return Number of bytes read, 0 on EOF, -1 on error
 */
ssize_t _read(int fd, void *buf, size_t count) {
    ssize_t ret;
    asm volatile(
        "mov $0, %%eax\n"   /* __NR_read */
        "syscall"
        : "=a"(ret)
        : "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * Calculate string length
 * @param s Null-terminated string
 * @return String length (not including null terminator)
 */
static size_t _strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

/**
 * Print string to stdout with newline
 * @param s Null-terminated string
 * @return 0 on success, -1 on error
 */
int puts(const char *s) {
    size_t len = _strlen(s);
    ssize_t ret = _write(1, s, len);
    if (ret < 0) return -1;

    ret = _write(1, "\n", 1);
    if (ret < 0) return -1;

    return 0;
}

/**
 * Print character to stdout
 * @param c Character to print
 * @return Character printed, or EOF on error
 */
int putchar(int c) {
    unsigned char ch = (unsigned char)c;
    ssize_t ret = _write(1, &ch, 1);
    return (ret == 1) ? c : -1;
}

/**
 * Simple integer to string conversion (for basic printf)
 * @param num Number to convert
 * @param buf Output buffer (must be at least 21 bytes)
 * @return Pointer to start of string in buffer
 */
static char *_itoa(long num, char *buf) {
    char *p = buf + 20;
    int neg = 0;

    *p-- = '\0';

    if (num < 0) {
        neg = 1;
        num = -num;
    } else if (num == 0) {
        *p-- = '0';
    }

    while (num > 0) {
        *p-- = '0' + (num % 10);
        num /= 10;
    }

    if (neg) *p-- = '-';

    return p + 1;
}

/**
 * Minimal printf implementation (supports %s, %d, %c only)
 * @param fmt Format string
 * @return Number of characters printed, -1 on error
 */
int printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int count = 0;
    char buf[32];

    for (const char *p = fmt; *p; p++) {
        if (*p == '%' && *(p + 1)) {
            p++;
            switch (*p) {
                case 's': {
                    const char *s = va_arg(args, const char *);
                    size_t len = _strlen(s);
                    _write(1, s, len);
                    count += len;
                    break;
                }
                case 'd': {
                    int num = va_arg(args, int);
                    char *str = _itoa(num, buf);
                    size_t len = _strlen(str);
                    _write(1, str, len);
                    count += len;
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    _write(1, &c, 1);
                    count++;
                    break;
                }
                case '%': {
                    _write(1, "%", 1);
                    count++;
                    break;
                }
                default:
                    _write(1, "%", 1);
                    _write(1, p, 1);
                    count += 2;
                    break;
            }
        } else {
            _write(1, p, 1);
            count++;
        }
    }

    va_end(args);
    return count;
}

#endif /* BUILD_CRT_WRAPPERS */
