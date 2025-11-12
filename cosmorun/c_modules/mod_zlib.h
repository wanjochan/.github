#ifndef COSMORUN_ZLIB_H
#define COSMORUN_ZLIB_H

/*
 * mod_zlib - Node.js-style compression module for cosmorun
 *
 * Provides compression/decompression similar to Node.js zlib:
 * - Basic compression with levels 0-9
 * - Gzip format support
 * - Streaming API for large data
 * - Integration with mod_buffer
 *
 * Note: This implementation uses a simplified deflate algorithm
 * since full zlib is not available in this Cosmopolitan build.
 */

#include "src/cosmo_libc.h"
#include "mod_buffer.h"

/* ==================== Constants ==================== */

/* Compression levels */
#define ZLIB_NO_COMPRESSION      0
#define ZLIB_BEST_SPEED          1
#define ZLIB_BEST_COMPRESSION    9
#define ZLIB_DEFAULT_COMPRESSION 6

/* Compression strategies */
#define ZLIB_STRATEGY_DEFAULT    0
#define ZLIB_STRATEGY_FILTERED   1
#define ZLIB_STRATEGY_HUFFMAN    2
#define ZLIB_STRATEGY_RLE        3
#define ZLIB_STRATEGY_FIXED      4

/* Return codes */
#define ZLIB_OK                  0
#define ZLIB_STREAM_END          1
#define ZLIB_NEED_MORE          -1
#define ZLIB_ERROR_MEM          -2
#define ZLIB_ERROR_BUF          -3
#define ZLIB_ERROR_DATA         -4
#define ZLIB_ERROR_UNSUPPORTED  -5

/* Format types */
#define ZLIB_FORMAT_RAW         0
#define ZLIB_FORMAT_ZLIB        1
#define ZLIB_FORMAT_GZIP        2

/* ==================== Compression Context ==================== */

typedef struct {
    uint8_t *buffer;           // Internal buffer for streaming
    size_t buffer_size;        // Size of internal buffer
    size_t buffer_pos;         // Current position in buffer
    int level;                 // Compression level
    int format;                // Output format (raw/zlib/gzip)
    int finished;              // Stream finished flag
    uint32_t crc32;            // CRC32 for gzip
    size_t total_in;           // Total bytes processed
    size_t total_out;          // Total bytes output
} zlib_context_t;

/* ==================== Core Compression API ==================== */

/*
 * Compress data in one shot
 *
 * input: Input data to compress
 * input_len: Length of input data
 * output: Pointer to receive allocated output buffer (caller must free)
 * output_len: Pointer to receive output length
 * level: Compression level (0-9)
 *
 * Returns: ZLIB_OK on success, negative on error
 */
int zlib_compress(const uint8_t *input, size_t input_len,
                  uint8_t **output, size_t *output_len, int level);

/*
 * Decompress data in one shot
 *
 * input: Compressed input data
 * input_len: Length of input data
 * output: Pointer to receive allocated output buffer (caller must free)
 * output_len: Pointer to receive output length
 *
 * Returns: ZLIB_OK on success, negative on error
 */
int zlib_decompress(const uint8_t *input, size_t input_len,
                    uint8_t **output, size_t *output_len);

/*
 * Compress data in gzip format
 *
 * Same parameters as zlib_compress but outputs gzip format
 */
int zlib_gzip_compress(const uint8_t *input, size_t input_len,
                       uint8_t **output, size_t *output_len, int level);

/*
 * Decompress gzip data
 *
 * Same parameters as zlib_decompress but expects gzip format
 */
int zlib_gzip_decompress(const uint8_t *input, size_t input_len,
                         uint8_t **output, size_t *output_len);

/* ==================== Streaming API ==================== */

/*
 * Initialize deflate (compression) stream
 *
 * level: Compression level (0-9)
 * format: ZLIB_FORMAT_RAW, ZLIB_FORMAT_ZLIB, or ZLIB_FORMAT_GZIP
 *
 * Returns: Allocated context or NULL on error
 */
zlib_context_t* zlib_deflate_init(int level, int format);

/*
 * Process chunk of data through deflate stream
 *
 * ctx: Compression context
 * input: Input chunk
 * input_len: Length of input chunk
 * output: Pointer to receive allocated output (caller must free)
 * output_len: Pointer to receive output length
 *
 * Returns: ZLIB_OK on success, negative on error
 */
int zlib_deflate_update(zlib_context_t *ctx, const uint8_t *input,
                        size_t input_len, uint8_t **output, size_t *output_len);

/*
 * Finalize deflate stream and get remaining data
 *
 * ctx: Compression context
 * output: Pointer to receive allocated output (caller must free)
 * output_len: Pointer to receive output length
 *
 * Returns: ZLIB_OK on success, negative on error
 */
int zlib_deflate_end(zlib_context_t *ctx, uint8_t **output, size_t *output_len);

/*
 * Initialize inflate (decompression) stream
 *
 * format: ZLIB_FORMAT_RAW, ZLIB_FORMAT_ZLIB, or ZLIB_FORMAT_GZIP
 *
 * Returns: Allocated context or NULL on error
 */
zlib_context_t* zlib_inflate_init(int format);

/*
 * Process chunk of data through inflate stream
 *
 * ctx: Decompression context
 * input: Input chunk
 * input_len: Length of input chunk
 * output: Pointer to receive allocated output (caller must free)
 * output_len: Pointer to receive output length
 *
 * Returns: ZLIB_OK on success, ZLIB_STREAM_END when done, negative on error
 */
int zlib_inflate_update(zlib_context_t *ctx, const uint8_t *input,
                        size_t input_len, uint8_t **output, size_t *output_len);

/*
 * Finalize inflate stream
 *
 * ctx: Decompression context
 * output: Pointer to receive allocated output (caller must free)
 * output_len: Pointer to receive output length
 *
 * Returns: ZLIB_OK on success, negative on error
 */
int zlib_inflate_end(zlib_context_t *ctx, uint8_t **output, size_t *output_len);

/* ==================== Context Management ==================== */

/*
 * Free compression/decompression context
 */
void zlib_context_free(zlib_context_t *ctx);

/* ==================== Utility Functions ==================== */

/*
 * Calculate CRC32 checksum
 */
uint32_t zlib_crc32(uint32_t crc, const uint8_t *data, size_t len);

/*
 * Calculate Adler32 checksum (used in zlib format)
 */
uint32_t zlib_adler32(uint32_t adler, const uint8_t *data, size_t len);

/*
 * Get error message for error code
 */
const char* zlib_error_message(int error_code);

#endif /* COSMORUN_ZLIB_H */
