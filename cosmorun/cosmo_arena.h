/* Arena allocator for AST - reduces malloc calls by batch allocation */
#ifndef COSMO_ARENA_H
#define COSMO_ARENA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque arena type */
typedef struct arena_t arena_t;

/* Create a new arena with specified chunk size
 * chunk_size: Size of each memory chunk (default 64KB recommended)
 * Returns: Pointer to arena or NULL on failure
 */
arena_t* arena_create(size_t chunk_size);

/* Allocate memory from arena (O(1) bump-pointer allocation)
 * arena: Arena to allocate from
 * size: Number of bytes to allocate
 * Returns: Pointer to allocated memory or NULL on failure
 */
void* arena_alloc(arena_t *arena, size_t size);

/* Reset arena to initial state, keeping allocated chunks for reuse
 * This is much faster than destroy+create
 * arena: Arena to reset
 */
void arena_reset(arena_t *arena);

/* Destroy arena and free all memory
 * arena: Arena to destroy
 */
void arena_destroy(arena_t *arena);

/* Get statistics about arena usage
 * arena: Arena to query
 * total_allocated: (out) Total bytes allocated from system
 * total_used: (out) Total bytes actually used
 * chunk_count: (out) Number of chunks allocated
 */
void arena_stats(arena_t *arena, size_t *total_allocated,
                 size_t *total_used, size_t *chunk_count);

#ifdef __cplusplus
}
#endif

#endif /* COSMO_ARENA_H */
