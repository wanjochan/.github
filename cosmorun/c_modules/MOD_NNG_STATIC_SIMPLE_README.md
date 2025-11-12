# mod_nng_static_simple - Simplified static NNG implementation for cosmorun

**File**: `c_modules/mod_nng_static_simple.c`
**Dependencies**:

- mod_std (internal)

## Overview

`mod_nng_static_simple` provides simplified static nng implementation for cosmorun.

## Quick Start

```c
#include "mod_nng_static_simple.c"

// TODO: Add minimal example
```

## API Reference

#### `nng_context_t* nng_init(const char *lib_path)`
- **Description**: Helper functions
- **Parameters**:
  - `const char *lib_path`
- **Returns**: `nng_context_t*`

#### `void nng_cleanup(nng_context_t *ctx)`
- **Parameters**:
  - `nng_context_t *ctx`
- **Returns**: None

#### `int nng_listen_rep(nng_context_t *ctx, const char *url)`
- **Parameters**:
  - `nng_context_t *ctx`
  - `const char *url`
- **Returns**: `int`

#### `int nng_dial_req(nng_context_t *ctx, const char *url)`
- **Parameters**:
  - `nng_context_t *ctx`
  - `const char *url`
- **Returns**: `int`

#### `std_string_t* nng_recv_msg(nng_context_t *ctx)`
- **Parameters**:
  - `nng_context_t *ctx`
- **Returns**: `std_string_t*`

#### `int nng_send_msg(nng_context_t *ctx, const char *data)`
- **Parameters**:
  - `nng_context_t *ctx`
  - `const char *data`
- **Returns**: `int`

#### `int nng_bind_pub(nng_context_t *ctx, const char *url)`
- **Parameters**:
  - `nng_context_t *ctx`
  - `const char *url`
- **Returns**: `int`

#### `int nng_dial_sub(nng_context_t *ctx, const char *url)`
- **Parameters**:
  - `nng_context_t *ctx`
  - `const char *url`
- **Returns**: `int`

#### `int nng_sub_subscribe(nng_context_t *ctx, const char *topic)`
- **Parameters**:
  - `nng_context_t *ctx`
  - `const char *topic`
- **Returns**: `int`

#### `int nng_set_recv_timeout(nng_context_t *ctx, int timeout_ms)`
- **Parameters**:
  - `nng_context_t *ctx`
  - `int timeout_ms`
- **Returns**: `int`

#### `int nng_set_send_timeout(nng_context_t *ctx, int timeout_ms)`
- **Parameters**:
  - `nng_context_t *ctx`
  - `int timeout_ms`
- **Returns**: `int`

#### `void nng_close_socket(nng_context_t *ctx)`
- **Parameters**:
  - `nng_context_t *ctx`
- **Returns**: None

#### `int nng_selftest_reqrep(const char *lib_path)`
- **Parameters**:
  - `const char *lib_path`
- **Returns**: `int`

#### `int nng_selftest_pubsub(const char *lib_path)`
- **Parameters**:
  - `const char *lib_path`
- **Returns**: `int`


## Example Usage

```c
// See c_modules/example_nng_static_simple.c for complete examples
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

- Example: `c_modules/example_nng_static_simple.c` (if available)
- Related modules: Check #include dependencies
