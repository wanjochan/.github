# mod_json - JSON Parsing and Generation Module

**File**: `c_modules/mod_json.c`
**Dependencies**: None (dynamically loads libcjson)
**External libs**: `libcjson` (optional, dynamically loaded)

## Overview

`mod_json` provides JSON parsing and generation capabilities by dynamically loading the cJSON library. It falls back gracefully if the library is not available. The module wraps cJSON's API for seamless integration with CosmoRun.

## Quick Start

```c
#include "mod_json.c"

// Parse JSON string
void *root = cJSON_Parse("{\"name\":\"Alice\",\"age\":30}");
if (root) {
    void *name_item = cJSON_GetObjectItem(root, "name");
    const char *name = cJSON_GetStringValue(name_item);
    printf("Name: %s\n", name);

    cJSON_Delete(root);
}
```

## Common Use Cases

### 1. Parse JSON Object

```c
const char *json = "{\"user\":\"alice\",\"id\":123,\"active\":true}";
cJSON *root = cJSON_Parse(json);

if (!root) {
    const char *error = cJSON_GetErrorPtr();
    fprintf(stderr, "Parse error: %s\n", error);
    return;
}

// Extract values
cJSON *user = cJSON_GetObjectItem(root, "user");
cJSON *id = cJSON_GetObjectItem(root, "id");
cJSON *active = cJSON_GetObjectItem(root, "active");

printf("User: %s\n", cJSON_GetStringValue(user));
printf("ID: %d\n", cJSON_GetNumberValue(id));
printf("Active: %s\n", cJSON_IsTrue(active) ? "yes" : "no");

cJSON_Delete(root);
```

### 2. Create JSON Object

```c
cJSON *root = cJSON_CreateObject();

cJSON_AddStringToObject(root, "name", "Bob");
cJSON_AddNumberToObject(root, "age", 25);
cJSON_AddBoolToObject(root, "verified", 1);

// Nested object
cJSON *address = cJSON_CreateObject();
cJSON_AddStringToObject(address, "city", "New York");
cJSON_AddStringToObject(address, "zip", "10001");
cJSON_AddItemToObject(root, "address", address);

// Convert to string
char *json_str = cJSON_Print(root);
printf("%s\n", json_str);

free(json_str);
cJSON_Delete(root);
```

### 3. Work with JSON Arrays

```c
// Parse array
const char *json = "[1, 2, 3, 4, 5]";
cJSON *array = cJSON_Parse(json);

int array_size = cJSON_GetArraySize(array);
printf("Array size: %d\n", array_size);

// Iterate through array
for (int i = 0; i < array_size; i++) {
    cJSON *item = cJSON_GetArrayItem(array, i);
    printf("  [%d] = %d\n", i, (int)cJSON_GetNumberValue(item));
}

cJSON_Delete(array);
```

### 4. Create JSON Array

```c
cJSON *array = cJSON_CreateArray();

cJSON_AddItemToArray(array, cJSON_CreateNumber(10));
cJSON_AddItemToArray(array, cJSON_CreateNumber(20));
cJSON_AddItemToArray(array, cJSON_CreateNumber(30));

// Array of objects
cJSON *obj = cJSON_CreateObject();
cJSON_AddStringToObject(obj, "name", "Item 1");
cJSON_AddItemToArray(array, obj);

char *json_str = cJSON_Print(array);
printf("%s\n", json_str);

free(json_str);
cJSON_Delete(array);
```

## API Reference

### Parsing Functions

#### `cJSON* cJSON_Parse(const char *value)`
Parse JSON string into object tree.
- **Returns**: cJSON object or NULL on error
- **Must free**: `cJSON_Delete(object)`

#### `const char* cJSON_GetErrorPtr(void)`
Get pointer to parse error location.
- **Returns**: Error position in original string

#### `cJSON* cJSON_ParseWithLength(const char *value, size_t buffer_length)`
Parse JSON with explicit length (for non-null-terminated strings).

### Creation Functions

#### `cJSON* cJSON_CreateObject(void)`
Create empty JSON object `{}`.

#### `cJSON* cJSON_CreateArray(void)`
Create empty JSON array `[]`.

#### `cJSON* cJSON_CreateString(const char *string)`
Create string value.

#### `cJSON* cJSON_CreateNumber(double num)`
Create number value.

#### `cJSON* cJSON_CreateBool(int boolean)`
Create boolean value.

#### `cJSON* cJSON_CreateNull(void)`
Create null value.

### Object Manipulation

#### `cJSON* cJSON_GetObjectItem(const cJSON *object, const char *string)`
Get object property by key.
- **Returns**: cJSON item or NULL if not found

#### `int cJSON_HasObjectItem(const cJSON *object, const char *string)`
Check if object has key.
- **Returns**: 1 if exists, 0 otherwise

#### `void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item)`
Add item to object with key.

