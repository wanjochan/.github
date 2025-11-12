/* cosmo_cache.c - Incremental Compilation Cache Implementation
 * Fast hash-based caching using xxHash for 10x speedup on re-compilation
 */

#include "cosmo_cache.h"
#include "cosmo_libc.h"
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

// ============================================================================
// xxHash Implementation (fast, non-cryptographic hash)
// ============================================================================

#define XXH_PRIME32_1 0x9E3779B1U
#define XXH_PRIME32_2 0x85EBCA77U
#define XXH_PRIME32_3 0xC2B2AE3DU
#define XXH_PRIME32_4 0x27D4EB2FU
#define XXH_PRIME32_5 0x165667B1U

static uint32_t xxh32_round(uint32_t acc, uint32_t input) {
    acc += input * XXH_PRIME32_2;
    acc = (acc << 13) | (acc >> 19);
    acc *= XXH_PRIME32_1;
    return acc;
}

static uint32_t xxh32(const void *data, size_t len, uint32_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    const uint8_t *end = p + len;
    uint32_t h32;

    if (len >= 16) {
        const uint8_t *limit = end - 16;
        uint32_t v1 = seed + XXH_PRIME32_1 + XXH_PRIME32_2;
        uint32_t v2 = seed + XXH_PRIME32_2;
        uint32_t v3 = seed + 0;
        uint32_t v4 = seed - XXH_PRIME32_1;

        do {
            v1 = xxh32_round(v1, *(uint32_t *)p); p += 4;
            v2 = xxh32_round(v2, *(uint32_t *)p); p += 4;
            v3 = xxh32_round(v3, *(uint32_t *)p); p += 4;
            v4 = xxh32_round(v4, *(uint32_t *)p); p += 4;
        } while (p <= limit);

        h32 = ((v1 << 1) | (v1 >> 31)) + ((v2 << 7) | (v2 >> 25)) +
              ((v3 << 12) | (v3 >> 20)) + ((v4 << 18) | (v4 >> 14));
    } else {
        h32 = seed + XXH_PRIME32_5;
    }

    h32 += (uint32_t)len;

    while (p + 4 <= end) {
        h32 += (*(uint32_t *)p) * XXH_PRIME32_3;
        h32 = ((h32 << 17) | (h32 >> 15)) * XXH_PRIME32_4;
        p += 4;
    }

    while (p < end) {
        h32 += (*p++) * XXH_PRIME32_5;
        h32 = ((h32 << 11) | (h32 >> 21)) * XXH_PRIME32_1;
    }

    h32 ^= h32 >> 15;
    h32 *= XXH_PRIME32_2;
    h32 ^= h32 >> 13;
    h32 *= XXH_PRIME32_3;
    h32 ^= h32 >> 16;

    return h32;
}

// ============================================================================
// Cache State
// ============================================================================

typedef struct {
    int initialized;
    int enabled;
    int max_entries;
    size_t max_size;
    time_t timeout_seconds;
    cosmo_cache_stats_t stats;
    char cache_dir[512];
    char objects_dir[512];
    char index_file[512];
} cosmo_cache_state_t;

static cosmo_cache_state_t g_cache_state = {0};

// ============================================================================
// Index Management (for LRU-2)
// ============================================================================

typedef struct {
    uint8_t hash[COSMO_CACHE_HASH_SIZE];
    time_t last_access;
    time_t penultimate_access;
    int access_count;
    char object_path[256];
} index_entry_t;

static int read_index(index_entry_t **entries_out, int *count_out) {
    FILE *fp = fopen(g_cache_state.index_file, "rb");
    if (!fp) {
        *entries_out = NULL;
        *count_out = 0;
        return 0; // No index yet, not an error
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    int count = file_size / sizeof(index_entry_t);
    index_entry_t *entries = (index_entry_t *)malloc(count * sizeof(index_entry_t));
    if (!entries) {
        fclose(fp);
        return -1;
    }

    size_t read_count = fread(entries, sizeof(index_entry_t), count, fp);
    fclose(fp);

    if (read_count != (size_t)count) {
        free(entries);
        return -1;
    }

    *entries_out = entries;
    *count_out = count;
    return 0;
}

static int write_index(index_entry_t *entries, int count) {
    FILE *fp = fopen(g_cache_state.index_file, "wb");
    if (!fp) {
        return -1;
    }

    size_t written = fwrite(entries, sizeof(index_entry_t), count, fp);
    fclose(fp);

    return (written == (size_t)count) ? 0 : -1;
}

static int find_index_entry(index_entry_t *entries, int count, const uint8_t *hash) {
    for (int i = 0; i < count; i++) {
        if (memcmp(entries[i].hash, hash, COSMO_CACHE_HASH_SIZE) == 0) {
            return i;
        }
    }
    return -1;
}

static void update_access_time(index_entry_t *entry) {
    time_t now = time(NULL);
    entry->penultimate_access = entry->last_access;
    entry->last_access = now;
    entry->access_count++;
}

// ============================================================================
// Helper Functions
// ============================================================================

static int ensure_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }

    #ifdef _WIN32
    return mkdir(path);
    #else
    return mkdir(path, 0755);
    #endif
}

