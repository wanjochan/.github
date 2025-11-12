/* Arena allocator implementation - chunk-based bump-pointer allocation */
#include "cosmo_arena.h"
#include <stdlib.h>
#include <string.h>

#define DEFAULT_CHUNK_SIZE (64 * 1024)  /* 64KB default chunk */
#define ALIGNMENT 8  /* 8-byte alignment for all allocations */

/* Align size to ALIGNMENT boundary */
#define ALIGN_UP(size) (((size) + ALIGNMENT - 1) & ~(ALIGNMENT - 1))

/* Memory chunk in linked list */
typedef struct chunk_t {
    struct chunk_t *next;  /* Next chunk in list */
    size_t capacity;       /* Total capacity of this chunk */
    size_t used;          /* Bytes used in this chunk */
    uint8_t data[];       /* Flexible array member for actual data */
} chunk_t;

/* Arena structure */
struct arena_t {
    chunk_t *head;         /* Current chunk for allocation */
    chunk_t *first;        /* First chunk (for reset) */
    size_t chunk_size;     /* Default chunk size */
    size_t total_allocated; /* Total bytes allocated from system */
    size_t total_used;     /* Total bytes actually used */
    size_t chunk_count;    /* Number of chunks */
};

/* Create new chunk */
static chunk_t* chunk_create(size_t min_size, size_t default_size) {
    size_t capacity = (min_size > default_size) ? min_size : default_size;
    chunk_t *chunk = (chunk_t*)malloc(sizeof(chunk_t) + capacity);
    if (!chunk) return NULL;

    chunk->next = NULL;
    chunk->capacity = capacity;
    chunk->used = 0;
    return chunk;
}

arena_t* arena_create(size_t chunk_size) {
    if (chunk_size == 0) {
        chunk_size = DEFAULT_CHUNK_SIZE;
    }

    arena_t *arena = (arena_t*)malloc(sizeof(arena_t));
    if (!arena) return NULL;

    /* Create first chunk */
    chunk_t *first_chunk = chunk_create(0, chunk_size);
    if (!first_chunk) {
        free(arena);
        return NULL;
    }

    arena->head = first_chunk;
    arena->first = first_chunk;
    arena->chunk_size = chunk_size;
    arena->total_allocated = chunk_size;
    arena->total_used = 0;
    arena->chunk_count = 1;

    return arena;
}

void* arena_alloc(arena_t *arena, size_t size) {
    if (!arena || size == 0) return NULL;

    /* Align size */
    size = ALIGN_UP(size);

    chunk_t *chunk = arena->head;

    /* Check if current chunk has enough space */
    if (chunk->used + size > chunk->capacity) {
        /* Need new chunk */
        chunk_t *new_chunk = chunk_create(size, arena->chunk_size);
        if (!new_chunk) {
            return NULL;
        }

        /* Link new chunk */
        new_chunk->next = chunk;
        arena->head = new_chunk;
        chunk = new_chunk;

        arena->total_allocated += chunk->capacity;
        arena->chunk_count++;
    }

    /* Allocate from current chunk (fast bump pointer) */
    void *ptr = chunk->data + chunk->used;
    chunk->used += size;
    arena->total_used += size;

    return ptr;
}

void arena_reset(arena_t *arena) {
    if (!arena) return;

    /* Reset all chunks to unused state */
    chunk_t *chunk = arena->head;
    while (chunk) {
        chunk->used = 0;
        chunk = chunk->next;
    }

    /* Reset head to first chunk */
    arena->head = arena->first;
    arena->total_used = 0;
}

void arena_destroy(arena_t *arena) {
    if (!arena) return;

    /* Free all chunks */
    chunk_t *chunk = arena->head;
    while (chunk) {
        chunk_t *next = chunk->next;
        free(chunk);
        chunk = next;
    }

    free(arena);
}

void arena_stats(arena_t *arena, size_t *total_allocated,
                 size_t *total_used, size_t *chunk_count) {
    if (!arena) return;

    if (total_allocated) *total_allocated = arena->total_allocated;
    if (total_used) *total_used = arena->total_used;
    if (chunk_count) *chunk_count = arena->chunk_count;
}
