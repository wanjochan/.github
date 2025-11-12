# mod_duckdb - 

**File**: `c_modules/mod_duckdb.c`
**Dependencies**:

- mod_duckdb (internal)

## Overview

`mod_duckdb` provides .

## Quick Start

```c
#include "mod_duckdb.c"

// TODO: Add minimal example
```

## API Reference

#### `int mod_duckdb_init(void)`
- **Parameters**: None
- **Returns**: `int`

#### `int duckdb_selftest(const char *lib_path)`
- **Parameters**:
  - `const char *lib_path`
- **Returns**: `int`

#### `int duckdb_selftest_default(void)`
- **Parameters**: None
- **Returns**: `int`

#### `int duckdb_arrow_roundtrip(const char *arrow_path)`
- **Parameters**:
  - `const char *arrow_path`
- **Returns**: `int`

#### `duckdb_context_t* duckdb_init(const char *lib_path)`
- **Description**: * Initialize DuckDB context and load library  * @param lib_path Optional library path (NULL for auto-detection)  * @return Pointer to allocated context, or NULL on failure
- **Parameters**:
  - `const char *lib_path`
- **Returns**: `duckdb_context_t*`

#### `int duckdb_open_db(duckdb_context_t *ctx, const char *db_path)`
- **Description**: * Open database and create connection  * @param ctx DuckDB context  * @param db_path Database path (":memory:" for in-memory, NULL for ":memory:")  * @return 0 on success, -1 on failure
- **Parameters**:
  - `duckdb_context_t *ctx`
  - `const char *db_path`
- **Returns**: `int`

#### `int duckdb_exec(duckdb_context_t *ctx, const char *sql, duckdb_result *out_result)`
- **Description**: * Execute query and return result  * @param ctx DuckDB context  * @param sql SQL query string  * @param out_result Pointer to result structure (caller must destroy it)  * @return 0 on success, -1 on failure
- **Parameters**:
  - `duckdb_context_t *ctx`
  - `const char *sql`
  - `duckdb_result *out_result`
- **Returns**: `int`

#### `long duckdb_get_row_count(duckdb_context_t *ctx, duckdb_result *result)`
- **Description**: * Get row count from result
- **Parameters**:
  - `duckdb_context_t *ctx`
  - `duckdb_result *result`
- **Returns**: `long`

#### `long duckdb_get_column_count(duckdb_context_t *ctx, duckdb_result *result)`
- **Description**: * Get column count from result
- **Parameters**:
  - `duckdb_context_t *ctx`
  - `duckdb_result *result`
- **Returns**: `long`

#### `char* duckdb_ctx_varchar(duckdb_context_t *ctx, duckdb_result *result, long col, long row)`
- **Description**: * Get column name from result  * Get varchar value from result  * @return Allocated string (caller must free with duckdb_free_value)  * Note: Renamed from duckdb_get_varchar to avoid conflict with official API
- **Parameters**:
  - `duckdb_context_t *ctx`
  - `duckdb_result *result`
  - `long col`
  - `long row`
- **Returns**: `char*`

#### `void duckdb_free_value(duckdb_context_t *ctx, void *value)`
- **Description**: * Free value returned by DuckDB
- **Parameters**:
  - `duckdb_context_t *ctx`
  - `void *value`
- **Returns**: None

#### `void duckdb_free_result(duckdb_context_t *ctx, duckdb_result *result)`
- **Description**: * Destroy query result (wrapper to avoid name conflict with duckdb.h)
- **Parameters**:
  - `duckdb_context_t *ctx`
  - `duckdb_result *result`
- **Returns**: None

#### `void duckdb_close_db(duckdb_context_t *ctx)`
- **Description**: * Get error message from result  * Close connection and database
- **Parameters**:
  - `duckdb_context_t *ctx`
- **Returns**: None

#### `void duckdb_cleanup(duckdb_context_t *ctx)`
- **Description**: * Cleanup DuckDB context and free resource
- **Parameters**:
  - `duckdb_context_t *ctx`
- **Returns**: None


## Example Usage

```c
// See c_modules/example_duckdb.c for complete examples
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

- Example: `c_modules/example_duckdb.c` (if available)
- Related modules: Check #include dependencies
