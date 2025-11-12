# mod_nng - NNG (nanomsg-next-gen) runtime module for cosmorun

**File**: `c_modules/mod_nng.c`
**Dependencies**:

- mod_nng (internal)

## Overview

`mod_nng` provides nng (nanomsg-next-gen) runtime module for cosmorun.

## Quick Start

```c
#include "mod_nng.c"

// TODO: Add minimal example
```

## API Reference

#### `typedef int (*nng_rep0_open_t)(nng_socket *);`
- **Parameters**:
  - `nng_socket *`
- **Returns**: `typedef`

#### `typedef int (*nng_req0_open_t)(nng_socket *);`
- **Parameters**:
  - `nng_socket *`
- **Returns**: `typedef`

#### `typedef int (*nng_pub0_open_t)(nng_socket *);`
- **Parameters**:
  - `nng_socket *`
- **Returns**: `typedef`

#### `typedef int (*nng_sub0_open_t)(nng_socket *);`
- **Parameters**:
  - `nng_socket *`
- **Returns**: `typedef`

#### `typedef int (*nng_close_t)(nng_socket);`
- **Parameters**:
  - `nng_socket`
- **Returns**: `typedef`

#### `typedef int (*nng_listen_t)(nng_socket, const char *, void *, int);`
- **Parameters**:
  - `nng_socket`
  - `const char *`
  - `void *`
  - `int`
- **Returns**: `typedef`

#### `typedef int (*nng_dial_t)(nng_socket, const char *, void *, int);`
- **Parameters**:
  - `nng_socket`
  - `const char *`
  - `void *`
  - `int`
- **Returns**: `typedef`

#### `typedef int (*nng_send_t)(nng_socket, void *, size_t, int);`
- **Parameters**:
  - `nng_socket`
  - `void *`
  - `size_t`
  - `int`
- **Returns**: `typedef`

#### `typedef int (*nng_recv_t)(nng_socket, void *, size_t *, int);`
- **Parameters**:
  - `nng_socket`
  - `void *`
  - `size_t *`
  - `int`
- **Returns**: `typedef`

#### `typedef int (*nng_socket_set_ms_t)(nng_socket, const char *, nng_duration);`
- **Parameters**:
  - `nng_socket`
  - `const char *`
  - `nng_duration`
- **Returns**: `typedef`

#### `typedef int (*nng_socket_set_t)(nng_socket, const char *, const void *, size_t);`
- **Parameters**:
  - `nng_socket`
  - `const char *`
  - `const void *`
  - `size_t`
- **Returns**: `typedef`

#### `typedef int (*nng_sub0_subscribe_t)(nng_socket, const void *, size_t);`
- **Parameters**:
  - `nng_socket`
  - `const void *`
  - `size_t`
- **Returns**: `typedef`

#### `nng_context_t* nng_init(const char *lib_path)`
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
// See c_modules/example_nng.c for complete examples
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

- Example: `c_modules/example_nng.c` (if available)
- Related modules: Check #include dependencies
