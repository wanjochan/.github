#ifndef COSMORUN_STD_OPTIMIZED_H
#define COSMORUN_STD_OPTIMIZED_H

/*
 * mod_std_optimized.h - High-performance HashMap with Robin Hood hashing
 *
 * Drop-in replacement for std_hashmap with significant performance improvements.
 * Uses open addressing (Robin Hood hashing) for better cache locality.
 */

#include "../cosmorun_system/libc_shim/sys_libc_shim.h"
#include <stdint.h>

/* ==================== Optimized HashMap ==================== */

typedef struct {
    char *key;
    void *value;
    uint32_t hash;      // Cached hash value for faster comparisons
} opt_hashmap_entry_t;

typedef struct {
    opt_hashmap_entry_t *entries;
    size_t capacity;
    size_t size;
    size_t resize_threshold;
} opt_hashmap_t;

typedef struct {
    size_t size;
    size_t capacity;
    double load_factor;
    double avg_probe_distance;
} opt_hashmap_stats_t;

/* Create/destroy hashmaps */
opt_hashmap_t* opt_hashmap_new(void);
opt_hashmap_t* opt_hashmap_with_capacity(size_t capacity);
void opt_hashmap_free(opt_hashmap_t *map);

/* HashMap operations */
void opt_hashmap_set(opt_hashmap_t *map, const char *key, void *value);
void* opt_hashmap_get(opt_hashmap_t *map, const char *key);
int opt_hashmap_has(opt_hashmap_t *map, const char *key);
void opt_hashmap_remove(opt_hashmap_t *map, const char *key);
size_t opt_hashmap_size(opt_hashmap_t *map);
void opt_hashmap_clear(opt_hashmap_t *map);

/* HashMap iteration */
typedef void (*opt_hashmap_iter_fn)(const char *key, void *value, void *userdata);
void opt_hashmap_foreach(opt_hashmap_t *map, opt_hashmap_iter_fn fn, void *userdata);

/* HashMap statistics */
double opt_hashmap_load_factor(opt_hashmap_t *map);
void opt_hashmap_stats(opt_hashmap_t *map, opt_hashmap_stats_t *stats);

#endif /* COSMORUN_STD_OPTIMIZED_H */
