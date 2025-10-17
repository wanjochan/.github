/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Lightweight HTTP Server - Modularized with cosmorun                         │
│ Based on cosmopolitan/redbean.c architecture                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "cosmo_libc.h"

extern uint16_t htons(uint16_t hostshort);
extern int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern int atoi(const char *str);
extern int socket(int domain, int type, int protocol);

// Core HTTP server configuration
#define DEFAULT_PORT 8080
#define BACKLOG 128
#define MAX_WORKERS 16
#define MAX_THREADS_PER_WORKER 32
#define MAX_COROUTINES_PER_THREAD 1000
#define BUFFER_SIZE 65536
#define MAX_HEADER_SIZE 8192

// Server configuration structure
typedef struct {
  int num_processes;         // p: number of worker processes
  int threads_per_process;   // t: threads per worker process
  int max_coroutines;        // c: max concurrent coroutines per thread
  int port;
  int backlog;
} server_config_t;

// HTTP status codes
#define HTTP_OK 200
#define HTTP_BAD_REQUEST 400
#define HTTP_NOT_FOUND 404
#define HTTP_METHOD_NOT_ALLOWED 405
#define HTTP_INTERNAL_ERROR 500

// Thread pool state for each worker process
typedef struct {
  pthread_t thread_id;
  int thread_index;
  int server_fd;
  server_config_t *config;
  volatile int active;
} thread_context_t;

// Global state
static volatile sig_atomic_t g_shutdown = 0;
static int g_server_fd = -1;
static server_config_t g_config;
static pid_t g_workers[MAX_WORKERS];

// Forward declarations
static void signal_handler(int sig);
static void setup_signals(void);
static int create_server_socket(int port);
static void event_loop(int server_fd);
static void handle_client(int client_fd);
static void send_response(int fd, int status, const char* content_type,
                          const char* body, size_t body_len);
static const char* status_text(int code);
static void worker_process(int server_fd);
static void* thread_worker(void* arg);
static void coroutine_handle_client(int client_fd, server_config_t *config);

// Signal handler for graceful shutdown
static void signal_handler(int sig) {
  g_shutdown = 1;
  if (g_server_fd >= 0) {
    close(g_server_fd);
    g_server_fd = -1;
  }
}

static void setup_signals(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  // Ignore SIGPIPE (broken pipe when client disconnects)
  signal(SIGPIPE, SIG_IGN);
}

static const char* status_text(int code) {
  switch (code) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    default: return "Unknown";
  }
}

static void send_response(int fd, int status, const char* content_type,
                          const char* body, size_t body_len) {
  char header[1024];
  int header_len = snprintf(header, sizeof(header),
    "HTTP/1.1 %d %s\r\n"
    "Content-Type: %s\r\n"
    "Content-Length: %zu\r\n"
    "Connection: close\r\n"
    "Server: cosmorun-webserver/1.0\r\n"
    "\r\n",
    status, status_text(status), content_type, body_len);

  write(fd, header, header_len);
  if (body && body_len > 0) {
    write(fd, body, body_len);
  }
}

// Coroutine-based client handler (currently simplified, can be extended with real coroutines)
static void coroutine_handle_client(int client_fd, server_config_t *config) {
  // NOTE: This is a placeholder for future coroutine implementation
  // In a full implementation, this would:
  // 1. Create a coroutine context with setjmp/longjmp or libco
  // 2. Implement async I/O with yield points
  // 3. Support c concurrent coroutines per thread via semaphore
  //
  // Current implementation: direct synchronous handling
  handle_client(client_fd);
}

