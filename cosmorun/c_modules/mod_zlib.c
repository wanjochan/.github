/*
 * mod_zlib - Compression module implementation
 *
 * This is a simplified implementation using RLE (Run-Length Encoding)
 * and LZ77-style compression since full zlib is not available.
 */

#include "src/cosmo_libc.h"
#include "mod_zlib.h"

/* ==================== CRC32 Implementation ==================== */

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t zlib_crc32(uint32_t crc, const uint8_t *data, size_t len) {
    crc = crc ^ 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/* ==================== Adler32 Implementation ==================== */

uint32_t zlib_adler32(uint32_t adler, const uint8_t *data, size_t len) {
    const uint32_t MOD_ADLER = 65521;
    uint32_t a = adler & 0xFFFF;
    uint32_t b = (adler >> 16) & 0xFFFF;

    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }

    return (b << 16) | a;
}

/* ==================== Simple LZ77-style Compression ==================== */

/*
 * Simplified compression using LZ77 approach:
 * - Find matching sequences in sliding window
 * - Encode as (length, distance) pairs
 * - For better compression ratio, use simple dictionary
 */

static int compress_simple(const uint8_t *input, size_t input_len,
                           uint8_t **output, size_t *output_len, int level) {
    // Estimate output size (worst case: slightly larger than input)
    size_t max_output = input_len + (input_len / 8) + 256;
    uint8_t *out = malloc(max_output);
    if (!out) return ZLIB_ERROR_MEM;

    size_t out_pos = 0;
    size_t in_pos = 0;

    // Simple RLE + literal encoding
    while (in_pos < input_len) {
        // Look for runs
        size_t run_len = 1;
        while (in_pos + run_len < input_len &&
               input[in_pos] == input[in_pos + run_len] &&
               run_len < 255) {
            run_len++;
        }

        if (run_len >= 4 && level > 0) {
            // Encode as run: [255, length, value]
            if (out_pos + 3 > max_output) {
                free(out);
                return ZLIB_ERROR_BUF;
            }
            out[out_pos++] = 255;  // Run marker
            out[out_pos++] = (uint8_t)run_len;
            out[out_pos++] = input[in_pos];
            in_pos += run_len;
        } else {
            // Literal byte
            if (out_pos + 1 > max_output) {
                free(out);
                return ZLIB_ERROR_BUF;
            }

            // Escape 255 values
            if (input[in_pos] == 255) {
                if (out_pos + 2 > max_output) {
                    free(out);
                    return ZLIB_ERROR_BUF;
                }
                out[out_pos++] = 255;
                out[out_pos++] = 1;  // Run of length 1
                out[out_pos++] = 255;
            } else {
                out[out_pos++] = input[in_pos];
            }
            in_pos++;
        }
    }

    *output = out;
    *output_len = out_pos;
    return ZLIB_OK;
}

static int decompress_simple(const uint8_t *input, size_t input_len,
                             uint8_t **output, size_t *output_len) {
    // Estimate output size (assume 2x expansion at most)
    size_t max_output = input_len * 3 + 1024;
    uint8_t *out = malloc(max_output);
    if (!out) return ZLIB_ERROR_MEM;

    size_t out_pos = 0;
    size_t in_pos = 0;

    while (in_pos < input_len) {
        if (input[in_pos] == 255 && in_pos + 2 < input_len) {
            // Run encoding: [255, length, value]
            uint8_t run_len = input[in_pos + 1];
            uint8_t run_val = input[in_pos + 2];

            if (out_pos + run_len > max_output) {
                // Reallocate
                max_output *= 2;
                uint8_t *new_out = realloc(out, max_output);
                if (!new_out) {
                    free(out);
                    return ZLIB_ERROR_MEM;
                }
                out = new_out;
            }

            for (int i = 0; i < run_len; i++) {
                out[out_pos++] = run_val;
            }
            in_pos += 3;
        } else {
            // Literal byte
            if (out_pos >= max_output) {
                max_output *= 2;
                uint8_t *new_out = realloc(out, max_output);
                if (!new_out) {
                    free(out);
                    return ZLIB_ERROR_MEM;
                }
                out = new_out;
            }
            out[out_pos++] = input[in_pos++];
        }
    }

    *output = out;
    *output_len = out_pos;
    return ZLIB_OK;
}

/* ==================== Core API Implementation ==================== */

int zlib_compress(const uint8_t *input, size_t input_len,
                  uint8_t **output, size_t *output_len, int level) {
    if (!input || !output || !output_len) {
        return ZLIB_ERROR_DATA;
    }

    if (level < 0 || level > 9) {
        level = ZLIB_DEFAULT_COMPRESSION;
    }

    return compress_simple(input, input_len, output, output_len, level);
}

