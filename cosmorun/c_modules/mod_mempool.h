#ifndef COSMORUN_MEMPOOL_H
#define COSMORUN_MEMPOOL_H

/*
 * mod_mempool - High-performance memory pool allocator
 *
 * Provides fast arena-based allocation for small objects:
 * - O(1) allocation for fixed-size objects
 * - Bulk deallocation (reset pool)
 * - Multiple pools for different object sizes
 * - Thread-safe option with mutex
 * - Memory statistics tracking
 * - Configurable alignment (4/8/16 bytes)
 *
 * Use Cases:
 * - Small object allocation (strings, nodes, buffers)
 * - Temporary allocations with bulk cleanup
 * - Performance-critical paths avoiding malloc/free overhead
 */

#include "src/cosmo_libc.h"

/* ==================== Configuration ==================== */

/* Default block size for arena allocation */
#ifndef MEMPOOL_DEFAULT_BLOCK_SIZE
#define MEMPOOL_DEFAULT_BLOCK_SIZE (64 * 1024)  /* 64KB */
#endif

/* Alignment options */
typedef enum {
    MEMPOOL_ALIGN_4 = 4,
    MEMPOOL_ALIGN_8 = 8,
    MEMPOOL_ALIGN_16 = 16,
    MEMPOOL_ALIGN_DEFAULT = sizeof(void*)  /* Platform default */
} mempool_align_t;

/* Pool options */
typedef enum {
    MEMPOOL_OPT_NONE = 0,
    MEMPOOL_OPT_THREAD_SAFE = (1 << 0),   /* Use mutex for thread safety */
    MEMPOOL_OPT_ZERO_MEMORY = (1 << 1),   /* Zero allocated memory */
    MEMPOOL_OPT_TRACK_STATS = (1 << 2)    /* Track detailed statistics */
} mempool_options_t;

/* ==================== Types ==================== */

/* Memory block in the arena */
typedef struct mempool_block {
    unsigned char *memory;          /* Block memory */
    size_t size;                    /* Block size */
    size_t used;                    /* Bytes used in this block */
    struct mempool_block *next;     /* Next block in chain */
} mempool_block_t;

/* Free list node for fixed-size allocations */
typedef struct mempool_freelist {
    struct mempool_freelist *next;
} mempool_freelist_t;

/* Memory pool statistics */
typedef struct {
    size_t total_allocated;      /* Total bytes allocated from system */
    size_t total_used;            /* Total bytes used by user */
    size_t total_wasted;          /* Bytes wasted due to alignment */
    size_t peak_usage;            /* Peak memory usage */
    size_t allocation_count;     /* Number of allocations */
    size_t reset_count;          /* Number of pool resets */
    size_t block_count;          /* Number of blocks */
} mempool_stats_t;

/* Memory pool structure */
typedef struct mempool {
    size_t obj_size;              /* Fixed object size (0 for variable) */
    size_t alignment;             /* Memory alignment */
    size_t block_size;            /* Size of each block */
    mempool_options_t options;    /* Pool options */

    /* Block management */
    mempool_block_t *blocks;      /* Chain of memory blocks */
    mempool_block_t *current;     /* Current allocation block */

    /* Free list (for fixed-size with free support) */
    mempool_freelist_t *freelist; /* Free object list */

    /* Thread safety */
    pthread_mutex_t *mutex;       /* Mutex for thread-safe pools */

    /* Statistics */
    mempool_stats_t stats;        /* Memory statistics */
} mempool_t;

/* ==================== Pool Creation ==================== */

/*
 * Create memory pool with fixed object size
 *
 * @param obj_size Fixed size of objects to allocate (0 for variable size)
 * @param initial_capacity Initial number of objects to pre-allocate
 * @return New memory pool or NULL on error
 */
mempool_t* mempool_create(size_t obj_size, size_t initial_capacity);

/*
 * Create memory pool with options
 *
 * @param obj_size Fixed size of objects (0 for variable size)
 * @param initial_capacity Initial capacity hint
 * @param alignment Memory alignment (4, 8, 16 bytes)
 * @param options Pool options (thread-safe, zero memory, stats)
 * @return New memory pool or NULL on error
 */
mempool_t* mempool_create_ex(size_t obj_size, size_t initial_capacity,
                               mempool_align_t alignment,
                               mempool_options_t options);

/*
 * Destroy memory pool and free all memory
 *
 * @param pool Memory pool to destroy
 */
void mempool_destroy(mempool_t *pool);

/* ==================== Allocation ==================== */

/*
 * Allocate object from pool (O(1) for fixed-size)
 *
 * @param pool Memory pool
 * @return Pointer to allocated memory or NULL on error
 */
void* mempool_alloc(mempool_t *pool);

/*
 * Allocate memory of specific size (for variable-size pools)
 *
 * @param pool Memory pool
 * @param size Size of memory to allocate
 * @return Pointer to allocated memory or NULL on error
 */
void* mempool_alloc_size(mempool_t *pool, size_t size);

/*
 * Free individual object back to pool (optional, uses free list)
 * Only works for fixed-size pools. For variable-size, use mempool_reset.
 *
 * @param pool Memory pool
 * @param ptr Pointer to free
 */
void mempool_free(mempool_t *pool, void *ptr);

/*
 * Reset pool (bulk free all allocations)
 * Much faster than individual free calls.
 *
 * @param pool Memory pool to reset
 */
void mempool_reset(mempool_t *pool);

/* ==================== Statistics ==================== */

/*
 * Get pool statistics
 *
 * @param pool Memory pool
 * @return Pointer to statistics structure
 */
const mempool_stats_t* mempool_get_stats(mempool_t *pool);

/*
 * Print pool statistics to stdout
 *
 * @param pool Memory pool
 */
void mempool_print_stats(mempool_t *pool);

/*
 * Get current memory usage
 *
 * @param pool Memory pool
 * @return Current bytes used
 */
size_t mempool_get_usage(mempool_t *pool);

/*
 * Get pool capacity
 *
 * @param pool Memory pool
 * @return Total bytes allocated from system
 */
size_t mempool_get_capacity(mempool_t *pool);

/* ==================== Utility ==================== */

/*
 * Align size to specified alignment
 *
 * @param size Size to align
 * @param alignment Alignment boundary
 * @return Aligned size
 */
size_t mempool_align_size(size_t size, size_t alignment);

#endif /* COSMORUN_MEMPOOL_H */
