# mod_timers - 

**File**: `c_modules/mod_timers.c`
**Dependencies**:

- mod_timers (internal)

## Overview

`mod_timers` provides .

## Quick Start

```c
#include "mod_timers.c"

// TODO: Add minimal example
```

## API Reference

#### `int64_t timers_get_monotonic_time(void)`
- **Description**: * Get current monotonic time in microsecond  * Uses gettimeofday for compatibility
- **Parameters**: None
- **Returns**: `int64_t`

#### `void timers_init(timer_manager_t *mgr)`
- **Description**: Search rest of list
- **Parameters**:
  - `timer_manager_t *mgr`
- **Returns**: None

#### `void timers_cleanup(timer_manager_t *mgr)`
- **Parameters**:
  - `timer_manager_t *mgr`
- **Returns**: None

#### `timer_id_t timers_set_timeout(timer_manager_t *mgr, timer_callback_fn callback, void *ctx, int delay_ms)`
- **Parameters**:
  - `timer_manager_t *mgr`
  - `timer_callback_fn callback`
  - `void *ctx`
  - `int delay_ms`
- **Returns**: `timer_id_t`

#### `timer_id_t timers_set_interval(timer_manager_t *mgr, timer_callback_fn callback, void *ctx, int interval_ms)`
- **Description**: Handle negative delay as 0
- **Parameters**:
  - `timer_manager_t *mgr`
  - `timer_callback_fn callback`
  - `void *ctx`
  - `int interval_ms`
- **Returns**: `timer_id_t`

#### `timer_id_t timers_set_immediate(timer_manager_t *mgr, timer_callback_fn callback, void *ctx)`
- **Description**: Enforce minimum interval of 1ms
- **Parameters**:
  - `timer_manager_t *mgr`
  - `timer_callback_fn callback`
  - `void *ctx`
- **Returns**: `timer_id_t`

#### `void timers_clear_timeout(timer_manager_t *mgr, timer_id_t id)`
- **Parameters**:
  - `timer_manager_t *mgr`
  - `timer_id_t id`
- **Returns**: None

#### `void timers_clear_interval(timer_manager_t *mgr, timer_id_t id)`
- **Parameters**:
  - `timer_manager_t *mgr`
  - `timer_id_t id`
- **Returns**: None

#### `void timers_clear_immediate(timer_manager_t *mgr, timer_id_t id)`
- **Parameters**:
  - `timer_manager_t *mgr`
  - `timer_id_t id`
- **Returns**: None

#### `int timers_process(timer_manager_t *mgr)`
- **Parameters**:
  - `timer_manager_t *mgr`
- **Returns**: `int`

#### `int64_t timers_get_next_timeout(timer_manager_t *mgr)`
- **Description**: Free one-shot timers (timeout/immediate)
- **Parameters**:
  - `timer_manager_t *mgr`
- **Returns**: `int64_t`

#### `int timers_count(timer_manager_t *mgr)`
- **Description**: Timer ready to fire
- **Parameters**:
  - `timer_manager_t *mgr`
- **Returns**: `int`


## Example Usage

```c
// See c_modules/example_timers.c for complete examples
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

- Example: `c_modules/example_timers.c` (if available)
- Related modules: Check #include dependencies
