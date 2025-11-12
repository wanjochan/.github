# mod_path - 

**File**: `c_modules/mod_path.c`
**Dependencies**:

- mod_path (internal)

## Overview

`mod_path` provides .

## Quick Start

```c
#include "mod_path.c"

// TODO: Add minimal example
```

## API Reference

#### `char path_sep(void)`
- **Description**: Helper: get preferred separator
- **Parameters**: None
- **Returns**: `char`

#### `char path_delimiter(void)`
- **Description**: Helper: get PATH delimiter
- **Parameters**: None
- **Returns**: `char`

#### `int path_is_absolute(const char* path)`
- **Description**: Check if path is absolute
- **Parameters**:
  - `const char* path`
- **Returns**: `int`

#### `char* path_join2(const char* path1, const char* path2)`
- **Description**: Join exactly two paths
- **Parameters**:
  - `const char* path1`
  - `const char* path2`
- **Returns**: `char*`

#### `char* path_join3(const char* p1, const char* p2, const char* p3)`
- **Description**: Join 3 paths
- **Parameters**:
  - `const char* p1`
  - `const char* p2`
  - `const char* p3`
- **Returns**: `char*`

#### `char* path_join4(const char* p1, const char* p2, const char* p3, const char* p4)`
- **Description**: Join 4 paths
- **Parameters**:
  - `const char* p1`
  - `const char* p2`
  - `const char* p3`
  - `const char* p4`
- **Returns**: `char*`

#### `char* path_dirname(const char* path)`
- **Description**: Get directory name
- **Parameters**:
  - `const char* path`
- **Returns**: `char*`

#### `char* path_basename(const char* path)`
- **Description**: Get base name
- **Parameters**:
  - `const char* path`
- **Returns**: `char*`

#### `char* path_extname(const char* path)`
- **Description**: Get extension
- **Parameters**:
  - `const char* path`
- **Returns**: `char*`

#### `char* path_normalize(const char* path)`
- **Description**: Normalize path
- **Parameters**:
  - `const char* path`
- **Returns**: `char*`

#### `char* path_resolve(const char* path)`
- **Description**: Resolve to absolute path
- **Parameters**:
  - `const char* path`
- **Returns**: `char*`

#### `path_parse_t* path_parse(const char* path)`
- **Description**: Parse path into components
- **Parameters**:
  - `const char* path`
- **Returns**: `path_parse_t*`

#### `void path_parse_free(path_parse_t* parsed)`
- **Description**: Free parsed path
- **Parameters**:
  - `path_parse_t* parsed`
- **Returns**: None


## Example Usage

```c
// See c_modules/example_path.c for complete examples
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

- Example: `c_modules/example_path.c` (if available)
- Related modules: Check #include dependencies
