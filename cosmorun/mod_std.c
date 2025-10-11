/*
 * mod_std.c - Standard library utilities implementation
 */

#include "mod_std.h"

/* ==================== String Implementation ==================== */

std_string_t* std_string_new(const char *initial) {
    std_string_t *str = (std_string_t*)malloc(sizeof(std_string_t));
    if (!str) return NULL;

    size_t len = initial ? strlen(initial) : 0;
    str->capacity = len + 16;
    str->data = (char*)malloc(str->capacity);
    if (!str->data) {
        free(str);
        return NULL;
    }

    if (initial) {
        memcpy(str->data, initial, len);
    }
    str->data[len] = '\0';
    str->len = len;

    return str;
}

std_string_t* std_string_with_capacity(size_t capacity) {
    std_string_t *str = (std_string_t*)malloc(sizeof(std_string_t));
    if (!str) return NULL;

    str->capacity = capacity;
    str->data = (char*)malloc(capacity);
    if (!str->data) {
        free(str);
        return NULL;
    }

    str->data[0] = '\0';
    str->len = 0;

    return str;
}

void std_string_free(std_string_t *str) {
    if (!str) return;
    if (str->data) free(str->data);
    free(str);
}

static void std_string_ensure_capacity(std_string_t *str, size_t required) {
    if (str->capacity >= required) return;

    size_t new_capacity = str->capacity * 2;
    while (new_capacity < required) {
        new_capacity *= 2;
    }

    char *new_data = (char*)realloc(str->data, new_capacity);
    if (!new_data) return;  // Keep old data on failure

    str->data = new_data;
    str->capacity = new_capacity;
}

void std_string_append(std_string_t *str, const char *data) {
    if (!str || !data) return;

    size_t data_len = strlen(data);
    std_string_ensure_capacity(str, str->len + data_len + 1);

    memcpy(str->data + str->len, data, data_len);
    str->len += data_len;
    str->data[str->len] = '\0';
}

void std_string_append_char(std_string_t *str, char c) {
    if (!str) return;

    std_string_ensure_capacity(str, str->len + 2);
    str->data[str->len++] = c;
    str->data[str->len] = '\0';
}

void std_string_clear(std_string_t *str) {
    if (!str) return;
    str->len = 0;
    str->data[0] = '\0';
}

const char* std_string_cstr(std_string_t *str) {
    return str ? str->data : NULL;
}

size_t std_string_len(std_string_t *str) {
    return str ? str->len : 0;
}

std_string_t* std_string_format(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Calculate required size
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (size < 0) {
        va_end(args);
        return NULL;
    }

    std_string_t *str = std_string_with_capacity(size + 1);
    if (!str) {
        va_end(args);
        return NULL;
    }

    vsnprintf(str->data, size + 1, fmt, args);
    str->len = size;
    va_end(args);

    return str;
}

/* ==================== Vector Implementation ==================== */

std_vector_t* std_vector_new(void) {
    return std_vector_with_capacity(16);
}

std_vector_t* std_vector_with_capacity(size_t capacity) {
    std_vector_t *vec = (std_vector_t*)malloc(sizeof(std_vector_t));
    if (!vec) return NULL;

    vec->data = (void**)malloc(sizeof(void*) * capacity);
    if (!vec->data) {
        free(vec);
        return NULL;
    }

    vec->len = 0;
    vec->capacity = capacity;

    return vec;
}

void std_vector_free(std_vector_t *vec) {
    if (!vec) return;
    if (vec->data) free(vec->data);
    free(vec);
}

static void std_vector_ensure_capacity(std_vector_t *vec, size_t required) {
    if (vec->capacity >= required) return;

    size_t new_capacity = vec->capacity * 2;
    while (new_capacity < required) {
        new_capacity *= 2;
    }

    void **new_data = (void**)realloc(vec->data, sizeof(void*) * new_capacity);
    if (!new_data) return;

    vec->data = new_data;
    vec->capacity = new_capacity;
}

void std_vector_push(std_vector_t *vec, void *item) {
    if (!vec) return;

    std_vector_ensure_capacity(vec, vec->len + 1);
    vec->data[vec->len++] = item;
}