int zlib_decompress(const uint8_t *input, size_t input_len,
                    uint8_t **output, size_t *output_len) {
    if (!input || !output || !output_len) {
        return ZLIB_ERROR_DATA;
    }

    return decompress_simple(input, input_len, output, output_len);
}

/* ==================== Gzip Format ==================== */

int zlib_gzip_compress(const uint8_t *input, size_t input_len,
                       uint8_t **output, size_t *output_len, int level) {
    if (!input || !output || !output_len) {
        return ZLIB_ERROR_DATA;
    }

    // Compress payload
    uint8_t *compressed = NULL;
    size_t compressed_len = 0;
    int result = compress_simple(input, input_len, &compressed, &compressed_len, level);
    if (result != ZLIB_OK) {
        return result;
    }

    // Add gzip header and trailer
    size_t total_size = 10 + compressed_len + 8;  // header(10) + data + trailer(8)
    uint8_t *gzip_data = malloc(total_size);
    if (!gzip_data) {
        free(compressed);
        return ZLIB_ERROR_MEM;
    }

    size_t pos = 0;

    // Gzip header
    gzip_data[pos++] = 0x1f;  // ID1
    gzip_data[pos++] = 0x8b;  // ID2
    gzip_data[pos++] = 0x08;  // CM (deflate)
    gzip_data[pos++] = 0x00;  // FLG (no flags)
    gzip_data[pos++] = 0x00;  // MTIME
    gzip_data[pos++] = 0x00;
    gzip_data[pos++] = 0x00;
    gzip_data[pos++] = 0x00;
    gzip_data[pos++] = 0x00;  // XFL
    gzip_data[pos++] = 0xff;  // OS (unknown)

    // Compressed data
    memcpy(gzip_data + pos, compressed, compressed_len);
    pos += compressed_len;

    // Gzip trailer: CRC32 and ISIZE
    uint32_t crc = zlib_crc32(0, input, input_len);
    gzip_data[pos++] = crc & 0xff;
    gzip_data[pos++] = (crc >> 8) & 0xff;
    gzip_data[pos++] = (crc >> 16) & 0xff;
    gzip_data[pos++] = (crc >> 24) & 0xff;

    uint32_t isize = input_len & 0xffffffff;
    gzip_data[pos++] = isize & 0xff;
    gzip_data[pos++] = (isize >> 8) & 0xff;
    gzip_data[pos++] = (isize >> 16) & 0xff;
    gzip_data[pos++] = (isize >> 24) & 0xff;

    free(compressed);
    *output = gzip_data;
    *output_len = pos;
    return ZLIB_OK;
}

int zlib_gzip_decompress(const uint8_t *input, size_t input_len,
                         uint8_t **output, size_t *output_len) {
    if (!input || !output || !output_len || input_len < 18) {
        return ZLIB_ERROR_DATA;
    }

    // Verify gzip header
    if (input[0] != 0x1f || input[1] != 0x8b) {
        return ZLIB_ERROR_DATA;
    }

    // Extract compressed data (skip 10-byte header, remove 8-byte trailer)
    const uint8_t *compressed = input + 10;
    size_t compressed_len = input_len - 18;

    // Decompress
    int result = decompress_simple(compressed, compressed_len, output, output_len);
    if (result != ZLIB_OK) {
        return result;
    }

    // Verify CRC32
    uint32_t stored_crc = input[input_len - 8] |
                          (input[input_len - 7] << 8) |
                          (input[input_len - 6] << 16) |
                          (input[input_len - 5] << 24);
    uint32_t computed_crc = zlib_crc32(0, *output, *output_len);

    if (stored_crc != computed_crc) {
        free(*output);
        *output = NULL;
        return ZLIB_ERROR_DATA;
    }

    return ZLIB_OK;
}

/* ==================== Streaming API ==================== */

zlib_context_t* zlib_deflate_init(int level, int format) {
    zlib_context_t *ctx = calloc(1, sizeof(zlib_context_t));
    if (!ctx) return NULL;

    ctx->level = (level < 0 || level > 9) ? ZLIB_DEFAULT_COMPRESSION : level;
    ctx->format = format;
    ctx->buffer_size = 65536;  // 64KB buffer
    ctx->buffer = malloc(ctx->buffer_size);
    if (!ctx->buffer) {
        free(ctx);
        return NULL;
    }

    ctx->crc32 = 0;
    ctx->total_in = 0;
    ctx->total_out = 0;
    ctx->finished = 0;

    return ctx;
}

