/*
 * mod_async.c - Async I/O Module Implementation
 *
 * CosmoRun v2: Migrated to use System Layer API (shim layer) instead of direct libc calls
 */

#include "mod_async.h"

/* CosmoRun v2: Include System Layer libc shim for memory and string operations */
#include "../cosmorun_system/libc_shim/sys_libc_shim.h"

/* mod_ds.h is included separately - either via mod_ds.c or as header */
#ifndef DS_LIST_H_INCLUDED
#include "mod_ds.h"
#endif

/* Platform-specific event mechanisms */
#ifndef __COSMORUN__
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

#ifdef __linux__
#include <sys/epoll.h>
#define USE_EPOLL 1
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/event.h>
#define USE_KQUEUE 1
#endif
#endif /* !__COSMORUN__ */

/* Use select() in cosmorun environment */
#ifdef __COSMORUN__
/* All types are already defined in cosmo_libc.h */

/* Cosmopolitan libc has broken O_* constants that don't match Linux at runtime.
 * Define correct POSIX O_* constants for file operations. These values are
 * standard across Linux/BSD/macOS. We must use these instead of <fcntl.h>. */
#undef O_RDONLY
#undef O_WRONLY
#undef O_RDWR
#undef O_CREAT
#undef O_EXCL
#undef O_NOCTTY
#undef O_TRUNC
#undef O_APPEND
#undef O_NONBLOCK

#define O_RDONLY    0000000   /* Open for reading only */
#define O_WRONLY    0000001   /* Open for writing only */
#define O_RDWR      0000002   /* Open for reading and writing */
#define O_CREAT     0000100   /* Create file if it does not exist (64 decimal) */
#define O_EXCL      0000200   /* Exclusive use flag (128 decimal) */
#define O_NOCTTY    0000400   /* Do not assign controlling terminal (256 decimal) */
#define O_TRUNC     0001000   /* Truncate file to zero length (512 decimal) */
#define O_APPEND    0002000   /* Set append mode (1024 decimal) */
#define O_NONBLOCK  0004000   /* No delay (2048 decimal) */

/* Add missing fcntl constants for CosmoRun environment */
#ifndef F_GETFL
#define F_GETFL 3   /* Get file descriptor flags */
#endif
#ifndef F_SETFL
#define F_SETFL 4   /* Set file descriptor flags */
#endif

/* Add missing errno values */
#ifndef EHOSTUNREACH
#define EHOSTUNREACH 113
#endif
#ifndef ECONNRESET
#define ECONNRESET 104
#endif
#ifndef EINPROGRESS
#define EINPROGRESS 115
#endif
#ifndef ENOMEM
#define ENOMEM 12   /* Out of memory */
#endif
/* Add missing socket options */
#ifndef SO_ERROR
#define SO_ERROR 4
#endif
#endif

/* Use Cosmopolitan's cross-platform stat() - works on Linux, macOS, Windows
 * Cosmopolitan provides a unified struct stat ABI across all platforms */
#ifdef __COSMORUN__
#define cosmo_stat stat
#endif

/* ==================== Constants ==================== */

#define MAX_EVENTS 64
#define BUFFER_SIZE 8192
#define MAX_TIMERS 1024

/* ==================== Internal Structures ==================== */

typedef enum {
    HANDLE_FILE_READ,
    HANDLE_FILE_WRITE,
    HANDLE_FILE_STAT,
    HANDLE_TCP_CONNECT,
    HANDLE_TCP_SERVER,
    HANDLE_SOCKET_READ,
    HANDLE_SOCKET_WRITE
} async_handle_type_t;

struct async_handle {
    async_handle_type_t type;
    int fd;
    bool active;
    async_loop_t *loop;

    /* Callbacks and user data */
    union {
        async_read_cb read_cb;
        async_write_cb write_cb;
        async_stat_cb stat_cb;
        async_connect_cb connect_cb;
        async_accept_cb accept_cb;
        async_socket_cb socket_cb;
    } callback;
    void *udata;

