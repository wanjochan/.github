#ifndef COSMORUN_BUFFER_H
#define COSMORUN_BUFFER_H

/*
 * mod_buffer - Node.js-style Buffer module for cosmorun v2
 *
 * Provides binary data handling similar to Node.js Buffer:
 * - Dynamic memory management (uses System Layer API)
 * - Encoding support: UTF8, ASCII, HEX, BASE64, BINARY
 * - Buffer manipulation: slice, concat, copy, fill
 * - String conversion with encoding
 * - Search and comparison operations
 *
 * 本模块提供缓冲区管理和编码/解码功能
 * 作为 v1 向 v2 的迁移，使用 shim 层替代 libc
 */

/* 包含 System Layer API (shim 层) */
#include "../cosmorun_system/libc_shim/sys_libc_shim.h"

/* ==================== Buffer Structure ==================== */

typedef enum {
    BUFFER_ENCODING_UTF8 = 0,
    BUFFER_ENCODING_ASCII,
    BUFFER_ENCODING_HEX,
    BUFFER_ENCODING_BASE64,
    BUFFER_ENCODING_BINARY
} buffer_encoding_t;

typedef struct {
    unsigned char *data;
    size_t length;
    size_t capacity;
} buffer_t;

/* ==================== Buffer Creation ==================== */

/* Create buffer with specified size (zero-filled) */
buffer_t* buffer_alloc(size_t size);

/* Create buffer with specified size (uninitialized) */
buffer_t* buffer_alloc_unsafe(size_t size);

/* Create buffer from string with encoding */
buffer_t* buffer_from_string(const char *str, buffer_encoding_t encoding);

/* Create buffer from raw bytes */
buffer_t* buffer_from_bytes(const unsigned char *data, size_t length);

/* Create buffer by concatenating multiple buffers */
buffer_t* buffer_concat(buffer_t **buffers, size_t count);

/* Free buffer */
void buffer_free(buffer_t *buf);

/* ==================== String Conversion ==================== */

/* Convert buffer to string with encoding */
char* buffer_to_string(buffer_t *buf, buffer_encoding_t encoding);

/* Convert buffer to string with range */
char* buffer_to_string_range(buffer_t *buf, buffer_encoding_t encoding,
                              size_t start, size_t end);

/* Write string to buffer at offset */
int buffer_write(buffer_t *buf, const char *str, size_t offset,
                 buffer_encoding_t encoding);

/* ==================== Buffer Manipulation ==================== */

/* Copy data from source to target buffer */
int buffer_copy(buffer_t *source, buffer_t *target,
                size_t target_start, size_t source_start, size_t source_end);

/* Create a new buffer that references same memory (slice) */
buffer_t* buffer_slice(buffer_t *buf, size_t start, size_t end);

/* Fill buffer with value */
void buffer_fill(buffer_t *buf, unsigned char value, size_t start, size_t end);

/* ==================== Comparison ==================== */

/* Check if two buffers are equal */
int buffer_equals(buffer_t *a, buffer_t *b);

/* Compare two buffers (returns -1, 0, or 1) */
int buffer_compare(buffer_t *a, buffer_t *b);

/* ==================== Search ==================== */

/* Find first occurrence of value (byte or buffer) */
int buffer_index_of(buffer_t *buf, const unsigned char *value, size_t value_len);

/* Find last occurrence of value */
int buffer_last_index_of(buffer_t *buf, const unsigned char *value, size_t value_len);

/* Check if buffer includes value */
int buffer_includes(buffer_t *buf, const unsigned char *value, size_t value_len);

/* ==================== Utility ==================== */

/* Get buffer length */
size_t buffer_length(buffer_t *buf);

/* Resize buffer (internal use) */
int buffer_resize(buffer_t *buf, size_t new_size);

/* Get encoding name as string */
const char* buffer_encoding_name(buffer_encoding_t encoding);

#endif /* COSMORUN_BUFFER_H */
