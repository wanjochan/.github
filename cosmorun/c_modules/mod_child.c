/*
 * mod_child.c - Child process management implementation
 */

/* NOTE: cosmo_libc.h is already included by the test file
 * so we DON'T include system headers here to avoid redefinition conflicts.
 * All types (pid_t, sigset_t, etc) come from cosmo_libc.h */

/* Include module headers */
#include "mod_child.h"

/* External declarations */
extern char **environ;

/* Signal definitions */
#ifndef SIGTERM
#define SIGTERM 15
#endif
#ifndef SIGKILL
#define SIGKILL 9
#endif
#ifndef SIGHUP
#define SIGHUP 1
#endif
#ifndef SIGINT
#define SIGINT 2
#endif

/* waitpid options */
#ifndef WNOHANG
#define WNOHANG 1
#endif
#ifndef WUNTRACED
#define WUNTRACED 2
#endif

/* fcntl constants */
#ifndef F_SETFD
#define F_SETFD 2
#endif
#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif
#ifndef O_RDONLY
#define O_RDONLY 0
#endif
#ifndef O_WRONLY
#define O_WRONLY 1
#endif
#ifndef O_RDWR
#define O_RDWR 2
#endif

/* File descriptors */
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

/* ==================== Helper Functions ==================== */

static void child_set_error(child_process_t *child, const char *error) {
    if (!child) return;
    if (child->error_message) shim_free(child->error_message);
    child->error_message = error ? shim_strdup(error) : NULL;
    if (child->emitter && error) {
        event_emit(child->emitter, "error", (void*)error);
    }
}

static int setup_pipe(int fds[2]) {
    if (pipe(fds) < 0) {
        return -1;
    }
    // Set close-on-exec for parent side
    fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    return 0;
}

static void close_fd(int *fd) {
    if (fd && *fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

/* ==================== Initialization ==================== */

void child_options_init(child_options_t *options) {
    if (!options) return;
    shim_memset(options, 0, sizeof(child_options_t));
    options->stdin_cfg.mode = CHILD_STDIO_PIPE;
    options->stdout_cfg.mode = CHILD_STDIO_PIPE;
    options->stderr_cfg.mode = CHILD_STDIO_PIPE;
}

/* ==================== Core Implementation ==================== */

child_process_t* child_spawn(const char *command, char **args, child_options_t *options) {
    if (!command || !args) return NULL;

    // Allocate child structure
    child_process_t *child = (child_process_t*)shim_calloc(1, sizeof(child_process_t));
    if (!child) return NULL;

    // Initialize
    child->pid = -1;
    child->exit_code = -1;
    child->running = 0;
    child->stdin_fd = -1;
    child->stdout_fd = -1;
    child->stderr_fd = -1;
    child->emitter = event_emitter_new();

    // Copy options
    if (options) {
        shim_memcpy(&child->options, options, sizeof(child_options_t));
    } else {
        child_options_init(&child->options);
    }

    // Apply convenience flags
    if (child->options.inherit_stdio) {
        child->options.stdin_cfg.mode = CHILD_STDIO_INHERIT;
        child->options.stdout_cfg.mode = CHILD_STDIO_INHERIT;
        child->options.stderr_cfg.mode = CHILD_STDIO_INHERIT;
    }
    if (child->options.capture_stdout) {
        child->options.stdout_cfg.mode = CHILD_STDIO_PIPE;
    }
    if (child->options.capture_stderr) {
        child->options.stderr_cfg.mode = CHILD_STDIO_PIPE;
    }

    // Create pipes
    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};

    if (child->options.stdin_cfg.mode == CHILD_STDIO_PIPE) {
        if (setup_pipe(stdin_pipe) < 0) {
            child_set_error(child, "Failed to create stdin pipe");
            child_free(child);
            return NULL;
        }
    }

    if (child->options.stdout_cfg.mode == CHILD_STDIO_PIPE) {
        if (setup_pipe(stdout_pipe) < 0) {
            child_set_error(child, "Failed to create stdout pipe");
            close_fd(&stdin_pipe[0]);
            close_fd(&stdin_pipe[1]);
            child_free(child);
            return NULL;
        }
    }

    if (child->options.stderr_cfg.mode == CHILD_STDIO_PIPE) {
        if (setup_pipe(stderr_pipe) < 0) {
            child_set_error(child, "Failed to create stderr pipe");
            close_fd(&stdin_pipe[0]);
            close_fd(&stdin_pipe[1]);
            close_fd(&stdout_pipe[0]);
            close_fd(&stdout_pipe[1]);
            child_free(child);
            return NULL;
        }
    }

    // Fork process
    pid_t pid = fork();
    if (pid < 0) {
        child_set_error(child, "Fork failed");
        close_fd(&stdin_pipe[0]);
        close_fd(&stdin_pipe[1]);
        close_fd(&stdout_pipe[0]);
        close_fd(&stdout_pipe[1]);
        close_fd(&stderr_pipe[0]);
        close_fd(&stderr_pipe[1]);
        child_free(child);
        return NULL;
    }

    if (pid == 0) {
        // Child process

        // Setup stdio redirection
        if (child->options.stdin_cfg.mode == CHILD_STDIO_PIPE) {
            dup2(stdin_pipe[0], STDIN_FILENO);
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
        } else if (child->options.stdin_cfg.mode == CHILD_STDIO_IGNORE) {
            int devnull = open("/dev/null", O_RDONLY);
            if (devnull >= 0) {
                dup2(devnull, STDIN_FILENO);
                close(devnull);
            }
        }

        if (child->options.stdout_cfg.mode == CHILD_STDIO_PIPE) {
            dup2(stdout_pipe[1], STDOUT_FILENO);
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
        } else if (child->options.stdout_cfg.mode == CHILD_STDIO_IGNORE) {
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDOUT_FILENO);
                close(devnull);
            }
        }

        if (child->options.stderr_cfg.mode == CHILD_STDIO_PIPE) {
            dup2(stderr_pipe[1], STDERR_FILENO);
            close(stderr_pipe[0]);
            close(stderr_pipe[1]);
        } else if (child->options.stderr_cfg.mode == CHILD_STDIO_IGNORE) {
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }
        }

        // Change working directory
        if (child->options.cwd) {
            if (chdir(child->options.cwd) < 0) {
                _exit(127);
            }
        }

        // Execute command
        if (child->options.use_shell) {
            const char *shell_args[] = {"/bin/sh", "-c", command, NULL};
            if (child->options.env) {
                execve("/bin/sh", (char**)shell_args, child->options.env);
            } else {
                execv("/bin/sh", (char**)shell_args);
            }
        } else {
            if (child->options.env) {
                execve(command, args, child->options.env);
            } else {
                execv(command, args);
            }
        }

        // If we reach here, exec failed
        _exit(127);
    }

    // Parent process
    child->pid = pid;
    child->running = 1;

    // Close child-side of pipes and store parent-side
    if (child->options.stdin_cfg.mode == CHILD_STDIO_PIPE) {
        close(stdin_pipe[0]);
        child->stdin_fd = stdin_pipe[1];
        // Make stdin writable stream
        // Note: For now we just store the fd, full stream integration would require more work
    }

    if (child->options.stdout_cfg.mode == CHILD_STDIO_PIPE) {
        close(stdout_pipe[1]);
        child->stdout_fd = stdout_pipe[0];
        // Make stdout readable stream
    }

    if (child->options.stderr_cfg.mode == CHILD_STDIO_PIPE) {
        close(stderr_pipe[1]);
        child->stderr_fd = stderr_pipe[0];
        // Make stderr readable stream
    }

    // Emit spawn event
    if (child->emitter) {
        event_emit(child->emitter, "spawn", NULL);
    }

    return child;
}

