========================================
  mod_buffer - Node.js-style Buffer Module
========================================

OVERVIEW
--------
A complete implementation of Node.js Buffer API for cosmorun, providing:
- Dynamic binary data handling
- Multiple encoding support (UTF8, ASCII, HEX, BASE64, BINARY)
- Buffer manipulation (slice, concat, copy, fill)
- Search and comparison operations
- String conversion with encoding

FILES
-----
mod_buffer.h    - Header file with API declarations
mod_buffer.c    - Implementation (~550 lines)
test_mod_buffer.c - Comprehensive test suite (25 tests, 62 assertions)
buffer_demo.c   - Usage examples and demonstrations

COMPILATION
-----------
Single file test:
  ./cosmorun.exe c_modules/mod_buffer.c c_modules/test_mod_buffer.c

Run demo:
  ./cosmorun.exe c_modules/mod_buffer.c c_modules/buffer_demo.c

QUICK START
-----------

1. Create Buffer:
   buffer_t *buf = buffer_alloc(100);              // Zero-filled
   buffer_t *buf = buffer_alloc_unsafe(100);        // Uninitialized
   buffer_t *buf = buffer_from_string("Hello", BUFFER_ENCODING_UTF8);

2. Encoding Conversions:
   buffer_t *buf = buffer_from_string("48656c6c6f", BUFFER_ENCODING_HEX);
   char *hex = buffer_to_string(buf, BUFFER_ENCODING_HEX);
   char *base64 = buffer_to_string(buf, BUFFER_ENCODING_BASE64);

3. Buffer Operations:
   buffer_t *slice = buffer_slice(buf, 0, 5);
   buffer_t *concat = buffer_concat(buffers, count);
   buffer_copy(source, target, 0, 0, 5);
   buffer_fill(buf, 0x00, 0, 10);

4. Search:
   int pos = buffer_index_of(buf, value, value_len);
   int includes = buffer_includes(buf, value, value_len);

5. Comparison:
   int equal = buffer_equals(buf1, buf2);
   int cmp = buffer_compare(buf1, buf2);  // Returns -1, 0, or 1

ENCODING TYPES
--------------
BUFFER_ENCODING_UTF8     - UTF-8 text encoding
BUFFER_ENCODING_ASCII    - ASCII text encoding
BUFFER_ENCODING_HEX      - Hexadecimal string (e.g., "48656c6c6f")
BUFFER_ENCODING_BASE64   - Base64 encoding (e.g., "SGVsbG8=")
BUFFER_ENCODING_BINARY   - Raw binary data

API REFERENCE
-------------

Creation:
  buffer_t* buffer_alloc(size_t size)
  buffer_t* buffer_alloc_unsafe(size_t size)
  buffer_t* buffer_from_string(const char *str, buffer_encoding_t encoding)
  buffer_t* buffer_from_bytes(const unsigned char *data, size_t length)
  buffer_t* buffer_concat(buffer_t **buffers, size_t count)
  void buffer_free(buffer_t *buf)

Conversion:
  char* buffer_to_string(buffer_t *buf, buffer_encoding_t encoding)
  char* buffer_to_string_range(buffer_t *buf, buffer_encoding_t encoding,
                                size_t start, size_t end)
  int buffer_write(buffer_t *buf, const char *str, size_t offset,
                   buffer_encoding_t encoding)

Manipulation:
  buffer_t* buffer_slice(buffer_t *buf, size_t start, size_t end)
  int buffer_copy(buffer_t *source, buffer_t *target,
                  size_t target_start, size_t source_start, size_t source_end)
  void buffer_fill(buffer_t *buf, unsigned char value, size_t start, size_t end)

Comparison:
  int buffer_equals(buffer_t *a, buffer_t *b)
  int buffer_compare(buffer_t *a, buffer_t *b)

Search:
  int buffer_index_of(buffer_t *buf, const unsigned char *value, size_t value_len)
  int buffer_last_index_of(buffer_t *buf, const unsigned char *value, size_t value_len)
  int buffer_includes(buffer_t *buf, const unsigned char *value, size_t value_len)

Utility:
  size_t buffer_length(buffer_t *buf)
  int buffer_resize(buffer_t *buf, size_t new_size)
  const char* buffer_encoding_name(buffer_encoding_t encoding)

MEMORY MANAGEMENT
-----------------
- All buffer_* functions that create buffers return dynamically allocated memory
- Always call buffer_free() when done with a buffer
- buffer_to_string() returns dynamically allocated strings - must free() them
- NULL-safe: most functions handle NULL inputs gracefully

EXAMPLE USAGE
-------------

// Create and manipulate buffer
buffer_t *buf = buffer_from_string("Hello World", BUFFER_ENCODING_UTF8);

// Convert to different encodings
char *hex = buffer_to_string(buf, BUFFER_ENCODING_HEX);
printf("HEX: %s\n", hex);  // Output: 48656c6c6f20576f726c64
free(hex);

// Slice buffer
buffer_t *slice = buffer_slice(buf, 6, 11);
char *world = buffer_to_string(slice, BUFFER_ENCODING_UTF8);
printf("Sliced: %s\n", world);  // Output: World
free(world);

// Search
const unsigned char *search = (const unsigned char*)"World";
int pos = buffer_index_of(buf, search, 5);
printf("Found at: %d\n", pos);  // Output: 6

// Cleanup
buffer_free(slice);
buffer_free(buf);

TEST RESULTS
------------
All 25 tests passed with 62 assertions:
- Basic buffer operations
- Encoding conversions (HEX, BASE64, UTF8)
- Buffer manipulation (slice, concat, copy, fill)
- Search operations (indexOf, includes)
- Comparison (equals, compare)
- Edge cases (NULL handling, empty buffers, binary data)

IMPLEMENTATION DETAILS
----------------------
- Dynamic memory management with automatic capacity expansion
- Base64 encoder/decoder with proper padding handling
- Hex encoder/decoder with validation
- Efficient memcmp-based search algorithms
- NULL-safe error handling
- Compatible with TCC compiler and Cosmopolitan libc

CODING STYLE
------------
- Follows cosmorun conventions (see mod_std.c)
- Clean separation between header and implementation
- Comprehensive error checking
- Consistent naming: buffer_* prefix for all functions
- Static helper functions for internal operations

VERSION
-------
1.0.0 - Initial release with complete Node.js Buffer API compatibility
