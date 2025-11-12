# mod_mempool - High-performance memory pool allocator implementation

**File**: `c_modules/mod_mempool.c`
**Dependencies**:

- mod_mempool (internal)

## Overview

`mod_mempool` provides high-performance memory pool allocator implementation.

## Quick Start

```c
#include "mod_mempool.c"

// TODO: Add minimal example
```

## API Reference

#### `size_t mempool_align_size(size_t size, size_t alignment)`
- **Parameters**:
  - `size_t size`
  - `size_t alignment`
- **Returns**: `size_t`

#### `mempool_t* mempool_create(size_t obj_size, size_t initial_capacity)`
- **Description**: * Free memory block  * Lock pool if thread-safe  * Unlock pool if thread-safe
- **Parameters**:
  - `size_t obj_size`
  - `size_t initial_capacity`
- **Returns**: `mempool_t*`

#### `mempool_t* mempool_create_ex(size_t obj_size, size_t initial_capacity, mempool_align_t alignment, mempool_options_t options)`
- **Parameters**:
  - `size_t obj_size`
  - `size_t initial_capacity`
  - `mempool_align_t alignment`
  - `mempool_options_t options`
- **Returns**: `mempool_t*`

#### `void mempool_destroy(mempool_t *pool)`
- **Description**: Initialize statistics
- **Parameters**:
  - `mempool_t *pool`
- **Returns**: None

#### `void* mempool_alloc(mempool_t *pool)`
- **Description**: Destroy mutex
- **Parameters**:
  - `mempool_t *pool`
- **Returns**: `void*`

#### `void* mempool_alloc_size(mempool_t *pool, size_t size)`
- **Parameters**:
  - `mempool_t *pool`
  - `size_t size`
- **Returns**: `void*`

#### `void mempool_free(mempool_t *pool, void *ptr)`
- **Description**: Zero memory if requested
- **Parameters**:
  - `mempool_t *pool`
  - `void *ptr`
- **Returns**: None

#### `void mempool_reset(mempool_t *pool)`
- **Description**: Add to free list
- **Parameters**:
  - `mempool_t *pool`
- **Returns**: None

#### `void mempool_print_stats(mempool_t *pool)`
- **Description**: Update statistics
- **Parameters**:
  - `mempool_t *pool`
- **Returns**: None

#### `size_t mempool_get_usage(mempool_t *pool)`
- **Description**: Calculate integer percentage to avoid float operations
- **Parameters**:
  - `mempool_t *pool`
- **Returns**: `size_t`

#### `size_t mempool_get_capacity(mempool_t *pool)`
- **Parameters**:
  - `mempool_t *pool`
- **Returns**: `size_t`


## Example Usage

```c
// See c_modules/example_mempool.c for complete examples
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

- Example: `c_modules/example_mempool.c` (if available)
- Related modules: Check #include dependencies
