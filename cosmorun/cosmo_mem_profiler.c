/* Memory profiler implementation for cosmorun
 * Provides runtime memory tracking and allocation profiling
 */

#include "cosmo_mem_profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Size histogram buckets */
#define BUCKET_0_64      0
#define BUCKET_64_256    1
#define BUCKET_256_1K    2
#define BUCKET_1K_4K     3
#define BUCKET_4K_PLUS   4
#define BUCKET_COUNT     5

/* Allocation tracking */
#define MAX_ALLOCATIONS  1024
#define MAX_ALLOC_SITES  128
#define MAX_MODULES      64

typedef struct {
    void *ptr;
    size_t size;
    void *callsite;
    const char *module;
} allocation_record_t;

typedef struct {
    void *callsite;
    size_t count;
    size_t total_bytes;
} alloc_site_t;

typedef struct {
    const char *name;
    size_t bytes_allocated;
    size_t allocation_count;
    size_t peak_bytes;
} module_stats_t;

typedef struct {
    allocation_record_t allocations[MAX_ALLOCATIONS];
    int allocation_count;

    alloc_site_t sites[MAX_ALLOC_SITES];
    int site_count;

    module_stats_t modules[MAX_MODULES];
    int module_count;

    size_t total_allocated;
    size_t peak_memory;
    size_t current_memory;
    size_t total_alloc_count;
    size_t histogram[BUCKET_COUNT];

    int enabled;
} mem_profiler_t;

static mem_profiler_t profiler = {0};

/* Get size bucket index */
static int get_bucket(size_t size) {
    if (size <= 64) return BUCKET_0_64;
    if (size <= 256) return BUCKET_64_256;
    if (size <= 1024) return BUCKET_256_1K;
    if (size <= 4096) return BUCKET_1K_4K;
    return BUCKET_4K_PLUS;
}

/* Find or create allocation site record */
static alloc_site_t* find_or_create_site(void *callsite) {
    /* Find existing site */
    for (int i = 0; i < profiler.site_count; i++) {
        if (profiler.sites[i].callsite == callsite) {
            return &profiler.sites[i];
        }
    }

    /* Create new site if room available */
    if (profiler.site_count < MAX_ALLOC_SITES) {
        alloc_site_t *site = &profiler.sites[profiler.site_count++];
        site->callsite = callsite;
        site->count = 0;
        site->total_bytes = 0;
        return site;
    }

    return NULL;
}

/* Find or create module stats */
static module_stats_t* find_or_create_module(const char *name) {
    if (!name) name = "unknown";

    /* Find existing module */
    for (int i = 0; i < profiler.module_count; i++) {
        if (strcmp(profiler.modules[i].name, name) == 0) {
            return &profiler.modules[i];
        }
    }

    /* Create new module if room available */
    if (profiler.module_count < MAX_MODULES) {
        module_stats_t *mod = &profiler.modules[profiler.module_count++];
        mod->name = name;
        mod->bytes_allocated = 0;
        mod->allocation_count = 0;
        mod->peak_bytes = 0;
        return mod;
    }

    return NULL;
}

/* Initialize memory profiler */
void mem_profiler_init(void) {
    memset(&profiler, 0, sizeof(profiler));
    profiler.enabled = 1;
}

/* Shutdown and free profiler resources */
void mem_profiler_shutdown(void) {
    profiler.enabled = 0;
    memset(&profiler, 0, sizeof(profiler));
}

/* Tracked malloc wrapper */
void* mem_profiler_malloc(size_t size) {
    return mem_profiler_malloc_module(size, NULL);
}

/* Tracked malloc with module name */
void* mem_profiler_malloc_module(size_t size, const char *module) {
    void *ptr = malloc(size);

    if (!profiler.enabled || !ptr) {
        return ptr;
    }

    /* Get caller address */
    void *callsite = __builtin_return_address(0);

    /* Record allocation */
    if (profiler.allocation_count < MAX_ALLOCATIONS) {
        allocation_record_t *rec = &profiler.allocations[profiler.allocation_count++];
        rec->ptr = ptr;
        rec->size = size;
        rec->callsite = callsite;
        rec->module = module;
    }

    /* Update statistics */
    profiler.total_allocated += size;
    profiler.current_memory += size;
    profiler.total_alloc_count++;

    if (profiler.current_memory > profiler.peak_memory) {
        profiler.peak_memory = profiler.current_memory;
    }

    /* Update histogram */
    int bucket = get_bucket(size);
    profiler.histogram[bucket]++;

    /* Track allocation site */
    alloc_site_t *site = find_or_create_site(callsite);
    if (site) {
        site->count++;
        site->total_bytes += size;
    }

    /* Track module statistics */
    module_stats_t *mod = find_or_create_module(module);
    if (mod) {
        mod->bytes_allocated += size;
        mod->allocation_count++;
        if (mod->bytes_allocated > mod->peak_bytes) {
            mod->peak_bytes = mod->bytes_allocated;
        }
    }

    return ptr;
}