void* std_vector_pop(std_vector_t *vec) {
    if (!vec || vec->len == 0) return NULL;
    return vec->data[--vec->len];
}

void* std_vector_get(std_vector_t *vec, size_t index) {
    if (!vec || index >= vec->len) return NULL;
    return vec->data[index];
}

void std_vector_set(std_vector_t *vec, size_t index, void *item) {
    if (!vec || index >= vec->len) return;
    vec->data[index] = item;
}

size_t std_vector_len(std_vector_t *vec) {
    return vec ? vec->len : 0;
}

void std_vector_clear(std_vector_t *vec) {
    if (!vec) return;
    vec->len = 0;
}

/* ==================== HashMap Implementation ==================== */

static unsigned long std_hash_string(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    return hash;
}

std_hashmap_t* std_hashmap_new(void) {
    return std_hashmap_with_capacity(16);
}

std_hashmap_t* std_hashmap_with_capacity(size_t capacity) {
    std_hashmap_t *map = (std_hashmap_t*)malloc(sizeof(std_hashmap_t));
    if (!map) return NULL;

    map->buckets = (std_hashmap_entry_t**)calloc(capacity, sizeof(std_hashmap_entry_t*));
    if (!map->buckets) {
        free(map);
        return NULL;
    }

    map->bucket_count = capacity;
    map->size = 0;

    return map;
}

void std_hashmap_free(std_hashmap_t *map) {
    if (!map) return;

    for (size_t i = 0; i < map->bucket_count; i++) {
        std_hashmap_entry_t *entry = map->buckets[i];
        while (entry) {
            std_hashmap_entry_t *next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }

    free(map->buckets);
    free(map);
}

void std_hashmap_set(std_hashmap_t *map, const char *key, void *value) {
    if (!map || !key) return;

    unsigned long hash = std_hash_string(key);
    size_t index = hash % map->bucket_count;

    // Check if key already exists
    std_hashmap_entry_t *entry = map->buckets[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }

    // Create new entry
    std_hashmap_entry_t *new_entry = (std_hashmap_entry_t*)malloc(sizeof(std_hashmap_entry_t));
    if (!new_entry) return;

    new_entry->key = strdup(key);
    new_entry->value = value;
    new_entry->next = map->buckets[index];
    map->buckets[index] = new_entry;
    map->size++;
}

void* std_hashmap_get(std_hashmap_t *map, const char *key) {
    if (!map || !key) return NULL;

    unsigned long hash = std_hash_string(key);
    size_t index = hash % map->bucket_count;

    std_hashmap_entry_t *entry = map->buckets[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }

    return NULL;
}

int std_hashmap_has(std_hashmap_t *map, const char *key) {
    return std_hashmap_get(map, key) != NULL;
}

void std_hashmap_remove(std_hashmap_t *map, const char *key) {
    if (!map || !key) return;

    unsigned long hash = std_hash_string(key);
    size_t index = hash % map->bucket_count;

    std_hashmap_entry_t *entry = map->buckets[index];
    std_hashmap_entry_t *prev = NULL;

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                map->buckets[index] = entry->next;
            }
            free(entry->key);
            free(entry);
            map->size--;
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

size_t std_hashmap_size(std_hashmap_t *map) {
    return map ? map->size : 0;
}

void std_hashmap_foreach(std_hashmap_t *map, std_hashmap_iter_fn fn, void *userdata) {
    if (!map || !fn) return;

    for (size_t i = 0; i < map->bucket_count; i++) {
        std_hashmap_entry_t *entry = map->buckets[i];
        while (entry) {
            fn(entry->key, entry->value, userdata);
            entry = entry->next;
        }
    }
}

/* ==================== Error Handling Implementation ==================== */

std_error_t* std_error_new(int code, const char *message) {
    std_error_t *err = (std_error_t*)malloc(sizeof(std_error_t));
    if (!err) return NULL;

    err->code = code;
    err->message = message ? strdup(message) : NULL;

    return err;
}

void std_error_free(std_error_t *err) {
    if (!err) return;
    if (err->message) free(err->message);
    free(err);
}

const char* std_error_message(std_error_t *err) {
    return err ? err->message : NULL;
}

int std_error_code(std_error_t *err) {
    return err ? err->code : 0;
}
