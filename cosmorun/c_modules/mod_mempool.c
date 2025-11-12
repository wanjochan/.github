/*
 * mod_mempool.c - High-performance memory pool allocator implementation
 */

#include "mod_mempool.h"

/* ==================== Internal Helpers ==================== */

/*
 * Align size to specified alignment
 */
size_t mempool_align_size(size_t size, size_t alignment) {
    if (alignment == 0) return size;
    return (size + alignment - 1) & ~(alignment - 1);
}

/*
 * Allocate new memory block
 */
static mempool_block_t* mempool_block_create(size_t size) {
    mempool_block_t *block = (mempool_block_t*)malloc(sizeof(mempool_block_t));
    if (!block) return NULL;

    block->memory = (unsigned char*)malloc(size);
    if (!block->memory) {
        free(block);
        return NULL;
    }

    block->size = size;
    block->used = 0;
    block->next = NULL;

    return block;
}

/*
 * Free memory block
 */
static void mempool_block_free(mempool_block_t *block) {
    if (!block) return;
    free(block->memory);
    free(block);
}

/*
 * Lock pool if thread-safe
 */
static inline void mempool_lock(mempool_t *pool) {
    if (pool->options & MEMPOOL_OPT_THREAD_SAFE) {
        pthread_mutex_lock(pool->mutex);
    }
}

/*
 * Unlock pool if thread-safe
 */
static inline void mempool_unlock(mempool_t *pool) {
    if (pool->options & MEMPOOL_OPT_THREAD_SAFE) {
        pthread_mutex_unlock(pool->mutex);
    }
}

/* ==================== Pool Creation ==================== */

mempool_t* mempool_create(size_t obj_size, size_t initial_capacity) {
    return mempool_create_ex(obj_size, initial_capacity,
                              MEMPOOL_ALIGN_DEFAULT,
                              MEMPOOL_OPT_TRACK_STATS);
}

mempool_t* mempool_create_ex(size_t obj_size, size_t initial_capacity,
                               mempool_align_t alignment,
                               mempool_options_t options) {
    /* Allocate pool structure */
    mempool_t *pool = (mempool_t*)calloc(1, sizeof(mempool_t));
    if (!pool) return NULL;

    /* Set pool parameters */
    pool->obj_size = obj_size;
    pool->alignment = alignment;
    pool->options = options;

    /* Calculate block size */
    if (obj_size > 0 && initial_capacity > 0) {
        size_t aligned_obj_size = mempool_align_size(obj_size, alignment);
        pool->block_size = aligned_obj_size * initial_capacity;
        /* Ensure minimum block size */
        if (pool->block_size < MEMPOOL_DEFAULT_BLOCK_SIZE) {
            pool->block_size = MEMPOOL_DEFAULT_BLOCK_SIZE;
        }
    } else {
        pool->block_size = MEMPOOL_DEFAULT_BLOCK_SIZE;
    }

    /* Create initial block */
    pool->blocks = mempool_block_create(pool->block_size);
    if (!pool->blocks) {
        free(pool);
        return NULL;
    }
    pool->current = pool->blocks;

    /* Initialize free list */
    pool->freelist = NULL;

    /* Initialize mutex for thread-safe pools */
    if (options & MEMPOOL_OPT_THREAD_SAFE) {
        pool->mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
        if (!pool->mutex) {
            mempool_block_free(pool->blocks);
            free(pool);
            return NULL;
        }
        pthread_mutex_init(pool->mutex, NULL);
    } else {
        pool->mutex = NULL;
    }

    /* Initialize statistics */
    memset(&pool->stats, 0, sizeof(mempool_stats_t));
    pool->stats.total_allocated = pool->block_size;
    pool->stats.block_count = 1;

    return pool;
}

void mempool_destroy(mempool_t *pool) {
    if (!pool) return;

    /* Free all blocks */
    mempool_block_t *block = pool->blocks;
    while (block) {
        mempool_block_t *next = block->next;
        mempool_block_free(block);
        block = next;
    }

    /* Destroy mutex */
    if (pool->mutex) {
        pthread_mutex_destroy(pool->mutex);
        free(pool->mutex);
    }

    free(pool);
}

/* ==================== Allocation ==================== */

void* mempool_alloc(mempool_t *pool) {
    if (!pool) return NULL;

    if (pool->obj_size == 0) {
        fprintf(stderr, "mempool_alloc: pool has variable size, use mempool_alloc_size\n");
        return NULL;
    }

    return mempool_alloc_size(pool, pool->obj_size);
}

