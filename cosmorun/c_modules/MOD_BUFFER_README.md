# mod_buffer - Buffer implementation

**File**: `c_modules/mod_buffer.c`
**Dependencies**:

- mod_buffer (internal)
- mod_error_impl (internal)

## Overview

`mod_buffer` provides buffer implementation.

## Quick Start

```c
#include "mod_buffer.c"

// TODO: Add minimal example
```

## API Reference

#### `buffer_t* buffer_alloc(size_t size)`
- **Parameters**:
  - `size_t size`
- **Returns**: `buffer_t*`

#### `buffer_t* buffer_alloc_unsafe(size_t size)`
- **Parameters**:
  - `size_t size`
- **Returns**: `buffer_t*`

#### `buffer_t* buffer_from_string(const char *str, buffer_encoding_t encoding)`
- **Parameters**:
  - `const char *str`
  - `buffer_encoding_t encoding`
- **Returns**: `buffer_t*`

#### `buffer_t* buffer_from_bytes(const unsigned char *data, size_t length)`
- **Parameters**:
  - `const unsigned char *data`
  - `size_t length`
- **Returns**: `buffer_t*`

#### `buffer_t* buffer_concat(buffer_t **buffers, size_t count)`
- **Parameters**:
  - `buffer_t **buffers`
  - `size_t count`
- **Returns**: `buffer_t*`

#### `void buffer_free(buffer_t *buf)`
- **Parameters**:
  - `buffer_t *buf`
- **Returns**: None

#### `char* buffer_to_string(buffer_t *buf, buffer_encoding_t encoding)`
- **Parameters**:
  - `buffer_t *buf`
  - `buffer_encoding_t encoding`
- **Returns**: `char*`

#### `char* buffer_to_string_range(buffer_t *buf, buffer_encoding_t encoding, size_t start, size_t end)`
- **Parameters**:
  - `buffer_t *buf`
  - `buffer_encoding_t encoding`
  - `size_t start`
  - `size_t end`
- **Returns**: `char*`

#### `int buffer_write(buffer_t *buf, const char *str, size_t offset, buffer_encoding_t encoding)`
- **Parameters**:
  - `buffer_t *buf`
  - `const char *str`
  - `size_t offset`
  - `buffer_encoding_t encoding`
- **Returns**: `int`

#### `int buffer_copy(buffer_t *source, buffer_t *target, size_t target_start, size_t source_start, size_t source_end)`
- **Parameters**:
  - `buffer_t *source`
  - `buffer_t *target`
  - `size_t target_start`
  - `size_t source_start`
  - `size_t source_end`
- **Returns**: `int`

#### `buffer_t* buffer_slice(buffer_t *buf, size_t start, size_t end)`
- **Parameters**:
  - `buffer_t *buf`
  - `size_t start`
  - `size_t end`
- **Returns**: `buffer_t*`

#### `void buffer_fill(buffer_t *buf, unsigned char value, size_t start, size_t end)`
- **Parameters**:
  - `buffer_t *buf`
  - `unsigned char value`
  - `size_t start`
  - `size_t end`
- **Returns**: None

#### `int buffer_equals(buffer_t *a, buffer_t *b)`
- **Parameters**:
  - `buffer_t *a`
  - `buffer_t *b`
- **Returns**: `int`

#### `int buffer_compare(buffer_t *a, buffer_t *b)`
- **Parameters**:
  - `buffer_t *a`
  - `buffer_t *b`
- **Returns**: `int`

#### `int buffer_index_of(buffer_t *buf, const unsigned char *value, size_t value_len)`
- **Parameters**:
  - `buffer_t *buf`
  - `const unsigned char *value`
  - `size_t value_len`
- **Returns**: `int`

#### `int buffer_last_index_of(buffer_t *buf, const unsigned char *value, size_t value_len)`
- **Parameters**:
  - `buffer_t *buf`
  - `const unsigned char *value`
  - `size_t value_len`
- **Returns**: `int`

#### `int buffer_includes(buffer_t *buf, const unsigned char *value, size_t value_len)`
- **Parameters**:
  - `buffer_t *buf`
  - `const unsigned char *value`
  - `size_t value_len`
- **Returns**: `int`

#### `size_t buffer_length(buffer_t *buf)`
- **Parameters**:
  - `buffer_t *buf`
- **Returns**: `size_t`

#### `int buffer_resize(buffer_t *buf, size_t new_size)`
- **Parameters**:
  - `buffer_t *buf`
  - `size_t new_size`
- **Returns**: `int`


## Example Usage

```c
// See c_modules/example_buffer.c for complete examples
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

- Example: `c_modules/example_buffer.c` (if available)
- Related modules: Check #include dependencies
