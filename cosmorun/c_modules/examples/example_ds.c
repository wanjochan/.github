/*
 * example_ds.c - Examples of using the Data Structures library
 *
 * Build and run:
 *   ./cosmorun.exe c_modules/examples/example_ds.c
 */

#include "src/cosmo_libc.h"
#include "c_modules/mod_ds.c"

void example_list(void) {
    printf("\n=== List Example ===\n");

    ds_list_t *list = ds_list_new();

    // Add items
    ds_list_push(list, "Alice");
    ds_list_push(list, "Bob");
    ds_list_push(list, "Charlie");
    ds_list_insert(list, 1, "Dave");  // Insert at index 1

    printf("List size: %zu\n", ds_list_size(list));

    // Iterate
    printf("Items: ");
    for (int i = 0; i < (int)ds_list_size(list); i++) {
        printf("%s ", (char *)ds_list_get(list, i));
    }
    printf("\n");

    // Search
    if (ds_list_contains(list, "Bob", ds_compare_string)) {
        printf("Found 'Bob' in list\n");
    }

    ds_list_free(list);
}

void example_map(void) {
    printf("\n=== Map Example ===\n");

    ds_map_t *map = ds_map_new();

    // Store key-value pairs
    ds_map_put(map, "name", "John Doe");
    ds_map_put(map, "city", "New York");
    ds_map_put(map, "country", "USA");

    // Retrieve values
    printf("Name: %s\n", (char *)ds_map_get(map, "name"));
    printf("City: %s\n", (char *)ds_map_get(map, "city"));

    // Update value
    ds_map_put(map, "city", "San Francisco");
    printf("Updated City: %s\n", (char *)ds_map_get(map, "city"));

    printf("Map size: %zu\n", ds_map_size(map));

    ds_map_free(map);
}

void example_set(void) {
    printf("\n=== Set Example ===\n");

    ds_set_t *set1 = ds_set_new();
    ds_set_t *set2 = ds_set_new();

    // Add items (duplicates automatically filtered)
    ds_set_add(set1, "apple");
    ds_set_add(set1, "banana");
    ds_set_add(set1, "cherry");
    ds_set_add(set1, "apple");  // Duplicate - ignored

    ds_set_add(set2, "banana");
    ds_set_add(set2, "cherry");
    ds_set_add(set2, "date");

    printf("Set1 size: %zu (no duplicates)\n", ds_set_size(set1));

    // Set operations
    ds_set_t *union_set = ds_set_union(set1, set2);
    printf("Union size: %zu\n", ds_set_size(union_set));

    ds_set_t *inter_set = ds_set_intersection(set1, set2);
    printf("Intersection size: %zu\n", ds_set_size(inter_set));

    // Check membership
    if (ds_set_contains(set1, "banana")) {
        printf("Set1 contains 'banana'\n");
    }

    ds_set_free(set1);
    ds_set_free(set2);
    ds_set_free(union_set);
    ds_set_free(inter_set);
}

void example_queue(void) {
    printf("\n=== Queue Example (FIFO) ===\n");

    ds_queue_t *queue = ds_queue_new();

    // Enqueue items
    ds_queue_enqueue(queue, "Task 1");
    ds_queue_enqueue(queue, "Task 2");
    ds_queue_enqueue(queue, "Task 3");

    printf("Queue size: %zu\n", ds_queue_size(queue));

    // Process queue (FIFO - first in, first out)
    while (!ds_queue_is_empty(queue)) {
        char *task = ds_queue_dequeue(queue);
        printf("Processing: %s\n", task);
    }

    ds_queue_free(queue);
}

void example_stack(void) {
    printf("\n=== Stack Example (LIFO) ===\n");

    ds_stack_t *stack = ds_stack_new();

    // Push items
    ds_stack_push(stack, "Page 1");
    ds_stack_push(stack, "Page 2");
    ds_stack_push(stack, "Page 3");

    printf("Stack size: %zu\n", ds_stack_size(stack));

    // Pop items (LIFO - last in, first out)
    printf("Back navigation:\n");
    while (!ds_stack_is_empty(stack)) {
        char *page = ds_stack_pop(stack);
        printf("  <- %s\n", page);
    }

    ds_stack_free(stack);
}

void example_practical_use_case(void) {
    printf("\n=== Practical Use Case: Request Router ===\n");

    // Use map to store routes
    ds_map_t *routes = ds_map_new();
    ds_map_put(routes, "/", "index_handler");
    ds_map_put(routes, "/about", "about_handler");
    ds_map_put(routes, "/api/users", "users_api_handler");

    // Use queue for request processing
    ds_queue_t *requests = ds_queue_new();
    ds_queue_enqueue(requests, "/");
    ds_queue_enqueue(requests, "/about");
    ds_queue_enqueue(requests, "/api/users");

    // Process requests
    printf("Processing incoming requests:\n");
    while (!ds_queue_is_empty(requests)) {
        char *path = ds_queue_dequeue(requests);
        char *handler = ds_map_get(routes, path);
        if (handler) {
            printf("  %s -> %s\n", path, handler);
        } else {
            printf("  %s -> 404_handler\n", path);
        }
    }

    ds_map_free(routes);
    ds_queue_free(requests);
}

int main(void) {
    printf("========================================\n");
    printf("CosmoRun Data Structures Examples\n");
    printf("========================================\n");

    example_list();
    example_map();
    example_set();
    example_queue();
    example_stack();
    example_practical_use_case();

    printf("\n✓ All examples completed successfully!\n");
    return 0;
}
