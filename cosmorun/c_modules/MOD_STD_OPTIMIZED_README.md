# mod_std_optimized - High-performance HashMap implementation

**File**: `c_modules/mod_std_optimized.c`
**Dependencies**:

- mod_std_optimized (internal)

## Overview

`mod_std_optimized` provides high-performance hashmap implementation.

## Quick Start

```c
#include "mod_std_optimized.c"

// TODO: Add minimal example
```

## API Reference

#### `opt_hashmap_t* opt_hashmap_new(void)`
- **Parameters**: None
- **Returns**: `opt_hashmap_t*`

#### `opt_hashmap_t* opt_hashmap_with_capacity(size_t capacity)`
- **Parameters**:
  - `size_t capacity`
- **Returns**: `opt_hashmap_t*`

#### `void opt_hashmap_free(opt_hashmap_t *map)`
- **Parameters**:
  - `opt_hashmap_t *map`
- **Returns**: None

#### `void opt_hashmap_set(opt_hashmap_t *map, const char *key, void *value)`
- **Parameters**:
  - `opt_hashmap_t *map`
  - `const char *key`
  - `void *value`
- **Returns**: None

#### `void* opt_hashmap_get(opt_hashmap_t *map, const char *key)`
- **Parameters**:
  - `opt_hashmap_t *map`
  - `const char *key`
- **Returns**: `void*`

#### `int opt_hashmap_has(opt_hashmap_t *map, const char *key)`
- **Parameters**:
  - `opt_hashmap_t *map`
  - `const char *key`
- **Returns**: `int`

#### `void opt_hashmap_remove(opt_hashmap_t *map, const char *key)`
- **Parameters**:
  - `opt_hashmap_t *map`
  - `const char *key`
- **Returns**: None

#### `size_t opt_hashmap_size(opt_hashmap_t *map)`
- **Parameters**:
  - `opt_hashmap_t *map`
- **Returns**: `size_t`

#### `void opt_hashmap_foreach(opt_hashmap_t *map, opt_hashmap_iter_fn fn, void *userdata)`
- **Parameters**:
  - `opt_hashmap_t *map`
  - `opt_hashmap_iter_fn fn`
  - `void *userdata`
- **Returns**: None

#### `void opt_hashmap_clear(opt_hashmap_t *map)`
- **Parameters**:
  - `opt_hashmap_t *map`
- **Returns**: None

#### `double opt_hashmap_load_factor(opt_hashmap_t *map)`
- **Parameters**:
  - `opt_hashmap_t *map`
- **Returns**: `double`

#### `void opt_hashmap_stats(opt_hashmap_t *map, opt_hashmap_stats_t *stats)`
- **Parameters**:
  - `opt_hashmap_t *map`
  - `opt_hashmap_stats_t *stats`
- **Returns**: None


## Example Usage

```c
// See c_modules/example_std_optimized.c for complete examples
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

- Example: `c_modules/example_std_optimized.c` (if available)
- Related modules: Check #include dependencies
