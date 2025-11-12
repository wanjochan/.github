/* cosmo_cache.h - Incremental Compilation Cache
 * Provides hash-based caching of compiled code for 10x faster re-builds
 */

#ifndef COSMO_CACHE_H
#define COSMO_CACHE_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Cache Configuration
// ============================================================================

#define COSMO_CACHE_DIR ".cosmorun_cache"
#define COSMO_CACHE_OBJECTS_DIR ".cosmorun_cache/objects"
#define COSMO_CACHE_INDEX_FILE ".cosmorun_cache/index.db"
#define COSMO_CACHE_MAX_ENTRIES 10000
#define COSMO_CACHE_HASH_SIZE 16  // 128-bit hash

// ============================================================================
// Cache Entry Structure
// ============================================================================

typedef struct {
    uint8_t hash[COSMO_CACHE_HASH_SIZE];  // Source code hash
    time_t timestamp;                      // Last modified time
    time_t last_access;                    // Last access time (for LRU-2)
    time_t penultimate_access;             // 2nd-last access time (for LRU-2)
    size_t source_size;                    // Source code size
    size_t code_size;                      // Compiled code size
    int access_count;                      // Total number of accesses
    char object_path[256];                 // Path to cached object
} cosmo_cache_entry_t;

// ============================================================================
// Cache Statistics
// ============================================================================

typedef struct {
    uint64_t hits;           // Cache hits
    uint64_t misses;         // Cache misses
    uint64_t stores;         // Number of stores
    uint64_t invalidations;  // Number of invalidations
    uint64_t evictions;      // Number of LRU-2 evictions
    size_t total_entries;    // Total entries in cache
    size_t total_size;       // Total size of cached objects
    double hit_rate;         // Cache hit rate (hits / (hits + misses))
} cosmo_cache_stats_t;

// ============================================================================
// Cache API
// ============================================================================

/**
 * Initialize cache system
 * Creates cache directory structure if not exists
 * @return 0 on success, -1 on error
 */
int cosmo_cache_init(void);

/**
 * Cleanup cache system
 * Flushes any pending writes and releases resources
 */
void cosmo_cache_cleanup(void);

/**
 * Lookup cached compiled code by source hash
 * @param source Source code buffer
 * @param source_size Size of source code
 * @param code_out Output buffer for compiled code (allocated by function)
 * @param code_size_out Output size of compiled code
 * @return 0 on cache hit, -1 on cache miss or error
 */
int cosmo_cache_lookup(const void *source, size_t source_size,
                       void **code_out, size_t *code_size_out);

/**
 * Store compiled code to cache
 * @param source Source code buffer
 * @param source_size Size of source code
 * @param code Compiled code buffer
 * @param code_size Size of compiled code
 * @return 0 on success, -1 on error
 */
int cosmo_cache_store(const void *source, size_t source_size,
                      const void *code, size_t code_size);

/**
 * Clear all cache entries
 * @return 0 on success, -1 on error
 */
int cosmo_cache_clear(void);

/**
 * Get cache statistics
 * @param stats Output statistics structure
 */
void cosmo_cache_get_stats(cosmo_cache_stats_t *stats);

/**
 * Enable or disable cache
 * @param enabled 1 to enable, 0 to disable
 */
void cosmo_cache_set_enabled(int enabled);

/**
 * Check if cache is enabled
 * @return 1 if enabled, 0 if disabled
 */
int cosmo_cache_is_enabled(void);

/**
 * Compute hash of source code
 * @param source Source code buffer
 * @param source_size Size of source code
 * @param hash_out Output hash buffer (must be COSMO_CACHE_HASH_SIZE bytes)
 */
void cosmo_cache_compute_hash(const void *source, size_t source_size,
                              uint8_t *hash_out);

/**
 * Set maximum number of cache entries (triggers LRU-2 eviction when exceeded)
 * @param max_entries Maximum entries (0 = unlimited)
 * @return 0 on success, -1 on error
 */
int cosmo_cache_set_max_entries(int max_entries);

/**
 * Get maximum number of cache entries
 * @return Current max entries (0 = unlimited)
 */
int cosmo_cache_get_max_entries(void);

/**
 * Set maximum cache size in bytes (triggers eviction when exceeded)
 * @param max_bytes Maximum cache size in bytes (0 = unlimited)
 * @return 0 on success, -1 on error
 */
int cosmo_cache_set_max_size(size_t max_bytes);

/**
 * Get maximum cache size in bytes
 * @return Current max size (0 = unlimited)
 */
size_t cosmo_cache_get_max_size(void);

/**
 * Set cache entry timeout in seconds (unused entries evicted after timeout)
 * @param timeout_seconds Timeout in seconds (0 = no timeout)
 * @return 0 on success, -1 on error
 */
int cosmo_cache_set_timeout(time_t timeout_seconds);

/**
 * Evict entries that haven't been accessed within timeout period
 * @return Number of entries evicted
 */
int cosmo_cache_evict_timeout(void);

/**
 * Evict N oldest entries using LRU-2 algorithm
 * @param count Number of entries to evict
 * @return Number of entries actually evicted
 */
int cosmo_cache_evict_lru2(int count);

#ifdef __cplusplus
}
#endif

#endif // COSMO_CACHE_H