    /* File I/O state */
    char *file_path;
    char *buffer;
    int buffer_len;
    int bytes_transferred;

    /* Network state */
    char *host;
    int port;
};

struct async_timer {
    uint64_t expire_time;  /* Absolute time in ms */
    int interval_ms;       /* 0 for one-shot, >0 for repeating */
    async_timer_cb callback;
    void *udata;
    bool active;
    async_loop_t *loop;
};

struct async_loop {
    bool running;
    bool should_stop;

#ifdef USE_EPOLL
    int epoll_fd;
#elif defined(USE_KQUEUE)
    int kqueue_fd;
#endif

    /* Active handles */
    ds_list_t *handles;

    /* Timers */
    async_timer_t *timers[MAX_TIMERS];
    int timer_count;
};

/* ==================== Helper Functions ==================== */

static uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void handle_free(async_handle_t *handle) {
    if (!handle) return;

    if (handle->fd >= 0) {
        close(handle->fd);
    }
    /* CosmoRun v2: Use free instead of free */
    if (handle->file_path) shim_free(handle->file_path);
    if (handle->buffer) shim_free(handle->buffer);
    if (handle->host) shim_free(handle->host);
    shim_free(handle);
}

/* ==================== Event Loop ==================== */

async_loop_t* async_loop_new(void) {
    /* CosmoRun v2: Use calloc instead of calloc */
    async_loop_t *loop = shim_calloc(1, sizeof(async_loop_t));
    if (!loop) return NULL;

    loop->handles = ds_list_new();
    if (!loop->handles) {
        /* CosmoRun v2: Use free instead of free */
        shim_free(loop);
        return NULL;
    }

#ifdef USE_EPOLL
    loop->epoll_fd = epoll_create1(0);
    if (loop->epoll_fd == -1) {
        ds_list_free(loop->handles);
        /* CosmoRun v2: Use free instead of free */
        shim_free(loop);
        return NULL;
    }
#elif defined(USE_KQUEUE)
    loop->kqueue_fd = kqueue();
    if (loop->kqueue_fd == -1) {
        ds_list_free(loop->handles);
        /* CosmoRun v2: Use free instead of free */
        shim_free(loop);
        return NULL;
    }
#endif

    return loop;
}

void async_loop_free(async_loop_t *loop) {
    if (!loop) return;

#ifdef USE_EPOLL
    if (loop->epoll_fd >= 0) {
        close(loop->epoll_fd);
    }
#elif defined(USE_KQUEUE)
    if (loop->kqueue_fd >= 0) {
        close(loop->kqueue_fd);
    }
#endif

    /* Free all handles */
    if (loop->handles) {
        for (size_t i = 0; i < ds_list_size(loop->handles); i++) {
            async_handle_t *handle = ds_list_get(loop->handles, i);
            handle_free(handle);
        }
        ds_list_free(loop->handles);
    }

    /* Free all timers - CosmoRun v2: Use free instead of free */
    for (int i = 0; i < loop->timer_count; i++) {
        if (loop->timers[i]) {
            shim_free(loop->timers[i]);
        }
    }

    /* CosmoRun v2: Use free instead of free */
    shim_free(loop);
}

void async_loop_stop(async_loop_t *loop) {
    if (loop) {
        loop->should_stop = true;
    }
}

/* Process timers and return ms until next timer */
static int process_timers(async_loop_t *loop) {
    uint64_t now = get_time_ms();
    int next_timeout = -1;

    for (int i = 0; i < loop->timer_count; i++) {
        async_timer_t *timer = loop->timers[i];
        if (!timer || !timer->active) continue;

        if (now >= timer->expire_time) {
            /* Timer fired */
            if (timer->callback) {
                timer->callback(timer, timer->udata);
            }

            if (timer->interval_ms > 0) {
                /* Repeating timer - reschedule */
                timer->expire_time = now + timer->interval_ms;
            } else {
                /* One-shot timer - mark inactive */
                timer->active = false;
            }
        }

        /* Calculate next timeout */
        if (timer->active) {
            int ms_until = (int)(timer->expire_time - now);
            if (ms_until < 0) ms_until = 0;
            if (next_timeout == -1 || ms_until < next_timeout) {
                next_timeout = ms_until;
            }
        }
    }

    return next_timeout;
}