static void hash_to_hex(const uint8_t *hash, char *hex_out) {
    static const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < COSMO_CACHE_HASH_SIZE; i++) {
        hex_out[i * 2] = hex_chars[hash[i] >> 4];
        hex_out[i * 2 + 1] = hex_chars[hash[i] & 0xF];
    }
    hex_out[COSMO_CACHE_HASH_SIZE * 2] = '\0';
}

static void compute_object_path(const uint8_t *hash, char *path_out) {
    char hex[COSMO_CACHE_HASH_SIZE * 2 + 1];
    hash_to_hex(hash, hex);
    snprintf(path_out, 512, "%s/%s.o", g_cache_state.objects_dir, hex);
}

// ============================================================================
// Cache API Implementation
// ============================================================================

int cosmo_cache_init(void) {
    if (g_cache_state.initialized) {
        return 0;
    }

    // Setup cache directories
    snprintf(g_cache_state.cache_dir, sizeof(g_cache_state.cache_dir),
             "%s", COSMO_CACHE_DIR);
    snprintf(g_cache_state.objects_dir, sizeof(g_cache_state.objects_dir),
             "%s", COSMO_CACHE_OBJECTS_DIR);
    snprintf(g_cache_state.index_file, sizeof(g_cache_state.index_file),
             "%s", COSMO_CACHE_INDEX_FILE);

    // Create directories
    if (ensure_directory(g_cache_state.cache_dir) != 0) {
        return -1;
    }
    if (ensure_directory(g_cache_state.objects_dir) != 0) {
        return -1;
    }

    // Initialize state
    g_cache_state.enabled = 1;
    g_cache_state.initialized = 1;
    g_cache_state.max_entries = 1000; // Default max entries
    g_cache_state.max_size = 100 * 1024 * 1024; // Default 100MB max cache size
    g_cache_state.timeout_seconds = 3600; // Default 1 hour timeout
    memset(&g_cache_state.stats, 0, sizeof(g_cache_state.stats));

    return 0;
}

void cosmo_cache_cleanup(void) {
    if (!g_cache_state.initialized) {
        return;
    }

    g_cache_state.initialized = 0;
}

void cosmo_cache_compute_hash(const void *source, size_t source_size,
                               uint8_t *hash_out) {
    // Compute 128-bit hash using xxHash32 (4x32-bit = 128-bit)
    uint32_t *hash32 = (uint32_t *)hash_out;

    // Use different seeds for each 32-bit chunk
    hash32[0] = xxh32(source, source_size, 0x12345678);
    hash32[1] = xxh32(source, source_size, 0x9ABCDEF0);
    hash32[2] = xxh32(source, source_size, 0x13579BDF);
    hash32[3] = xxh32(source, source_size, 0x2468ACE0);
}

