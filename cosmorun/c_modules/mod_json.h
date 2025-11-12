#ifndef COSMORUN_JSON_H
#define COSMORUN_JSON_H

/*
 * mod_json - JSON parsing and serialization for cosmorun
 *
 * Provides JSON parsing, serialization, and manipulation using cJSON library
 * Encapsulation pattern follows mod_duckdb.c for dynamic loading
 */

#include "src/cosmo_libc.h"
#include "mod_std.h"

/* ==================== JSON Value Types ==================== */

typedef enum {
    JSON_TYPE_NULL = 0,
    JSON_TYPE_FALSE = 1,
    JSON_TYPE_TRUE = 2,
    JSON_TYPE_NUMBER = 3,
    JSON_TYPE_STRING = 4,
    JSON_TYPE_ARRAY = 5,
    JSON_TYPE_OBJECT = 6
} json_type_t;

/* Opaque JSON value handle (wraps cJSON*) */
typedef struct json_value_t json_value_t;

/* ==================== JSON Context ==================== */

typedef struct {
    void *lib_handle;           // dlopen handle for libcJSON

    // Function pointers for cJSON API
    void* (*parse_fn)(const char *value);
    void* (*parse_with_length_fn)(const char *value, size_t buffer_length);
    char* (*print_fn)(const void *item);
    char* (*print_unformatted_fn)(const void *item);
    void  (*delete_fn)(void *item);
    void  (*free_fn)(void *ptr);

    // Object operations
    void* (*create_object_fn)(void);
    void* (*get_object_item_fn)(const void *object, const char *string);
    int   (*has_object_item_fn)(const void *object, const char *string);
    int   (*add_item_to_object_fn)(void *object, const char *string, void *item);
    void* (*detach_item_from_object_fn)(void *object, const char *string);
    void  (*delete_item_from_object_fn)(void *object, const char *string);

    // Array operations
    void* (*create_array_fn)(void);
    int   (*get_array_size_fn)(const void *array);
    void* (*get_array_item_fn)(const void *array, int index);
    int   (*add_item_to_array_fn)(void *array, void *item);
    void* (*detach_item_from_array_fn)(void *array, int which);
    void  (*delete_item_from_array_fn)(void *array, int which);

    // Value creation
    void* (*create_null_fn)(void);
    void* (*create_true_fn)(void);
    void* (*create_false_fn)(void);
    void* (*create_bool_fn)(int boolean);
    void* (*create_number_fn)(double num);
    void* (*create_string_fn)(const char *string);

    // Value inspection
    int   (*is_null_fn)(const void *item);
    int   (*is_false_fn)(const void *item);
    int   (*is_true_fn)(const void *item);
    int   (*is_bool_fn)(const void *item);
    int   (*is_number_fn)(const void *item);
    int   (*is_string_fn)(const void *item);
    int   (*is_array_fn)(const void *item);
    int   (*is_object_fn)(const void *item);
} json_context_t;

/* ==================== Context Management ==================== */

/**
 * Initialize JSON context and load cJSON library
 * @param lib_path Optional library path (NULL for auto-detection)
 * @return Pointer to allocated context, or NULL on failure
 */
json_context_t* json_init(const char *lib_path);

/**
 * Cleanup JSON context and free resources
 * @param ctx JSON context
 */
void json_cleanup(json_context_t *ctx);

/* ==================== Parsing & Serialization ==================== */

/**
 * Parse JSON string
 * @param ctx JSON context
 * @param str JSON string to parse
 * @return Parsed JSON value, or NULL on error
 */
json_value_t* json_parse(json_context_t *ctx, const char *str);

/**
 * Parse JSON string with known length
 * @param ctx JSON context
 * @param str JSON string to parse
 * @param length Length of string
 * @return Parsed JSON value, or NULL on error
 */
json_value_t* json_parse_length(json_context_t *ctx, const char *str, size_t length);

/**
 * Serialize JSON value to string (formatted)
 * @param ctx JSON context
 * @param val JSON value
 * @return Allocated JSON string (must be freed with json_free_string)
 */
char* json_stringify(json_context_t *ctx, json_value_t *val);

/**
 * Serialize JSON value to string (compact)
 * @param ctx JSON context
 * @param val JSON value
 * @return Allocated JSON string (must be freed with json_free_string)
 */