/* ==================== I/O Processing Functions (Common) ==================== */

static void process_file_read(async_handle_t *handle) {
    if (!handle || !handle->callback.read_cb) return;


    /* Read until EOF or error */
    while (1) {
        char buffer[BUFFER_SIZE];
        ssize_t n = read(handle->fd, buffer, sizeof(buffer));


        if (n > 0) {
            /* Append to buffer */
            char *new_buf = shim_realloc(handle->buffer, handle->buffer_len + n);
            if (!new_buf) {
                handle->callback.read_cb(NULL, 0, -ENOMEM, handle->udata);
                handle->active = false;
                return;
            }
            handle->buffer = new_buf;
            shim_memcpy(handle->buffer + handle->buffer_len, buffer, n);
            handle->buffer_len += n;
            /* Continue reading */
        } else if (n == 0) {
            /* EOF - deliver result, transfer ownership of buffer to callback */
            handle->callback.read_cb(handle->buffer, handle->buffer_len, 0, handle->udata);
            handle->buffer = NULL;  /* Callback now owns the buffer */
            handle->active = false;
            break;
        } else {
            /* Error or EAGAIN */
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* No more data available now, continue in next iteration */
                break;
            } else {
                /* Real error */
                handle->callback.read_cb(NULL, 0, -errno, handle->udata);
                handle->active = false;
                break;
            }
        }
    }
}

static void process_file_write(async_handle_t *handle) {
    if (!handle || !handle->callback.write_cb) return;

    /* Write as much as possible */
    while (handle->bytes_transferred < handle->buffer_len) {
        ssize_t n = write(handle->fd,
                          handle->buffer + handle->bytes_transferred,
                          handle->buffer_len - handle->bytes_transferred);

        if (n > 0) {
            handle->bytes_transferred += n;
        } else if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* Can't write more now, continue later */
                break;
            } else {
                /* Error */
                handle->callback.write_cb(-errno, handle->udata);
                handle->active = false;
                return;
            }
        } else {
            /* n == 0, shouldn't happen for write */
            break;
        }
    }

    /* Check if write is complete */
    if (handle->bytes_transferred >= handle->buffer_len) {
        /* Write complete */
        handle->callback.write_cb(0, handle->udata);
        handle->active = false;
    }
}

static void process_tcp_connect(async_handle_t *handle) {
    if (!handle || !handle->callback.connect_cb) return;

    int error = 0;
    socklen_t len = sizeof(error);

    if (getsockopt(handle->fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
        error = errno;
    }

    if (error == 0) {
        /* Connection succeeded */
        handle->callback.connect_cb(handle->fd, 0, handle->udata);
        handle->fd = -1;  /* Transfer ownership */
    } else {
        /* Connection failed */
        handle->callback.connect_cb(-1, -error, handle->udata);
    }

    handle->active = false;
}

static void process_tcp_accept(async_handle_t *handle) {
    if (!handle || !handle->callback.accept_cb) return;

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = accept(handle->fd, (struct sockaddr*)&client_addr, &addr_len);

    if (client_fd >= 0) {
        set_nonblocking(client_fd);
        handle->callback.accept_cb(client_fd, handle->udata);
    }
    /* Accept errors are non-fatal for server */
}

static void process_socket_read(async_handle_t *handle) {
    if (!handle || !handle->callback.socket_cb) return;

    char buffer[BUFFER_SIZE];
    ssize_t n = recv(handle->fd, buffer, sizeof(buffer), 0);

    if (n > 0) {
        handle->callback.socket_cb(buffer, n, 0, handle->udata);
    } else if (n == 0) {
        /* Connection closed */
        handle->callback.socket_cb(NULL, 0, -ECONNRESET, handle->udata);
        handle->active = false;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        /* Error */
        handle->callback.socket_cb(NULL, 0, -errno, handle->udata);
        handle->active = false;
    }
}

static void process_socket_write(async_handle_t *handle) {
    if (!handle || !handle->callback.write_cb) return;

    ssize_t n = send(handle->fd,
                     handle->buffer + handle->bytes_transferred,
                     handle->buffer_len - handle->bytes_transferred, 0);

    if (n > 0) {
        handle->bytes_transferred += n;

        if (handle->bytes_transferred >= handle->buffer_len) {
            /* Write complete */
            handle->callback.write_cb(0, handle->udata);
            handle->active = false;
        }
    } else if (n == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        /* Error */
        handle->callback.write_cb(-errno, handle->udata);
        handle->active = false;
    }
}

/* ==================== Platform-Specific Event Loop ==================== */

#ifdef USE_EPOLL

static int add_to_epoll(async_loop_t *loop, async_handle_t *handle, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = handle;
    return epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, handle->fd, &ev);
}

