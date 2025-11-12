/*
 * example_std.c - Standard Utilities Examples
 *
 * Demonstrates:
 * - Dynamic strings
 * - Hash maps
 * - Error handling
 * - Common utility functions
 */

#include "src/cosmo_libc.h"
#include "c_modules/mod_std.c"

void example_strings() {
    printf("\n========================================\n");
    printf("  Dynamic String Examples\n");
    printf("========================================\n\n");

    /* Example 1: Create and append */
    printf("1. Create and Append:\n");
    std_string_t *str = std_string_new("Hello");
    printf("   Initial: '%s'\n", std_string_cstr(str));

    std_string_append(str, ", ");
    std_string_append(str, "World");
    std_string_append_char(str, '!');

    printf("   After append: '%s'\n", std_string_cstr(str));
    printf("   Length: %zu, Capacity: %zu\n", str->len, str->capacity);
    std_string_free(str);
    printf("\n");

    /* Example 2: Build string incrementally */
    printf("2. Build String Incrementally:\n");
    str = std_string_with_capacity(256);

    for (int i = 1; i <= 5; i++) {
        char temp[32];
        snprintf(temp, sizeof(temp), "Item %d", i);
        std_string_append(str, temp);
        if (i < 5) std_string_append(str, ", ");
    }

    printf("   Result: '%s'\n", std_string_cstr(str));
    std_string_free(str);
    printf("\n");

    /* Example 3: Clear and reuse */
    printf("3. Clear and Reuse:\n");
    str = std_string_new("First value");
    printf("   Before clear: '%s'\n", std_string_cstr(str));

    std_string_clear(str);
    std_string_append(str, "Second value");
    printf("   After clear: '%s'\n", std_string_cstr(str));
    std_string_free(str);
    printf("\n");
}

void example_hashmaps() {
    printf("\n========================================\n");
    printf("  HashMap Examples\n");
    printf("========================================\n\n");

    /* Example 1: Basic key-value storage */
    printf("1. Basic HashMap:\n");
    std_hashmap_t *map = std_hashmap_new();

    std_hashmap_set(map, "name", "Alice");
    std_hashmap_set(map, "city", "New York");
    std_hashmap_set(map, "role", "Developer");

    printf("   name: %s\n", (char*)std_hashmap_get(map, "name"));
    printf("   city: %s\n", (char*)std_hashmap_get(map, "city"));
    printf("   role: %s\n", (char*)std_hashmap_get(map, "role"));
    printf("\n");

    /* Example 2: Check key existence */
    printf("2. Check Key Existence:\n");
    if (std_hashmap_has(map, "name")) {
        printf("   Key 'name' exists\n");
    }
    if (!std_hashmap_has(map, "age")) {
        printf("   Key 'age' does not exist\n");
    }
    printf("\n");

    /* Example 3: Update values */
    printf("3. Update Values:\n");
    printf("   Before: city = %s\n", (char*)std_hashmap_get(map, "city"));
    std_hashmap_set(map, "city", "San Francisco");
    printf("   After: city = %s\n", (char*)std_hashmap_get(map, "city"));

    std_hashmap_free(map);
    printf("\n");
}

void example_error_handling() {
    printf("\n========================================\n");
    printf("  Error Handling Examples\n");
    printf("========================================\n\n");

    /* Example 1: Create and use errors */
    printf("1. Create Error Object:\n");
    std_error_t *err = std_error_new(404, "Resource not found");

    printf("   Error code: %d\n", err->code);
    printf("   Error message: %s\n", err->message);

    std_error_free(err);
    printf("\n");

    /* Example 2: Error in function return */
    printf("2. Function Error Pattern:\n");
    printf("   Typical usage:\n");
    printf("   int my_function(void **result, std_error_t **err) {\n");
    printf("     if (/* error condition */) {\n");
    printf("       *err = std_error_new(ERR_CODE, \"Description\");\n");
    printf("       return -1;\n");
    printf("     }\n");
    printf("     *result = /* success value */;\n");
    printf("     return 0;\n");
    printf("   }\n");
    printf("\n");
}

void example_vector_operations() {
    printf("\n========================================\n");
    printf("  Vector Operations\n");
    printf("========================================\n\n");

    /* Example: Using vectors (dynamic arrays) */
    printf("1. Dynamic Vector:\n");
    std_vector_t *vec = std_vector_new();

    std_vector_push(vec, "First");
    std_vector_push(vec, "Second");
    std_vector_push(vec, "Third");

    printf("   Vector size: %zu\n", std_vector_len(vec));

    for (size_t i = 0; i < std_vector_len(vec); i++) {
        printf("   [%zu] = %s\n", i, (char*)std_vector_get(vec, i));
    }

    printf("\n2. Pop from vector:\n");
    const char *last = std_vector_pop(vec);
    printf("   Popped: %s\n", last);
    printf("   Remaining size: %zu\n", std_vector_len(vec));

    std_vector_free(vec);
    printf("\n");
}

void example_vector_advanced() {
    printf("\n========================================\n");
    printf("  Vector Advanced Usage\n");
    printf("========================================\n\n");

    printf("1. Pre-allocate Capacity:\n");
    std_vector_t *vec = std_vector_with_capacity(100);
    printf("   Created vector with capacity for 100 items\n");

    printf("   Adding items in bulk...\n");
    for (int i = 0; i < 50; i++) {
        std_vector_push(vec, (void*)(long)i);
    }
    printf("   Added 50 items\n");

    printf("\n2. Random Access:\n");
    std_vector_set(vec, 0, (void*)999L);
    printf("   Set first item to 999\n");
    printf("   First item: %ld\n", (long)std_vector_get(vec, 0));

    printf("\n3. Clear Vector:\n");
    std_vector_clear(vec);
    printf("   Cleared vector, size: %zu\n", std_vector_len(vec));

    std_vector_free(vec);
    printf("\n");
}

int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  CosmoRun Standard Module Examples    ║\n");
    printf("╚════════════════════════════════════════╝\n");

    example_strings();
    example_hashmaps();
    example_error_handling();
    example_vector_operations();
    example_vector_advanced();

    printf("========================================\n");
    printf("  All standard module examples done!\n");
    printf("========================================\n\n");

    return 0;
}
