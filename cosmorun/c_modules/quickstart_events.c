/*
 * quickstart_events.c - Quick start guide for mod_events
 *
 * This is a minimal example showing the basics of EventEmitter.
 * To run: ./cosmorun.exe quickstart_events.c
 */

#include "mod_events.h"

// Simple listener that prints a message
void on_hello(const char *event, void *data, void *ctx) {
    char *name = (char*)data;
    printf("Hello, %s!\n", name);
}

// Listener that only fires once
void on_startup(const char *event, void *data, void *ctx) {
    printf("Application started!\n");
}

// Listener with context
void on_count(const char *event, void *data, void *ctx) {
    int *counter = (int*)ctx;
    (*counter)++;
    printf("Event #%d\n", *counter);
}

int main(void) {
    // Create an EventEmitter
    event_emitter_t *emitter = event_emitter_new();

    printf("=== EventEmitter Quick Start ===\n\n");

    // Example 1: Basic event
    printf("1. Basic event:\n");
    event_on(emitter, "hello", on_hello, NULL);
    event_emit(emitter, "hello", "World");
    printf("\n");

    // Example 2: Once listener (fires only on first emit)
    printf("2. Once listener:\n");
    event_once(emitter, "startup", on_startup, NULL);
    event_emit(emitter, "startup", NULL);  // Triggers
    event_emit(emitter, "startup", NULL);  // Ignored
    printf("\n");

    // Example 3: Using context
    printf("3. Listener with context:\n");
    int counter = 0;
    event_on(emitter, "tick", on_count, &counter);
    event_emit(emitter, "tick", NULL);
    event_emit(emitter, "tick", NULL);
    event_emit(emitter, "tick", NULL);
    printf("\n");

    // Example 4: Multiple listeners
    printf("4. Multiple listeners:\n");
    event_on(emitter, "multi", on_hello, NULL);
    event_on(emitter, "multi", on_hello, NULL);
    event_emit(emitter, "multi", "Alice");
    printf("\n");

    // Example 5: Check listener count
    printf("5. Listener count:\n");
    printf("   'multi' has %zu listeners\n",
           event_listener_count(emitter, "multi"));
    printf("\n");

    // Clean up
    event_emitter_free(emitter);

    printf("Done!\n");
    return 0;
}