int async_loop_run(async_loop_t *loop) {
    if (!loop) return -1;

    loop->running = true;
    /* Preserve should_stop if already set, but still run at least once */
    int was_stopped = loop->should_stop;

    struct epoll_event events[MAX_EVENTS];

    do {
        /* Process timers and get timeout */
        int timeout_ms = process_timers(loop);

        /* No active handles or timers - exit */
        if (ds_list_size(loop->handles) == 0 && timeout_ms == -1) {
            break;
        }

        /* Wait for events */
        int nfds = epoll_wait(loop->epoll_fd, events, MAX_EVENTS, timeout_ms);

        if (nfds == -1) {
            if (errno == EINTR) continue;
            loop->running = false;
            return -1;
        }

        /* Process events */
        for (int i = 0; i < nfds; i++) {
            async_handle_t *handle = events[i].data.ptr;
            if (!handle || !handle->active) continue;

            uint32_t evt = events[i].events;

            /* Error conditions */
            if (evt & (EPOLLERR | EPOLLHUP)) {
                if (handle->type == HANDLE_FILE_READ) {
                    if (handle->callback.read_cb) {
                        handle->callback.read_cb(NULL, 0, -EIO, handle->udata);
                    }
                } else if (handle->type == HANDLE_SOCKET_READ) {
                    if (handle->callback.socket_cb) {
                        handle->callback.socket_cb(NULL, 0, -EIO, handle->udata);
                    }
                } else if (handle->type == HANDLE_FILE_WRITE ||
                           handle->type == HANDLE_SOCKET_WRITE) {
                    if (handle->callback.write_cb) {
                        handle->callback.write_cb(-EIO, handle->udata);
                    }
                } else if (handle->type == HANDLE_TCP_CONNECT) {
                    if (handle->callback.connect_cb) {
                        handle->callback.connect_cb(-1, -EIO, handle->udata);
                    }
                }
                handle->active = false;
                continue;
            }

            /* Read events */
            if (evt & EPOLLIN) {
                switch (handle->type) {
                    case HANDLE_FILE_READ:
                        process_file_read(handle);
                        break;
                    case HANDLE_TCP_SERVER:
                        process_tcp_accept(handle);
                        break;
                    case HANDLE_SOCKET_READ:
                        process_socket_read(handle);
                        break;
                    default:
                        break;
                }
            }

            /* Write events */
            if (evt & EPOLLOUT) {
                switch (handle->type) {
                    case HANDLE_FILE_WRITE:
                        process_file_write(handle);
                        break;
                    case HANDLE_TCP_CONNECT:
                        process_tcp_connect(handle);
                        break;
                    case HANDLE_SOCKET_WRITE:
                        process_socket_write(handle);
                        break;
                    default:
                        break;
                }
            }
        }

        /* Remove inactive handles */
        for (int i = ds_list_size(loop->handles) - 1; i >= 0; i--) {
            async_handle_t *handle = ds_list_get(loop->handles, i);
            if (!handle->active) {
                ds_list_remove(loop->handles, i);
                handle_free(handle);
            }
        }
    } while (loop->running && !loop->should_stop);

    loop->running = false;
    return 0;
}

