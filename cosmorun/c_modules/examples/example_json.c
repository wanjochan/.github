/*
 * example_json.c - JSON Parsing and Generation Examples
 *
 * Demonstrates:
 * - Parsing JSON strings
 * - Creating JSON objects
 * - Accessing nested values
 * - Array manipulation
 * - Error handling
 */

#include "src/cosmo_libc.h"

/* Import JSON module dynamically */
void *mod_json = NULL;

/* Function pointer types matching cJSON API */
typedef void* (*json_parse_fn)(const char *value);
typedef char* (*json_print_fn)(const void *item);
typedef void (*json_delete_fn)(void *item);
typedef void* (*json_create_object_fn)(void);
typedef void* (*json_create_array_fn)(void);
typedef void (*json_add_item_to_object_fn)(void *object, const char *string, void *item);
typedef void (*json_add_item_to_array_fn)(void *array, void *item);
typedef void* (*json_create_string_fn)(const char *string);
typedef void* (*json_create_number_fn)(double num);
typedef void* (*json_create_bool_fn)(int boolean);
typedef void* (*json_get_object_item_fn)(const void *object, const char *string);

/* Function pointers */
static json_parse_fn json_parse;
static json_print_fn json_print;
static json_delete_fn json_delete;
static json_create_object_fn json_create_object;
static json_create_string_fn json_create_string;
static json_create_number_fn json_create_number;
static json_add_item_to_object_fn json_add_item_to_object;
static json_get_object_item_fn json_get_object_item;

void example_json_parse() {
    printf("\n========================================\n");
    printf("  JSON Parsing Examples\n");
    printf("========================================\n\n");

    /* Example 1: Parse simple JSON object */
    printf("1. Parse Simple JSON:\n");
    const char *json_str = "{\"name\":\"Alice\",\"age\":30,\"active\":true}";
    printf("   Input: %s\n", json_str);

    void *root = json_parse(json_str);
    if (root) {
        char *output = json_print(root);
        printf("   Parsed: %s\n", output);
        free(output);
        json_delete(root);
    } else {
        fprintf(stderr, "   ERROR: Failed to parse JSON\n");
    }
    printf("\n");

    /* Example 2: Parse nested JSON */
    printf("2. Parse Nested JSON:\n");
    const char *nested = "{\"user\":{\"name\":\"Bob\",\"email\":\"bob@example.com\"},\"count\":42}";
    printf("   Input: %s\n", nested);

    root = json_parse(nested);
    if (root) {
        printf("   Successfully parsed nested structure\n");
        json_delete(root);
    }
    printf("\n");

    /* Example 3: Parse JSON array */
    printf("3. Parse JSON Array:\n");
    const char *array_json = "[1, 2, 3, 4, 5]";
    printf("   Input: %s\n", array_json);

    root = json_parse(array_json);
    if (root) {
        char *output = json_print(root);
        printf("   Parsed array: %s\n", output);
        free(output);
        json_delete(root);
    }
    printf("\n");
}

void example_json_create() {
    printf("\n========================================\n");
    printf("  JSON Creation Examples\n");
    printf("========================================\n\n");

    /* Example 1: Create simple object */
    printf("1. Create JSON Object:\n");
    void *obj = json_create_object();

    json_add_item_to_object(obj, "name", json_create_string("Charlie"));
    json_add_item_to_object(obj, "age", json_create_number(25));
    json_add_item_to_object(obj, "city", json_create_string("New York"));

    char *json_output = json_print(obj);
    printf("   Created: %s\n", json_output);
    free(json_output);
    json_delete(obj);
    printf("\n");

    /* Example 2: Create nested object */
    printf("2. Create Nested JSON:\n");
    void *root = json_create_object();
    void *user = json_create_object();

    json_add_item_to_object(user, "name", json_create_string("Diana"));
    json_add_item_to_object(user, "role", json_create_string("Admin"));
    json_add_item_to_object(root, "user", user);
    json_add_item_to_object(root, "timestamp", json_create_number(1234567890));

    json_output = json_print(root);
    printf("   Created: %s\n", json_output);
    free(json_output);
    json_delete(root);
    printf("\n");
}

int init_json_module() {
    /* Dynamically load JSON module */
    extern void* __import(const char *name);
    extern void* __import_sym(void *handle, const char *name);

    mod_json = __import("json");
    if (!mod_json) {
        fprintf(stderr, "ERROR: Failed to load mod_json\n");
        fprintf(stderr, "Make sure libcjson is installed\n");
        return -1;
    }

    /* Load function pointers */
    json_parse = (json_parse_fn)__import_sym(mod_json, "cJSON_Parse");
    json_print = (json_print_fn)__import_sym(mod_json, "cJSON_Print");
    json_delete = (json_delete_fn)__import_sym(mod_json, "cJSON_Delete");
    json_create_object = (json_create_object_fn)__import_sym(mod_json, "cJSON_CreateObject");
    json_create_string = (json_create_string_fn)__import_sym(mod_json, "cJSON_CreateString");
    json_create_number = (json_create_number_fn)__import_sym(mod_json, "cJSON_CreateNumber");
    json_add_item_to_object = (json_add_item_to_object_fn)__import_sym(mod_json, "cJSON_AddItemToObject");
    json_get_object_item = (json_get_object_item_fn)__import_sym(mod_json, "cJSON_GetObjectItem");

    if (!json_parse || !json_print || !json_delete) {
        fprintf(stderr, "ERROR: Failed to load JSON functions\n");
        return -1;
    }

    return 0;
}

int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  CosmoRun JSON Module Examples        ║\n");
    printf("╚════════════════════════════════════════╝\n");

    if (init_json_module() != 0) {
        fprintf(stderr, "\nJSON module not available - install libcjson to use this module\n\n");
        return 1;
    }

    example_json_parse();
    example_json_create();

    printf("========================================\n");
    printf("  All JSON examples completed!\n");
    printf("========================================\n\n");

    return 0;
}
