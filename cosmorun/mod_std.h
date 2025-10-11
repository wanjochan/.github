#ifndef COSMORUN_STD_H
#define COSMORUN_STD_H

/*
 * mod_std - Standard library utilities for cosmorun
 *
 * Provides common data structures and algorithms:
 * - Dynamic strings
 * - Hash maps
 * - Vectors (dynamic arrays)
 * - Error handling
 */

#include "cosmo_libc.h"

/* ==================== String Utilities ==================== */

typedef struct {
    char *data;
    size_t len;
    size_t capacity;
} std_string_t;

/* Create/destroy strings */
std_string_t* std_string_new(const char *initial);
std_string_t* std_string_with_capacity(size_t capacity);
void std_string_free(std_string_t *str);

/* String operations */
void std_string_append(std_string_t *str, const char *data);
void std_string_append_char(std_string_t *str, char c);
void std_string_clear(std_string_t *str);
const char* std_string_cstr(std_string_t *str);
size_t std_string_len(std_string_t *str);

/* String formatting */
std_string_t* std_string_format(const char *fmt, ...);

/* ==================== Vector (Dynamic Array) ==================== */

typedef struct {
    void **data;
    size_t len;
    size_t capacity;
} std_vector_t;

/* Create/destroy vectors */
std_vector_t* std_vector_new(void);
std_vector_t* std_vector_with_capacity(size_t capacity);
void std_vector_free(std_vector_t *vec);

/* Vector operations */
void std_vector_push(std_vector_t *vec, void *item);
void* std_vector_pop(std_vector_t *vec);
void* std_vector_get(std_vector_t *vec, size_t index);
void std_vector_set(std_vector_t *vec, size_t index, void *item);
size_t std_vector_len(std_vector_t *vec);
void std_vector_clear(std_vector_t *vec);

/* ==================== Hash Map ==================== */

typedef struct std_hashmap_entry {
    char *key;
    void *value;
    struct std_hashmap_entry *next;
} std_hashmap_entry_t;

typedef struct {
    std_hashmap_entry_t **buckets;
    size_t bucket_count;
    size_t size;
} std_hashmap_t;

/* Create/destroy hashmaps */
std_hashmap_t* std_hashmap_new(void);
std_hashmap_t* std_hashmap_with_capacity(size_t capacity);
void std_hashmap_free(std_hashmap_t *map);

/* HashMap operations */
void std_hashmap_set(std_hashmap_t *map, const char *key, void *value);
void* std_hashmap_get(std_hashmap_t *map, const char *key);
int std_hashmap_has(std_hashmap_t *map, const char *key);
void std_hashmap_remove(std_hashmap_t *map, const char *key);
size_t std_hashmap_size(std_hashmap_t *map);

/* HashMap iteration */
typedef void (*std_hashmap_iter_fn)(const char *key, void *value, void *userdata);
void std_hashmap_foreach(std_hashmap_t *map, std_hashmap_iter_fn fn, void *userdata);

/* ==================== Error Handling ==================== */

typedef struct {
    int code;
    char *message;
} std_error_t;

/* Error functions */
std_error_t* std_error_new(int code, const char *message);
void std_error_free(std_error_t *err);
const char* std_error_message(std_error_t *err);
int std_error_code(std_error_t *err);

#endif /* COSMORUN_STD_H */
