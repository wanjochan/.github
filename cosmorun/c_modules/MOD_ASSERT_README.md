# mod_assert - Assertion utilities implementation

**File**: `c_modules/mod_assert.c`
**Dependencies**:

- mod_assert (internal)

## Overview

`mod_assert` provides assertion utilities implementation.

## Quick Start

```c
#include "mod_assert.c"

// TODO: Add minimal example
```

## API Reference

#### `void assert_true(int condition, const char* message, const char* file, int line)`
- **Description**: Internal helper: increment stats
- **Parameters**:
  - `int condition`
  - `const char* message`
  - `const char* file`
  - `int line`
- **Returns**: None

#### `void assert_false(int condition, const char* message, const char* file, int line)`
- **Parameters**:
  - `int condition`
  - `const char* message`
  - `const char* file`
  - `int line`
- **Returns**: None

#### `void assert_eq_int(int actual, int expected, const char* file, int line)`
- **Parameters**:
  - `int actual`
  - `int expected`
  - `const char* file`
  - `int line`
- **Returns**: None

#### `void assert_ne_int(int actual, int expected, const char* file, int line)`
- **Parameters**:
  - `int actual`
  - `int expected`
  - `const char* file`
  - `int line`
- **Returns**: None

#### `void assert_gt_int(int actual, int expected, const char* file, int line)`
- **Parameters**:
  - `int actual`
  - `int expected`
  - `const char* file`
  - `int line`
- **Returns**: None

#### `void assert_lt_int(int actual, int expected, const char* file, int line)`
- **Parameters**:
  - `int actual`
  - `int expected`
  - `const char* file`
  - `int line`
- **Returns**: None

#### `void assert_eq_str(const char* actual, const char* expected, const char* file, int line)`
- **Parameters**:
  - `const char* actual`
  - `const char* expected`
  - `const char* file`
  - `int line`
- **Returns**: None

#### `void assert_ne_str(const char* actual, const char* expected, const char* file, int line)`
- **Description**: Handle NULL cases
- **Parameters**:
  - `const char* actual`
  - `const char* expected`
  - `const char* file`
  - `int line`
- **Returns**: None

#### `void assert_null(const void* ptr, const char* file, int line)`
- **Description**: If one is NULL, they're not equal - pass
- **Parameters**:
  - `const void* ptr`
  - `const char* file`
  - `int line`
- **Returns**: None

#### `void assert_not_null(const void* ptr, const char* file, int line)`
- **Parameters**:
  - `const void* ptr`
  - `const char* file`
  - `int line`
- **Returns**: None

#### `assert_stats_t* assert_get_stats(void)`
- **Parameters**: None
- **Returns**: `assert_stats_t*`

#### `void assert_reset_stats(void)`
- **Parameters**: None
- **Returns**: None

#### `void assert_print_summary(void)`
- **Parameters**: None
- **Returns**: None


## Example Usage

```c
// See c_modules/example_assert.c for complete examples
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

- Example: `c_modules/example_assert.c` (if available)
- Related modules: Check #include dependencies
