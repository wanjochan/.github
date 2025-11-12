# mod_url - URL parsing and manipulation implementation

**File**: `c_modules/mod_url.c`
**Dependencies**:

- mod_url (internal)

## Overview

`mod_url` provides url parsing and manipulation implementation.

## Quick Start

```c
#include "mod_url.c"

// TODO: Add minimal example
```

## API Reference

#### `int mod_url_init(void)`
- **Description**: Auto-called by __import() when module loads
- **Parameters**: None
- **Returns**: `int`

#### `char* url_encode(const char *str)`
- **Description**: Redirect function calls to pointers
- **Parameters**:
  - `const char *str`
- **Returns**: `char*`

#### `char* url_decode(const char *str)`
- **Parameters**:
  - `const char *str`
- **Returns**: `char*`

#### `char* url_normalize_path(const char *path)`
- **Parameters**:
  - `const char *path`
- **Returns**: `char*`

#### `int url_is_supported_protocol(const char *protocol)`
- **Description**: Build normalized path
- **Parameters**:
  - `const char *protocol`
- **Returns**: `int`

#### `int url_get_default_port(const char *protocol)`
- **Parameters**:
  - `const char *protocol`
- **Returns**: `int`

#### `char* url_extract_protocol(const char *url_string)`
- **Parameters**:
  - `const char *url_string`
- **Returns**: `char*`

#### `int url_is_absolute(const char *url_string)`
- **Parameters**:
  - `const char *url_string`
- **Returns**: `int`

#### `int url_is_ipv6(const char *hostname)`
- **Parameters**:
  - `const char *hostname`
- **Returns**: `int`

#### `int url_validate_hostname(const char *hostname)`
- **Parameters**:
  - `const char *hostname`
- **Returns**: `int`

#### `void url_set_query_param(url_t *url, const char *key, const char *value)`
- **Description**: Found end of pair
- **Parameters**:
  - `url_t *url`
  - `const char *key`
  - `const char *value`
- **Returns**: None

#### `void url_remove_query_param(url_t *url, const char *key)`
- **Description**: Free old value if exists
- **Parameters**:
  - `url_t *url`
  - `const char *key`
- **Returns**: None

#### `char* url_build_query_string(url_t *url)`
- **Parameters**:
  - `url_t *url`
- **Returns**: `char*`

#### `url_t* url_parse(const char *url_string)`
- **Parameters**:
  - `const char *url_string`
- **Returns**: `url_t*`

#### `char* url_format(url_t *url)`
- **Description**: Parse hash
- **Parameters**:
  - `url_t *url`
- **Returns**: `char*`

#### `char* url_resolve(const char *base, const char *relative)`
- **Parameters**:
  - `const char *base`
  - `const char *relative`
- **Returns**: `char*`

#### `void url_free(url_t *url)`
- **Description**: Relative path
- **Parameters**:
  - `url_t *url`
- **Returns**: None


## Example Usage

```c
// See c_modules/example_url.c for complete examples
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

- Example: `c_modules/example_url.c` (if available)
- Related modules: Check #include dependencies
