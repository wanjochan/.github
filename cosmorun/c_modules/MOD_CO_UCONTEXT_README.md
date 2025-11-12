# mod_co_ucontext - 

**File**: `c_modules/mod_co_ucontext.c`
**Dependencies**:

- mod_coroutine (internal)

## Overview

`mod_co_ucontext` provides .

## Quick Start

```c
#include "mod_co_ucontext.c"

// TODO: Add minimal example
```

## API Reference

#### `coro_t* coro_new(coro_func_t func, void *arg)`
- **Description**: Return to caller with NULL
- **Parameters**:
  - `coro_func_t func`
  - `void *arg`
- **Returns**: `coro_t*`

#### `void* coro_start(coro_t *co, void *value)`
- **Description**: Create context with entry wrapper
- **Parameters**:
  - `coro_t *co`
  - `void *value`
- **Returns**: `void*`

#### `void* coro_resume(coro_t *co, void *value)`
- **Description**: Coroutine suspended or terminated, we're back in main
- **Parameters**:
  - `coro_t *co`
  - `void *value`
- **Returns**: `void*`

#### `void* coro_suspend(void *value)`
- **Description**: Coroutine suspended or terminated, we're back in main
- **Parameters**:
  - `void *value`
- **Returns**: `void*`

#### `coro_t* coro_current(void)`
- **Description**: Return value passed by resume()
- **Parameters**: None
- **Returns**: `coro_t*`

#### `void coro_free(coro_t *co)`
- **Parameters**:
  - `coro_t *co`
- **Returns**: None

#### `coro_state_t coro_state(coro_t *co)`
- **Description**: Safety check: don't free running or suspended coroutine
- **Parameters**:
  - `coro_t *co`
- **Returns**: `coro_state_t`

#### `int coro_is_alive(coro_t *co)`
- **Parameters**:
  - `coro_t *co`
- **Returns**: `int`

#### `void coro_set_default_stack_size(size_t bytes)`
- **Parameters**:
  - `size_t bytes`
- **Returns**: None

#### `size_t coro_get_default_stack_size(void)`
- **Parameters**: None
- **Returns**: `size_t`

#### `coroutine_api* coroutine_module_init(void)`
- **Parameters**: None
- **Returns**: `coroutine_api*`


## Example Usage

```c
// See c_modules/example_co_ucontext.c for complete examples
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

- Example: `c_modules/example_co_ucontext.c` (if available)
- Related modules: Check #include dependencies
