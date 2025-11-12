# mod_zlib - 

**File**: `c_modules/mod_zlib.c`
**Dependencies**:

- mod_zlib (internal)

## Overview

`mod_zlib` provides .

## Quick Start

```c
#include "mod_zlib.c"

// TODO: Add minimal example
```

## API Reference

#### `uint32_t zlib_crc32(uint32_t crc, const uint8_t *data, size_t len)`
- **Parameters**:
  - `uint32_t crc`
  - `const uint8_t *data`
  - `size_t len`
- **Returns**: `uint32_t`

#### `uint32_t zlib_adler32(uint32_t adler, const uint8_t *data, size_t len)`
- **Parameters**:
  - `uint32_t adler`
  - `const uint8_t *data`
  - `size_t len`
- **Returns**: `uint32_t`

#### `int zlib_compress(const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len, int level)`
- **Description**: * Simplified compression using LZ77 approach:  * - Find matching sequences in sliding window  * - Encode as (length, distance) pair  * - For better compression ratio, use simple dictionary
- **Parameters**:
  - `const uint8_t *input`
  - `size_t input_len`
  - `uint8_t **output`
  - `size_t *output_len`
  - `int level`
- **Returns**: `int`

#### `int zlib_decompress(const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len)`
- **Parameters**:
  - `const uint8_t *input`
  - `size_t input_len`
  - `uint8_t **output`
  - `size_t *output_len`
- **Returns**: `int`

#### `int zlib_gzip_compress(const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len, int level)`
- **Parameters**:
  - `const uint8_t *input`
  - `size_t input_len`
  - `uint8_t **output`
  - `size_t *output_len`
  - `int level`
- **Returns**: `int`

#### `int zlib_gzip_decompress(const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len)`
- **Parameters**:
  - `const uint8_t *input`
  - `size_t input_len`
  - `uint8_t **output`
  - `size_t *output_len`
- **Returns**: `int`

#### `zlib_context_t* zlib_deflate_init(int level, int format)`
- **Parameters**:
  - `int level`
  - `int format`
- **Returns**: `zlib_context_t*`

#### `int zlib_deflate_update(zlib_context_t *ctx, const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len)`
- **Parameters**:
  - `zlib_context_t *ctx`
  - `const uint8_t *input`
  - `size_t input_len`
  - `uint8_t **output`
  - `size_t *output_len`
- **Returns**: `int`

#### `int zlib_deflate_end(zlib_context_t *ctx, uint8_t **output, size_t *output_len)`
- **Parameters**:
  - `zlib_context_t *ctx`
  - `uint8_t **output`
  - `size_t *output_len`
- **Returns**: `int`

#### `zlib_context_t* zlib_inflate_init(int format)`
- **Parameters**:
  - `int format`
- **Returns**: `zlib_context_t*`

#### `int zlib_inflate_update(zlib_context_t *ctx, const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len)`
- **Parameters**:
  - `zlib_context_t *ctx`
  - `const uint8_t *input`
  - `size_t input_len`
  - `uint8_t **output`
  - `size_t *output_len`
- **Returns**: `int`

#### `int zlib_inflate_end(zlib_context_t *ctx, uint8_t **output, size_t *output_len)`
- **Parameters**:
  - `zlib_context_t *ctx`
  - `uint8_t **output`
  - `size_t *output_len`
- **Returns**: `int`

#### `void zlib_context_free(zlib_context_t *ctx)`
- **Parameters**:
  - `zlib_context_t *ctx`
- **Returns**: None


## Example Usage

```c
// See c_modules/example_zlib.c for complete examples
```

## Memory Management

Functions returning pointers (e.g., `char*`, struct pointers) allocate memory that must be freed by the caller using the corresponding `_free()` function.

## Error Handling

Functions returning `int` typically return:
- `0` or positive value on success
- `-1` on error
- Check errno for system error details

## Thread Safety

Not thread-safe by default. Use external locking for multi-threaded access.

## See Also

- Example: `c_modules/example_zlib.c` (if available)
- Related modules: Check #include dependencies
