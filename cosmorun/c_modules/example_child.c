/*
 * example_child.c - Examples demonstrating mod_child usage
 */

#include "src/cosmo_libc.h"
#include "mod_std.c"
#include "mod_events.c"
#include "mod_child.c"

/* ==================== Example 1: Simple Command Execution ==================== */

void example_simple_spawn(void) {
    printf("\n=== Example 1: Simple Spawn ===\n");

    char *args[] = {"/bin/echo", "Hello from child process!", NULL};
    child_options_t opts = CHILD_OPTIONS_INIT;
    opts.capture_stdout = 1;

    child_process_t *child = child_spawn("/bin/echo", args, &opts);
    if (!child) {
        printf("Failed to spawn child\n");
        return;
    }

    // Read output
    char buf[256];
    if (child->stdout_fd >= 0) {
        ssize_t n = read(child->stdout_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("Child output: %s", buf);
        }
    }

    // Wait for completion
    int exit_code = child_wait(child, -1);
    printf("Child exited with code: %d\n", exit_code);

    child_free(child);
}

/* ==================== Example 2: Pipe Data to Child ==================== */

void example_stdin_pipe(void) {
    printf("\n=== Example 2: Pipe Data to Child ===\n");

    char *args[] = {"/bin/cat", NULL};
    child_options_t opts = CHILD_OPTIONS_INIT;
    opts.capture_stdout = 1;

    child_process_t *child = child_spawn("/bin/cat", args, &opts);
    if (!child) {
        printf("Failed to spawn child\n");
        return;
    }

    // Write to stdin
    const char *input = "This is data sent to child stdin\n";
    if (child->stdin_fd >= 0) {
        write(child->stdin_fd, input, strlen(input));
        close(child->stdin_fd);
        child->stdin_fd = -1;
    }

    // Read stdout
    char buf[256];
    if (child->stdout_fd >= 0) {
        ssize_t n = read(child->stdout_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("Child echoed: %s", buf);
        }
    }

    child_wait(child, -1);
    child_free(child);
}

/* ==================== Example 3: Environment Variables ==================== */

void example_environment(void) {
    printf("\n=== Example 3: Environment Variables ===\n");

    char *args[] = {"/bin/sh", "-c", "echo MY_VAR=$MY_VAR", NULL};
    child_options_t opts = CHILD_OPTIONS_INIT;
    opts.capture_stdout = 1;

    // Build custom environment
    const char *env_pairs[] = {
        "MY_VAR=custom_value",
        "PATH=/bin:/usr/bin"
    };
    opts.env = child_build_env(env_pairs, 2);

    child_process_t *child = child_spawn("/bin/sh", args, &opts);
    if (!child) {
        printf("Failed to spawn child\n");
        return;
    }

    // Read output
    char buf[256];
    if (child->stdout_fd >= 0) {
        ssize_t n = read(child->stdout_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("Output: %s", buf);
        }
    }

    child_wait(child, -1);

    // Cleanup
    if (opts.env) {
        for (char **ep = opts.env; *ep; ep++) free(*ep);
        free(opts.env);
    }
    child_free(child);
}

/* ==================== Example 4: Signal Handling ==================== */

void example_signal(void) {
    printf("\n=== Example 4: Signal Handling ===\n");

    char *args[] = {"/bin/sleep", "30", NULL};
    child_options_t opts = CHILD_OPTIONS_INIT;

    child_process_t *child = child_spawn("/bin/sleep", args, &opts);
    if (!child) {
        printf("Failed to spawn child\n");
        return;
    }

    printf("Child spawned with PID: %d\n", child->pid);
    printf("Waiting 1 second before killing...\n");

    // Let it run briefly
    child_wait(child, 0);  // Non-blocking check

    // Send signal
    printf("Sending SIGTERM to child...\n");
    child_kill(child, SIGTERM);

    // Wait for it to exit
    int exit_code = child_wait(child, -1);
    printf("Child terminated (exit_code: %d, signaled: %d)\n",
           exit_code, child->signaled);

    child_free(child);
}

/* ==================== Example 5: Event Listeners ==================== */

void on_exit_event(const char *event, void *data, void *ctx) {
    (void)event; (void)ctx;
    int exit_code = data ? *(int*)data : -1;
    printf("Event: Child exited with code %d\n", exit_code);
}

void example_events(void) {
    printf("\n=== Example 5: Event Listeners ===\n");

    char *args[] = {"/bin/sh", "-c", "exit 42", NULL};
    child_options_t opts = CHILD_OPTIONS_INIT;

    child_process_t *child = child_spawn("/bin/sh", args, &opts);
    if (!child) {
        printf("Failed to spawn child\n");
        return;
    }

    // Register exit event listener
    child_on(child, "exit", on_exit_event, NULL);

    // Wait for child
    child_wait(child, -1);

    child_free(child);
}

/* ==================== Example 6: Synchronous Execution ==================== */

void example_exec_sync(void) {
    printf("\n=== Example 6: Synchronous Execution ===\n");

    char stdout_buf[512];
    char stderr_buf[512];

    printf("Executing: ls -la /tmp\n");
    int exit_code = child_exec_sync("ls -la /tmp",
                                     stdout_buf, sizeof(stdout_buf),
                                     stderr_buf, sizeof(stderr_buf));

    printf("Exit code: %d\n", exit_code);
    if (strlen(stdout_buf) > 0) {
        printf("stdout (first 200 chars):\n%.200s\n", stdout_buf);
    }
    if (strlen(stderr_buf) > 0) {
        printf("stderr: %s\n", stderr_buf);
    }
}

/* ==================== Example 7: Check if Running ==================== */

void example_is_running(void) {
    printf("\n=== Example 7: Check if Running ===\n");

    char *args[] = {"/bin/sleep", "2", NULL};
    child_options_t opts = CHILD_OPTIONS_INIT;

    child_process_t *child = child_spawn("/bin/sleep", args, &opts);
    if (!child) {
        printf("Failed to spawn child\n");
        return;
    }

    printf("Spawned sleep process\n");
    printf("Is running: %d\n", child_is_running(child));

    printf("Waiting for process to complete...\n");
    child_wait(child, -1);

    printf("Is running: %d\n", child_is_running(child));

    child_free(child);
}

/* ==================== Main ==================== */

int main(void) {
    printf("=== mod_child Examples ===\n");

    example_simple_spawn();
    example_stdin_pipe();
    example_environment();
    example_signal();
    example_events();
    example_exec_sync();
    example_is_running();

    printf("\n=== All examples completed ===\n");
    return 0;
}
