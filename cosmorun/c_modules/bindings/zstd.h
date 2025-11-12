/* zstd FFI Bindings for CosmoRun
 * Provides fast compression and decompression using Zstandard
 *
 * Usage: __import("bindings/zstd")
 */

#ifndef COSMORUN_BINDINGS_ZSTD_H
#define COSMORUN_BINDINGS_ZSTD_H

/* TCC compatibility: include types before standard headers */
#ifdef __TINYC__
#include "tcc_stdint.h"
#endif

#include <stddef.h>
#ifndef __TINYC__
#include <stdint.h>
#endif

/* Zstd version */
#define ZSTD_VERSION_MAJOR    1
#define ZSTD_VERSION_MINOR    4
#define ZSTD_VERSION_RELEASE  8
#define ZSTD_VERSION_NUMBER  (ZSTD_VERSION_MAJOR *100*100 + ZSTD_VERSION_MINOR *100 + ZSTD_VERSION_RELEASE)

unsigned ZSTD_versionNumber(void);
const char* ZSTD_versionString(void);

/* Compression levels */
#define ZSTD_MIN_CLEVEL     1
#define ZSTD_MAX_CLEVEL    22
#define ZSTD_DEFAULT_CLEVEL 3

/* Error handling */
typedef size_t ZSTD_ErrorCode;
unsigned ZSTD_isError(size_t code);
const char* ZSTD_getErrorName(size_t code);
int ZSTD_minCLevel(void);
int ZSTD_maxCLevel(void);

/* Simple compression/decompression */
size_t ZSTD_compress(void* dst, size_t dstCapacity, const void* src, size_t srcSize, int compressionLevel);
size_t ZSTD_decompress(void* dst, size_t dstCapacity, const void* src, size_t compressedSize);

/* Get bounds */
size_t ZSTD_compressBound(size_t srcSize);
unsigned long long ZSTD_getFrameContentSize(const void* src, size_t srcSize);
unsigned long long ZSTD_getDecompressedSize(const void* src, size_t srcSize);

/* Streaming compression context */
typedef struct ZSTD_CCtx_s ZSTD_CCtx;

ZSTD_CCtx* ZSTD_createCCtx(void);
size_t ZSTD_freeCCtx(ZSTD_CCtx* cctx);

size_t ZSTD_compressCCtx(ZSTD_CCtx* cctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, int compressionLevel);

/* Streaming decompression context */
typedef struct ZSTD_DCtx_s ZSTD_DCtx;

ZSTD_DCtx* ZSTD_createDCtx(void);
size_t ZSTD_freeDCtx(ZSTD_DCtx* dctx);

size_t ZSTD_decompressDCtx(ZSTD_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize);

/* Streaming buffer */
typedef struct {
    char* dst;
    size_t size;
    size_t pos;
} ZSTD_outBuffer;

typedef struct {
    const char* src;
    size_t size;
    size_t pos;
} ZSTD_inBuffer;

/* Streaming compression */
size_t ZSTD_compressStream(ZSTD_CCtx* cctx, ZSTD_outBuffer* output, ZSTD_inBuffer* input);
size_t ZSTD_flushStream(ZSTD_CCtx* cctx, ZSTD_outBuffer* output);
size_t ZSTD_endStream(ZSTD_CCtx* cctx, ZSTD_outBuffer* output);
size_t ZSTD_CStreamInSize(void);
size_t ZSTD_CStreamOutSize(void);

/* Streaming decompression */
size_t ZSTD_decompressStream(ZSTD_DCtx* dctx, ZSTD_outBuffer* output, ZSTD_inBuffer* input);
size_t ZSTD_DStreamInSize(void);
size_t ZSTD_DStreamOutSize(void);

/* Dictionary compression */
typedef struct ZSTD_CDict_s ZSTD_CDict;
typedef struct ZSTD_DDict_s ZSTD_DDict;

ZSTD_CDict* ZSTD_createCDict(const void* dictBuffer, size_t dictSize, int compressionLevel);
size_t ZSTD_freeCDict(ZSTD_CDict* CDict);

size_t ZSTD_compress_usingCDict(ZSTD_CCtx* cctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, const ZSTD_CDict* cdict);

