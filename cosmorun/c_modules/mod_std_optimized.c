/*
 * mod_std_optimized.c - High-performance HashMap implementation
 *
 * Optimizations applied:
 * 1. Robin Hood hashing (open addressing) - better cache locality
 * 2. MurmurHash3 - superior hash distribution
 * 3. Inline critical functions - reduced call overhead
 * 4. Dynamic resizing with optimal load factor (75%)
 * 5. Distance-based probing - faster lookups
 * 6. Single allocation for entry arrays - reduced malloc overhead
 */

#include "mod_std_optimized.h"
/* 迁移到 v2 System Layer: 使用 shim_* API 替代 libc 调用 */

/* ==================== MurmurHash3 Implementation ==================== */

static inline uint32_t murmur3_rotl32(uint32_t x, int8_t r) {
    return (x << r) | (x >> (32 - r));
}

static inline uint32_t murmur3_fmix32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

static inline uint32_t std_hash_murmur3(const char *key, uint32_t seed) {
    const uint8_t *data = (const uint8_t *)key;
    const int len = (int)shim_strlen(key);
    const int nblocks = len / 4;

    uint32_t h1 = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    // Body
    const uint32_t *blocks = (const uint32_t *)(data + nblocks * 4);
    for (int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i];

        k1 *= c1;
        k1 = murmur3_rotl32(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = murmur3_rotl32(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

    // Tail
    const uint8_t *tail = (const uint8_t *)(data + nblocks * 4);
    uint32_t k1 = 0;

    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
                k1 *= c1;
                k1 = murmur3_rotl32(k1, 15);
                k1 *= c2;
                h1 ^= k1;
    }

    // Finalization
    h1 ^= len;
    h1 = murmur3_fmix32(h1);

    return h1;
}

/* ==================== Robin Hood HashMap Implementation ==================== */

#define HASHMAP_INITIAL_CAPACITY 16
#define HASHMAP_MAX_LOAD_FACTOR 0.75
#define HASHMAP_SEED 0x9747b28c

static inline int opt_entry_is_empty(opt_hashmap_entry_t *entry) {
    return entry->key == NULL;
}

static inline uint32_t opt_entry_distance(opt_hashmap_t *map, size_t idx, uint32_t hash) {
    size_t ideal = hash % map->capacity;
    if (idx >= ideal) {
        return idx - ideal;
    }
    return map->capacity + idx - ideal;
}

opt_hashmap_t* opt_hashmap_new(void) {
    return opt_hashmap_with_capacity(HASHMAP_INITIAL_CAPACITY);
}

opt_hashmap_t* opt_hashmap_with_capacity(size_t capacity) {
    /* 使用 shim_malloc 替代 malloc */
    opt_hashmap_t *map = (opt_hashmap_t *)shim_malloc(sizeof(opt_hashmap_t));
    if (!map) return NULL;

    /* 单次分配所有项：更好的缓存局部性，使用 shim_calloc 替代 calloc */
    map->entries = (opt_hashmap_entry_t *)shim_calloc(capacity, sizeof(opt_hashmap_entry_t));
    if (!map->entries) {
        shim_free(map);
        return NULL;
    }

    map->capacity = capacity;
    map->size = 0;
    map->resize_threshold = (size_t)(capacity * HASHMAP_MAX_LOAD_FACTOR);

    return map;
}

void opt_hashmap_free(opt_hashmap_t *map) {
    if (!map) return;

    /* 单次遍历释放所有键，使用 shim_free 替代 free */
    for (size_t i = 0; i < map->capacity; i++) {
        if (!opt_entry_is_empty(&map->entries[i])) {
            shim_free(map->entries[i].key);
        }
    }

    shim_free(map->entries);
    shim_free(map);
}

static void opt_hashmap_resize(opt_hashmap_t *map) {
    size_t old_capacity = map->capacity;
    opt_hashmap_entry_t *old_entries = map->entries;

    /* 容量翻倍，使用 shim_calloc 替代 calloc */
    size_t new_capacity = old_capacity * 2;
    opt_hashmap_entry_t *new_entries = (opt_hashmap_entry_t *)shim_calloc(
        new_capacity, sizeof(opt_hashmap_entry_t));

    if (!new_entries) return; /* 分配失败时保持旧项，使用 shim_calloc 替代 calloc */

    map->entries = new_entries;
    map->capacity = new_capacity;
    map->size = 0;
    map->resize_threshold = (size_t)(new_capacity * HASHMAP_MAX_LOAD_FACTOR);

    /* 重新哈希所有项，使用 shim_free 替代 free */
    for (size_t i = 0; i < old_capacity; i++) {
        if (!opt_entry_is_empty(&old_entries[i])) {
            opt_hashmap_set(map, old_entries[i].key, old_entries[i].value);
            shim_free(old_entries[i].key); /* 重新哈希后释放旧键 */
        }
    }

    shim_free(old_entries);
}

void opt_hashmap_set(opt_hashmap_t *map, const char *key, void *value) {
    if (!map || !key) return;

    /* 检查是否需要重新调整大小 */
    if (map->size >= map->resize_threshold) {
        opt_hashmap_resize(map);
    }

    uint32_t hash = std_hash_murmur3(key, HASHMAP_SEED);
    size_t idx = hash % map->capacity;

    /* 为 Robin Hood 插入准备新项，使用 shim_strdup 替代 strdup */
    opt_hashmap_entry_t new_entry;
    new_entry.key = shim_strdup(key);
    new_entry.value = value;
    new_entry.hash = hash;
    uint32_t distance = 0;

    /* Robin Hood 线性探测，使用 shim_strcmp 替代 strcmp */
    while (1) {
        opt_hashmap_entry_t *curr = &map->entries[idx];

        /* 空槽 - 在这里插入 */
        if (opt_entry_is_empty(curr)) {
            *curr = new_entry;
            map->size++;
            return;
        }

        /* 键存在 - 更新值，使用 shim_strcmp 替代 strcmp */
        if (curr->hash == hash && shim_strcmp(curr->key, key) == 0) {
            shim_free(new_entry.key); /* 释放重复的键，使用 shim_free 替代 free */
            curr->value = value;
            return;
        }

        /* Robin Hood: 从富人那里偷（如果新项离家更远则交换） */
        uint32_t curr_distance = opt_entry_distance(map, idx, curr->hash);
        if (distance > curr_distance) {
            opt_hashmap_entry_t temp = *curr;
            *curr = new_entry;
            new_entry = temp;
            distance = curr_distance;
        }

        /* 移到下一个槽 */
        idx = (idx + 1) % map->capacity;
        distance++;
    }
}

void* opt_hashmap_get(opt_hashmap_t *map, const char *key) {
    if (!map || !key) return NULL;

    uint32_t hash = std_hash_murmur3(key, HASHMAP_SEED);
    size_t idx = hash % map->capacity;
    uint32_t distance = 0;

    /* 线性探测并尽早退出 */
    while (1) {
        opt_hashmap_entry_t *curr = &map->entries[idx];

        /* 空槽或项离理想位置太远 - 键未找到 */
        if (opt_entry_is_empty(curr)) {
            return NULL;
        }

        uint32_t curr_distance = opt_entry_distance(map, idx, curr->hash);
        if (distance > curr_distance) {
            return NULL; /* Robin Hood 属性：我们已经走得太远了 */
        }

        /* 检查这是否是我们的键，使用 shim_strcmp 替代 strcmp */
        if (curr->hash == hash && shim_strcmp(curr->key, key) == 0) {
            return curr->value;
        }

        idx = (idx + 1) % map->capacity;
        distance++;
    }
}

int opt_hashmap_has(opt_hashmap_t *map, const char *key) {
    return opt_hashmap_get(map, key) != NULL;
}

void opt_hashmap_remove(opt_hashmap_t *map, const char *key) {
    if (!map || !key) return;

    uint32_t hash = std_hash_murmur3(key, HASHMAP_SEED);
    size_t idx = hash % map->capacity;
    uint32_t distance = 0;

    /* 查找项 */
    while (1) {
        opt_hashmap_entry_t *curr = &map->entries[idx];

        if (opt_entry_is_empty(curr)) {
            return; /* 未找到 */
        }

        uint32_t curr_distance = opt_entry_distance(map, idx, curr->hash);
        if (distance > curr_distance) {
            return; /* 未找到 */
        }

        /* 找到 - 现在向后移动项以维护 Robin Hood 不变性，使用 shim_strcmp 替代 strcmp */
        if (curr->hash == hash && shim_strcmp(curr->key, key) == 0) {
            /* 找到 - 使用 shim_free 替代 free */
            shim_free(curr->key);

            size_t next_idx = (idx + 1) % map->capacity;
            while (!opt_entry_is_empty(&map->entries[next_idx])) {
                uint32_t next_distance = opt_entry_distance(map, next_idx,
                                                             map->entries[next_idx].hash);
                if (next_distance == 0) {
                    break; /* 下一项在理想位置 */
                }

                /* 向后移动项 */
                map->entries[idx] = map->entries[next_idx];
                idx = next_idx;
                next_idx = (next_idx + 1) % map->capacity;
            }

            /* 标记最后移动的位置为空 */
            map->entries[idx].key = NULL;
            map->entries[idx].value = NULL;
            map->size--;
            return;
        }

        idx = (idx + 1) % map->capacity;
        distance++;
    }
}

size_t opt_hashmap_size(opt_hashmap_t *map) {
    return map ? map->size : 0;
}

void opt_hashmap_foreach(opt_hashmap_t *map, opt_hashmap_iter_fn fn, void *userdata) {
    if (!map || !fn) return;

    for (size_t i = 0; i < map->capacity; i++) {
        if (!opt_entry_is_empty(&map->entries[i])) {
            fn(map->entries[i].key, map->entries[i].value, userdata);
        }
    }
}

void opt_hashmap_clear(opt_hashmap_t *map) {
    if (!map) return;

    /* 清除所有项，使用 shim_free 替代 free */
    for (size_t i = 0; i < map->capacity; i++) {
        if (!opt_entry_is_empty(&map->entries[i])) {
            shim_free(map->entries[i].key);
            map->entries[i].key = NULL;
            map->entries[i].value = NULL;
        }
    }

    map->size = 0;
}

double opt_hashmap_load_factor(opt_hashmap_t *map) {
    if (!map || map->capacity == 0) return 0.0;
    return (double)map->size / (double)map->capacity;
}

void opt_hashmap_stats(opt_hashmap_t *map, opt_hashmap_stats_t *stats) {
    if (!map || !stats) return;

    stats->size = map->size;
    stats->capacity = map->capacity;
    stats->load_factor = opt_hashmap_load_factor(map);

    // Calculate average probe distance
    size_t total_distance = 0;
    size_t count = 0;

    for (size_t i = 0; i < map->capacity; i++) {
        if (!opt_entry_is_empty(&map->entries[i])) {
            total_distance += opt_entry_distance(map, i, map->entries[i].hash);
            count++;
        }
    }

    stats->avg_probe_distance = count > 0 ? (double)total_distance / count : 0.0;
}
