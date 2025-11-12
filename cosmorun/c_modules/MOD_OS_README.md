# mod_os - Operating system utilities implementation

**File**: `c_modules/mod_os.c`
**Dependencies**:

- mod_os (internal)

## Overview

`mod_os` provides operating system utilities implementation.

## Quick Start

```c
#include "mod_os.c"

// TODO: Add minimal example
```

## API Reference

#### `os_process_t* os_exec(const char *command, char **args, char **env)`
- **Description**: msync() constants - define if not available
- **Parameters**:
  - `const char *command`
  - `char **args`
  - `char **env`
- **Returns**: `os_process_t*`

#### `int os_wait(os_process_t *proc, int timeout_ms)`
- **Parameters**:
  - `os_process_t *proc`
  - `int timeout_ms`
- **Returns**: `int`

#### `int os_kill(os_process_t *proc, int signal)`
- **Parameters**:
  - `os_process_t *proc`
  - `int signal`
- **Returns**: `int`

#### `void os_process_free(os_process_t *proc)`
- **Parameters**:
  - `os_process_t *proc`
- **Returns**: None

#### `int os_getpid(void)`
- **Parameters**: None
- **Returns**: `int`

#### `int os_getppid(void)`
- **Parameters**: None
- **Returns**: `int`

#### `int os_exists(const char *path)`
- **Parameters**:
  - `const char *path`
- **Returns**: `int`

#### `int os_is_file(const char *path)`
- **Parameters**:
  - `const char *path`
- **Returns**: `int`

#### `int os_is_dir(const char *path)`
- **Parameters**:
  - `const char *path`
- **Returns**: `int`

#### `os_fileinfo_t* os_stat(const char *path)`
- **Parameters**:
  - `const char *path`
- **Returns**: `os_fileinfo_t*`

#### `void os_fileinfo_free(os_fileinfo_t *info)`
- **Parameters**:
  - `os_fileinfo_t *info`
- **Returns**: None

#### `int os_mkdir(const char *path)`
- **Parameters**:
  - `const char *path`
- **Returns**: `int`

#### `int os_rmdir(const char *path)`
- **Parameters**:
  - `const char *path`
- **Returns**: `int`

#### `int os_remove(const char *path)`
- **Parameters**:
  - `const char *path`
- **Returns**: `int`

#### `int os_rename(const char *old_path, const char *new_path)`
- **Parameters**:
  - `const char *old_path`
  - `const char *new_path`
- **Returns**: `int`

#### `os_dirlist_t* os_listdir(const char *path)`
- **Description**: Simple dynamic array for directory entries (no dependency on mod_std)
- **Parameters**:
  - `const char *path`
- **Returns**: `os_dirlist_t*`

#### `void os_dirlist_free(os_dirlist_t *list)`
- **Description**: For now, create a wrapper
- **Parameters**:
  - `os_dirlist_t *list`
- **Returns**: None

#### `char* os_path_join(const char *base, const char *name)`
- **Parameters**:
  - `const char *base`
  - `const char *name`
- **Returns**: `char*`

#### `char* os_path_dirname(const char *path)`
- **Parameters**:
  - `const char *path`
- **Returns**: `char*`

#### `char* os_path_basename(const char *path)`
- **Parameters**:
  - `const char *path`
- **Returns**: `char*`

#### `char* os_path_abs(const char *path)`
- **Parameters**:
  - `const char *path`
- **Returns**: `char*`

#### `char* os_getcwd(void)`
- **Parameters**: None
- **Returns**: `char*`

#### `int os_chdir(const char *path)`
- **Parameters**:
  - `const char *path`
- **Returns**: `int`

#### `int os_setenv(const char *name, const char *value)`
- **Parameters**:
  - `const char *name`
  - `const char *value`
- **Returns**: `int`

#### `int os_unsetenv(const char *name)`
- **Parameters**:
  - `const char *name`
- **Returns**: `int`

#### `simple_hashmap_t* os_environ(void)`
- **Description**: Simple hashmap implementation (structure defined in mod_os.h)
- **Parameters**: None
- **Returns**: `simple_hashmap_t*`

#### `void os_environ_free(simple_hashmap_t *map)`
- **Description**: Fixed size for simplicity
- **Parameters**:
  - `simple_hashmap_t *map`
- **Returns**: None

#### `os_sysinfo_t* os_sysinfo(void)`
- **Parameters**: None
- **Returns**: `os_sysinfo_t*`

#### `void os_sysinfo_free(os_sysinfo_t *info)`
- **Parameters**:
  - `os_sysinfo_t *info`
- **Returns**: None

#### `char* os_tmpfile(void)`
- **Parameters**: None
- **Returns**: `char*`

#### `char* os_tmpdir(void)`
- **Parameters**: None
- **Returns**: `char*`

#### `os_mmap_t* os_mmap_file(const char *filename, int prot, int flags)`
- **Description**: Map a file into memory
- **Parameters**:
  - `const char *filename`
  - `int prot`
  - `int flags`
- **Returns**: `os_mmap_t*`

#### `os_mmap_t* os_mmap_create(size_t size, int prot, int flags)`
- **Description**: Create anonymous memory mapping
- **Parameters**:
  - `size_t size`
  - `int prot`
  - `int flags`
- **Returns**: `os_mmap_t*`

#### `int os_mmap_sync(os_mmap_t *map)`
- **Description**: Synchronize memory mapping to disk
- **Parameters**:
  - `os_mmap_t *map`
- **Returns**: `int`

#### `void os_mmap_free(os_mmap_t *map)`
- **Description**: Free memory mapping
- **Parameters**:
  - `os_mmap_t *map`
- **Returns**: None

#### `int os_mmap_advise(os_mmap_t *map, int advice)`
- **Description**: Provide memory access advice
- **Parameters**:
  - `os_mmap_t *map`
  - `int advice`
- **Returns**: `int`

#### `size_t os_mmap_get_pagesize(void)`
- **Description**: Get system page size
- **Parameters**: None
- **Returns**: `size_t`


## Example Usage

```c
// See c_modules/example_os.c for complete examples
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

- Example: `c_modules/example_os.c` (if available)
- Related modules: Check #include dependencies
