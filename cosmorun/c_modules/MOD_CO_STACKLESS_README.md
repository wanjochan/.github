# mod_co_stackless - 

**File**: `c_modules/mod_co_stackless.c`
**Dependencies**:
None (pure C implementation)

## Overview

`mod_co_stackless` provides .

## Quick Start

```c
#include "mod_co_stackless.c"

// TODO: Add minimal example
```

## API Reference

#### `co_t* co_new(co_func_t func, void *arg)`
- **Description**: Create new coroutine
- **Parameters**:
  - `co_func_t func`
  - `void *arg`
- **Returns**: `co_t*`

#### `void* co_start(co_t *co)`
- **Description**: Start or resume coroutine
- **Parameters**:
  - `co_t *co`
- **Returns**: `void*`

#### `void co_yield_current(void *value)`
- **Description**: Yield from current coroutine
- **Parameters**:
  - `void *value`
- **Returns**: None

#### `co_t* co_current(void)`
- **Description**: Get current coroutine
- **Parameters**: None
- **Returns**: `co_t*`

#### `void co_free(co_t *co)`
- **Description**: Free coroutine
- **Parameters**:
  - `co_t *co`
- **Returns**: None

#### `co_state_t co_state(co_t *co)`
- **Description**: Get coroutine state
- **Parameters**:
  - `co_t *co`
- **Returns**: `co_state_t`

#### `int co_is_alive(co_t *co)`
- **Description**: Check if coroutine is alive
- **Parameters**:
  - `co_t *co`
- **Returns**: `int`

#### `void co_set_default_stack_size(unsigned long bytes)`
- **Description**: * Configuration API - For compatibility with stackful version  * Stackless doesn't use stack, so these are no-op
- **Parameters**:
  - `unsigned long bytes`
- **Returns**: None

#### `void co_set_user_data(co_t *co, void *data)`
- **Description**: Set user data pointer
- **Parameters**:
  - `co_t *co`
  - `void *data`
- **Returns**: None

#### `void* co_get_user_data(co_t *co)`
- **Description**: Get user data pointer
- **Parameters**:
  - `co_t *co`
- **Returns**: `void*`

#### `void co_reset(co_t *co)`
- **Description**: Reset coroutine to initial state (allows reuse)
- **Parameters**:
  - `co_t *co`
- **Returns**: None

#### `co_api* co_module_init(void)`
- **Description**: Note: user_data and func/arg are preserved   * Module API - Same structure as stackful for compatibility
- **Parameters**: None
- **Returns**: `co_api*`


## Example Usage

```c
// See c_modules/example_co_stackless.c for complete examples
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

- Example: `c_modules/example_co_stackless.c` (if available)
- Related modules: Check #include dependencies