static void handle_client(int client_fd) {
  char buffer[BUFFER_SIZE];
  ssize_t received = read(client_fd, buffer, sizeof(buffer) - 1);

  if (received <= 0) {
    close(client_fd);
    return;
  }

  buffer[received] = '\0';

  // Parse HTTP request line (very basic)
  char method[16], path[1024], version[16];
  if (sscanf(buffer, "%15s %1023s %15s", method, path, version) != 3) {
    send_response(client_fd, HTTP_BAD_REQUEST, "text/plain",
                  "Bad Request", 11);
    close(client_fd);
    return;
  }

  // Disabled for performance: printf("[PID:%d] %s %s %s\n", getpid(), method, path, version);

  // Only support GET for now
  if (strcmp(method, "GET") != 0) {
    send_response(client_fd, HTTP_METHOD_NOT_ALLOWED, "text/plain",
                  "Method Not Allowed", 18);
    close(client_fd);
    return;
  }

  // Simple routing
  if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
    const char* html =
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head><title>Cosmorun WebServer</title></head>\n"
      "<body>\n"
      "<h1>Welcome to Cosmorun WebServer</h1>\n"
      "<p>This is a lightweight HTTP server built with cosmopolitan libc.</p>\n"
      "<ul>\n"
      "<li><a href=\"/\">Home</a></li>\n"
      "<li><a href=\"/status\">Server Status</a></li>\n"
      "<li><a href=\"/test\">Test Page</a></li>\n"
      "</ul>\n"
      "</body>\n"
      "</html>\n";
    send_response(client_fd, HTTP_OK, "text/html", html, strlen(html));
  }
  else if (strcmp(path, "/status") == 0) {
    char status_page[4096];
    int len = snprintf(status_page, sizeof(status_page),
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head><title>Server Status</title></head>\n"
      "<body>\n"
      "<h1>Server Status</h1>\n"
      "<h2>Configuration</h2>\n"
      "<table border=\"1\" style=\"border-collapse: collapse;\">\n"
      "<tr><td><b>Port</b></td><td>%d</td></tr>\n"
      "<tr><td><b>Processes (p)</b></td><td>%d</td></tr>\n"
      "<tr><td><b>Threads/Process (t)</b></td><td>%d</td></tr>\n"
      "<tr><td><b>Max Coroutines (c)</b></td><td>%d</td></tr>\n"
      "<tr><td><b>Total Workers</b></td><td>%d (p × t)</td></tr>\n"
      "<tr><td><b>Max Concurrency</b></td><td>%d (p × t × c)</td></tr>\n"
      "</table>\n"
      "<h2>Runtime Info</h2>\n"
      "<table border=\"1\" style=\"border-collapse: collapse;\">\n"
      "<tr><td><b>Current PID</b></td><td>%d</td></tr>\n"
      "</table>\n"
      "<p><a href=\"/\">Back to Home</a></p>\n"
      "</body>\n"
      "</html>\n",
      g_config.port, g_config.num_processes, g_config.threads_per_process,
      g_config.max_coroutines,
      g_config.num_processes * g_config.threads_per_process,
      g_config.num_processes * g_config.threads_per_process * g_config.max_coroutines,
      getpid());
    send_response(client_fd, HTTP_OK, "text/html", status_page, len);
  }
  else if (strcmp(path, "/test") == 0) {
    const char* test = "Test page OK\n";
    send_response(client_fd, HTTP_OK, "text/plain", test, strlen(test));
  }
  else {
    const char* not_found =
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head><title>404 Not Found</title></head>\n"
      "<body>\n"
      "<h1>404 - Page Not Found</h1>\n"
      "<p><a href=\"/\">Back to Home</a></p>\n"
      "</body>\n"
      "</html>\n";
    send_response(client_fd, HTTP_NOT_FOUND, "text/html", not_found, strlen(not_found));
  }

  close(client_fd);
}

static int create_server_socket(int port) {
  int fd, optval = 1;
  struct sockaddr_in addr;

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("socket");
    return -1;
  }

  // Set socket options
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
    perror("setsockopt(SO_REUSEADDR)");
    close(fd);
    return -1;
  }

#ifdef SO_REUSEPORT
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
    perror("setsockopt(SO_REUSEPORT)");
    close(fd);
    return -1;
  }
#endif

  // Bind to address
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  //addr.sin_port = (in_port_t)(htons((uint16_t)port));
  addr.sin_port = htons(port);

  if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(fd);
    return -1;
  }

  // Listen
  if (listen(fd, BACKLOG) < 0) {
    perror("listen");
    close(fd);
    return -1;
  }

  printf("Server listening on http://0.0.0.0:%d\n", port);
  return fd;
}

