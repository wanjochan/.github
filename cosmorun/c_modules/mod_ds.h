#include "../cosmorun_system/libc_shim/sys_libc_shim.h"
/*
 * mod_ds.h - Core Data Structures Library for CosmoRun v2
 *
 * 提供基础容器: List, Map, Set, Queue, Stack
 * 所有结构使用通用 void* 存储以支持类型无关的操作
 *
 * Provides essential containers: List, Map, Set, Queue, Stack
 * All structures use generic void* storage for type-agnostic operations
 */

#ifndef MOD_DS_H
#define MOD_DS_H

/* Types provided by sys_libc_shim.h and cosmo_libc.h */

/* ==================== Common Types ==================== */

typedef void (*ds_free_fn)(void *item);
typedef int (*ds_compare_fn)(const void *a, const void *b);
typedef uint32_t (*ds_hash_fn)(const void *key);

/* Default hash function for string keys */
uint32_t ds_hash_string(const void *key);

/* Default compare function for string keys */
int ds_compare_string(const void *a, const void *b);

/* ==================== Iterator ==================== */

typedef struct ds_iterator {
    void *current;
    void *context;
    int index;
    bool valid;
} ds_iterator_t;

/* ==================== List (Dynamic Array) ==================== */

typedef struct ds_list {
    void **items;
    size_t size;
    size_t capacity;
} ds_list_t;

/* Create/destroy */
ds_list_t *ds_list_new(void);
ds_list_t *ds_list_new_capacity(size_t capacity);
void ds_list_free(ds_list_t *list);
void ds_list_free_with(ds_list_t *list, ds_free_fn free_fn);

/* Operations - O(1) amortized append */
void ds_list_push(ds_list_t *list, void *item);
void *ds_list_pop(ds_list_t *list);
void ds_list_insert(ds_list_t *list, size_t index, void *item);
void *ds_list_remove(ds_list_t *list, size_t index);
void *ds_list_get(ds_list_t *list, size_t index);
void ds_list_set(ds_list_t *list, size_t index, void *item);
void ds_list_clear(ds_list_t *list);
size_t ds_list_size(ds_list_t *list);

/* Search - O(n) */
int ds_list_index_of(ds_list_t *list, void *item, ds_compare_fn cmp);
bool ds_list_contains(ds_list_t *list, void *item, ds_compare_fn cmp);

/* Iterator */
ds_iterator_t ds_list_iterator(ds_list_t *list);
bool ds_iterator_next(ds_iterator_t *iter);
void *ds_iterator_get(ds_iterator_t *iter);

/* ==================== Map (Hash Table) ==================== */

#define DS_MAP_DEFAULT_CAPACITY 16
#define DS_MAP_LOAD_FACTOR 0.75

typedef struct ds_map_entry {
    void *key;
    void *value;
    uint32_t hash;
    struct ds_map_entry *next;
} ds_map_entry_t;

typedef struct ds_map {
    ds_map_entry_t **buckets;
    size_t capacity;
    size_t size;
    ds_hash_fn hash_fn;
    ds_compare_fn key_cmp;
} ds_map_t;

/* Create/destroy */
ds_map_t *ds_map_new(void);
ds_map_t *ds_map_new_with(ds_hash_fn hash_fn, ds_compare_fn key_cmp);
void ds_map_free(ds_map_t *map);
void ds_map_free_with(ds_map_t *map, ds_free_fn key_free, ds_free_fn value_free);

/* Operations - O(1) average */
void ds_map_put(ds_map_t *map, void *key, void *value);
void *ds_map_get(ds_map_t *map, const void *key);
void *ds_map_remove(ds_map_t *map, const void *key);
bool ds_map_contains(ds_map_t *map, const void *key);
void ds_map_clear(ds_map_t *map);
size_t ds_map_size(ds_map_t *map);

/* Utility */
ds_list_t *ds_map_keys(ds_map_t *map);
ds_list_t *ds_map_values(ds_map_t *map);

/* Iterator */
ds_iterator_t ds_map_iterator(ds_map_t *map);

/* ==================== Set (Hash Set) ==================== */

typedef struct ds_set {
    ds_map_t *map;  /* Reuse map implementation with NULL values */
} ds_set_t;

/* Create/destroy */
ds_set_t *ds_set_new(void);
ds_set_t *ds_set_new_with(ds_hash_fn hash_fn, ds_compare_fn cmp);
void ds_set_free(ds_set_t *set);
void ds_set_free_with(ds_set_t *set, ds_free_fn free_fn);

/* Operations - O(1) average */
void ds_set_add(ds_set_t *set, void *item);
bool ds_set_remove(ds_set_t *set, const void *item);
bool ds_set_contains(ds_set_t *set, const void *item);
void ds_set_clear(ds_set_t *set);
size_t ds_set_size(ds_set_t *set);

/* Set operations */
ds_set_t *ds_set_union(ds_set_t *a, ds_set_t *b);
ds_set_t *ds_set_intersection(ds_set_t *a, ds_set_t *b);
ds_set_t *ds_set_difference(ds_set_t *a, ds_set_t *b);

/* Utility */
ds_list_t *ds_set_to_list(ds_set_t *set);

/* Iterator */
ds_iterator_t ds_set_iterator(ds_set_t *set);

/* ==================== Queue (FIFO) ==================== */

typedef struct ds_queue_node {
    void *data;
    struct ds_queue_node *next;
} ds_queue_node_t;

typedef struct ds_queue {
    ds_queue_node_t *head;
    ds_queue_node_t *tail;
    size_t size;
} ds_queue_t;

/* Create/destroy */
ds_queue_t *ds_queue_new(void);
void ds_queue_free(ds_queue_t *queue);
void ds_queue_free_with(ds_queue_t *queue, ds_free_fn free_fn);

/* Operations - O(1) for all */
void ds_queue_enqueue(ds_queue_t *queue, void *item);
void *ds_queue_dequeue(ds_queue_t *queue);
void *ds_queue_peek(ds_queue_t *queue);
bool ds_queue_is_empty(ds_queue_t *queue);
size_t ds_queue_size(ds_queue_t *queue);
void ds_queue_clear(ds_queue_t *queue);

/* ==================== Stack (LIFO) ==================== */

typedef struct ds_stack_node {
    void *data;
    struct ds_stack_node *next;
} ds_stack_node_t;

typedef struct ds_stack {
    ds_stack_node_t *top;
    size_t size;
} ds_stack_t;

/* Create/destroy */
ds_stack_t *ds_stack_new(void);
void ds_stack_free(ds_stack_t *stack);
void ds_stack_free_with(ds_stack_t *stack, ds_free_fn free_fn);

/* Operations - O(1) for all */
void ds_stack_push(ds_stack_t *stack, void *item);
void *ds_stack_pop(ds_stack_t *stack);
void *ds_stack_peek(ds_stack_t *stack);
bool ds_stack_is_empty(ds_stack_t *stack);
size_t ds_stack_size(ds_stack_t *stack);
void ds_stack_clear(ds_stack_t *stack);

#endif /* MOD_DS_H */
