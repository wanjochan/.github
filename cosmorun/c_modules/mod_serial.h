/*
 * mod_serial.h - Serialization Module for CosmoRun
 *
 * Provides native JSON and MessagePack encoding/decoding without external dependencies
 * Focuses on RFC 8259 compliance, zero-copy parsing, and type safety
 */

#ifndef MOD_SERIAL_H
#define MOD_SERIAL_H

#include "src/cosmo_libc.h"

/* ==================== Common Types ==================== */

typedef enum {
    SERIAL_NULL = 0,
    SERIAL_BOOL,
    SERIAL_INT,
    SERIAL_DOUBLE,
    SERIAL_STRING,
    SERIAL_ARRAY,
    SERIAL_MAP
} serial_type_t;

typedef struct serial_value serial_value_t;
typedef struct serial_error serial_error_t;

/* Error codes */
#define SERIAL_OK 0
#define SERIAL_ERR_PARSE -1
#define SERIAL_ERR_TYPE -2
#define SERIAL_ERR_MEMORY -3
#define SERIAL_ERR_INVALID -4
#define SERIAL_ERR_OVERFLOW -5

/* ==================== Error Handling ==================== */

struct serial_error {
    int code;
    int line;
    int column;
    char message[256];
};

/* Get last error (thread-local) */
serial_error_t* serial_get_error(void);

/* Clear last error */
void serial_clear_error(void);

/* ==================== JSON API ==================== */

/**
 * Parse JSON string into value tree
 * @param str JSON string (null-terminated)
 * @return Parsed value or NULL on error (check serial_get_error())
 */
serial_value_t* json_parse(const char *str);

/**
 * Parse JSON with known length (supports embedded nulls in strings)
 * @param str JSON string
 * @param len String length
 * @return Parsed value or NULL on error
 */
serial_value_t* json_parse_len(const char *str, size_t len);

/**
 * Serialize value to JSON string (formatted with indentation)
 * @param val Value to serialize
 * @return Allocated string (caller must free) or NULL on error
 */
char* json_stringify(const serial_value_t *val);

/**
 * Serialize value to JSON string (compact, no whitespace)
 * @param val Value to serialize
 * @return Allocated string (caller must free) or NULL on error
 */
char* json_stringify_compact(const serial_value_t *val);

/**
 * Free JSON-allocated value tree
 * @param val Value to free (NULL-safe)
 */
void json_free(serial_value_t *val);

/* ==================== MessagePack API ==================== */

/**
 * Unpack MessagePack binary data into value tree
 * @param data Binary data
 * @param len Data length
 * @return Parsed value or NULL on error
 */
serial_value_t* msgpack_unpack(const uint8_t *data, size_t len);

/**
 * Pack value tree into MessagePack binary format
 * @param val Value to pack
 * @param out_len Output: packed data length
 * @return Allocated binary data (caller must free) or NULL on error
 */
uint8_t* msgpack_pack(const serial_value_t *val, size_t *out_len);

/**
 * Free MessagePack-allocated value tree
 * @param val Value to free (NULL-safe)
 */
void msgpack_free(serial_value_t *val);

/* ==================== Value Type Inspection ==================== */

/**
 * Get value type
 * @param val Value to inspect
 * @return Type enum
 */
serial_type_t serial_get_type(const serial_value_t *val);

/**
 * Type checking predicates
 */
bool serial_is_null(const serial_value_t *val);
bool serial_is_bool(const serial_value_t *val);
bool serial_is_int(const serial_value_t *val);
bool serial_is_double(const serial_value_t *val);
bool serial_is_string(const serial_value_t *val);
bool serial_is_array(const serial_value_t *val);
bool serial_is_map(const serial_value_t *val);

/* ==================== Value Extraction ==================== */

/**
 * Extract typed values (returns default if type mismatch)
 * Use serial_get_type() to check type before extraction
 */
bool serial_get_bool(const serial_value_t *val);
int64_t serial_get_int(const serial_value_t *val);
double serial_get_double(const serial_value_t *val);
const char* serial_get_string(const serial_value_t *val);

