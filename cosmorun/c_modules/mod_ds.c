/*
 * mod_ds.c - Core Data Structures Library Implementation for v2
 *
 * 使用 System Layer API (shim) 替代直接 libc 调用
 * Uses System Layer API (shim) instead of direct libc calls
 */

#include "mod_ds.h"

/* ==================== Helper Functions ==================== */

uint32_t ds_hash_string(const void *key) {
    if (!key) return 0;
    const char *str = (const char *)key;
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

int ds_compare_string(const void *a, const void *b) {
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return shim_strcmp((const char *)a, (const char *)b);
}

/* ==================== List Implementation ==================== */

ds_list_t *ds_list_new(void) {
    return ds_list_new_capacity(8);
}

ds_list_t *ds_list_new_capacity(size_t capacity) {
    ds_list_t *list = shim_malloc(sizeof(ds_list_t));
    if (!list) return NULL;

    list->items = shim_malloc(sizeof(void *) * capacity);
    if (!list->items) {
        shim_free(list);
        return NULL;
    }

    list->size = 0;
    list->capacity = capacity;
    return list;
}

void ds_list_free(ds_list_t *list) {
    if (!list) return;
    shim_free(list->items);
    shim_free(list);
}

void ds_list_free_with(ds_list_t *list, ds_free_fn free_fn) {
    if (!list) return;
    if (free_fn) {
        for (size_t i = 0; i < list->size; i++) {
            free_fn(list->items[i]);
        }
    }
    ds_list_free(list);
}

static void ds_list_grow(ds_list_t *list) {
    size_t new_capacity = list->capacity * 2;
    void **new_items = shim_realloc(list->items, sizeof(void *) * new_capacity);
    if (!new_items) return; /* Keep old capacity on failure */
    list->items = new_items;
    list->capacity = new_capacity;
}

void ds_list_push(ds_list_t *list, void *item) {
    if (!list) return;
    if (list->size >= list->capacity) {
        ds_list_grow(list);
    }
    list->items[list->size++] = item;
}

void *ds_list_pop(ds_list_t *list) {
    if (!list || list->size == 0) return NULL;
    return list->items[--list->size];
}

void ds_list_insert(ds_list_t *list, size_t index, void *item) {
    if (!list || index > list->size) return;
    if (list->size >= list->capacity) {
        ds_list_grow(list);
    }
    /* Shift elements right */
    shim_memmove(&list->items[index + 1], &list->items[index],
            sizeof(void *) * (list->size - index));
    list->items[index] = item;
    list->size++;
}

void *ds_list_remove(ds_list_t *list, size_t index) {
    if (!list || index >= list->size) return NULL;
    void *item = list->items[index];
    /* Shift elements left */
    shim_memmove(&list->items[index], &list->items[index + 1],
            sizeof(void *) * (list->size - index - 1));
    list->size--;
    return item;
}

void *ds_list_get(ds_list_t *list, size_t index) {
    if (!list || index >= list->size) return NULL;
    return list->items[index];
}

void ds_list_set(ds_list_t *list, size_t index, void *item) {
    if (!list || index >= list->size) return;
    list->items[index] = item;
}

void ds_list_clear(ds_list_t *list) {
    if (!list) return;
    list->size = 0;
}

size_t ds_list_size(ds_list_t *list) {
    return list ? list->size : 0;
}

int ds_list_index_of(ds_list_t *list, void *item, ds_compare_fn cmp) {
    if (!list) return -1;
    if (!cmp) {
        /* Direct pointer comparison */
        for (size_t i = 0; i < list->size; i++) {
            if (list->items[i] == item) return (int)i;
        }
    } else {
        /* Use comparator */
        for (size_t i = 0; i < list->size; i++) {
            if (cmp(list->items[i], item) == 0) return (int)i;
        }
    }
    return -1;
}

bool ds_list_contains(ds_list_t *list, void *item, ds_compare_fn cmp) {
    return ds_list_index_of(list, item, cmp) >= 0;
}

/* Iterator */
ds_iterator_t ds_list_iterator(ds_list_t *list) {
    ds_iterator_t iter;
    iter.context = list;
    iter.index = -1;
    iter.current = NULL;
    iter.valid = (list && list->size > 0);
    return iter;
}

bool ds_iterator_next(ds_iterator_t *iter) {
    if (!iter || !iter->valid) return false;
    ds_list_t *list = (ds_list_t *)iter->context;
    iter->index++;
    if (iter->index >= (int)list->size) {
        iter->valid = false;
        iter->current = NULL;
        return false;
    }
    iter->current = list->items[iter->index];
    return true;
}

void *ds_iterator_get(ds_iterator_t *iter) {
    return (iter && iter->valid) ? iter->current : NULL;
}

/* ==================== Map Implementation ==================== */

ds_map_t *ds_map_new(void) {
    return ds_map_new_with(ds_hash_string, ds_compare_string);
}

ds_map_t *ds_map_new_with(ds_hash_fn hash_fn, ds_compare_fn key_cmp) {
    ds_map_t *map = shim_malloc(sizeof(ds_map_t));
    if (!map) return NULL;

    map->buckets = shim_calloc(DS_MAP_DEFAULT_CAPACITY, sizeof(ds_map_entry_t *));
    if (!map->buckets) {
        shim_free(map);
        return NULL;
    }

    map->capacity = DS_MAP_DEFAULT_CAPACITY;
    map->size = 0;
    map->hash_fn = hash_fn ? hash_fn : ds_hash_string;
    map->key_cmp = key_cmp ? key_cmp : ds_compare_string;
    return map;
}

void ds_map_free(ds_map_t *map) {
    if (!map) return;
    for (size_t i = 0; i < map->capacity; i++) {
        ds_map_entry_t *entry = map->buckets[i];
        while (entry) {
            ds_map_entry_t *next = entry->next;
            shim_free(entry);
            entry = next;
        }
    }
    shim_free(map->buckets);
    shim_free(map);
}

void ds_map_free_with(ds_map_t *map, ds_free_fn key_free, ds_free_fn value_free) {
    if (!map) return;
    for (size_t i = 0; i < map->capacity; i++) {
        ds_map_entry_t *entry = map->buckets[i];
        while (entry) {
            ds_map_entry_t *next = entry->next;
            if (key_free) key_free(entry->key);
            if (value_free) value_free(entry->value);
            shim_free(entry);
            entry = next;
        }
    }
    shim_free(map->buckets);
    shim_free(map);
}

static void ds_map_resize(ds_map_t *map) {
    size_t new_capacity = map->capacity * 2;
    ds_map_entry_t **new_buckets = shim_calloc(new_capacity, sizeof(ds_map_entry_t *));
    if (!new_buckets) return;

    /* Rehash all entries */
    for (size_t i = 0; i < map->capacity; i++) {
        ds_map_entry_t *entry = map->buckets[i];
        while (entry) {
            ds_map_entry_t *next = entry->next;
            size_t bucket = entry->hash % new_capacity;
            entry->next = new_buckets[bucket];
            new_buckets[bucket] = entry;
            entry = next;
        }
    }

    shim_free(map->buckets);
    map->buckets = new_buckets;
    map->capacity = new_capacity;
}

void ds_map_put(ds_map_t *map, void *key, void *value) {
    if (!map || !key) return;

    uint32_t hash = map->hash_fn(key);
    size_t bucket = hash % map->capacity;

    /* Check if key exists */
    ds_map_entry_t *entry = map->buckets[bucket];
    while (entry) {
        if (entry->hash == hash && map->key_cmp(entry->key, key) == 0) {
            entry->value = value;  /* Update existing */
            return;
        }
        entry = entry->next;
    }

    /* Add new entry */
    ds_map_entry_t *new_entry = shim_malloc(sizeof(ds_map_entry_t));
    if (!new_entry) return;

    new_entry->key = key;
    new_entry->value = value;
    new_entry->hash = hash;
    new_entry->next = map->buckets[bucket];
    map->buckets[bucket] = new_entry;
    map->size++;

    /* Resize if load factor exceeded (0.75 = 3/4) */
    if (map->size * 4 > map->capacity * 3) {
        ds_map_resize(map);
    }
}

void *ds_map_get(ds_map_t *map, const void *key) {
    if (!map || !key) return NULL;

    uint32_t hash = map->hash_fn(key);
    size_t bucket = hash % map->capacity;

    ds_map_entry_t *entry = map->buckets[bucket];
    while (entry) {
        if (entry->hash == hash && map->key_cmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

void *ds_map_remove(ds_map_t *map, const void *key) {
    if (!map || !key) return NULL;

    uint32_t hash = map->hash_fn(key);
    size_t bucket = hash % map->capacity;

    ds_map_entry_t **entry_ptr = &map->buckets[bucket];
    while (*entry_ptr) {
        ds_map_entry_t *entry = *entry_ptr;
        if (entry->hash == hash && map->key_cmp(entry->key, key) == 0) {
            void *value = entry->value;
            *entry_ptr = entry->next;
            shim_free(entry);
            map->size--;
            return value;
        }
        entry_ptr = &entry->next;
    }
    return NULL;
}

bool ds_map_contains(ds_map_t *map, const void *key) {
    return ds_map_get(map, key) != NULL;
}

void ds_map_clear(ds_map_t *map) {
    if (!map) return;
    for (size_t i = 0; i < map->capacity; i++) {
        ds_map_entry_t *entry = map->buckets[i];
        while (entry) {
            ds_map_entry_t *next = entry->next;
            shim_free(entry);
            entry = next;
        }
        map->buckets[i] = NULL;
    }
    map->size = 0;
}

size_t ds_map_size(ds_map_t *map) {
    return map ? map->size : 0;
}

ds_list_t *ds_map_keys(ds_map_t *map) {
    if (!map) return NULL;
    ds_list_t *keys = ds_list_new_capacity(map->size);
    for (size_t i = 0; i < map->capacity; i++) {
        ds_map_entry_t *entry = map->buckets[i];
        while (entry) {
            ds_list_push(keys, entry->key);
            entry = entry->next;
        }
    }
    return keys;
}

ds_list_t *ds_map_values(ds_map_t *map) {
    if (!map) return NULL;
    ds_list_t *values = ds_list_new_capacity(map->size);
    for (size_t i = 0; i < map->capacity; i++) {
        ds_map_entry_t *entry = map->buckets[i];
        while (entry) {
            ds_list_push(values, entry->value);
            entry = entry->next;
        }
    }
    return values;
}

ds_iterator_t ds_map_iterator(ds_map_t *map) {
    ds_iterator_t iter;
    iter.context = map;
    iter.index = -1;
    iter.current = NULL;
    iter.valid = (map && map->size > 0);
    return iter;
}

/* ==================== Set Implementation ==================== */

ds_set_t *ds_set_new(void) {
    return ds_set_new_with(ds_hash_string, ds_compare_string);
}

ds_set_t *ds_set_new_with(ds_hash_fn hash_fn, ds_compare_fn cmp) {
    ds_set_t *set = shim_malloc(sizeof(ds_set_t));
    if (!set) return NULL;
    set->map = ds_map_new_with(hash_fn, cmp);
    if (!set->map) {
        shim_free(set);
        return NULL;
    }
    return set;
}

void ds_set_free(ds_set_t *set) {
    if (!set) return;
    ds_map_free(set->map);
    shim_free(set);
}

void ds_set_free_with(ds_set_t *set, ds_free_fn free_fn) {
    if (!set) return;
    ds_map_free_with(set->map, free_fn, NULL);
    shim_free(set);
}

void ds_set_add(ds_set_t *set, void *item) {
    if (!set || !item) return;
    ds_map_put(set->map, item, (void *)1);  /* Use dummy value */
}

bool ds_set_remove(ds_set_t *set, const void *item) {
    if (!set || !item) return false;
    return ds_map_remove(set->map, item) != NULL;
}

bool ds_set_contains(ds_set_t *set, const void *item) {
    if (!set || !item) return false;
    return ds_map_contains(set->map, item);
}

void ds_set_clear(ds_set_t *set) {
    if (!set) return;
    ds_map_clear(set->map);
}

size_t ds_set_size(ds_set_t *set) {
    return set ? ds_map_size(set->map) : 0;
}

ds_set_t *ds_set_union(ds_set_t *a, ds_set_t *b) {
    if (!a || !b) return NULL;
    ds_set_t *result = ds_set_new();

    /* Add all from a */
    ds_list_t *a_items = ds_map_keys(a->map);
    for (size_t i = 0; i < a_items->size; i++) {
        ds_set_add(result, a_items->items[i]);
    }
    ds_list_free(a_items);

    /* Add all from b */
    ds_list_t *b_items = ds_map_keys(b->map);
    for (size_t i = 0; i < b_items->size; i++) {
        ds_set_add(result, b_items->items[i]);
    }
    ds_list_free(b_items);

    return result;
}

ds_set_t *ds_set_intersection(ds_set_t *a, ds_set_t *b) {
    if (!a || !b) return NULL;
    ds_set_t *result = ds_set_new();

    /* Add items in both */
    ds_list_t *a_items = ds_map_keys(a->map);
    for (size_t i = 0; i < a_items->size; i++) {
        if (ds_set_contains(b, a_items->items[i])) {
            ds_set_add(result, a_items->items[i]);
        }
    }
    ds_list_free(a_items);

    return result;
}

ds_set_t *ds_set_difference(ds_set_t *a, ds_set_t *b) {
    if (!a || !b) return NULL;
    ds_set_t *result = ds_set_new();

    /* Add items in a but not in b */
    ds_list_t *a_items = ds_map_keys(a->map);
    for (size_t i = 0; i < a_items->size; i++) {
        if (!ds_set_contains(b, a_items->items[i])) {
            ds_set_add(result, a_items->items[i]);
        }
    }
    ds_list_free(a_items);

    return result;
}

ds_list_t *ds_set_to_list(ds_set_t *set) {
    return set ? ds_map_keys(set->map) : NULL;
}

ds_iterator_t ds_set_iterator(ds_set_t *set) {
    return set ? ds_map_iterator(set->map) : (ds_iterator_t){0};
}

/* ==================== Queue Implementation ==================== */

ds_queue_t *ds_queue_new(void) {
    ds_queue_t *queue = shim_malloc(sizeof(ds_queue_t));
    if (!queue) return NULL;
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    return queue;
}

void ds_queue_free(ds_queue_t *queue) {
    if (!queue) return;
    while (queue->head) {
        ds_queue_node_t *next = queue->head->next;
        shim_free(queue->head);
        queue->head = next;
    }
    shim_free(queue);
}

void ds_queue_free_with(ds_queue_t *queue, ds_free_fn free_fn) {
    if (!queue) return;
    if (free_fn) {
        ds_queue_node_t *node = queue->head;
        while (node) {
            free_fn(node->data);
            node = node->next;
        }
    }
    ds_queue_free(queue);
}

void ds_queue_enqueue(ds_queue_t *queue, void *item) {
    if (!queue) return;

    ds_queue_node_t *node = shim_malloc(sizeof(ds_queue_node_t));
    if (!node) return;

    node->data = item;
    node->next = NULL;

    if (queue->tail) {
        queue->tail->next = node;
    } else {
        queue->head = node;
    }
    queue->tail = node;
    queue->size++;
}

void *ds_queue_dequeue(ds_queue_t *queue) {
    if (!queue || !queue->head) return NULL;

    ds_queue_node_t *node = queue->head;
    void *data = node->data;

    queue->head = node->next;
    if (!queue->head) {
        queue->tail = NULL;
    }

    shim_free(node);
    queue->size--;
    return data;
}

void *ds_queue_peek(ds_queue_t *queue) {
    return (queue && queue->head) ? queue->head->data : NULL;
}

bool ds_queue_is_empty(ds_queue_t *queue) {
    return !queue || queue->size == 0;
}

size_t ds_queue_size(ds_queue_t *queue) {
    return queue ? queue->size : 0;
}

void ds_queue_clear(ds_queue_t *queue) {
    if (!queue) return;
    while (queue->head) {
        ds_queue_node_t *next = queue->head->next;
        shim_free(queue->head);
        queue->head = next;
    }
    queue->tail = NULL;
    queue->size = 0;
}

/* ==================== Stack Implementation ==================== */

ds_stack_t *ds_stack_new(void) {
    ds_stack_t *stack = shim_malloc(sizeof(ds_stack_t));
    if (!stack) return NULL;
    stack->top = NULL;
    stack->size = 0;
    return stack;
}

void ds_stack_free(ds_stack_t *stack) {
    if (!stack) return;
    while (stack->top) {
        ds_stack_node_t *next = stack->top->next;
        shim_free(stack->top);
        stack->top = next;
    }
    shim_free(stack);
}

void ds_stack_free_with(ds_stack_t *stack, ds_free_fn free_fn) {
    if (!stack) return;
    if (free_fn) {
        ds_stack_node_t *node = stack->top;
        while (node) {
            free_fn(node->data);
            node = node->next;
        }
    }
    ds_stack_free(stack);
}

void ds_stack_push(ds_stack_t *stack, void *item) {
    if (!stack) return;

    ds_stack_node_t *node = shim_malloc(sizeof(ds_stack_node_t));
    if (!node) return;

    node->data = item;
    node->next = stack->top;
    stack->top = node;
    stack->size++;
}

void *ds_stack_pop(ds_stack_t *stack) {
    if (!stack || !stack->top) return NULL;

    ds_stack_node_t *node = stack->top;
    void *data = node->data;

    stack->top = node->next;
    shim_free(node);
    stack->size--;
    return data;
}

void *ds_stack_peek(ds_stack_t *stack) {
    return (stack && stack->top) ? stack->top->data : NULL;
}

bool ds_stack_is_empty(ds_stack_t *stack) {
    return !stack || stack->size == 0;
}

size_t ds_stack_size(ds_stack_t *stack) {
    return stack ? stack->size : 0;
}

void ds_stack_clear(ds_stack_t *stack) {
    if (!stack) return;
    while (stack->top) {
        ds_stack_node_t *next = stack->top->next;
        shim_free(stack->top);
        stack->top = next;
    }
    stack->size = 0;
}