int cosmo_cache_lookup(const void *source, size_t source_size,
                       void **code_out, size_t *code_size_out) {
    if (!g_cache_state.initialized || !g_cache_state.enabled) {
        return -1;
    }

    // Compute hash
    uint8_t hash[COSMO_CACHE_HASH_SIZE];
    cosmo_cache_compute_hash(source, source_size, hash);

    // Get object path
    char object_path[512];
    compute_object_path(hash, object_path);

    // Check if cached object exists
    struct stat st;
    if (stat(object_path, &st) != 0) {
        g_cache_state.stats.misses++;
        // Update hit rate
        uint64_t total = g_cache_state.stats.hits + g_cache_state.stats.misses;
        g_cache_state.stats.hit_rate = total > 0 ? (double)g_cache_state.stats.hits / total : 0.0;
        return -1;
    }

    // Read cached object
    FILE *fp = fopen(object_path, "rb");
    if (!fp) {
        g_cache_state.stats.misses++;
        uint64_t total = g_cache_state.stats.hits + g_cache_state.stats.misses;
        g_cache_state.stats.hit_rate = total > 0 ? (double)g_cache_state.stats.hits / total : 0.0;
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    void *code = malloc(file_size);
    if (!code) {
        fclose(fp);
        g_cache_state.stats.misses++;
        uint64_t total = g_cache_state.stats.hits + g_cache_state.stats.misses;
        g_cache_state.stats.hit_rate = total > 0 ? (double)g_cache_state.stats.hits / total : 0.0;
        return -1;
    }

    size_t read_size = fread(code, 1, file_size, fp);
    fclose(fp);

    if (read_size != (size_t)file_size) {
        free(code);
        g_cache_state.stats.misses++;
        uint64_t total = g_cache_state.stats.hits + g_cache_state.stats.misses;
        g_cache_state.stats.hit_rate = total > 0 ? (double)g_cache_state.stats.hits / total : 0.0;
        return -1;
    }

    // Update access time in index (LRU-2)
    index_entry_t *entries = NULL;
    int count = 0;
    if (read_index(&entries, &count) == 0 && entries) {
        int idx = find_index_entry(entries, count, hash);
        if (idx >= 0) {
            update_access_time(&entries[idx]);
            write_index(entries, count);
        }
        free(entries);
    }

    *code_out = code;
    *code_size_out = file_size;
    g_cache_state.stats.hits++;

    // Update hit rate
    uint64_t total = g_cache_state.stats.hits + g_cache_state.stats.misses;
    g_cache_state.stats.hit_rate = total > 0 ? (double)g_cache_state.stats.hits / total : 0.0;

    return 0;
}

int cosmo_cache_store(const void *source, size_t source_size,
                      const void *code, size_t code_size) {
    if (!g_cache_state.initialized || !g_cache_state.enabled) {
        return -1;
    }

    // Compute hash
    uint8_t hash[COSMO_CACHE_HASH_SIZE];
    cosmo_cache_compute_hash(source, source_size, hash);

    // Get object path
    char object_path[512];
    compute_object_path(hash, object_path);

    // Write cached object
    FILE *fp = fopen(object_path, "wb");
    if (!fp) {
        return -1;
    }

    size_t written = fwrite(code, 1, code_size, fp);
    fclose(fp);

    if (written != code_size) {
        unlink(object_path);
        return -1;
    }

    // Update index with new entry
    index_entry_t *entries = NULL;
    int count = 0;
    read_index(&entries, &count);

    // Check if entry already exists
    int existing_idx = -1;
    if (entries) {
        existing_idx = find_index_entry(entries, count, hash);
    }

    if (existing_idx >= 0) {
        // Update existing entry
        update_access_time(&entries[existing_idx]);
        strncpy(entries[existing_idx].object_path, object_path, sizeof(entries[existing_idx].object_path) - 1);
    } else {
        // Add new entry
        index_entry_t *new_entries = (index_entry_t *)realloc(entries, (count + 1) * sizeof(index_entry_t));
        if (!new_entries) {
            free(entries);
            return -1;
        }
        entries = new_entries;

        memcpy(entries[count].hash, hash, COSMO_CACHE_HASH_SIZE);
        time_t now = time(NULL);
        entries[count].last_access = now;
        entries[count].penultimate_access = now;
        entries[count].access_count = 1;
        strncpy(entries[count].object_path, object_path, sizeof(entries[count].object_path) - 1);
        entries[count].object_path[sizeof(entries[count].object_path) - 1] = '\0';
        count++;

        g_cache_state.stats.total_entries++;
    }

    write_index(entries, count);
    free(entries);

    g_cache_state.stats.stores++;
    g_cache_state.stats.total_size += code_size;

    // Check if we need to evict due to entry count
    if (g_cache_state.max_entries > 0 && (int)g_cache_state.stats.total_entries > g_cache_state.max_entries) {
        // Evict 10% of entries
        int evict_count = g_cache_state.max_entries / 10;
        if (evict_count < 1) evict_count = 1;
        cosmo_cache_evict_lru2(evict_count);
    }

    // Check if we need to evict due to cache size
    if (g_cache_state.max_size > 0 && g_cache_state.stats.total_size > g_cache_state.max_size) {
        // Evict 10% of entries to reduce size
        int evict_count = (int)g_cache_state.stats.total_entries / 10;
        if (evict_count < 1) evict_count = 1;
        cosmo_cache_evict_lru2(evict_count);
    }

    // Evict old entries based on timeout
    if (g_cache_state.timeout_seconds > 0) {
        cosmo_cache_evict_timeout();
    }

    return 0;
}

int cosmo_cache_clear(void) {
    if (!g_cache_state.initialized) {
        return -1;
    }

    // Remove all cached objects
    DIR *dir = opendir(g_cache_state.objects_dir);
    if (!dir) {
        return -1;
    }

    struct dirent *entry;
    char path[512];
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        snprintf(path, sizeof(path), "%s/%s",
                 g_cache_state.objects_dir, entry->d_name);

        if (unlink(path) == 0) {
            count++;
        }
    }

    closedir(dir);

    // Reset stats
    g_cache_state.stats.total_entries = 0;
    g_cache_state.stats.total_size = 0;
    g_cache_state.stats.invalidations += count;

    return 0;
}

