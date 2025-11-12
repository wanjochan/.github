# mod_error - Unified error handling system implementation

**File**: `c_modules/mod_error.c`
**Dependencies**:

- mod_error (internal)

## Overview

`mod_error` provides unified error handling system implementation.

## Quick Start

```c
#include "mod_error.c"

// TODO: Add minimal example
```

## API Reference

#### `void cosmorun_set_error(cosmorun_error_t code, const char *msg)`
- **Parameters**:
  - `cosmorun_error_t code`
  - `const char *msg`
- **Returns**: None

#### `void cosmorun_set_error_fmt(cosmorun_error_t code, const char *fmt, ...)`
- **Parameters**:
  - `cosmorun_error_t code`
  - `const char *fmt`
  - `...`
- **Returns**: None

#### `cosmorun_error_t cosmorun_get_last_error(void)`
- **Parameters**: None
- **Returns**: `cosmorun_error_t`

#### `void cosmorun_clear_error(void)`
- **Parameters**: None
- **Returns**: None


## Example Usage

```c
// See c_modules/example_error.c for complete examples
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

- Example: `c_modules/example_error.c` (if available)
- Related modules: Check #include dependencies