ZSTD_DDict* ZSTD_createDDict(const void* dictBuffer, size_t dictSize);
size_t ZSTD_freeDDict(ZSTD_DDict* ddict);

size_t ZSTD_decompress_usingDDict(ZSTD_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, const ZSTD_DDict* ddict);

/* Advanced parameters */
typedef enum {
    ZSTD_c_compressionLevel = 100,
    ZSTD_c_windowLog = 101,
    ZSTD_c_hashLog = 102,
    ZSTD_c_chainLog = 103,
    ZSTD_c_searchLog = 104,
    ZSTD_c_minMatch = 105,
    ZSTD_c_targetLength = 106,
    ZSTD_c_strategy = 107,
    ZSTD_c_enableLongDistanceMatching = 160,
    ZSTD_c_ldmHashLog = 161,
    ZSTD_c_ldmMinMatch = 162,
    ZSTD_c_ldmBucketSizeLog = 163,
    ZSTD_c_ldmHashRateLog = 164,
    ZSTD_c_contentSizeFlag = 200,
    ZSTD_c_checksumFlag = 201,
    ZSTD_c_dictIDFlag = 202,
    ZSTD_c_nbWorkers = 400,
    ZSTD_c_jobSize = 401,
    ZSTD_c_overlapLog = 402,
} ZSTD_cParameter;

size_t ZSTD_CCtx_setParameter(ZSTD_CCtx* cctx, ZSTD_cParameter param, int value);

/* High-level wrapper functions */

/* Compression wrapper */
typedef struct {
    void* compressed_data;
    size_t compressed_size;
    size_t original_size;
} zstd_compressed_t;

/* Compress data with automatic allocation */
static inline zstd_compressed_t* zstd_compress_easy(const void* src, size_t src_size, int level) {
    if (!src || src_size == 0) return NULL;

    zstd_compressed_t* result = (zstd_compressed_t*)malloc(sizeof(zstd_compressed_t));
    if (!result) return NULL;

    size_t max_compressed_size = ZSTD_compressBound(src_size);
    result->compressed_data = malloc(max_compressed_size);
    if (!result->compressed_data) {
        free(result);
        return NULL;
    }

    result->compressed_size = ZSTD_compress(result->compressed_data, max_compressed_size, src, src_size, level);
    if (ZSTD_isError(result->compressed_size)) {
        free(result->compressed_data);
        free(result);
        return NULL;
    }

    result->original_size = src_size;
    return result;
}

/* Decompress data with automatic allocation */
static inline void* zstd_decompress_easy(const void* compressed, size_t compressed_size, size_t* decompressed_size) {
    if (!compressed || compressed_size == 0) return NULL;

    unsigned long long frame_size = ZSTD_getFrameContentSize(compressed, compressed_size);
    if (frame_size == 0) return NULL;

    void* decompressed = malloc(frame_size);
    if (!decompressed) return NULL;

    size_t result = ZSTD_decompress(decompressed, frame_size, compressed, compressed_size);
    if (ZSTD_isError(result)) {
        free(decompressed);
        return NULL;
    }

    if (decompressed_size) *decompressed_size = result;
    return decompressed;
}

/* Free compressed data */
static inline void zstd_compressed_free(zstd_compressed_t* data) {
    if (!data) return;
    if (data->compressed_data) free(data->compressed_data);
    free(data);
}

/* Compress file to file */
static inline int zstd_compress_file(const char* src_path, const char* dst_path, int level) {
    FILE* fin = fopen(src_path, "rb");
    if (!fin) return 0;

    fseek(fin, 0, SEEK_END);
    size_t file_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    void* file_data = malloc(file_size);
    if (!file_data) {
        fclose(fin);
        return 0;
    }

    size_t read_size = fread(file_data, 1, file_size, fin);
    fclose(fin);

    if (read_size != file_size) {
        free(file_data);
        return 0;
    }

    zstd_compressed_t* compressed = zstd_compress_easy(file_data, file_size, level);
    free(file_data);

    if (!compressed) return 0;

    FILE* fout = fopen(dst_path, "wb");
    if (!fout) {
        zstd_compressed_free(compressed);
        return 0;
    }

    size_t written = fwrite(compressed->compressed_data, 1, compressed->compressed_size, fout);
    fclose(fout);

    int success = (written == compressed->compressed_size);
    zstd_compressed_free(compressed);
    return success;
}