#else /* !USE_EPOLL - Use select() fallback */

int async_loop_run(async_loop_t *loop) {
    if (!loop) return -1;

    loop->running = true;
    /* Preserve should_stop if already set, but still run at least once */
    int was_stopped = loop->should_stop;

    do {
        /* Process timers and get timeout */
        int timeout_ms = process_timers(loop);


        /* No active handles or timers - exit */
        if (ds_list_size(loop->handles) == 0 && timeout_ms == -1) {
            break;
        }

        /* Build fd_sets */
        fd_set readfds, writefds, exceptfds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);

        int maxfd = -1;

        for (size_t i = 0; i < ds_list_size(loop->handles); i++) {
            async_handle_t *handle = ds_list_get(loop->handles, i);
            if (!handle || !handle->active || handle->fd < 0) continue;

            if (handle->fd > maxfd) maxfd = handle->fd;

            switch (handle->type) {
                case HANDLE_FILE_READ:
                case HANDLE_TCP_SERVER:
                case HANDLE_SOCKET_READ:
                    FD_SET(handle->fd, &readfds);
                    break;
                case HANDLE_FILE_WRITE:
                case HANDLE_TCP_CONNECT:
                case HANDLE_SOCKET_WRITE:
                    FD_SET(handle->fd, &writefds);
                    break;
                default:
                    break;
            }

            /* Only monitor exceptfds for network sockets, not regular files */
            if (handle->type != HANDLE_FILE_READ && handle->type != HANDLE_FILE_WRITE) {
                FD_SET(handle->fd, &exceptfds);
            }
        }

        if (maxfd == -1 && timeout_ms == -1) {
            break;
        }

        /* Set timeout */
        struct timeval tv;
        struct timeval *tvp = NULL;
        if (timeout_ms >= 0) {
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            tvp = &tv;
        }

        /* Wait for events */
        int nfds = select(maxfd + 1, &readfds, &writefds, &exceptfds, tvp);


        if (nfds == -1) {
            if (errno == EINTR) continue;
            loop->running = false;
            return -1;
        }

        /* Process events */
        for (size_t i = 0; i < ds_list_size(loop->handles); i++) {
            async_handle_t *handle = ds_list_get(loop->handles, i);
            if (!handle || !handle->active || handle->fd < 0) continue;

            /* Check for errors */
            int is_except = FD_ISSET(handle->fd, &exceptfds);
            if (is_except) {
                if (handle->type == HANDLE_FILE_READ || handle->type == HANDLE_SOCKET_READ) {
                    if (handle->callback.read_cb) {
                        handle->callback.read_cb(NULL, 0, -EIO, handle->udata);
                    }
                } else if (handle->type == HANDLE_FILE_WRITE ||
                           handle->type == HANDLE_SOCKET_WRITE) {
                    if (handle->callback.write_cb) {
                        handle->callback.write_cb(-EIO, handle->udata);
                    }
                } else if (handle->type == HANDLE_TCP_CONNECT) {
                    if (handle->callback.connect_cb) {
                        handle->callback.connect_cb(-1, -EIO, handle->udata);
                    }
                }
                handle->active = false;
                continue;
            }

            /* Check for read events */
            int is_read_ready = FD_ISSET(handle->fd, &readfds);
            if (is_read_ready) {
                switch (handle->type) {
                    case HANDLE_FILE_READ:
                        process_file_read(handle);
                        break;
                    case HANDLE_TCP_SERVER:
                        process_tcp_accept(handle);
                        break;
                    case HANDLE_SOCKET_READ:
                        process_socket_read(handle);
                        break;
                    default:
                        break;
                }
            }

            /* Check for write events */
            if (FD_ISSET(handle->fd, &writefds)) {
                switch (handle->type) {
                    case HANDLE_FILE_WRITE:
                        process_file_write(handle);
                        break;
                    case HANDLE_TCP_CONNECT:
                        process_tcp_connect(handle);
                        break;
                    case HANDLE_SOCKET_WRITE:
                        process_socket_write(handle);
                        break;
                    default:
                        break;
                }
            }
        }

        /* Remove inactive handles */
        for (int i = ds_list_size(loop->handles) - 1; i >= 0; i--) {
            async_handle_t *handle = ds_list_get(loop->handles, i);
            if (!handle->active) {
                ds_list_remove(loop->handles, i);
                handle_free(handle);
            }
        }
    } while (loop->running && !loop->should_stop);

    loop->running = false;
    return 0;
}

