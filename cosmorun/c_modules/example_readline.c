/*
 * example_readline.c - Examples demonstrating mod_readline usage
 *
 * This file shows how to use mod_readline for building interactive
 * command-line applications with history support.
 */

#include "src/cosmo_libc.h"
#include "mod_std.c"
#include "mod_events.c"
#include "mod_readline.c"

/* ==================== Example 1: Simple REPL ==================== */

void example_simple_repl(void) {
    printf("\n=== Example 1: Simple REPL ===\n");
    printf("This is a test mode example. Type commands and they are echoed back.\n");
    printf("In real usage, readline_create() would read from stdin.\n\n");

    // Simulate user input for demonstration
    const char *simulated_inputs[] = {
        "help",
        "list",
        "status",
        "quit"
    };

    readline_t *rl = readline_create_test("myapp> ", simulated_inputs, 4);
    if (!rl) {
        fprintf(stderr, "Failed to create readline interface\n");
        return;
    }

    char *line;
    while ((line = readline_read(rl)) != NULL) {
        printf("You typed: %s\n", line);

        // Add to history
        readline_history_add(rl, line);

        // Process command
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
            free(line);
            break;
        }

        free(line);
    }

    printf("REPL exited.\n");
    readline_free(rl);
}

/* ==================== Example 2: REPL with History Persistence ==================== */

void example_repl_with_history(void) {
    printf("\n=== Example 2: REPL with History Persistence ===\n");
    printf("Demonstrates loading and saving history to file.\n\n");

    const char *history_file = "/tmp/myapp_history.txt";
    const char *simulated_inputs[] = {
        "command1",
        "command2",
        "command3"
    };

    readline_t *rl = readline_create_test("app> ", simulated_inputs, 3);
    if (!rl) {
        fprintf(stderr, "Failed to create readline interface\n");
        return;
    }

    // Load existing history (if any)
    int loaded = readline_history_load(rl, history_file);
    if (loaded > 0) {
        printf("Loaded %d history entries from %s\n", loaded, history_file);
    }

    // Read and process commands
    char *line;
    int command_count = 0;
    while ((line = readline_read(rl)) != NULL) {
        printf("[%d] Command: %s\n", ++command_count, line);

        // Add to history
        readline_history_add(rl, line);

        free(line);
    }

    // Save history before exiting
    if (readline_history_save(rl, history_file) == 0) {
        printf("\nSaved %zu history entries to %s\n",
               readline_history_size(rl), history_file);
    }

    // Display current history
    printf("\nCurrent history:\n");
    for (size_t i = 0; i < readline_history_size(rl); i++) {
        printf("  [%zu] %s\n", i, readline_history_get(rl, i));
    }

    readline_free(rl);
    printf("\nHistory saved. Next run will restore these commands.\n");
}

/* ==================== Example 3: REPL with History Limit ==================== */

void example_repl_with_limit(void) {
    printf("\n=== Example 3: REPL with History Size Limit ===\n");
    printf("Demonstrates limiting history to last N commands.\n\n");

    const char *simulated_inputs[] = {
        "cmd1",
        "cmd2",
        "cmd3",
        "cmd4",
        "cmd5",
        "cmd6"
    };

    readline_t *rl = readline_create_test("limited> ", simulated_inputs, 6);
    if (!rl) {
        fprintf(stderr, "Failed to create readline interface\n");
        return;
    }

    // Limit history to last 3 commands
    readline_history_set_max_size(rl, 3);
    printf("History limited to 3 entries\n\n");

    char *line;
    while ((line = readline_read(rl)) != NULL) {
        printf("Command: %s\n", line);
        readline_history_add(rl, line);

        // Show current history
        printf("  History (%zu entries): ", readline_history_size(rl));
        for (size_t i = 0; i < readline_history_size(rl); i++) {
            printf("%s", readline_history_get(rl, i));
            if (i < readline_history_size(rl) - 1) printf(", ");
        }
        printf("\n");

        free(line);
    }

    printf("\nFinal history (oldest entries were dropped):\n");
    for (size_t i = 0; i < readline_history_size(rl); i++) {
        printf("  [%zu] %s\n", i, readline_history_get(rl, i));
    }

    readline_free(rl);
}

