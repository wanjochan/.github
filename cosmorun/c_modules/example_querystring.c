/*
 * example_querystring.c - Usage examples for mod_querystring
 */

#include "src/cosmo_libc.h"
#include "mod_querystring.h"

void example_basic_usage(void) {
    printf("\n=== Example 1: Basic Parsing ===\n");

    const char* query = "name=John+Doe&age=30&city=New%20York";
    printf("Query string: %s\n\n", query);

    std_hashmap_t* params = qs_parse(query);
    if (!params) {
        printf("Failed to parse query string\n");
        return;
    }

    printf("Parsed parameters:\n");
    const char* name = (const char*)std_hashmap_get(params, "name");
    const char* age = (const char*)std_hashmap_get(params, "age");
    const char* city = (const char*)std_hashmap_get(params, "city");

    printf("  name: %s\n", name ? name : "NULL");
    printf("  age: %s\n", age ? age : "NULL");
    printf("  city: %s\n", city ? city : "NULL");

    std_hashmap_free(params);
}

void example_building_query(void) {
    printf("\n=== Example 2: Building Query Strings ===\n");

    std_hashmap_t* params = std_hashmap_new();
    std_hashmap_set(params, "product", strdup("Laptop"));
    std_hashmap_set(params, "price", strdup("1299.99"));
    std_hashmap_set(params, "category", strdup("Electronics & Gadgets"));

    char* query = qs_stringify(params);
    printf("Generated query string:\n  %s\n", query);

    free(query);
    std_hashmap_free(params);
}

void example_encoding_decoding(void) {
    printf("\n=== Example 3: URL Encoding/Decoding ===\n");

    const char* original = "Hello, World! (Special chars: @#$%)";
    printf("Original: %s\n", original);

    char* encoded = qs_encode(original);
    printf("Encoded:  %s\n", encoded);

    char* decoded = qs_decode(encoded);
    printf("Decoded:  %s\n", decoded);

    free(encoded);
    free(decoded);
}

void example_custom_separators(void) {
    printf("\n=== Example 4: Custom Separators ===\n");

    const char* query = "key1:value1;key2:value2;key3:value3";
    printf("Query string (with ; and :): %s\n\n", query);

    std_hashmap_t* params = qs_parse_custom(query, ';', ':');
    if (!params) {
        printf("Failed to parse\n");
        return;
    }

    printf("Parsed parameters:\n");
    const char* val1 = (const char*)std_hashmap_get(params, "key1");
    const char* val2 = (const char*)std_hashmap_get(params, "key2");
    const char* val3 = (const char*)std_hashmap_get(params, "key3");

    printf("  key1: %s\n", val1 ? val1 : "NULL");
    printf("  key2: %s\n", val2 ? val2 : "NULL");
    printf("  key3: %s\n", val3 ? val3 : "NULL");

    /* Stringify back with custom separators */
    char* rebuilt = qs_stringify_custom(params, ';', ':');
    printf("\nRebuilt query string:\n  %s\n", rebuilt);

    free(rebuilt);
    std_hashmap_free(params);
}

void example_roundtrip(void) {
    printf("\n=== Example 5: Roundtrip Conversion ===\n");

    /* Start with hashmap */
    std_hashmap_t* original_params = std_hashmap_new();
    std_hashmap_set(original_params, "search", strdup("query string parser"));
    std_hashmap_set(original_params, "lang", strdup("C"));
    std_hashmap_set(original_params, "year", strdup("2025"));

    /* Convert to query string */
    char* query = qs_stringify(original_params);
    printf("Step 1 - Stringify: %s\n", query);

    /* Parse back to hashmap */
    std_hashmap_t* reparsed_params = qs_parse(query);
    printf("Step 2 - Parse back:\n");

    const char* search = (const char*)std_hashmap_get(reparsed_params, "search");
    const char* lang = (const char*)std_hashmap_get(reparsed_params, "lang");
    const char* year = (const char*)std_hashmap_get(reparsed_params, "year");

    printf("  search: %s\n", search ? search : "NULL");
    printf("  lang: %s\n", lang ? lang : "NULL");
    printf("  year: %s\n", year ? year : "NULL");

    /* Convert back to query string */
    char* final_query = qs_stringify(reparsed_params);
    printf("Step 3 - Stringify again: %s\n", final_query);

    free(query);
    free(final_query);
    std_hashmap_free(original_params);
    std_hashmap_free(reparsed_params);
}

void example_edge_cases(void) {
    printf("\n=== Example 6: Edge Cases ===\n");

    /* Empty query string */
    std_hashmap_t* params1 = qs_parse("");
    printf("Empty query string: %zu parameters\n", std_hashmap_size(params1));
    std_hashmap_free(params1);

    /* Key without value */
    std_hashmap_t* params2 = qs_parse("flag1&flag2&key=value");
    printf("\nQuery with keys without values: 'flag1&flag2&key=value'\n");
    printf("  flag1: '%s'\n", (const char*)std_hashmap_get(params2, "flag1"));
    printf("  flag2: '%s'\n", (const char*)std_hashmap_get(params2, "flag2"));
    printf("  key: '%s'\n", (const char*)std_hashmap_get(params2, "key"));
    std_hashmap_free(params2);

    /* URL with encoded special characters */
    const char* complex_query = "email=user%40example.com&msg=Hello%2C%20World%21";
    std_hashmap_t* params3 = qs_parse(complex_query);
    printf("\nComplex encoding: '%s'\n", complex_query);
    printf("  email: %s\n", (const char*)std_hashmap_get(params3, "email"));
    printf("  msg: %s\n", (const char*)std_hashmap_get(params3, "msg"));
    std_hashmap_free(params3);
}

int main(void) {
    printf("=====================================\n");
    printf("mod_querystring - Usage Examples\n");
    printf("=====================================\n");

    example_basic_usage();
    example_building_query();
    example_encoding_decoding();
    example_custom_separators();
    example_roundtrip();
    example_edge_cases();

    printf("\n=====================================\n");
    printf("All examples completed!\n");
    printf("=====================================\n");

    return 0;
}
