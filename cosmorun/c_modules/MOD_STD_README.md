# mod_std - Standard Utilities Module

**File**: `c_modules/mod_std.c`
**Dependencies**: None (pure C implementation)
**External libs**: None

## Overview

`mod_std` provides essential data structures and utilities used throughout CosmoRun. It includes dynamic strings, hash maps, lists, error handling, and memory management. This is the foundation module used by most other modules.

## Quick Start

```c
#include "mod_std.c"

// Dynamic string
std_string_t *str = std_string_new("Hello");
std_string_append(str, " World");
printf("%s\n", std_string_cstr(str));  // "Hello World"
std_string_free(str);

// Hash map
std_hashmap_t *map = std_hashmap_new();
std_hashmap_set(map, "name", "Alice");
const char *name = std_hashmap_get(map, "name");
std_hashmap_free(map);
```

## Common Use Cases

### 1. Dynamic String Building

```c
// Create empty string with capacity
std_string_t *str = std_string_with_capacity(256);

// Build string incrementally
std_string_append(str, "Line 1\n");
std_string_append(str, "Line 2\n");

for (int i = 3; i <= 10; i++) {
    char line[32];
    snprintf(line, sizeof(line), "Line %d\n", i);
    std_string_append(str, line);
}

printf("%s", std_string_cstr(str));
std_string_free(str);
```

### 2. Configuration with HashMap

```c
std_hashmap_t *config = std_hashmap_new();

// Set configuration values
std_hashmap_set(config, "host", "localhost");
std_hashmap_set(config, "port", "8080");
std_hashmap_set(config, "debug", "true");

// Read configuration
const char *host = std_hashmap_get(config, "host");
const char *port = std_hashmap_get(config, "port");

if (std_hashmap_has(config, "debug")) {
    printf("Debug mode enabled\n");
}

std_hashmap_free(config);
```

### 3. Vector Operations

```c
std_vector_t *vec = std_vector_new();

// Add items
std_vector_push(vec, "First");
std_vector_push(vec, "Second");
std_vector_push(vec, "Third");

// Iterate
for (size_t i = 0; i < std_vector_len(vec); i++) {
    printf("[%zu] = %s\n", i, (char*)std_vector_get(vec, i));
}

// Pop item
void *last = std_vector_pop(vec);

std_vector_free(vec);
```

### 4. Error Handling

```c
int process_data(const char *input, char **output, std_error_t **err) {
    if (!input || strlen(input) == 0) {
        *err = std_error_new(ERR_INVALID_INPUT, "Input cannot be empty");
        return -1;
    }

    // Process input...

    if (/* processing failed */) {
        *err = std_error_new(ERR_PROCESSING, "Failed to process data");
        return -1;
    }

    *output = strdup("processed data");
    return 0;
}

// Usage
char *result;
std_error_t *err = NULL;

if (process_data("test", &result, &err) < 0) {
    fprintf(stderr, "Error %d: %s\n", err->code, err->message);
    std_error_free(err);
    return -1;
}

printf("Result: %s\n", result);
free(result);
```

## API Reference

### String Functions

#### `std_string_t* std_string_new(const char *initial)`
Create string with initial content.
- **Returns**: String object or NULL on OOM
- **Must free**: `std_string_free(str)`

#### `std_string_t* std_string_with_capacity(size_t capacity)`
Create empty string with pre-allocated capacity.

#### `void std_string_append(std_string_t *str, const char *data)`
Append C string.

#### `void std_string_append_char(std_string_t *str, char c)`
Append single character.

#### `void std_string_clear(std_string_t *str)`
Clear content but keep capacity.

#### `const char* std_string_cstr(std_string_t *str)`
Get C string pointer (null-terminated).

#### `size_t std_string_len(std_string_t *str)`
Get current length.

#### `void std_string_free(std_string_t *str)`
Free string and all memory.

### HashMap Functions

#### `std_hashmap_t* std_hashmap_new(void)`
Create empty hash map.
- **Returns**: HashMap object or NULL on OOM
- **Must free**: `std_hashmap_free(map)`

#### `void std_hashmap_set(std_hashmap_t *map, const char *key, void *value)`
Set key-value pair.
- **Note**: Does not copy value, stores pointer

#### `void* std_hashmap_get(std_hashmap_t *map, const char *key)`
Get value by key.
- **Returns**: Value pointer or NULL if not found

#### `int std_hashmap_has(std_hashmap_t *map, const char *key)`
Check if key exists.
- **Returns**: 1 if exists, 0 otherwise

#### `void std_hashmap_remove(std_hashmap_t *map, const char *key)`
Remove key-value pair.

#### `size_t std_hashmap_size(std_hashmap_t *map)`
Get number of entries.

#### `void std_hashmap_clear(std_hashmap_t *map)`
Remove all entries.

#### `void std_hashmap_free(std_hashmap_t *map)`
Free hash map (does not free values).

### Vector Functions

#### `std_vector_t* std_vector_new(void)`
Create empty vector.
- **Returns**: Vector object or NULL on OOM
- **Must free**: `std_vector_free(vec)`

#### `std_vector_t* std_vector_with_capacity(size_t capacity)`
Create vector with pre-allocated capacity.

#### `void std_vector_push(std_vector_t *vec, void *item)`
Add item to end.

#### `void* std_vector_pop(std_vector_t *vec)`
Remove and return last item.
- **Returns**: Item pointer or NULL if empty

#### `void* std_vector_get(std_vector_t *vec, size_t index)`
Get item at index.
- **Returns**: Item pointer or NULL if out of bounds