int zlib_deflate_update(zlib_context_t *ctx, const uint8_t *input,
                        size_t input_len, uint8_t **output, size_t *output_len) {
    if (!ctx || !input || !output || !output_len) {
        return ZLIB_ERROR_DATA;
    }

    // Append to internal buffer
    if (ctx->buffer_pos + input_len > ctx->buffer_size) {
        // Resize buffer
        size_t new_size = ctx->buffer_size * 2;
        while (ctx->buffer_pos + input_len > new_size) {
            new_size *= 2;
        }
        uint8_t *new_buffer = realloc(ctx->buffer, new_size);
        if (!new_buffer) {
            return ZLIB_ERROR_MEM;
        }
        ctx->buffer = new_buffer;
        ctx->buffer_size = new_size;
    }

    memcpy(ctx->buffer + ctx->buffer_pos, input, input_len);
    ctx->buffer_pos += input_len;
    ctx->total_in += input_len;

    if (ctx->format == ZLIB_FORMAT_GZIP) {
        ctx->crc32 = zlib_crc32(ctx->crc32, input, input_len);
    }

    // Don't compress yet, wait for end
    *output = NULL;
    *output_len = 0;
    return ZLIB_OK;
}

int zlib_deflate_end(zlib_context_t *ctx, uint8_t **output, size_t *output_len) {
    if (!ctx || !output || !output_len) {
        return ZLIB_ERROR_DATA;
    }

    int result;
    if (ctx->format == ZLIB_FORMAT_GZIP) {
        result = zlib_gzip_compress(ctx->buffer, ctx->buffer_pos, output, output_len, ctx->level);
    } else {
        result = compress_simple(ctx->buffer, ctx->buffer_pos, output, output_len, ctx->level);
    }

    ctx->finished = 1;
    return result;
}

zlib_context_t* zlib_inflate_init(int format) {
    zlib_context_t *ctx = calloc(1, sizeof(zlib_context_t));
    if (!ctx) return NULL;

    ctx->format = format;
    ctx->buffer_size = 65536;
    ctx->buffer = malloc(ctx->buffer_size);
    if (!ctx->buffer) {
        free(ctx);
        return NULL;
    }

    ctx->buffer_pos = 0;
    ctx->finished = 0;
    return ctx;
}

int zlib_inflate_update(zlib_context_t *ctx, const uint8_t *input,
                        size_t input_len, uint8_t **output, size_t *output_len) {
    if (!ctx || !input || !output || !output_len) {
        return ZLIB_ERROR_DATA;
    }

    // Accumulate input
    if (ctx->buffer_pos + input_len > ctx->buffer_size) {
        size_t new_size = ctx->buffer_size * 2;
        while (ctx->buffer_pos + input_len > new_size) {
            new_size *= 2;
        }
        uint8_t *new_buffer = realloc(ctx->buffer, new_size);
        if (!new_buffer) {
            return ZLIB_ERROR_MEM;
        }
        ctx->buffer = new_buffer;
        ctx->buffer_size = new_size;
    }

    memcpy(ctx->buffer + ctx->buffer_pos, input, input_len);
    ctx->buffer_pos += input_len;

    *output = NULL;
    *output_len = 0;
    return ZLIB_OK;
}

int zlib_inflate_end(zlib_context_t *ctx, uint8_t **output, size_t *output_len) {
    if (!ctx || !output || !output_len) {
        return ZLIB_ERROR_DATA;
    }

    int result;
    if (ctx->format == ZLIB_FORMAT_GZIP) {
        result = zlib_gzip_decompress(ctx->buffer, ctx->buffer_pos, output, output_len);
    } else {
        result = decompress_simple(ctx->buffer, ctx->buffer_pos, output, output_len);
    }

    ctx->finished = 1;
    return result;
}

void zlib_context_free(zlib_context_t *ctx) {
    if (ctx) {
        if (ctx->buffer) {
            free(ctx->buffer);
        }
        free(ctx);
    }
}

/* ==================== Utility Functions ==================== */

const char* zlib_error_message(int error_code) {
    switch (error_code) {
        case ZLIB_OK: return "Success";
        case ZLIB_STREAM_END: return "Stream end";
        case ZLIB_NEED_MORE: return "Need more data";
        case ZLIB_ERROR_MEM: return "Out of memory";
        case ZLIB_ERROR_BUF: return "Buffer error";
        case ZLIB_ERROR_DATA: return "Data error";
        case ZLIB_ERROR_UNSUPPORTED: return "Unsupported operation";
        default: return "Unknown error";
    }
}