// Thread worker function: each thread accepts and handles clients
static void* thread_worker(void* arg) {
  thread_context_t* ctx = (thread_context_t*)arg;

  printf("Thread %d started in worker PID %d\n", ctx->thread_index, getpid());

  ctx->active = 1;

  while (!g_shutdown && ctx->active) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Direct accept (SO_REUSEPORT allows kernel load balancing)
    int client_fd = accept(ctx->server_fd, (struct sockaddr*)&client_addr, &client_len);

    if (client_fd < 0) {
      if (errno == EINTR || errno == EAGAIN) continue;
      if (g_shutdown) break;
      perror("accept");
      continue;
    }

    // Handle client with coroutine scheduler (currently synchronous)
    coroutine_handle_client(client_fd, ctx->config);
  }

  printf("Thread %d exiting from worker PID %d\n", ctx->thread_index, getpid());
  return NULL;
}

// Worker process: creates thread pool
static void worker_process(int server_fd) {
  printf("Worker %d started with %d threads\n", getpid(), g_config.threads_per_process);

  thread_context_t* threads = malloc(sizeof(thread_context_t) * g_config.threads_per_process);
  if (!threads) {
    perror("malloc");
    exit(1);
  }

  // Create thread pool
  for (int i = 0; i < g_config.threads_per_process; i++) {
    threads[i].thread_index = i;
    threads[i].server_fd = server_fd;
    threads[i].config = &g_config;
    threads[i].active = 0;

    if (pthread_create(&threads[i].thread_id, NULL, thread_worker, &threads[i]) != 0) {
      perror("pthread_create");
      continue;
    }
  }

  // Wait for shutdown signal
  while (!g_shutdown) {
    usleep(100000);  // Sleep 100ms
  }

  // Shutdown: mark threads inactive and wait for them
  printf("Worker %d shutting down threads...\n", getpid());
  for (int i = 0; i < g_config.threads_per_process; i++) {
    threads[i].active = 0;
  }

  for (int i = 0; i < g_config.threads_per_process; i++) {
    pthread_join(threads[i].thread_id, NULL);
  }

  free(threads);
  printf("Worker %d exiting\n", getpid());
  exit(0);
}

static void event_loop(int server_fd) {
  // Fork worker processes
  for (int i = 0; i < g_config.num_processes; i++) {
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      continue;
    }
    if (pid == 0) {
      // Child process
      worker_process(server_fd);
      exit(0);  // Should never reach here
    }
    g_workers[i] = pid;
    printf("Spawned worker %d (PID: %d)\n", i, pid);
  }

  printf("Server running with %d workers × %d threads = %d total workers. Press Ctrl+C to stop.\n",
         g_config.num_processes, g_config.threads_per_process,
         g_config.num_processes * g_config.threads_per_process);

  // Master process: wait for workers
  while (!g_shutdown) {
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);

    if (pid > 0) {
      printf("Worker %d exited with status %d\n", pid, WEXITSTATUS(status));

      // Restart worker if not shutting down
      if (!g_shutdown) {
        for (int i = 0; i < g_config.num_processes; i++) {
          if (g_workers[i] == pid) {
            pid_t new_pid = fork();
            if (new_pid == 0) {
              worker_process(server_fd);
              exit(0);
            }
            g_workers[i] = new_pid;
            printf("Respawned worker %d (PID: %d)\n", i, new_pid);
            break;
          }
        }
      }
    }

    usleep(100000);  // Sleep 100ms
  }

  // Shutdown: kill all workers
  printf("Shutting down workers...\n");
  for (int i = 0; i < g_config.num_processes; i++) {
    if (g_workers[i] > 0) {
      kill(g_workers[i], SIGTERM);
    }
  }

  // Wait for all workers to exit
  for (int i = 0; i < g_config.num_processes; i++) {
    if (g_workers[i] > 0) {
      waitpid(g_workers[i], NULL, 0);
    }
  }
}