#endif /* USE_EPOLL */

/* ==================== Async File I/O ==================== */

async_handle_t* async_read_file(async_loop_t *loop, const char *path,
                                  async_read_cb callback, void *udata) {
    if (!loop || !path || !callback) return NULL;


    int fd = open(path, O_RDONLY | O_NONBLOCK);

    if (fd == -1) {
        callback(NULL, 0, -errno, udata);
        return NULL;
    }

    async_handle_t *handle = shim_calloc(1, sizeof(async_handle_t));
    if (!handle) {
        close(fd);
        callback(NULL, 0, -ENOMEM, udata);
        return NULL;
    }

    handle->type = HANDLE_FILE_READ;
    handle->fd = fd;
    handle->active = true;
    handle->loop = loop;
    handle->callback.read_cb = callback;
    handle->udata = udata;
    handle->file_path = shim_strdup(path);

#ifdef USE_EPOLL
    if (add_to_epoll(loop, handle, EPOLLIN | EPOLLET) == -1) {
        handle_free(handle);
        callback(NULL, 0, -errno, udata);
        return NULL;
    }
#endif

    ds_list_push(loop->handles, handle);
    return handle;
}

async_handle_t* async_write_file(async_loop_t *loop, const char *path,
                                   const char *data, int len,
                                   async_write_cb callback, void *udata) {
    if (!loop || !path || !data || len <= 0 || !callback) return NULL;

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK, 0644);
    if (fd == -1) {
        callback(-errno, udata);
        return NULL;
    }

    async_handle_t *handle = shim_calloc(1, sizeof(async_handle_t));
    if (!handle) {
        close(fd);
        callback(-ENOMEM, udata);
        return NULL;
    }

    handle->type = HANDLE_FILE_WRITE;
    handle->fd = fd;
    handle->active = true;
    handle->loop = loop;
    handle->callback.write_cb = callback;
    handle->udata = udata;
    handle->file_path = shim_strdup(path);
    handle->buffer = shim_malloc(len);
    if (!handle->buffer) {
        handle_free(handle);
        callback(-ENOMEM, udata);
        return NULL;
    }
    shim_memcpy(handle->buffer, data, len);
    handle->buffer_len = len;
    handle->bytes_transferred = 0;

#ifdef USE_EPOLL
    if (add_to_epoll(loop, handle, EPOLLOUT | EPOLLET) == -1) {
        handle_free(handle);
        callback(-errno, udata);
        return NULL;
    }
#endif

    ds_list_push(loop->handles, handle);
    return handle;
}

async_handle_t* async_stat_file(async_loop_t *loop, const char *path,
                                  async_stat_cb callback, void *udata) {
    if (!loop || !path || !callback) return NULL;

    /* stat is typically fast, so we do it synchronously */
    struct stat st;
    shim_memset(&st, 0, sizeof(st));  /* Clear struct before syscall */

    int result = cosmo_stat(path, &st);
    if (result == -1) {
        int err = errno;
        callback(NULL, -err, udata);
        return NULL;
    }

    /* Call callback with stack pointer - callback must copy if needed */
    callback(&st, 0, udata);
    return NULL;  /* No handle needed for synchronous operation */
}

