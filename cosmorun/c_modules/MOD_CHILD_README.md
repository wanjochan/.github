# mod_child - Child process management implementation

**File**: `c_modules/mod_child.c`
**Dependencies**:

- mod_child (internal)

## Overview

`mod_child` provides child process management implementation.

## Quick Start

```c
#include "mod_child.c"

// TODO: Add minimal example
```

## API Reference

#### `void child_options_init(child_options_t *options)`
- **Description**: File descriptors
- **Parameters**:
  - `child_options_t *options`
- **Returns**: None

#### `child_process_t* child_spawn(const char *command, char **args, child_options_t *options)`
- **Parameters**:
  - `const char *command`
  - `char **args`
  - `child_options_t *options`
- **Returns**: `child_process_t*`

#### `int child_wait(child_process_t *child, int timeout_ms)`
- **Parameters**:
  - `child_process_t *child`
  - `int timeout_ms`
- **Returns**: `int`

#### `int child_kill(child_process_t *child, int signal)`
- **Parameters**:
  - `child_process_t *child`
  - `int signal`
- **Returns**: `int`

#### `int child_is_running(child_process_t *child)`
- **Parameters**:
  - `child_process_t *child`
- **Returns**: `int`

#### `void child_free(child_process_t *child)`
- **Parameters**:
  - `child_process_t *child`
- **Returns**: None

#### `void child_on(child_process_t *child, const char *event, event_listener_fn listener, void *userdata)`
- **Parameters**:
  - `child_process_t *child`
  - `const char *event`
  - `event_listener_fn listener`
  - `void *userdata`
- **Returns**: None

#### `void child_off(child_process_t *child, const char *event, event_listener_fn listener)`
- **Parameters**:
  - `child_process_t *child`
  - `const char *event`
  - `event_listener_fn listener`
- **Returns**: None

#### `int child_exec_sync( const char *command, char *stdout_buf, size_t stdout_size, char *stderr_buf, size_t stderr_size )`
- **Parameters**:
  - `const char *command`
  - `char *stdout_buf`
  - `size_t stdout_size`
  - `char *stderr_buf`
  - `size_t stderr_size`
- **Returns**: `int`

#### `child_process_t* child_exec(const char *command, child_exec_callback_t callback, void *userdata)`
- **Parameters**:
  - `const char *command`
  - `child_exec_callback_t callback`
  - `void *userdata`
- **Returns**: `child_process_t*`

#### `int child_parse_command(const char *command, char ***out_argv)`
- **Parameters**:
  - `const char *command`
  - `char ***out_argv`
- **Returns**: `int`

#### `void child_free_argv(char **argv, int argc)`
- **Parameters**:
  - `char **argv`
  - `int argc`
- **Returns**: None


## Example Usage

```c
// See c_modules/example_child.c for complete examples
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

- Example: `c_modules/example_child.c` (if available)
- Related modules: Check #include dependencies
