# mod_stream - Node.js-style Stream implementation for cosmorun

**File**: `c_modules/mod_stream.c`
**Dependencies**:

- mod_stream (internal)

## Overview

`mod_stream` provides node.js-style stream implementation for cosmorun.

## Quick Start

```c
#include "mod_stream.c"

// TODO: Add minimal example
```

## API Reference

#### `void stream_on(stream_t *stream, stream_event_type_t event, stream_event_listener_t callback, void *userdata)`
- **Parameters**:
  - `stream_t *stream`
  - `stream_event_type_t event`
  - `stream_event_listener_t callback`
  - `void *userdata`
- **Returns**: None

#### `void stream_off(stream_t *stream, stream_event_type_t event, stream_event_listener_t callback)`
- **Parameters**:
  - `stream_t *stream`
  - `stream_event_type_t event`
  - `stream_event_listener_t callback`
- **Returns**: None

#### `void stream_emit(stream_t *stream, stream_event_type_t event, void *data)`
- **Parameters**:
  - `stream_t *stream`
  - `stream_event_type_t event`
  - `void *data`
- **Returns**: None

#### `stream_t* stream_create(stream_type_t type, stream_options_t *options)`
- **Parameters**:
  - `stream_type_t type`
  - `stream_options_t *options`
- **Returns**: `stream_t*`

#### `void stream_destroy(stream_t *stream)`
- **Parameters**:
  - `stream_t *stream`
- **Returns**: None

#### `int stream_push(stream_t *stream, const void *chunk, size_t length)`
- **Parameters**:
  - `stream_t *stream`
  - `const void *chunk`
  - `size_t length`
- **Returns**: `int`

#### `ssize_t stream_read(stream_t *stream, void *buffer, size_t size)`
- **Parameters**:
  - `stream_t *stream`
  - `void *buffer`
  - `size_t size`
- **Returns**: `ssize_t`

#### `void stream_pause(stream_t *stream)`
- **Parameters**:
  - `stream_t *stream`
- **Returns**: None

#### `void stream_resume(stream_t *stream)`
- **Parameters**:
  - `stream_t *stream`
- **Returns**: None

#### `int stream_is_paused(stream_t *stream)`
- **Parameters**:
  - `stream_t *stream`
- **Returns**: `int`

#### `void stream_end_readable(stream_t *stream)`
- **Parameters**:
  - `stream_t *stream`
- **Returns**: None

#### `int stream_write(stream_t *stream, const void *chunk, size_t length)`
- **Parameters**:
  - `stream_t *stream`
  - `const void *chunk`
  - `size_t length`
- **Returns**: `int`

#### `void stream_end(stream_t *stream, const void *chunk, size_t length)`
- **Parameters**:
  - `stream_t *stream`
  - `const void *chunk`
  - `size_t length`
- **Returns**: None

#### `int stream_is_writable(stream_t *stream)`
- **Parameters**:
  - `stream_t *stream`
- **Returns**: `int`

#### `stream_t* stream_pipe(stream_t *source, stream_t *dest)`
- **Parameters**:
  - `stream_t *source`
  - `stream_t *dest`
- **Returns**: `stream_t*`

#### `void stream_unpipe(stream_t *source, stream_t *dest)`
- **Parameters**:
  - `stream_t *source`
  - `stream_t *dest`
- **Returns**: None

#### `int stream_is_readable(stream_t *stream)`
- **Parameters**:
  - `stream_t *stream`
- **Returns**: `int`

#### `int stream_is_ended(stream_t *stream)`
- **Parameters**:
  - `stream_t *stream`
- **Returns**: `int`

#### `int stream_is_finished(stream_t *stream)`
- **Parameters**:
  - `stream_t *stream`
- **Returns**: `int`

#### `int stream_is_destroyed(stream_t *stream)`
- **Parameters**:
  - `stream_t *stream`
- **Returns**: `int`

#### `size_t stream_readable_length(stream_t *stream)`
- **Parameters**:
  - `stream_t *stream`
- **Returns**: `size_t`

#### `size_t stream_writable_length(stream_t *stream)`
- **Parameters**:
  - `stream_t *stream`
- **Returns**: `size_t`

#### `stream_t* stream_from_buffer(const void *data, size_t length)`
- **Parameters**:
  - `const void *data`
  - `size_t length`
- **Returns**: `stream_t*`

#### `stream_t* stream_to_buffer(void)`
- **Parameters**: None
- **Returns**: `stream_t*`

#### `void* stream_get_buffer(stream_t *stream, size_t *out_length)`
- **Parameters**:
  - `stream_t *stream`
  - `size_t *out_length`
- **Returns**: `void*`


## Example Usage

```c
// See c_modules/example_stream.c for complete examples
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

- Example: `c_modules/example_stream.c` (if available)
- Related modules: Check #include dependencies