/* ==================== Async Network I/O ==================== */

async_handle_t* async_tcp_connect(async_loop_t *loop, const char *host,
                                    int port, async_connect_cb callback,
                                    void *udata) {
    if (!loop || !host || port <= 0 || !callback) return NULL;

    /* Create socket */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        callback(-1, -errno, udata);
        return NULL;
    }

    set_nonblocking(fd);

    /* Setup address - use inet_pton for IP addresses (avoids gethostbyname issues) */
    struct sockaddr_in addr;
    shim_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    /* Convert IP address string to binary form */
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        /* If not a valid IP, try as hostname - but this may hang on some platforms */
        struct hostent *he = gethostbyname(host);
        if (!he) {
            close(fd);
            callback(-1, -EHOSTUNREACH, udata);
            return NULL;
        }
        shim_memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    int result = connect(fd, (struct sockaddr*)&addr, sizeof(addr));

    /* Create handle for all cases (immediate success, in progress, or failure) */
    async_handle_t *handle = shim_calloc(1, sizeof(async_handle_t));
    if (!handle) {
        close(fd);
        callback(-1, -ENOMEM, udata);
        return NULL;
    }

    handle->type = HANDLE_TCP_CONNECT;
    handle->fd = fd;
    handle->loop = loop;
    handle->callback.connect_cb = callback;
    handle->udata = udata;
    handle->host = shim_strdup(host);
    handle->port = port;

    if (result == 0) {
        /* Immediate connection (localhost fast path) */
        handle->active = false;  /* Mark as completed */
        ds_list_push(loop->handles, handle);
        callback(fd, 0, udata);
        handle->fd = -1;  /* Transfer ownership - prevent double close */
        return handle;
    } else if (errno != EINPROGRESS) {
        /* Connection failed */
        int err = errno;
        handle->active = false;
        ds_list_push(loop->handles, handle);
        close(fd);
        callback(-1, -err, udata);
        return handle;
    }

    /* Connection in progress */
    handle->active = true;

#ifdef USE_EPOLL
    if (add_to_epoll(loop, handle, EPOLLOUT | EPOLLET) == -1) {
        handle_free(handle);
        callback(-1, -errno, udata);
        return NULL;
    }
#endif

    ds_list_push(loop->handles, handle);
    return handle;
}

async_handle_t* async_tcp_server(async_loop_t *loop, int port,
                                   async_accept_cb callback, void *udata) {
    if (!loop || port <= 0 || !callback) {
        return NULL;
    }

    /* Create socket */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        return NULL;
    }

    /* Set socket options */
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    #ifdef SO_REUSEPORT
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    #endif

    set_nonblocking(fd);

    /* Bind */
    struct sockaddr_in addr;
    shim_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(fd);
        return NULL;
    }

    /* Listen */
    if (listen(fd, 128) == -1) {
        close(fd);
        return NULL;
    }

    /* Create handle */
    async_handle_t *handle = shim_calloc(1, sizeof(async_handle_t));
    if (!handle) {
        close(fd);
        return NULL;
    }

    handle->type = HANDLE_TCP_SERVER;
    handle->fd = fd;
    handle->active = true;
    handle->loop = loop;
    handle->callback.accept_cb = callback;
    handle->udata = udata;
    handle->port = port;

#ifdef USE_EPOLL
    if (add_to_epoll(loop, handle, EPOLLIN | EPOLLET) == -1) {
        handle_free(handle);
        return NULL;
    }
#endif

    ds_list_push(loop->handles, handle);
    return handle;
}