#### `void std_vector_set(std_vector_t *vec, size_t index, void *item)`
Replace item at index.

#### `size_t std_vector_len(std_vector_t *vec)`
Get number of items.

#### `void std_vector_clear(std_vector_t *vec)`
Remove all items.

#### `void std_vector_free(std_vector_t *vec)`
Free vector (does not free items).

### Error Functions

#### `std_error_t* std_error_new(int code, const char *message)`
Create error object.
- **code**: Error code (application-defined)
- **message**: Error description
- **Must free**: `std_error_free(err)`

#### `void std_error_free(std_error_t *err)`
Free error object.

### Memory Pool (Advanced)

#### `std_mempool_t* std_mempool_new(size_t item_size, size_t initial_capacity)`
Create memory pool for fixed-size allocations.
- **item_size**: Size of each allocation
- **initial_capacity**: Number of items to pre-allocate

#### `void* std_mempool_alloc(std_mempool_t *pool)`
Allocate item from pool (O(1)).

#### `void std_mempool_free(std_mempool_t *pool, void *item)`
Return item to pool (O(1)).

#### `void std_mempool_destroy(std_mempool_t *pool)`
Destroy pool and free all memory.

## Data Structures

### std_string_t
```c
typedef struct {
    char *data;         // String data (null-terminated)
    size_t len;         // Current length
    size_t capacity;    // Allocated capacity
} std_string_t;
```

### std_error_t
```c
typedef struct {
    int code;           // Error code
    char *message;      // Error description
} std_error_t;
```

## Error Handling Patterns

### Return Error Object

```c
std_error_t* my_function(const char *input) {
    if (!input) {
        return std_error_new(ERR_INVALID, "Input is NULL");
    }

    // Success
    return NULL;
}

// Usage
std_error_t *err = my_function(data);
if (err) {
    fprintf(stderr, "Error: %s\n", err->message);
    std_error_free(err);
    return -1;
}
```

### Output Parameter

```c
int my_function(const char *input, char **output, std_error_t **err) {
    if (!input) {
        *err = std_error_new(ERR_INVALID, "Input is NULL");
        return -1;
    }

    *output = strdup("result");
    return 0;
}
```

## Performance Characteristics

### String
- **Append**: O(1) amortized (automatic capacity growth)
- **Access**: O(1)
- **Clear**: O(1)

### HashMap
- **Insert**: O(1) average
- **Lookup**: O(1) average
- **Delete**: O(1) average
- **Collision**: Open addressing with linear probing

### Vector
- **Push**: O(1) amortized
- **Pop**: O(1)
- **Get/Set**: O(1)
- **Clear**: O(1)

### Memory Pool
- **Allocate**: O(1)
- **Free**: O(1)
- **Memory**: Pre-allocated chunks

## Common Pitfalls

- **Ownership**: HashMap and Vector don't own stored pointers (manual free required)
- **String lifetime**: `std_string_cstr()` pointer invalid after string is freed
- **Reallocation**: String pointer may change on append (don't cache `str->data`)
- **HashMap values**: Stored as `void*`, no type checking
- **Vector indexing**: No bounds checking in release mode
- **Memory pools**: Item size must be exact (no dynamic sizing)

## Best Practices

### String Building

```c
// Good: Pre-allocate capacity
std_string_t *str = std_string_with_capacity(1024);
for (int i = 0; i < 100; i++) {
    std_string_append(str, "data");
}

// Bad: Frequent reallocations
std_string_t *str = std_string_new("");
for (int i = 0; i < 100; i++) {
    std_string_append(str, "data");  // May reallocate 100 times
}
```

### HashMap Usage

```c
// Good: Check before use
if (std_hashmap_has(map, "key")) {
    void *value = std_hashmap_get(map, "key");
    // Use value
}

// Bad: Assume key exists
void *value = std_hashmap_get(map, "key");
if (value) { ... }  // NULL could be valid value!
```

### Error Handling

```c
// Good: Descriptive errors
std_error_new(404, "Resource not found: /api/users/123");

// Bad: Generic errors
std_error_new(1, "Error");
```

## Memory Management

### Clean Up Resources

```c
std_string_t *str = std_string_new("test");
std_hashmap_t *map = std_hashmap_new();
std_vector_t *vec = std_vector_new();

// ... use data structures ...

// Clean up (in reverse order)
std_vector_free(vec);
std_hashmap_free(map);
std_string_free(str);
```

### Free HashMap Values

```c
// If values are dynamically allocated
for (each key in map) {
    void *value = std_hashmap_get(map, key);
    free(value);  // Or appropriate destructor
}
std_hashmap_free(map);
```

## Example Output

```
========================================
  Dynamic String Examples
========================================

1. Create and Append:
   Initial: 'Hello'
   After append: 'Hello, World!'
   Length: 13, Capacity: 32

2. Build String Incrementally:
   Result: 'Item 1, Item 2, Item 3, Item 4, Item 5'

========================================
  Vector Operations
========================================

1. Dynamic Vector:
   Vector size: 3
   [0] = First
   [1] = Second
   [2] = Third

2. Pop from vector:
   Popped: Third
   Remaining size: 2

✓ All standard module examples done!
```

## Thread Safety

**Not thread-safe by default**. For multi-threaded use:
- Use separate instances per thread
- Add external locking (mutex)
- Consider thread-local storage

## Related Modules

All modules depend on mod_std for basic functionality.

## See Also

- Full example: `c_modules/examples/example_std.c`
- C standard library equivalents: string.h, stdlib.h