/* Tracked free wrapper */
void mem_profiler_free(void *ptr) {
    if (!ptr) {
        free(ptr);
        return;
    }

    if (profiler.enabled) {
        /* Find and remove allocation record */
        for (int i = 0; i < profiler.allocation_count; i++) {
            if (profiler.allocations[i].ptr == ptr) {
                size_t size = profiler.allocations[i].size;
                const char *module = profiler.allocations[i].module;
                profiler.current_memory -= size;

                /* Update module stats on free */
                module_stats_t *mod = find_or_create_module(module);
                if (mod && mod->bytes_allocated >= size) {
                    mod->bytes_allocated -= size;
                }

                /* Remove record by shifting remaining entries */
                memmove(&profiler.allocations[i],
                        &profiler.allocations[i + 1],
                        (size_t)(profiler.allocation_count - i - 1) * sizeof(allocation_record_t));
                profiler.allocation_count--;
                break;
            }
        }
    }

    free(ptr);
}

/* Print memory usage statistics */
void mem_profiler_report(void) {
    printf("\n=== Memory Profiler Report ===\n\n");

    printf("Total allocated:     %zu bytes\n", profiler.total_allocated);
    printf("Peak memory usage:   %zu bytes\n", profiler.peak_memory);
    printf("Current memory:      %zu bytes\n", profiler.current_memory);
    printf("Allocation count:    %zu\n", profiler.total_alloc_count);
    printf("Active allocations:  %d\n", profiler.allocation_count);

    printf("\nSize Distribution:\n");
    printf("  0-64 bytes:        %zu allocations\n", profiler.histogram[BUCKET_0_64]);
    printf("  64-256 bytes:      %zu allocations\n", profiler.histogram[BUCKET_64_256]);
    printf("  256-1KB:           %zu allocations\n", profiler.histogram[BUCKET_256_1K]);
    printf("  1KB-4KB:           %zu allocations\n", profiler.histogram[BUCKET_1K_4K]);
    printf("  4KB+:              %zu allocations\n", profiler.histogram[BUCKET_4K_PLUS]);

    if (profiler.module_count > 0) {
        printf("\nMemory Usage by Module:\n");
        for (int i = 0; i < profiler.module_count; i++) {
            printf("  %s: current=%zu bytes, peak=%zu bytes, allocations=%zu\n",
                   profiler.modules[i].name,
                   profiler.modules[i].bytes_allocated,
                   profiler.modules[i].peak_bytes,
                   profiler.modules[i].allocation_count);
        }
    }

    if (profiler.site_count > 0) {
        printf("\nTop Allocation Sites (by total bytes):\n");

        /* Simple bubble sort to find top sites */
        alloc_site_t sorted[MAX_ALLOC_SITES];
        memcpy(sorted, profiler.sites, (size_t)profiler.site_count * sizeof(alloc_site_t));

        for (int i = 0; i < profiler.site_count - 1; i++) {
            for (int j = 0; j < profiler.site_count - i - 1; j++) {
                if (sorted[j].total_bytes < sorted[j + 1].total_bytes) {
                    alloc_site_t temp = sorted[j];
                    sorted[j] = sorted[j + 1];
                    sorted[j + 1] = temp;
                }
            }
        }

        /* Print top 10 (or fewer if less than 10 sites) */
        int count = profiler.site_count < 10 ? profiler.site_count : 10;
        for (int i = 0; i < count; i++) {
            printf("  #%d: %p - %zu bytes (%zu allocations)\n",
                   i + 1,
                   sorted[i].callsite,
                   sorted[i].total_bytes,
                   sorted[i].count);
        }
    }

    printf("\n=============================\n");
}

/* Query statistics programmatically */
size_t mem_profiler_get_total_allocated(void) {
    return profiler.total_allocated;
}

size_t mem_profiler_get_peak_memory(void) {
    return profiler.peak_memory;
}

size_t mem_profiler_get_allocation_count(void) {
    return profiler.total_alloc_count;
}