/* Decompress file to file */
static inline int zstd_decompress_file(const char* src_path, const char* dst_path) {
    FILE* fin = fopen(src_path, "rb");
    if (!fin) return 0;

    fseek(fin, 0, SEEK_END);
    size_t file_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    void* compressed_data = malloc(file_size);
    if (!compressed_data) {
        fclose(fin);
        return 0;
    }

    size_t read_size = fread(compressed_data, 1, file_size, fin);
    fclose(fin);

    if (read_size != file_size) {
        free(compressed_data);
        return 0;
    }

    size_t decompressed_size;
    void* decompressed = zstd_decompress_easy(compressed_data, file_size, &decompressed_size);
    free(compressed_data);

    if (!decompressed) return 0;

    FILE* fout = fopen(dst_path, "wb");
    if (!fout) {
        free(decompressed);
        return 0;
    }

    size_t written = fwrite(decompressed, 1, decompressed_size, fout);
    fclose(fout);

    int success = (written == decompressed_size);
    free(decompressed);
    return success;
}

/* Streaming compression helper */
typedef struct {
    ZSTD_CCtx* cctx;
    FILE* output_file;
    char* out_buffer;
    size_t out_buffer_size;
} zstd_stream_compressor_t;

static inline zstd_stream_compressor_t* zstd_stream_compressor_new(const char* output_path, int level) {
    zstd_stream_compressor_t* sc = (zstd_stream_compressor_t*)malloc(sizeof(zstd_stream_compressor_t));
    if (!sc) return NULL;

    sc->cctx = ZSTD_createCCtx();
    if (!sc->cctx) {
        free(sc);
        return NULL;
    }

    ZSTD_CCtx_setParameter(sc->cctx, ZSTD_c_compressionLevel, level);

    sc->output_file = fopen(output_path, "wb");
    if (!sc->output_file) {
        ZSTD_freeCCtx(sc->cctx);
        free(sc);
        return NULL;
    }

    sc->out_buffer_size = ZSTD_CStreamOutSize();
    sc->out_buffer = (char*)malloc(sc->out_buffer_size);
    if (!sc->out_buffer) {
        fclose(sc->output_file);
        ZSTD_freeCCtx(sc->cctx);
        free(sc);
        return NULL;
    }

    return sc;
}

static inline int zstd_stream_compress_chunk(zstd_stream_compressor_t* sc, const void* data, size_t size) {
    if (!sc || !data) return 0;

    ZSTD_inBuffer input = { data, size, 0 };
    while (input.pos < input.size) {
        ZSTD_outBuffer output = { sc->out_buffer, sc->out_buffer_size, 0 };
        size_t remaining = ZSTD_compressStream(sc->cctx, &output, &input);
        if (ZSTD_isError(remaining)) return 0;

        if (output.pos > 0) {
            fwrite(sc->out_buffer, 1, output.pos, sc->output_file);
        }
    }
    return 1;
}

static inline int zstd_stream_compressor_finish(zstd_stream_compressor_t* sc) {
    if (!sc) return 0;

    size_t remaining;
    do {
        ZSTD_outBuffer output = { sc->out_buffer, sc->out_buffer_size, 0 };
        remaining = ZSTD_endStream(sc->cctx, &output);
        if (ZSTD_isError(remaining)) return 0;

        if (output.pos > 0) {
            fwrite(sc->out_buffer, 1, output.pos, sc->output_file);
        }
    } while (remaining > 0);

    return 1;
}

static inline void zstd_stream_compressor_free(zstd_stream_compressor_t* sc) {
    if (!sc) return;
    if (sc->out_buffer) free(sc->out_buffer);
    if (sc->output_file) fclose(sc->output_file);
    if (sc->cctx) ZSTD_freeCCtx(sc->cctx);
    free(sc);
}

#endif /* COSMORUN_BINDINGS_ZSTD_H */