async_handle_t* async_socket_read(async_loop_t *loop, int fd,
                                    async_socket_cb callback, void *udata) {
    if (!loop || fd < 0 || !callback) return NULL;

    set_nonblocking(fd);

    async_handle_t *handle = shim_calloc(1, sizeof(async_handle_t));
    if (!handle) {
        callback(NULL, 0, -ENOMEM, udata);
        return NULL;
    }

    handle->type = HANDLE_SOCKET_READ;
    handle->fd = fd;
    handle->active = true;
    handle->loop = loop;
    handle->callback.socket_cb = callback;
    handle->udata = udata;

#ifdef USE_EPOLL
    if (add_to_epoll(loop, handle, EPOLLIN | EPOLLET) == -1) {
        handle_free(handle);
        callback(NULL, 0, -errno, udata);
        return NULL;
    }
#endif

    ds_list_push(loop->handles, handle);
    return handle;
}

async_handle_t* async_socket_write(async_loop_t *loop, int fd,
                                     const char *data, int len,
                                     async_write_cb callback, void *udata) {
    if (!loop || fd < 0 || !data || len <= 0 || !callback) return NULL;

    set_nonblocking(fd);

    async_handle_t *handle = shim_calloc(1, sizeof(async_handle_t));
    if (!handle) {
        callback(-ENOMEM, udata);
        return NULL;
    }

    handle->type = HANDLE_SOCKET_WRITE;
    handle->fd = fd;
    handle->active = true;
    handle->loop = loop;
    handle->callback.write_cb = callback;
    handle->udata = udata;
    handle->buffer = shim_malloc(len);
    if (!handle->buffer) {
        shim_free(handle);
        callback(-ENOMEM, udata);
        return NULL;
    }
    shim_memcpy(handle->buffer, data, len);
    handle->buffer_len = len;
    handle->bytes_transferred = 0;

    /* Try immediate send for non-blocking sockets */
    ssize_t n = send(fd, handle->buffer, handle->buffer_len, 0);
    if (n > 0) {
        handle->bytes_transferred = n;
        if (handle->bytes_transferred >= handle->buffer_len) {
            /* Write completed immediately - call callback and mark inactive */
            callback(0, udata);
            handle->active = false;
            /* Don't free here - will be freed when loop processes inactive handles */
        }
    } else if (n == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        /* Immediate error */
        callback(-errno, udata);
        handle->active = false;
    }

#ifdef USE_EPOLL
    if (add_to_epoll(loop, handle, EPOLLOUT | EPOLLET) == -1) {
        handle_free(handle);
        callback(-errno, udata);
        return NULL;
    }
#endif

    ds_list_push(loop->handles, handle);
    return handle;
}

/* ==================== Timers ==================== */

async_timer_t* async_timeout(async_loop_t *loop, int ms,
                               async_timer_cb callback, void *udata) {
    if (!loop || ms < 0 || !callback) return NULL;
    if (loop->timer_count >= MAX_TIMERS) return NULL;

    async_timer_t *timer = shim_calloc(1, sizeof(async_timer_t));
    if (!timer) return NULL;

    timer->expire_time = get_time_ms() + ms;
    timer->interval_ms = 0;  /* One-shot */
    timer->callback = callback;
    timer->udata = udata;
    timer->active = true;
    timer->loop = loop;

    loop->timers[loop->timer_count++] = timer;
    return timer;
}

async_timer_t* async_interval(async_loop_t *loop, int ms,
                                async_timer_cb callback, void *udata) {
    if (!loop || ms <= 0 || !callback) return NULL;
    if (loop->timer_count >= MAX_TIMERS) return NULL;

    async_timer_t *timer = shim_calloc(1, sizeof(async_timer_t));
    if (!timer) return NULL;

    timer->expire_time = get_time_ms() + ms;
    timer->interval_ms = ms;  /* Repeating */
    timer->callback = callback;
    timer->udata = udata;
    timer->active = true;
    timer->loop = loop;

    loop->timers[loop->timer_count++] = timer;
    return timer;
}

int async_timer_cancel(async_timer_t *timer) {
    if (!timer || !timer->active) return -1;
    timer->active = false;
    return 0;
}

/* ==================== Handle Management ==================== */

void async_handle_close(async_handle_t *handle) {
    if (handle) {
        handle->active = false;
    }
}

int async_handle_is_active(async_handle_t *handle) {
    return handle ? handle->active : 0;
}