void* mempool_alloc_size(mempool_t *pool, size_t size) {
    if (!pool || size == 0) return NULL;

    mempool_lock(pool);

    void *ptr = NULL;

    /* For fixed-size pools, check free list first */
    if (pool->obj_size > 0 && pool->obj_size == size && pool->freelist) {
        ptr = (void*)pool->freelist;
        pool->freelist = pool->freelist->next;

        /* Zero memory if requested */
        if (pool->options & MEMPOOL_OPT_ZERO_MEMORY) {
            memset(ptr, 0, size);
        }

        if (pool->options & MEMPOOL_OPT_TRACK_STATS) {
            pool->stats.allocation_count++;
        }

        mempool_unlock(pool);
        return ptr;
    }

    /* Align size */
    size_t aligned_size = mempool_align_size(size, pool->alignment);

    /* Check if current block has enough space */
    if (pool->current->used + aligned_size > pool->current->size) {
        /* Need new block */
        size_t new_block_size = pool->block_size;
        /* If allocation is larger than default block, allocate exact size */
        if (aligned_size > new_block_size) {
            new_block_size = aligned_size;
        }

        mempool_block_t *new_block = mempool_block_create(new_block_size);
        if (!new_block) {
            mempool_unlock(pool);
            return NULL;
        }

        /* Add to block chain */
        new_block->next = pool->blocks;
        pool->blocks = new_block;
        pool->current = new_block;

        /* Update statistics */
        if (pool->options & MEMPOOL_OPT_TRACK_STATS) {
            pool->stats.total_allocated += new_block_size;
            pool->stats.block_count++;
        }
    }

    /* Allocate from current block */
    ptr = pool->current->memory + pool->current->used;
    pool->current->used += aligned_size;

    /* Update statistics */
    if (pool->options & MEMPOOL_OPT_TRACK_STATS) {
        pool->stats.total_used += size;
        pool->stats.total_wasted += (aligned_size - size);
        pool->stats.allocation_count++;

        /* Update peak usage */
        size_t current_usage = 0;
        mempool_block_t *b = pool->blocks;
        while (b) {
            current_usage += b->used;
            b = b->next;
        }
        if (current_usage > pool->stats.peak_usage) {
            pool->stats.peak_usage = current_usage;
        }
    }

    /* Zero memory if requested */
    if (pool->options & MEMPOOL_OPT_ZERO_MEMORY) {
        memset(ptr, 0, size);
    }

    mempool_unlock(pool);
    return ptr;
}

void mempool_free(mempool_t *pool, void *ptr) {
    if (!pool || !ptr) return;

    /* Only works for fixed-size pools */
    if (pool->obj_size == 0) {
        fprintf(stderr, "mempool_free: variable-size pools don't support free, use mempool_reset\n");
        return;
    }

    mempool_lock(pool);

    /* Add to free list */
    mempool_freelist_t *node = (mempool_freelist_t*)ptr;
    node->next = pool->freelist;
    pool->freelist = node;

    mempool_unlock(pool);
}

void mempool_reset(mempool_t *pool) {
    if (!pool) return;

    mempool_lock(pool);

    /* Reset all blocks */
    mempool_block_t *block = pool->blocks;
    while (block) {
        block->used = 0;
        block = block->next;
    }

    /* Reset current to first block */
    pool->current = pool->blocks;

    /* Clear free list */
    pool->freelist = NULL;

    /* Update statistics */
    if (pool->options & MEMPOOL_OPT_TRACK_STATS) {
        pool->stats.reset_count++;
        pool->stats.total_used = 0;
        pool->stats.total_wasted = 0;
        pool->stats.allocation_count = 0;
    }

    mempool_unlock(pool);
}

/* ==================== Statistics ==================== */

const mempool_stats_t* mempool_get_stats(mempool_t *pool) {
    if (!pool) return NULL;
    return &pool->stats;
}

void mempool_print_stats(mempool_t *pool) {
    if (!pool) return;

    mempool_lock(pool);

    const mempool_stats_t *stats = &pool->stats;

    printf("\n========== Memory Pool Statistics ==========\n");
    printf("Object Size:       %zu bytes\n", pool->obj_size);
    printf("Alignment:         %zu bytes\n", pool->alignment);
    printf("Block Size:        %zu bytes\n", pool->block_size);
    printf("Block Count:       %zu\n", stats->block_count);
    printf("----------------------------------------\n");
    printf("Total Allocated:   %zu bytes\n", stats->total_allocated);
    printf("Total Used:        %zu bytes\n", stats->total_used);
    /* Calculate integer percentage to avoid float operations */
    size_t wasted_percent = stats->total_allocated > 0 ?
        (100 * stats->total_wasted / stats->total_allocated) : 0;
    printf("Total Wasted:      %zu bytes (%zu%%)\n",
           stats->total_wasted, wasted_percent);
    printf("Peak Usage:        %zu bytes\n", stats->peak_usage);
    printf("----------------------------------------\n");
    printf("Allocations:       %zu\n", stats->allocation_count);
    printf("Resets:            %zu\n", stats->reset_count);
    printf("============================================\n\n");

    mempool_unlock(pool);
}

size_t mempool_get_usage(mempool_t *pool) {
    if (!pool) return 0;

    mempool_lock(pool);

    size_t usage = 0;
    mempool_block_t *block = pool->blocks;
    while (block) {
        usage += block->used;
        block = block->next;
    }

    mempool_unlock(pool);

    return usage;
}

size_t mempool_get_capacity(mempool_t *pool) {
    if (!pool) return 0;

    mempool_lock(pool);
    size_t capacity = pool->stats.total_allocated;
    mempool_unlock(pool);

    return capacity;
}
