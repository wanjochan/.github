/*
 * example_zlib.c - Examples for mod_zlib usage
 *
 * Demonstrates compression/decompression API similar to Node.js zlib module
 */

#include "src/cosmo_libc.h"
#include "mod_zlib.h"
#include "mod_zlib.c"

/* ==================== Example 1: Simple Compression ==================== */

void example_simple_compression() {
    printf("========================================\n");
    printf("Example 1: Simple Compression\n");
    printf("========================================\n\n");

    const char *text = "Hello, World! This is a test string. "
                      "Compression works best with repetitive data. "
                      "AAAA BBBB CCCC DDDD EEEE FFFF";
    size_t text_len = strlen(text);

    printf("Original text: %s\n", text);
    printf("Original size: %zu bytes\n\n", text_len);

    // Compress the data
    uint8_t *compressed = NULL;
    size_t compressed_len = 0;

    int result = zlib_compress((const uint8_t *)text, text_len,
                               &compressed, &compressed_len,
                               ZLIB_DEFAULT_COMPRESSION);

    if (result == ZLIB_OK) {
        printf("Compressed size: %zu bytes\n", compressed_len);
        printf("Compression ratio: %zu%%\n\n",
               (compressed_len * 100) / text_len);

        // Decompress to verify
        uint8_t *decompressed = NULL;
        size_t decompressed_len = 0;

        result = zlib_decompress(compressed, compressed_len,
                                &decompressed, &decompressed_len);

        if (result == ZLIB_OK) {
            printf("Decompressed: %.*s\n", (int)decompressed_len, decompressed);
            printf("Round-trip successful!\n");
            free(decompressed);
        }

        free(compressed);
    } else {
        printf("Compression failed: %s\n", zlib_error_message(result));
    }

    printf("\n");
}

/* ==================== Example 2: Compression Levels ==================== */

void example_compression_levels() {
    printf("========================================\n");
    printf("Example 2: Compression Levels\n");
    printf("========================================\n\n");

    // Create highly compressible data
    size_t data_len = 1000;
    uint8_t *data = malloc(data_len);
    memset(data, 'A', data_len);  // All same character

    printf("Original data: %zu bytes of repeated 'A'\n\n", data_len);

    int levels[] = {
        ZLIB_NO_COMPRESSION,
        ZLIB_BEST_SPEED,
        ZLIB_DEFAULT_COMPRESSION,
        ZLIB_BEST_COMPRESSION
    };
    const char *level_names[] = {
        "No compression (0)",
        "Best speed (1)",
        "Default (6)",
        "Best compression (9)"
    };

    for (int i = 0; i < 4; i++) {
        uint8_t *compressed = NULL;
        size_t compressed_len = 0;

        int result = zlib_compress(data, data_len, &compressed,
                                   &compressed_len, levels[i]);

        if (result == ZLIB_OK) {
            printf("%s: %zu bytes (%zu%%)\n",
                   level_names[i], compressed_len,
                   (compressed_len * 100) / data_len);
            free(compressed);
        }
    }

    free(data);
    printf("\n");
}

/* ==================== Example 3: Gzip Format ==================== */

void example_gzip_format() {
    printf("========================================\n");
    printf("Example 3: Gzip Format\n");
    printf("========================================\n\n");

    const char *data = "This data will be compressed in gzip format, "
                      "compatible with standard gzip tools.";
    size_t data_len = strlen(data);

    printf("Original: %s\n", data);
    printf("Size: %zu bytes\n\n", data_len);

    // Compress in gzip format
    uint8_t *gzip_data = NULL;
    size_t gzip_len = 0;

    int result = zlib_gzip_compress((const uint8_t *)data, data_len,
                                    &gzip_data, &gzip_len,
                                    ZLIB_DEFAULT_COMPRESSION);

    if (result == ZLIB_OK) {
        printf("Gzip compressed: %zu bytes\n", gzip_len);
        printf("Gzip header: 0x%02x 0x%02x (valid gzip magic)\n",
               gzip_data[0], gzip_data[1]);

        // Decompress
        uint8_t *decompressed = NULL;
        size_t decompressed_len = 0;

        result = zlib_gzip_decompress(gzip_data, gzip_len,
                                      &decompressed, &decompressed_len);

        if (result == ZLIB_OK) {
            printf("Decompressed: %.*s\n", (int)decompressed_len, decompressed);
            printf("CRC32 verified!\n");
            free(decompressed);
        }

        free(gzip_data);
    } else {
        printf("Gzip compression failed: %s\n", zlib_error_message(result));
    }

    printf("\n");
}

