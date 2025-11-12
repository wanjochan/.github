/*
 * example_mod_events.c - Example usage of mod_events EventEmitter
 *
 * Demonstrates:
 * - Creating an EventEmitter
 * - Adding listeners with on() and once()
 * - Emitting events with data
 * - Removing listeners
 * - Managing multiple events
 */

#include "mod_events.h"

/* Example: Simple notification system */

typedef struct {
    char username[32];
    int level;
} user_data_t;

void on_user_login(const char *event, void *data, void *ctx) {
    user_data_t *user = (user_data_t*)data;
    printf("  [Auth] User '%s' logged in (level %d)\n", user->username, user->level);
}

void on_user_logout(const char *event, void *data, void *ctx) {
    user_data_t *user = (user_data_t*)data;
    printf("  [Auth] User '%s' logged out\n", user->username);
}

void send_welcome_email(const char *event, void *data, void *ctx) {
    user_data_t *user = (user_data_t*)data;
    printf("  [Email] Sending welcome email to '%s'\n", user->username);
}

void log_first_login(const char *event, void *data, void *ctx) {
    user_data_t *user = (user_data_t*)data;
    printf("  [Analytics] First-time login for '%s'\n", user->username);
}

void demo_basic_events(void) {
    printf("\n=== Demo 1: Basic Event System ===\n");

    event_emitter_t *emitter = event_emitter_new();

    // Add multiple listeners to same event
    event_on(emitter, "user:login", on_user_login, NULL);
    event_on(emitter, "user:login", send_welcome_email, NULL);

    // Add once listener (only triggers on first login)
    event_once(emitter, "user:login", log_first_login, NULL);

    // Add logout listener
    event_on(emitter, "user:logout", on_user_logout, NULL);

    // Simulate user login
    printf("\nFirst login:\n");
    user_data_t alice = {"alice", 5};
    event_emit(emitter, "user:login", &alice);

    // Second login - log_first_login won't trigger
    printf("\nSecond login:\n");
    user_data_t bob = {"bob", 3};
    event_emit(emitter, "user:login", &bob);

    // Logout
    printf("\nLogout:\n");
    event_emit(emitter, "user:logout", &alice);

    event_emitter_free(emitter);
}

/* Example: Data pipeline with events */

typedef struct {
    int value;
    char status[32];
} pipeline_data_t;

void validate_data(const char *event, void *data, void *ctx) {
    pipeline_data_t *pd = (pipeline_data_t*)data;
    if (pd->value > 0) {
        printf("  [Validator] Data valid (value=%d)\n", pd->value);
        strcpy(pd->status, "valid");
    } else {
        printf("  [Validator] Data invalid (value=%d)\n", pd->value);
        strcpy(pd->status, "invalid");
    }
}

void transform_data(const char *event, void *data, void *ctx) {
    pipeline_data_t *pd = (pipeline_data_t*)data;
    if (strcmp(pd->status, "valid") == 0) {
        pd->value *= 2;
        printf("  [Transformer] Transformed value to %d\n", pd->value);
    }
}

void save_data(const char *event, void *data, void *ctx) {
    pipeline_data_t *pd = (pipeline_data_t*)data;
    if (strcmp(pd->status, "valid") == 0) {
        printf("  [Storage] Saved value=%d\n", pd->value);
    } else {
        printf("  [Storage] Skipped invalid data\n");
    }
}

void demo_data_pipeline(void) {
    printf("\n=== Demo 2: Data Pipeline ===\n");

    event_emitter_t *emitter = event_emitter_new();

    // Pipeline: validate -> transform -> save
    event_on(emitter, "data:process", validate_data, NULL);
    event_on(emitter, "data:process", transform_data, NULL);
    event_on(emitter, "data:process", save_data, NULL);

    // Process valid data
    printf("\nProcessing valid data:\n");
    pipeline_data_t data1 = {10, ""};
    event_emit(emitter, "data:process", &data1);

    // Process invalid data
    printf("\nProcessing invalid data:\n");
    pipeline_data_t data2 = {-5, ""};
    event_emit(emitter, "data:process", &data2);

    event_emitter_free(emitter);
}

/* Example: Event listener management */

void handler_a(const char *event, void *data, void *ctx) {
    printf("  Handler A triggered\n");
}

void handler_b(const char *event, void *data, void *ctx) {
    printf("  Handler B triggered\n");
}

void handler_c(const char *event, void *data, void *ctx) {
    printf("  Handler C triggered\n");
}

void demo_listener_management(void) {
    printf("\n=== Demo 3: Listener Management ===\n");

    event_emitter_t *emitter = event_emitter_new();

    // Add listeners
    event_on(emitter, "test", handler_a, NULL);
    event_on(emitter, "test", handler_b, NULL);
    event_on(emitter, "test", handler_c, NULL);

    printf("\nWith all 3 handlers:\n");
    printf("  Listener count: %zu\n", event_listener_count(emitter, "test"));
    event_emit(emitter, "test", NULL);

    // Remove one handler
    event_off(emitter, "test", handler_b);

    printf("\nAfter removing handler B:\n");
    printf("  Listener count: %zu\n", event_listener_count(emitter, "test"));
    event_emit(emitter, "test", NULL);

    // Remove all handlers
    event_remove_all_listeners(emitter, "test");

    printf("\nAfter removing all handlers:\n");
    printf("  Listener count: %zu\n", event_listener_count(emitter, "test"));
    int count = event_emit(emitter, "test", NULL);
    printf("  Handlers called: %d\n", count);

    event_emitter_free(emitter);
}

/* Example: Multiple events */

void demo_multiple_events(void) {
    printf("\n=== Demo 4: Multiple Events ===\n");

    event_emitter_t *emitter = event_emitter_new();

    // Register different events
    event_on(emitter, "click", handler_a, NULL);
    event_on(emitter, "hover", handler_b, NULL);
    event_on(emitter, "scroll", handler_c, NULL);
    event_on(emitter, "click", handler_b, NULL);  // Multiple handlers for click

    // Get all event names
    size_t count = 0;
    char **names = event_names(emitter, &count);

    printf("\nRegistered events:\n");
    for (size_t i = 0; i < count; i++) {
        printf("  - %s (listeners: %zu)\n",
               names[i], event_listener_count(emitter, names[i]));
        free(names[i]);
    }
    free(names);

    printf("\nTriggering 'click' event:\n");
    event_emit(emitter, "click", NULL);

    event_emitter_free(emitter);
}

int main(void) {
    printf("===========================================\n");
    printf("  mod_events Examples\n");
    printf("===========================================\n");

    demo_basic_events();
    demo_data_pipeline();
    demo_listener_management();
    demo_multiple_events();

    printf("\n===========================================\n");
    printf("  All demos completed!\n");
    printf("===========================================\n");

    return 0;
}