static void print_usage(const char* prog) {
  printf("Usage: %s [OPTIONS]\n\n", prog);
  printf("Configurable Multi-Process × Multi-Thread × Coroutine Web Server\n\n");
  printf("Configuration via environment variables:\n");
  printf("  WEBSERVER_PORT         Port to listen on (default: %d)\n", DEFAULT_PORT);
  printf("  WEBSERVER_PROCESSES    Number of worker processes (p, default: 2)\n");
  printf("  WEBSERVER_THREADS      Threads per process (t, default: 4)\n");
  printf("  WEBSERVER_COROUTINES   Max coroutines per thread (c, default: 100)\n");
  printf("\nArchitecture:\n");
  printf("  Total concurrency = p × t × c\n");
  printf("  Example: 2 processes × 4 threads × 100 coroutines = 800 concurrent requests\n");
  printf("\nExamples:\n");
  printf("  # Default: 2 processes × 4 threads\n");
  printf("  %s\n\n", prog);
  printf("  # Custom: 4 processes × 8 threads × 200 coroutines\n");
  printf("  WEBSERVER_PROCESSES=4 WEBSERVER_THREADS=8 WEBSERVER_COROUTINES=200 %s\n\n", prog);
  printf("Note: Coroutine support is currently a placeholder (synchronous handling).\n");
  printf("      Future versions will implement true coroutine scheduling.\n");
}


int main(int argc, char* argv[]) {
  const char* env_val;

  // Check for help flag
  if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
    print_usage(argv[0]);
    return 0;
  }

  // Initialize configuration with defaults
  g_config.port = DEFAULT_PORT;
  g_config.num_processes = 2;                    // p: 2 processes
  g_config.threads_per_process = 4;              // t: 4 threads per process
  g_config.max_coroutines = 100;                 // c: 100 max coroutines per thread
  g_config.backlog = BACKLOG;

  // Parse environment variables
  env_val = getenv("WEBSERVER_PORT");
  if (env_val) {
    g_config.port = atoi(env_val);
    if (g_config.port <= 0 || g_config.port > 65535) {
      fprintf(stderr, "Invalid WEBSERVER_PORT: %s\n", env_val);
      return 1;
    }
  }

  env_val = getenv("WEBSERVER_PROCESSES");
  if (env_val) {
    g_config.num_processes = atoi(env_val);
    if (g_config.num_processes <= 0 || g_config.num_processes > MAX_WORKERS) {
      fprintf(stderr, "Invalid WEBSERVER_PROCESSES: %s (max: %d)\n",
              env_val, MAX_WORKERS);
      return 1;
    }
  }

  env_val = getenv("WEBSERVER_THREADS");
  if (env_val) {
    g_config.threads_per_process = atoi(env_val);
    if (g_config.threads_per_process <= 0 ||
        g_config.threads_per_process > MAX_THREADS_PER_WORKER) {
      fprintf(stderr, "Invalid WEBSERVER_THREADS: %s (max: %d)\n",
              env_val, MAX_THREADS_PER_WORKER);
      return 1;
    }
  }

  env_val = getenv("WEBSERVER_COROUTINES");
  if (env_val) {
    g_config.max_coroutines = atoi(env_val);
    if (g_config.max_coroutines <= 0 ||
        g_config.max_coroutines > MAX_COROUTINES_PER_THREAD) {
      fprintf(stderr, "Invalid WEBSERVER_COROUTINES: %s (max: %d)\n",
              env_val, MAX_COROUTINES_PER_THREAD);
      return 1;
    }
  }

  printf("=== Cosmorun WebServer Configuration ===\n");
  printf("Port:               %d\n", g_config.port);
  printf("Processes (p):      %d\n", g_config.num_processes);
  printf("Threads/proc (t):   %d\n", g_config.threads_per_process);
  printf("Max coroutines (c): %d\n", g_config.max_coroutines);
  printf("Total workers:      %d (p × t)\n",
         g_config.num_processes * g_config.threads_per_process);
  printf("Max concurrency:    %d (p × t × c, when coroutines enabled)\n",
         g_config.num_processes * g_config.threads_per_process * g_config.max_coroutines);
  printf("========================================\n\n");

  setup_signals();

  g_server_fd = create_server_socket(g_config.port);
  if (g_server_fd < 0) {
    fprintf(stderr, "Failed to create server socket\n");
    return 1;
  }

  event_loop(g_server_fd);

  close(g_server_fd);
  printf("\nServer shutdown complete\n");

  return 0;
}