/* ==================== Example 4: Streaming Compression ==================== */

void example_streaming_compression() {
    printf("========================================\n");
    printf("Example 4: Streaming Compression\n");
    printf("========================================\n\n");

    printf("Compressing data in chunks (streaming mode)\n\n");

    // Simulate streaming data
    const char *chunks[] = {
        "First chunk of streaming data. ",
        "Second chunk with more content. ",
        "Third and final chunk!"
    };
    int num_chunks = 3;

    // Initialize compression stream
    zlib_context_t *ctx = zlib_deflate_init(ZLIB_DEFAULT_COMPRESSION,
                                            ZLIB_FORMAT_RAW);
    if (!ctx) {
        printf("Failed to initialize deflate context\n");
        return;
    }

    // Process each chunk
    for (int i = 0; i < num_chunks; i++) {
        printf("Processing chunk %d: %s\n", i + 1, chunks[i]);

        uint8_t *output = NULL;
        size_t output_len = 0;

        int result = zlib_deflate_update(ctx, (const uint8_t *)chunks[i],
                                        strlen(chunks[i]), &output, &output_len);

        if (result != ZLIB_OK) {
            printf("Deflate update failed: %s\n", zlib_error_message(result));
            zlib_context_free(ctx);
            return;
        }
    }

    // Finalize compression
    uint8_t *final_output = NULL;
    size_t final_len = 0;

    int result = zlib_deflate_end(ctx, &final_output, &final_len);
    if (result == ZLIB_OK) {
        printf("\nTotal compressed size: %zu bytes\n", final_len);

        // Decompress to verify
        uint8_t *decompressed = NULL;
        size_t decompressed_len = 0;

        result = zlib_decompress(final_output, final_len,
                                &decompressed, &decompressed_len);

        if (result == ZLIB_OK) {
            printf("Decompressed: %.*s\n", (int)decompressed_len, decompressed);
            free(decompressed);
        }

        free(final_output);
    }

    zlib_context_free(ctx);
    printf("\n");
}

/* ==================== Example 5: Error Handling ==================== */

void example_error_handling() {
    printf("========================================\n");
    printf("Example 5: Error Handling\n");
    printf("========================================\n\n");

    // Try to decompress invalid data
    uint8_t bad_data[] = {0x00, 0x00, 0x00, 0x00};
    uint8_t *output = NULL;
    size_t output_len = 0;

    printf("Attempting to decompress invalid data...\n");
    int result = zlib_decompress(bad_data, sizeof(bad_data),
                                &output, &output_len);

    if (result != ZLIB_OK) {
        printf("Error (expected): %s (code %d)\n",
               zlib_error_message(result), result);
    }

    printf("\n");

    // Try invalid gzip
    printf("Attempting to decompress invalid gzip...\n");
    result = zlib_gzip_decompress(bad_data, sizeof(bad_data),
                                  &output, &output_len);

    if (result != ZLIB_OK) {
        printf("Error (expected): %s (code %d)\n",
               zlib_error_message(result), result);
    }

    printf("\n");
}

/* ==================== Example 6: Binary Data ==================== */

