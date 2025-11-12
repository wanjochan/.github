# mod_curl - libcurl HTTP client module for cosmorun

**File**: `c_modules/mod_curl.c`
**Dependencies**:

- mod_curl (internal)
- mod_error_impl (internal)

## Overview

`mod_curl` provides libcurl http client module for cosmorun.

## Quick Start

```c
#include "mod_curl.c"

// TODO: Add minimal example
```

## API Reference

#### `curl_context_t* curl_init(const char *lib_path)`
- **Description**: Read callback for uploading data
- **Parameters**:
  - `const char *lib_path`
- **Returns**: `curl_context_t*`

#### `void curl_cleanup(curl_context_t *ctx)`
- **Description**: Set error buffer
- **Parameters**:
  - `curl_context_t *ctx`
- **Returns**: None

#### `void curl_set_timeout(curl_context_t *ctx, long timeout_seconds)`
- **Parameters**:
  - `curl_context_t *ctx`
  - `long timeout_seconds`
- **Returns**: None

#### `void curl_set_connect_timeout(curl_context_t *ctx, long timeout_seconds)`
- **Parameters**:
  - `curl_context_t *ctx`
  - `long timeout_seconds`
- **Returns**: None

#### `void curl_add_header(curl_context_t *ctx, const char *key, const char *value)`
- **Parameters**:
  - `curl_context_t *ctx`
  - `const char *key`
  - `const char *value`
- **Returns**: None

#### `void curl_clear_headers(curl_context_t *ctx)`
- **Parameters**:
  - `curl_context_t *ctx`
- **Returns**: None

#### `std_string_t* curl_get(curl_context_t *ctx, const char *url)`
- **Description**: Set user agent
- **Parameters**:
  - `curl_context_t *ctx`
  - `const char *url`
- **Returns**: `std_string_t*`

#### `std_string_t* curl_post(curl_context_t *ctx, const char *url, const char *data)`
- **Description**: Cleanup headers
- **Parameters**:
  - `curl_context_t *ctx`
  - `const char *url`
  - `const char *data`
- **Returns**: `std_string_t*`

#### `std_string_t* curl_post_content_type(curl_context_t *ctx, const char *url, const char *data, const char *content_type)`
- **Description**: Cleanup headers
- **Parameters**:
  - `curl_context_t *ctx`
  - `const char *url`
  - `const char *data`
  - `const char *content_type`
- **Returns**: `std_string_t*`

#### `int curl_download(curl_context_t *ctx, const char *url, const char *filepath)`
- **Description**: Remove Content-Type header after request
- **Parameters**:
  - `curl_context_t *ctx`
  - `const char *url`
  - `const char *filepath`
- **Returns**: `int`

#### `std_string_t* curl_upload(curl_context_t *ctx, const char *url, const char *filepath)`
- **Description**: Remove incomplete file
- **Parameters**:
  - `curl_context_t *ctx`
  - `const char *url`
  - `const char *filepath`
- **Returns**: `std_string_t*`

#### `long curl_get_response_code(curl_context_t *ctx)`
- **Description**: Perform request
- **Parameters**:
  - `curl_context_t *ctx`
- **Returns**: `long`

#### `int curl_selftest(const char *lib_path)`
- **Parameters**:
  - `const char *lib_path`
- **Returns**: `int`

#### `int curl_selftest_default(void)`
- **Description**: Check response contains "Example Domain"
- **Parameters**: None
- **Returns**: `int`


## Example Usage

```c
// See c_modules/example_curl.c for complete examples
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

- Example: `c_modules/example_curl.c` (if available)
- Related modules: Check #include dependencies