int child_wait(child_process_t *child, int timeout_ms) {
    if (!child) return -1;
    if (!child->running) return child->exit_code;

    int status;
    int options = (timeout_ms == 0) ? WNOHANG : 0;

    pid_t result = waitpid(child->pid, &status, options);

    if (result == child->pid) {
        child->running = 0;

        if (WIFEXITED(status)) {
            child->exit_code = WEXITSTATUS(status);
            child->signaled = 0;
        } else if (WIFSIGNALED(status)) {
            child->signaled = 1;
            child->signal_code = WTERMSIG(status);
            child->exit_code = -WTERMSIG(status);
        }

        // Emit exit event
        if (child->emitter && !child->exit_handled) {
            event_emit(child->emitter, "exit", &child->exit_code);
            child->exit_handled = 1;
        }

        return child->exit_code;
    } else if (result == 0) {
        return -2;  // Still running (timeout)
    }

    return -1;  // Error
}

int child_kill(child_process_t *child, int signal) {
    if (!child || !child->running) return -1;

    int result = kill(child->pid, signal);
    if (result == 0) {
        child->killed = 1;
    }

    return result;
}

int child_is_running(child_process_t *child) {
    if (!child) return -1;
    if (!child->running) return 0;

    // Check without blocking
    return child_wait(child, 0) == -2 ? 1 : 0;
}

void child_free(child_process_t *child) {
    if (!child) return;

    // Close file descriptors
    close_fd(&child->stdin_fd);
    close_fd(&child->stdout_fd);
    close_fd(&child->stderr_fd);

    // Free streams (not implemented yet - for future stream integration)
    // if (child->stdin_stream) stream_destroy(child->stdin_stream);
    // if (child->stdout_stream) stream_destroy(child->stdout_stream);
    // if (child->stderr_stream) stream_destroy(child->stderr_stream);

    // Free event emitter
    if (child->emitter) event_emitter_free(child->emitter);

    // Free error message
    if (child->error_message) shim_free(child->error_message);

    shim_free(child);
}