void cosmo_cache_get_stats(cosmo_cache_stats_t *stats) {
    if (!stats) return;

    if (g_cache_state.initialized) {
        *stats = g_cache_state.stats;
    } else {
        memset(stats, 0, sizeof(cosmo_cache_stats_t));
    }
}

void cosmo_cache_set_enabled(int enabled) {
    if (g_cache_state.initialized) {
        g_cache_state.enabled = enabled ? 1 : 0;
    }
}

int cosmo_cache_is_enabled(void) {
    return g_cache_state.initialized && g_cache_state.enabled;
}

int cosmo_cache_set_max_entries(int max_entries) {
    if (!g_cache_state.initialized) {
        return -1;
    }
    g_cache_state.max_entries = max_entries;
    return 0;
}

int cosmo_cache_get_max_entries(void) {
    return g_cache_state.initialized ? g_cache_state.max_entries : 0;
}

int cosmo_cache_set_max_size(size_t max_bytes) {
    if (!g_cache_state.initialized) {
        return -1;
    }
    g_cache_state.max_size = max_bytes;
    return 0;
}

size_t cosmo_cache_get_max_size(void) {
    return g_cache_state.initialized ? g_cache_state.max_size : 0;
}

int cosmo_cache_set_timeout(time_t timeout_seconds) {
    if (!g_cache_state.initialized) {
        return -1;
    }
    g_cache_state.timeout_seconds = timeout_seconds;
    return 0;
}

int cosmo_cache_evict_timeout(void) {
    if (!g_cache_state.initialized || g_cache_state.timeout_seconds <= 0) {
        return 0;
    }

    // Read index
    index_entry_t *entries = NULL;
    int entry_count = 0;
    if (read_index(&entries, &entry_count) != 0 || !entries) {
        return 0;
    }

    time_t now = time(NULL);
    int evicted = 0;

    // Mark entries to evict
    for (int i = 0; i < entry_count; i++) {
        time_t age = now - entries[i].last_access;
        if (age > g_cache_state.timeout_seconds) {
            // Delete the cached object file
            if (unlink(entries[i].object_path) == 0) {
                evicted++;
                g_cache_state.stats.evictions++;
                if (g_cache_state.stats.total_entries > 0) {
                    g_cache_state.stats.total_entries--;
                }
            }
        }
    }

    // Compact the index by removing evicted entries
    if (evicted > 0) {
        index_entry_t *new_entries = (index_entry_t *)malloc((entry_count - evicted) * sizeof(index_entry_t));
        if (new_entries) {
            int new_count = 0;
            for (int i = 0; i < entry_count; i++) {
                time_t age = now - entries[i].last_access;
                if (age <= g_cache_state.timeout_seconds) {
                    new_entries[new_count++] = entries[i];
                }
            }
            write_index(new_entries, new_count);
            free(new_entries);
        }
    }

    free(entries);
    return evicted;
}

// Comparison function for qsort (sort by penultimate_access ascending)
static int compare_lru2(const void *a, const void *b) {
    const index_entry_t *ea = (const index_entry_t *)a;
    const index_entry_t *eb = (const index_entry_t *)b;

    // Sort by penultimate_access (oldest first)
    if (ea->penultimate_access < eb->penultimate_access) return -1;
    if (ea->penultimate_access > eb->penultimate_access) return 1;

    // Tie-break by last_access
    if (ea->last_access < eb->last_access) return -1;
    if (ea->last_access > eb->last_access) return 1;

    return 0;
}

int cosmo_cache_evict_lru2(int count) {
    if (!g_cache_state.initialized || count <= 0) {
        return 0;
    }

    // Read index
    index_entry_t *entries = NULL;
    int entry_count = 0;
    if (read_index(&entries, &entry_count) != 0 || !entries) {
        return 0;
    }

    if (count > entry_count) {
        count = entry_count;
    }

    // Sort entries by LRU-2 criteria (oldest penultimate_access first)
    qsort(entries, entry_count, sizeof(index_entry_t), compare_lru2);

    // Evict the first 'count' entries (oldest by LRU-2)
    int evicted = 0;
    for (int i = 0; i < count; i++) {
        // Delete the cached object file
        if (unlink(entries[i].object_path) == 0) {
            evicted++;
            g_cache_state.stats.evictions++;
            if (g_cache_state.stats.total_entries > 0) {
                g_cache_state.stats.total_entries--;
            }
        }
    }

    // Compact the index by removing evicted entries
    if (evicted > 0) {
        index_entry_t *new_entries = (index_entry_t *)malloc((entry_count - evicted) * sizeof(index_entry_t));
        if (new_entries) {
            memcpy(new_entries, entries + evicted, (entry_count - evicted) * sizeof(index_entry_t));
            write_index(new_entries, entry_count - evicted);
            free(new_entries);
        }
    }

    free(entries);
    return evicted;
}