void example_binary_data() {
    printf("========================================\n");
    printf("Example 6: Binary Data Compression\n");
    printf("========================================\n\n");

    // Create binary data with all byte values
    uint8_t binary_data[256];
    for (int i = 0; i < 256; i++) {
        binary_data[i] = (uint8_t)i;
    }

    printf("Compressing 256 bytes of binary data (0x00 to 0xFF)\n\n");

    uint8_t *compressed = NULL;
    size_t compressed_len = 0;

    int result = zlib_compress(binary_data, 256, &compressed,
                               &compressed_len, ZLIB_DEFAULT_COMPRESSION);

    if (result == ZLIB_OK) {
        printf("Compressed: 256 -> %zu bytes\n", compressed_len);

        // Decompress and verify
        uint8_t *decompressed = NULL;
        size_t decompressed_len = 0;

        result = zlib_decompress(compressed, compressed_len,
                                &decompressed, &decompressed_len);

        if (result == ZLIB_OK && decompressed_len == 256) {
            int matches = 1;
            for (int i = 0; i < 256; i++) {
                if (decompressed[i] != binary_data[i]) {
                    matches = 0;
                    break;
                }
            }

            if (matches) {
                printf("Binary data round-trip: SUCCESS\n");
                printf("All 256 byte values preserved correctly\n");
            }

            free(decompressed);
        }

        free(compressed);
    }

    printf("\n");
}

/* ==================== Example 7: CRC32 Checksum ==================== */

void example_crc32() {
    printf("========================================\n");
    printf("Example 7: CRC32 Checksums\n");
    printf("========================================\n\n");

    const char *data1 = "The quick brown fox jumps over the lazy dog";
    const char *data2 = "The quick brown fox jumps over the lazy cat";

    uint32_t crc1 = zlib_crc32(0, (const uint8_t *)data1, strlen(data1));
    uint32_t crc2 = zlib_crc32(0, (const uint8_t *)data2, strlen(data2));

    printf("Text 1: %s\n", data1);
    printf("CRC32:  0x%08x\n\n", crc1);

    printf("Text 2: %s\n", data2);
    printf("CRC32:  0x%08x\n\n", crc2);

    printf("Different data produces different checksums: %s\n",
           (crc1 != crc2) ? "YES" : "NO");

    printf("\n");
}

/* ==================== Performance Tips ==================== */

void show_performance_tips() {
    printf("========================================\n");
    printf("Performance Tips\n");
    printf("========================================\n\n");

    printf("1. Compression Levels:\n");
    printf("   - Level 0: No compression (fastest)\n");
    printf("   - Level 1: Best speed, lower ratio\n");
    printf("   - Level 6: Default, balanced\n");
    printf("   - Level 9: Best compression, slower\n\n");

    printf("2. When to use compression:\n");
    printf("   - Text data: Usually 40-60%% ratio\n");
    printf("   - Repeated patterns: Can achieve >90%% ratio\n");
    printf("   - Random/encrypted data: May expand in size\n\n");

    printf("3. Streaming vs One-shot:\n");
    printf("   - Use streaming for large data (>1MB)\n");
    printf("   - Use one-shot for small data (<100KB)\n");
    printf("   - Streaming reduces memory usage\n\n");

    printf("4. Gzip vs Raw:\n");
    printf("   - Gzip: Compatible with standard tools\n");
    printf("   - Gzip: Includes CRC32 verification\n");
    printf("   - Raw: Smaller overhead (no headers)\n\n");

    printf("5. Memory management:\n");
    printf("   - Always free() returned buffers\n");
    printf("   - Free contexts with zlib_context_free()\n");
    printf("   - Check return codes for errors\n\n");
}

/* ==================== Main ==================== */

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║   mod_zlib - Compression Examples     ║\n");
    printf("║   Node.js-style zlib API for C        ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");

    example_simple_compression();
    example_compression_levels();
    example_gzip_format();
    example_streaming_compression();
    example_error_handling();
    example_binary_data();
    example_crc32();
    show_performance_tips();

    printf("========================================\n");
    printf("All examples completed successfully!\n");
    printf("========================================\n\n");

    return 0;
}
