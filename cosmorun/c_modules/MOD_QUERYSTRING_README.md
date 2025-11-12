# mod_querystring - 

**File**: `c_modules/mod_querystring.c`
**Dependencies**:

- mod_querystring (internal)

## Overview

`mod_querystring` provides .

## Quick Start

```c
#include "mod_querystring.c"

// TODO: Add minimal example
```

## API Reference

#### `char* qs_encode(const char* str)`
- **Description**: URL encode a string
- **Parameters**:
  - `const char* str`
- **Returns**: `char*`

#### `char* qs_decode(const char* str)`
- **Description**: URL decode a string
- **Parameters**:
  - `const char* str`
- **Returns**: `char*`

#### `std_hashmap_t* qs_parse_custom(const char* query, char sep, char eq)`
- **Description**: Parse query string with custom separators
- **Parameters**:
  - `const char* query`
  - `char sep`
  - `char eq`
- **Returns**: `std_hashmap_t*`

#### `std_hashmap_t* qs_parse(const char* query_string)`
- **Description**: Parse query string with default separators
- **Parameters**:
  - `const char* query_string`
- **Returns**: `std_hashmap_t*`

#### `char* qs_stringify_custom(std_hashmap_t* params, char sep, char eq)`
- **Description**: Stringify hashmap with custom separators
- **Parameters**:
  - `std_hashmap_t* params`
  - `char sep`
  - `char eq`
- **Returns**: `char*`

#### `char* qs_stringify(std_hashmap_t* params)`
- **Description**: Stringify hashmap with default separators
- **Parameters**:
  - `std_hashmap_t* params`
- **Returns**: `char*`


## Example Usage

```c
// See c_modules/example_querystring.c for complete examples
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

- Example: `c_modules/example_querystring.c` (if available)
- Related modules: Check #include dependencies