char* json_stringify_compact(json_context_t *ctx, json_value_t *val);

/**
 * Free JSON-allocated string
 * @param ctx JSON context
 * @param str String to free
 */
void json_free_string(json_context_t *ctx, char *str);

/**
 * Free JSON value
 * @param ctx JSON context
 * @param val JSON value to free
 */
void json_free(json_context_t *ctx, json_value_t *val);

/* ==================== Value Type Inspection ==================== */

/**
 * Get JSON value type
 * @param ctx JSON context
 * @param val JSON value
 * @return Type of the value
 */
json_type_t json_type(json_context_t *ctx, json_value_t *val);

int json_is_null(json_context_t *ctx, json_value_t *val);
int json_is_bool(json_context_t *ctx, json_value_t *val);
int json_is_number(json_context_t *ctx, json_value_t *val);
int json_is_string(json_context_t *ctx, json_value_t *val);
int json_is_array(json_context_t *ctx, json_value_t *val);
int json_is_object(json_context_t *ctx, json_value_t *val);

/* ==================== Value Creation ==================== */

json_value_t* json_create_null(json_context_t *ctx);
json_value_t* json_create_bool(json_context_t *ctx, int value);
json_value_t* json_create_number(json_context_t *ctx, double value);
json_value_t* json_create_string(json_context_t *ctx, const char *value);
json_value_t* json_create_array(json_context_t *ctx);
json_value_t* json_create_object(json_context_t *ctx);

/* ==================== Value Extraction ==================== */

/**
 * Get boolean value (returns 0 for false, 1 for true)
 */
int json_get_bool(json_context_t *ctx, json_value_t *val);

/**
 * Get number value
 */
double json_get_number(json_context_t *ctx, json_value_t *val);

/**
 * Get string value (does not allocate, points to internal buffer)
 */
const char* json_get_string(json_context_t *ctx, json_value_t *val);

/* ==================== Object Operations ==================== */

/**
 * Get object property by key
 * @param ctx JSON context
 * @param obj JSON object
 * @param key Property key
 * @return JSON value or NULL if not found
 */
json_value_t* json_object_get(json_context_t *ctx, json_value_t *obj, const char *key);

/**
 * Check if object has property
 * @param ctx JSON context
 * @param obj JSON object
 * @param key Property key
 * @return 1 if exists, 0 otherwise
 */
int json_object_has(json_context_t *ctx, json_value_t *obj, const char *key);

/**
 * Set object property (takes ownership of value)
 * @param ctx JSON context
 * @param obj JSON object
 * @param key Property key
 * @param val JSON value
 * @return 0 on success, -1 on failure
 */
int json_object_set(json_context_t *ctx, json_value_t *obj, const char *key, json_value_t *val);

/**
 * Remove object property
 * @param ctx JSON context
 * @param obj JSON object
 * @param key Property key
 */
void json_object_delete(json_context_t *ctx, json_value_t *obj, const char *key);

/* ==================== Array Operations ==================== */

/**
 * Get array length
 * @param ctx JSON context
 * @param arr JSON array
 * @return Number of elements
 */
int json_array_length(json_context_t *ctx, json_value_t *arr);

/**
 * Get array element by index
 * @param ctx JSON context
 * @param arr JSON array
 * @param index Element index
 * @return JSON value or NULL if out of bounds
 */
json_value_t* json_array_get(json_context_t *ctx, json_value_t *arr, int index);

/**
 * Append value to array (takes ownership of value)
 * @param ctx JSON context
 * @param arr JSON array
 * @param val JSON value
 * @return 0 on success, -1 on failure
 */
int json_array_push(json_context_t *ctx, json_value_t *arr, json_value_t *val);

/**
 * Remove array element by index
 * @param ctx JSON context
 * @param arr JSON array
 * @param index Element index
 */
void json_array_delete(json_context_t *ctx, json_value_t *arr, int index);

/* ==================== Convenience Functions ==================== */

/**
 * Create JSON from format string (variadic)
 * Example: json_create_fmt(ctx, "{\"name\":\"%s\",\"age\":%d}", "John", 30)
 */
json_value_t* json_create_fmt(json_context_t *ctx, const char *fmt, ...);

#endif /* COSMORUN_JSON_H */
