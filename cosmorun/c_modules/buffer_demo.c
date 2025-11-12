/*
 * buffer_demo.c - Demonstration of mod_buffer functionality
 */

#include "src/cosmo_libc.h"
#include "mod_buffer.h"

void demo_basic_operations(void) {
    printf("\n=== Basic Buffer Operations ===\n");

    // Create buffer from string
    buffer_t *buf = buffer_from_string("Hello, World!", BUFFER_ENCODING_UTF8);
    printf("Created buffer with length: %zu\n", buffer_length(buf));

    // Convert to string
    char *str = buffer_to_string(buf, BUFFER_ENCODING_UTF8);
    printf("Buffer content: %s\n", str);

    free(str);
    buffer_free(buf);
}

void demo_encoding_conversions(void) {
    printf("\n=== Encoding Conversions ===\n");

    // Create buffer from string
    buffer_t *buf = buffer_from_string("Hello", BUFFER_ENCODING_UTF8);

    // Convert to hex
    char *hex = buffer_to_string(buf, BUFFER_ENCODING_HEX);
    printf("HEX encoding: %s\n", hex);
    free(hex);

    // Convert to base64
    char *base64 = buffer_to_string(buf, BUFFER_ENCODING_BASE64);
    printf("BASE64 encoding: %s\n", base64);
    free(base64);

    buffer_free(buf);

    // Create buffer from hex
    buffer_t *hex_buf = buffer_from_string("48656c6c6f", BUFFER_ENCODING_HEX);
    char *decoded = buffer_to_string(hex_buf, BUFFER_ENCODING_UTF8);
    printf("Decoded from HEX: %s\n", decoded);
    free(decoded);
    buffer_free(hex_buf);

    // Create buffer from base64
    buffer_t *b64_buf = buffer_from_string("SGVsbG8gV29ybGQh", BUFFER_ENCODING_BASE64);
    char *decoded2 = buffer_to_string(b64_buf, BUFFER_ENCODING_UTF8);
    printf("Decoded from BASE64: %s\n", decoded2);
    free(decoded2);
    buffer_free(b64_buf);
}

void demo_buffer_manipulation(void) {
    printf("\n=== Buffer Manipulation ===\n");

    // Concatenate buffers
    buffer_t *buf1 = buffer_from_string("Hello", BUFFER_ENCODING_UTF8);
    buffer_t *buf2 = buffer_from_string(", ", BUFFER_ENCODING_UTF8);
    buffer_t *buf3 = buffer_from_string("World!", BUFFER_ENCODING_UTF8);

    buffer_t *buffers[] = {buf1, buf2, buf3};
    buffer_t *concat = buffer_concat(buffers, 3);

    char *result = buffer_to_string(concat, BUFFER_ENCODING_UTF8);
    printf("Concatenated: %s\n", result);
    free(result);

    buffer_free(buf1);
    buffer_free(buf2);
    buffer_free(buf3);

    // Slice buffer
    buffer_t *slice = buffer_slice(concat, 7, 13);
    char *slice_str = buffer_to_string(slice, BUFFER_ENCODING_UTF8);
    printf("Sliced (7-13): %s\n", slice_str);
    free(slice_str);

    buffer_free(slice);
    buffer_free(concat);

    // Fill buffer
    buffer_t *fill_buf = buffer_alloc(10);
    buffer_fill(fill_buf, 'X', 0, 10);
    char *fill_str = buffer_to_string(fill_buf, BUFFER_ENCODING_UTF8);
    printf("Filled buffer: %s\n", fill_str);
    free(fill_str);
    buffer_free(fill_buf);
}

void demo_search_operations(void) {
    printf("\n=== Search Operations ===\n");

    buffer_t *buf = buffer_from_string("Hello World Hello", BUFFER_ENCODING_UTF8);

    // Search for substring
    const unsigned char *search = (const unsigned char*)"World";
    int pos = buffer_index_of(buf, search, 5);
    printf("Position of 'World': %d\n", pos);

    // Search for last occurrence
    const unsigned char *hello = (const unsigned char*)"Hello";
    int last_pos = buffer_last_index_of(buf, hello, 5);
    printf("Last position of 'Hello': %d\n", last_pos);

    // Check if includes
    if (buffer_includes(buf, search, 5)) {
        printf("Buffer includes 'World'\n");
    }

    const unsigned char *missing = (const unsigned char*)"xyz";
    if (!buffer_includes(buf, missing, 3)) {
        printf("Buffer does not include 'xyz'\n");
    }

    buffer_free(buf);
}

void demo_comparison(void) {
    printf("\n=== Buffer Comparison ===\n");

    buffer_t *buf1 = buffer_from_string("Apple", BUFFER_ENCODING_UTF8);
    buffer_t *buf2 = buffer_from_string("Banana", BUFFER_ENCODING_UTF8);
    buffer_t *buf3 = buffer_from_string("Apple", BUFFER_ENCODING_UTF8);

    // Equality check
    if (buffer_equals(buf1, buf3)) {
        printf("buf1 equals buf3\n");
    }

    if (!buffer_equals(buf1, buf2)) {
        printf("buf1 does not equal buf2\n");
    }

    // Comparison
    int cmp = buffer_compare(buf1, buf2);
    if (cmp < 0) {
        printf("Apple < Banana\n");
    }

    buffer_free(buf1);
    buffer_free(buf2);
    buffer_free(buf3);
}

void demo_binary_data(void) {
    printf("\n=== Binary Data Handling ===\n");

    // Create buffer from binary data
    unsigned char binary[] = {0x00, 0xFF, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x80, 0x01};
    buffer_t *buf = buffer_from_bytes(binary, sizeof(binary));

    printf("Binary buffer length: %zu\n", buffer_length(buf));
    printf("Binary data (hex): ");
    for (size_t i = 0; i < buffer_length(buf); i++) {
        printf("%02x ", buf->data[i]);
    }
    printf("\n");

    // Convert to hex string
    char *hex = buffer_to_string(buf, BUFFER_ENCODING_HEX);
    printf("As hex string: %s\n", hex);
    free(hex);

    buffer_free(buf);
}

void demo_write_operations(void) {
    printf("\n=== Write Operations ===\n");

    buffer_t *buf = buffer_alloc(50);

    // Write UTF8 string
    int written = buffer_write(buf, "Hello", 0, BUFFER_ENCODING_UTF8);
    printf("Written %d bytes\n", written);

    // Write hex string
    written = buffer_write(buf, "20576f726c6421", 5, BUFFER_ENCODING_HEX);
    printf("Written %d bytes from hex\n", written);

    // Display result
    char *result = buffer_to_string(buf, BUFFER_ENCODING_UTF8);
    printf("Buffer content: %s\n", result);
    free(result);

    buffer_free(buf);
}

int main(int argc, char **argv) {
    printf("========================================\n");
    printf("  Buffer Module Demonstration\n");
    printf("========================================\n");

    demo_basic_operations();
    demo_encoding_conversions();
    demo_buffer_manipulation();
    demo_search_operations();
    demo_comparison();
    demo_binary_data();
    demo_write_operations();

    printf("\n========================================\n");
    printf("  Demonstration Complete\n");
    printf("========================================\n\n");

    return 0;
}
