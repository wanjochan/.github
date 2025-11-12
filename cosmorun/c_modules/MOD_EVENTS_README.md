# mod_events - EventEmitter implementation

**File**: `c_modules/mod_events.c`
**Dependencies**:

- mod_events (internal)

## Overview

`mod_events` provides eventemitter implementation.

## Quick Start

```c
#include "mod_events.c"

// TODO: Add minimal example
```

## API Reference

#### `event_emitter_t* event_emitter_new(void)`
- **Description**: Remove listener from list by callback function pointer
- **Parameters**: None
- **Returns**: `event_emitter_t*`

#### `void event_emitter_free(event_emitter_t *emitter)`
- **Parameters**:
  - `event_emitter_t *emitter`
- **Returns**: None

#### `int event_on(event_emitter_t *emitter, const char *event, event_listener_fn listener, void *ctx)`
- **Parameters**:
  - `event_emitter_t *emitter`
  - `const char *event`
  - `event_listener_fn listener`
  - `void *ctx`
- **Returns**: `int`

#### `int event_once(event_emitter_t *emitter, const char *event, event_listener_fn listener, void *ctx)`
- **Parameters**:
  - `event_emitter_t *emitter`
  - `const char *event`
  - `event_listener_fn listener`
  - `void *ctx`
- **Returns**: `int`

#### `int event_off(event_emitter_t *emitter, const char *event, event_listener_fn listener)`
- **Parameters**:
  - `event_emitter_t *emitter`
  - `const char *event`
  - `event_listener_fn listener`
- **Returns**: `int`

#### `int event_emit(event_emitter_t *emitter, const char *event, void *data)`
- **Parameters**:
  - `event_emitter_t *emitter`
  - `const char *event`
  - `void *data`
- **Returns**: `int`

#### `size_t event_listener_count(event_emitter_t *emitter, const char *event)`
- **Parameters**:
  - `event_emitter_t *emitter`
  - `const char *event`
- **Returns**: `size_t`

#### `int event_remove_all_listeners(event_emitter_t *emitter, const char *event)`
- **Parameters**:
  - `event_emitter_t *emitter`
  - `const char *event`
- **Returns**: `int`


## Example Usage

```c
// See c_modules/example_events.c for complete examples
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

- Example: `c_modules/example_events.c` (if available)
- Related modules: Check #include dependencies