/* ==================== Value Creation ==================== */

/**
 * Create primitive values
 */
serial_value_t* serial_create_null(void);
serial_value_t* serial_create_bool(bool value);
serial_value_t* serial_create_int(int64_t value);
serial_value_t* serial_create_double(double value);
serial_value_t* serial_create_string(const char *str);
serial_value_t* serial_create_string_len(const char *str, size_t len);

/**
 * Create container values
 */
serial_value_t* serial_create_array(void);
serial_value_t* serial_create_map(void);

/* ==================== Array Operations ==================== */

/**
 * Get array length
 * @param arr Array value
 * @return Number of elements, or 0 if not an array
 */
size_t serial_array_len(const serial_value_t *arr);

/**
 * Get array element by index
 * @param arr Array value
 * @param index Element index (0-based)
 * @return Element value or NULL if out of bounds
 */
serial_value_t* serial_array_get(const serial_value_t *arr, size_t index);

/**
 * Append element to array (takes ownership)
 * @param arr Array value
 * @param val Value to append
 * @return 0 on success, negative on error
 */
int serial_array_push(serial_value_t *arr, serial_value_t *val);

/**
 * Set array element at index (takes ownership of val)
 * @param arr Array value
 * @param index Element index
 * @param val New value
 * @return 0 on success, negative on error
 */
int serial_array_set(serial_value_t *arr, size_t index, serial_value_t *val);

/* ==================== Map Operations ==================== */

/**
 * Get map size (number of key-value pairs)
 * @param map Map value
 * @return Number of entries, or 0 if not a map
 */
size_t serial_map_size(const serial_value_t *map);

/**
 * Get map value by key
 * @param map Map value
 * @param key Key string
 * @return Value or NULL if key not found
 */
serial_value_t* serial_map_get(const serial_value_t *map, const char *key);

/**
 * Set map entry (takes ownership of val)
 * @param map Map value
 * @param key Key string (will be copied)
 * @param val Value to associate with key
 * @return 0 on success, negative on error
 */
int serial_map_set(serial_value_t *map, const char *key, serial_value_t *val);

/**
 * Check if map has key
 * @param map Map value
 * @param key Key string
 * @return true if key exists
 */
bool serial_map_has(const serial_value_t *map, const char *key);

/**
 * Remove map entry
 * @param map Map value
 * @param key Key string
 * @return 0 on success, negative if key not found
 */
int serial_map_remove(serial_value_t *map, const char *key);

/* ==================== Streaming API ==================== */

typedef struct serial_stream serial_stream_t;

/**
 * Create streaming parser for large JSON documents
 * @param chunk_size Buffer chunk size (0 for default 4KB)
 * @return Stream handle or NULL on error
 */
serial_stream_t* json_stream_new(size_t chunk_size);

/**
 * Feed data to streaming parser
 * @param stream Stream handle
 * @param data Data chunk
 * @param len Data length
 * @return 0 if more data needed, 1 if parse complete, negative on error
 */
int json_stream_feed(serial_stream_t *stream, const char *data, size_t len);

/**
 * Get parsed value from completed stream
 * @param stream Stream handle
 * @return Parsed value (caller must free with json_free)
 */
serial_value_t* json_stream_value(serial_stream_t *stream);

/**
 * Free streaming parser
 * @param stream Stream handle
 */
void json_stream_free(serial_stream_t *stream);

/* ==================== Utility Functions ==================== */

/**
 * Clone value (deep copy)
 * @param val Value to clone
 * @return Cloned value (caller must free)
 */
serial_value_t* serial_clone(const serial_value_t *val);

/**
 * Compare two values for equality
 * @param a First value
 * @param b Second value
 * @return true if equal
 */
bool serial_equals(const serial_value_t *a, const serial_value_t *b);

/**
 * Get memory usage of value tree
 * @param val Value to measure
 * @return Approximate bytes used
 */
size_t serial_memory_usage(const serial_value_t *val);

#endif /* MOD_SERIAL_H */