/* ==================== Example 4: Event-Driven REPL ==================== */

static void on_line_received(const char *event, void *data, void *ctx) {
    char *line = (char*)data;
    int *line_count = (int*)ctx;

    (*line_count)++;
    printf("  [Event Handler] Line #%d received: %s\n", *line_count, line);

    // Could process command here, log it, send to server, etc.
}

static void on_readline_close(const char *event, void *data, void *ctx) {
    printf("  [Event Handler] Readline interface closed\n");
}

void example_event_driven_repl(void) {
    printf("\n=== Example 4: Event-Driven REPL ===\n");
    printf("Demonstrates using EventEmitter for 'line' and 'close' events.\n\n");

    const char *simulated_inputs[] = {
        "event1",
        "event2",
        "event3"
    };

    readline_t *rl = readline_create_test("events> ", simulated_inputs, 3);
    if (!rl) {
        fprintf(stderr, "Failed to create readline interface\n");
        return;
    }

    // Create and attach event emitter
    event_emitter_t *emitter = event_emitter_new();
    if (!emitter) {
        fprintf(stderr, "Failed to create event emitter\n");
        readline_free(rl);
        return;
    }

    int line_count = 0;
    event_on(emitter, "line", on_line_received, &line_count);
    event_on(emitter, "close", on_readline_close, NULL);

    readline_set_emitter(rl, emitter);  // Takes ownership of emitter

    // Read lines - events will be fired automatically
    printf("Reading lines (events will be fired):\n");
    char *line;
    while ((line = readline_read(rl)) != NULL) {
        // Event already fired, just add to history
        readline_history_add(rl, line);
        free(line);
    }

    // Close will also fire an event
    readline_close(rl);

    printf("\nTotal lines processed: %d\n", line_count);
    readline_free(rl);
}

/* ==================== Example 5: Calculator REPL ==================== */

static double evaluate_expression(const char *expr) {
    // Simple evaluation (just demonstrates usage)
    // In real app, would use proper parser
    double a, b;
    char op;

    if (sscanf(expr, "%lf %c %lf", &a, &op, &b) == 3) {
        switch (op) {
            case '+': return a + b;
            case '-': return a - b;
            case '*': return a * b;
            case '/': return b != 0 ? a / b : 0;
            default: return 0;
        }
    }
    return 0;
}

void example_calculator_repl(void) {
    printf("\n=== Example 5: Calculator REPL ===\n");
    printf("Simple calculator with history.\n");
    printf("Enter expressions like: 10 + 5, 20 * 3, etc.\n\n");

    const char *simulated_inputs[] = {
        "10 + 5",
        "20 * 3",
        "100 / 4",
        "15 - 7",
        "quit"
    };

    readline_t *rl = readline_create_test("calc> ", simulated_inputs, 5);
    if (!rl) {
        fprintf(stderr, "Failed to create readline interface\n");
        return;
    }

    char *line;
    while ((line = readline_read(rl)) != NULL) {
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
            free(line);
            break;
        }

        if (strcmp(line, "history") == 0) {
            printf("Calculation history:\n");
            for (size_t i = 0; i < readline_history_size(rl); i++) {
                printf("  [%zu] %s\n", i, readline_history_get(rl, i));
            }
        } else if (strcmp(line, "clear") == 0) {
            readline_history_clear(rl);
            printf("History cleared\n");
        } else {
            double result = evaluate_expression(line);
            printf("  = %.2f\n", result);
            readline_history_add(rl, line);
        }

        free(line);
    }

    printf("\nCalculator exited.\n");
    readline_free(rl);
}

/* ==================== Main ==================== */

int main(void) {
    printf("===========================================\n");
    printf("  mod_readline Examples\n");
    printf("===========================================\n");

    example_simple_repl();
    example_repl_with_history();
    example_repl_with_limit();
    example_event_driven_repl();
    example_calculator_repl();

    printf("\n===========================================\n");
    printf("  All examples completed!\n");
    printf("===========================================\n");
    printf("\nNote: These examples use test mode for demonstration.\n");
    printf("In real usage, replace readline_create_test() with\n");
    printf("readline_create() to read from actual stdin.\n");

    return 0;
}