/* ==================== Event API ==================== */

void child_on(child_process_t *child, const char *event, event_listener_fn listener, void *userdata) {
    if (!child || !child->emitter) return;
    event_on(child->emitter, event, listener, userdata);
}

void child_off(child_process_t *child, const char *event, event_listener_fn listener) {
    if (!child || !child->emitter) return;
    event_off(child->emitter, event, listener);
}

/* ==================== Exec Functions ==================== */

int child_exec_sync(
    const char *command,
    char *stdout_buf,
    size_t stdout_size,
    char *stderr_buf,
    size_t stderr_size
) {
    if (!command) return -1;

    // Setup options - don't set use_shell since we're already using shell
    child_options_t opts = CHILD_OPTIONS_INIT;
    opts.capture_stdout = (stdout_buf && stdout_size > 0) ? 1 : 0;
    opts.capture_stderr = (stderr_buf && stderr_size > 0) ? 1 : 0;

    // Spawn child with shell
    char *args[] = {"/bin/sh", "-c", (char*)command, NULL};
    child_process_t *child = child_spawn("/bin/sh", args, &opts);

    if (!child) return -1;

    // Read stdout
    if (stdout_buf && stdout_size > 0 && child->stdout_fd >= 0) {
        ssize_t n = read(child->stdout_fd, stdout_buf, stdout_size - 1);
        if (n > 0) {
            stdout_buf[n] = '\0';
        } else {
            stdout_buf[0] = '\0';
        }
    }

    // Read stderr
    if (stderr_buf && stderr_size > 0 && child->stderr_fd >= 0) {
        ssize_t n = read(child->stderr_fd, stderr_buf, stderr_size - 1);
        if (n > 0) {
            stderr_buf[n] = '\0';
        } else {
            stderr_buf[0] = '\0';
        }
    }

    // Wait for child
    int exit_code = child_wait(child, -1);
    child_free(child);

    return exit_code;
}

child_process_t* child_exec(const char *command, child_exec_callback_t callback, void *userdata) {
    if (!command) return NULL;

    // For now, implement simple blocking version
    // Full async version would require event loop integration

    char stdout_buf[4096];
    char stderr_buf[4096];

    int exit_code = child_exec_sync(command, stdout_buf, sizeof(stdout_buf),
                                     stderr_buf, sizeof(stderr_buf));

    if (callback) {
        const char *error = (exit_code != 0) ? "Command failed" : NULL;
        callback(error, stdout_buf, shim_strlen(stdout_buf),
                stderr_buf, shim_strlen(stderr_buf), userdata);
    }

    return NULL;
}

/* ==================== Utility Functions ==================== */

int child_parse_command(const char *command, char ***out_argv) {
    if (!command || !out_argv) return -1;

    // Count arguments (simple space-based splitting)
    int argc = 0;
    int in_arg = 0;
    for (const char *p = command; *p; p++) {
        if (*p == ' ' || *p == '\t') {
            in_arg = 0;
        } else if (!in_arg) {
            argc++;
            in_arg = 1;
        }
    }

    if (argc == 0) return 0;

    // Allocate argv array (+ NULL terminator)
    char **argv = (char**)shim_calloc(argc + 1, sizeof(char*));
    if (!argv) return -1;

    // Parse arguments
    int idx = 0;
    const char *start = NULL;
    in_arg = 0;

    for (const char *p = command; *p || in_arg; p++) {
        if (*p == '\0' || *p == ' ' || *p == '\t') {
            if (in_arg) {
                size_t len = p - start;
                argv[idx] = (char*)shim_malloc(len + 1);
                if (argv[idx]) {
                    shim_memcpy(argv[idx], start, len);
                    argv[idx][len] = '\0';
                    idx++;
                }
                in_arg = 0;
            }
            if (*p == '\0') break;
        } else if (!in_arg) {
            start = p;
            in_arg = 1;
        }
    }

    argv[idx] = NULL;
    *out_argv = argv;
    return idx;
}

void child_free_argv(char **argv, int argc) {
    if (!argv) return;
    for (int i = 0; i < argc; i++) {
        if (argv[i]) shim_free(argv[i]);
    }
    shim_free(argv);
}

char** child_build_env(const char **pairs, size_t count) {
    if (!pairs || count == 0) return NULL;

    char **env = (char**)shim_calloc(count + 1, sizeof(char*));
    if (!env) return NULL;

    for (size_t i = 0; i < count; i++) {
        env[i] = shim_strdup(pairs[i]);
    }
    env[count] = NULL;

    return env;
}
