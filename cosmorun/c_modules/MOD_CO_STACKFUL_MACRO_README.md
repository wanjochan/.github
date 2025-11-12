# mod_co_stackful_macro - 

**File**: `c_modules/mod_co_stackful_macro.c`
**Dependencies**:
None (pure C implementation)

## Overview

`mod_co_stackful_macro` provides .

## Quick Start

```c
#include "mod_co_stackful_macro.c"

// TODO: Add minimal example
```

## API Reference

#### `co_t* co_new(co_func_t func, void *arg)`
- **Description**: API Implementation
- **Parameters**:
  - `co_func_t func`
  - `void *arg`
- **Returns**: `co_t*`

#### `void* co_start(co_t *co)`
- **Parameters**:
  - `co_t *co`
- **Returns**: `void*`

#### `void co_yield_current(void *value)`
- **Parameters**:
  - `void *value`
- **Returns**: None

#### `co_t* co_current(void)`
- **Parameters**: None
- **Returns**: `co_t*`

#### `void co_free(co_t *co)`
- **Parameters**:
  - `co_t *co`
- **Returns**: None

#### `co_state_t co_state(co_t *co)`
- **Parameters**:
  - `co_t *co`
- **Returns**: `co_state_t`

#### `int co_is_alive(co_t *co)`
- **Parameters**:
  - `co_t *co`
- **Returns**: `int`

#### `void co_set_default_stack_size(unsigned long bytes)`
- **Parameters**:
  - `unsigned long bytes`
- **Returns**: None

#### `co_api* co_module_init(void)`
- **Description**: Module API
- **Parameters**: None
- **Returns**: `co_api*`


## Example Usage

```c
// See c_modules/example_co_stackful_macro.c for complete examples
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

- Example: `c_modules/example_co_stackful_macro.c` (if available)
- Related modules: Check #include dependencies