#### `void cJSON_AddStringToObject(cJSON *object, const char *name, const char *string)`
Convenience function to add string property.

#### `void cJSON_AddNumberToObject(cJSON *object, const char *name, double number)`
Convenience function to add number property.

#### `void cJSON_AddBoolToObject(cJSON *object, const char *name, int boolean)`
Convenience function to add boolean property.

### Array Manipulation

#### `int cJSON_GetArraySize(const cJSON *array)`
Get number of items in array.

#### `cJSON* cJSON_GetArrayItem(const cJSON *array, int index)`
Get array item by index.
- **Returns**: cJSON item or NULL if out of bounds

#### `void cJSON_AddItemToArray(cJSON *array, cJSON *item)`
Append item to array.

### Value Extraction

#### `char* cJSON_GetStringValue(const cJSON *item)`
Get string value from item.
- **Returns**: String pointer or NULL

#### `double cJSON_GetNumberValue(const cJSON *item)`
Get number value from item.
- **Returns**: Number value or 0.0 if not a number

#### `int cJSON_IsTrue(const cJSON *item)`
Check if item is boolean true.

#### `int cJSON_IsFalse(const cJSON *item)`
Check if item is boolean false.

#### `int cJSON_IsNull(const cJSON *item)`
Check if item is null.

### Serialization

#### `char* cJSON_Print(const cJSON *item)`
Convert to formatted JSON string.
- **Must free** the returned string

#### `char* cJSON_PrintUnformatted(const cJSON *item)`
Convert to compact JSON string (no whitespace).
- **Must free** the returned string

### Memory Management

#### `void cJSON_Delete(cJSON *item)`
Free cJSON object and all children.
- **Important**: Always call this when done

## Error Handling

### Check Parse Errors

```c
cJSON *root = cJSON_Parse(json_string);
if (!root) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr) {
        fprintf(stderr, "Parse error before: %.20s\n", error_ptr);
    }
    return -1;
}
```

### Validate Object Structure

```c
if (!cJSON_HasObjectItem(root, "required_field")) {
    fprintf(stderr, "Missing required field\n");
    cJSON_Delete(root);
    return -1;
}
```

### Safe Value Access

```c
cJSON *item = cJSON_GetObjectItem(root, "name");
if (item && cJSON_IsString(item)) {
    const char *name = cJSON_GetStringValue(item);
    printf("Name: %s\n", name);
} else {
    fprintf(stderr, "Invalid or missing 'name' field\n");
}
```

## Performance Tips

1. **Reuse objects**: When creating multiple similar objects, consider using a template

2. **Unformatted output**: Use `cJSON_PrintUnformatted()` for network transmission (smaller size)

3. **Streaming**: For very large JSON files, consider streaming parsers (not provided by cJSON)

4. **Memory**: cJSON allocates memory dynamically. Always free with `cJSON_Delete()`

5. **Validation**: Validate JSON structure early to avoid crashes from missing fields

## Common Pitfalls

- **Memory leaks**: Always `cJSON_Delete()` after `cJSON_Parse()` or `cJSON_Create*()`
- **Ownership**: When adding items to arrays/objects, cJSON takes ownership (don't delete manually)
- **String lifetimes**: Strings returned by `cJSON_GetStringValue()` are owned by the cJSON object
- **NULL checks**: Always check for NULL after `cJSON_Parse()` and `cJSON_GetObjectItem()`
- **Library availability**: Check if cJSON loaded successfully before using

## Library Detection

The module attempts to load cJSON from these locations:

**Linux**:
- `lib/libcjson.so`
- `./libcjson.so`
- `/usr/lib/libcjson.so`
- `/usr/local/lib/libcjson.so`

**macOS**:
- `lib/libcjson.dylib`
- `/usr/local/lib/libcjson.dylib`
- `/opt/homebrew/lib/libcjson.dylib`

**Windows**:
- `lib/libcjson.dll`
- `cjson.dll`

## Installation

### Ubuntu/Debian
```bash
sudo apt-get install libcjson-dev
```

### macOS
```bash
brew install cjson
```

### From Source
```bash
git clone https://github.com/DaveGamble/cJSON.git
cd cJSON
mkdir build && cd build
cmake ..
make && sudo make install
```

## Example Output

```
========================================
  JSON Parsing Examples
========================================

1. Parse Simple JSON:
   Input: {"name":"Alice","age":30,"active":true}
   Parsed: {
       "name": "Alice",
       "age": 30,
       "active": true
   }

2. Create JSON Object:
   Created: {"name":"Charlie","age":25,"city":"New York"}

✓ All JSON examples completed!
```

## Related Modules

- **mod_http**: Often used together for REST APIs
- **mod_curl**: HTTP client that frequently uses JSON payloads

## See Also

- Full example: `c_modules/examples/example_json.c`
- cJSON documentation: https://github.com/DaveGamble/cJSON
- JSON specification: RFC 8259
