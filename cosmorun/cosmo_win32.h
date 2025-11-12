/*
 * cosmo_win32.h - Windows API mapping layer for CosmoRun
 *
 * Maps POSIX/Linux syscalls to Win32 API equivalents.
 * This is a research foundation for Windows native support.
 */

#ifndef COSMO_WIN32_H
#define COSMO_WIN32_H

#ifdef _WIN32

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

/* ==================== Process Management ==================== */

/* POSIX: fork() → Win32: CreateProcess()
 * NOTE: Win32 has no fork() equivalent with copy-on-write semantics.
 * Use CreateProcess() to spawn new process from executable.
 * Alternative: Use threads for concurrency instead of fork().
 */
#define fork() _fork_not_available_on_windows()

/* POSIX: execv/execl → Win32: CreateProcess()
 * execv family replaces current process image.
 * Win32 requires spawning new process and terminating current one.
 */
static inline int win32_execv(const char *path, char *const argv[]) {
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;

    /* Build command line from argv */
    char cmdline[32768] = {0};
    int pos = 0;
    for (int i = 0; argv[i]; i++) {
        if (i > 0) cmdline[pos++] = ' ';
        int len = strlen(argv[i]);
        memcpy(cmdline + pos, argv[i], len);
        pos += len;
    }

    if (!CreateProcessA(path, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        return -1;
    }

    /* Wait for child and exit with its code */
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    ExitProcess(exit_code);
    return 0; /* Never reached */
}

/* POSIX: wait/waitpid → Win32: WaitForSingleObject + GetExitCodeProcess */
static inline int win32_waitpid(HANDLE hProcess, int *status, int options) {
    DWORD timeout = (options & 1) ? 0 : INFINITE; /* WNOHANG = 1 */
    DWORD result = WaitForSingleObject(hProcess, timeout);

    if (result == WAIT_TIMEOUT) return 0;
    if (result == WAIT_FAILED) return -1;

    if (status) {
        DWORD exit_code;
        GetExitCodeProcess(hProcess, &exit_code);
        *status = exit_code << 8; /* Match POSIX WEXITSTATUS macro */
    }
    return (int)(intptr_t)hProcess;
}

/* POSIX: kill() → Win32: TerminateProcess() */
static inline int win32_kill(HANDLE hProcess, int sig) {
    /* Windows has no signals, map common signals to terminate */
    if (sig == 9 || sig == 15) { /* SIGKILL or SIGTERM */
        return TerminateProcess(hProcess, sig) ? 0 : -1;
    }
    return -1; /* Unsupported signal */
}

/* ==================== File I/O ==================== */

/* POSIX: open() → Win32: CreateFile()
 * Flags mapping: O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_TRUNC, O_APPEND
 */
static inline HANDLE win32_open(const char *path, int flags, int mode) {
    DWORD access = 0;
    DWORD creation = OPEN_EXISTING;
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE;

    /* Access mode */
    if (flags & 0x0002) access = GENERIC_READ | GENERIC_WRITE; /* O_RDWR */
    else if (flags & 0x0001) access = GENERIC_WRITE; /* O_WRONLY */
    else access = GENERIC_READ; /* O_RDONLY */

    /* Creation disposition */
    if (flags & 0x0100) { /* O_CREAT */
        creation = (flags & 0x0200) ? CREATE_NEW : OPEN_ALWAYS; /* O_EXCL */
    }
    if (flags & 0x1000) creation = TRUNCATE_EXISTING; /* O_TRUNC */

    HANDLE h = CreateFileA(path, access, share, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
    return (h == INVALID_HANDLE_VALUE) ? (HANDLE)-1 : h;
}

/* POSIX: read() → Win32: ReadFile() */
static inline int win32_read(HANDLE fd, void *buf, size_t count) {
    DWORD bytes_read;
    if (!ReadFile(fd, buf, (DWORD)count, &bytes_read, NULL)) return -1;
    return (int)bytes_read;
}

/* POSIX: write() → Win32: WriteFile() */
static inline int win32_write(HANDLE fd, const void *buf, size_t count) {
    DWORD bytes_written;
    if (!WriteFile(fd, buf, (DWORD)count, &bytes_written, NULL)) return -1;
    return (int)bytes_written;
}

/* POSIX: close() → Win32: CloseHandle() */
static inline int win32_close(HANDLE fd) {
    return CloseHandle(fd) ? 0 : -1;
}

/* POSIX: lseek() → Win32: SetFilePointer() */
static inline long win32_lseek(HANDLE fd, long offset, int whence) {
    DWORD move_method;
    switch (whence) {
        case 0: move_method = FILE_BEGIN; break;    /* SEEK_SET */
        case 1: move_method = FILE_CURRENT; break;  /* SEEK_CUR */
        case 2: move_method = FILE_END; break;      /* SEEK_END */
        default: return -1;
    }
    return SetFilePointer(fd, offset, NULL, move_method);
}

/* POSIX: stat/fstat → Win32: GetFileAttributesEx / GetFileInformationByHandle */
static inline int win32_stat(const char *path, struct _stat64 *buf) {
    return _stat64(path, buf);
}

static inline int win32_fstat(HANDLE fd, struct _stat64 *buf) {
    int fd_int = _open_osfhandle((intptr_t)fd, 0);
    if (fd_int == -1) return -1;
    return _fstat64(fd_int, buf);
}

/* ==================== Async I/O ==================== */

/* POSIX: select() → Win32: WaitForMultipleObjects() / select() for sockets
 * NOTE: Windows select() only works with sockets, not file handles.
 * For file handles, use overlapped I/O with WaitForMultipleObjects().
 */
static inline int win32_select(int nfds, fd_set *readfds, fd_set *writefds,
                                fd_set *exceptfds, struct timeval *timeout) {
    /* For sockets, native select() works */
    return select(nfds, readfds, writefds, exceptfds, timeout);
}

/* POSIX: epoll → Win32: I/O Completion Ports (IOCP)
 * epoll is Linux-specific high-performance event notification.
 * Windows equivalent: CreateIoCompletionPort() + GetQueuedCompletionStatus()
 */
static inline HANDLE win32_epoll_create(int size) {
    /* Create IOCP with specified concurrency */
    return CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, size);
}

static inline int win32_epoll_ctl(HANDLE iocp, int op, HANDLE fd, void *event) {
    /* Associate handle with IOCP */
    if (CreateIoCompletionPort(fd, iocp, (ULONG_PTR)event, 0) == NULL) {
        return -1;
    }
    return 0;
}

static inline int win32_epoll_wait(HANDLE iocp, void *events, int maxevents, int timeout) {
    DWORD bytes;
    ULONG_PTR key;
    LPOVERLAPPED overlapped;

    DWORD wait_ms = (timeout < 0) ? INFINITE : timeout;
    if (!GetQueuedCompletionStatus(iocp, &bytes, &key, &overlapped, wait_ms)) {
        return (overlapped == NULL) ? -1 : 1; /* Error or completion */
    }
    return 1; /* One event */
}

/* POSIX: poll() → Win32: WSAPoll() for sockets, or WaitForMultipleObjects() */
static inline int win32_poll(struct pollfd *fds, int nfds, int timeout) {
    /* Windows 10+ has WSAPoll for socket descriptors */
    return WSAPoll(fds, nfds, timeout);
}

/* ==================== Memory Management ==================== */

/* POSIX: mmap() → Win32: CreateFileMapping() + MapViewOfFile()
 * mmap is complex - requires file handle mapping or anonymous memory
 */
static inline void* win32_mmap(void *addr, size_t length, int prot, int flags,
                                HANDLE fd, off_t offset) {
    DWORD protect = PAGE_READWRITE;
    if (prot & 0x01) protect = PAGE_READONLY;      /* PROT_READ */
    if (prot & 0x02) protect = PAGE_READWRITE;     /* PROT_WRITE */
    if (prot & 0x04) protect = PAGE_EXECUTE_READ;  /* PROT_EXEC */

    HANDLE mapping;
    if (flags & 0x20) { /* MAP_ANONYMOUS */
        mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, protect,
                                     (DWORD)(length >> 32), (DWORD)length, NULL);
    } else {
        mapping = CreateFileMappingA(fd, NULL, protect, 0, 0, NULL);
    }

    if (mapping == NULL) return (void*)-1;

    void *ptr = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS,
                              (DWORD)(offset >> 32), (DWORD)offset, length);
    CloseHandle(mapping);
    return ptr ? ptr : (void*)-1;
}

/* POSIX: munmap() → Win32: UnmapViewOfFile() */
static inline int win32_munmap(void *addr, size_t length) {
    return UnmapViewOfFile(addr) ? 0 : -1;
}

/* ==================== Pipes and IPC ==================== */

/* POSIX: pipe() → Win32: CreatePipe() */
static inline int win32_pipe(HANDLE fds[2]) {
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    return CreatePipe(&fds[0], &fds[1], &sa, 0) ? 0 : -1;
}

/* POSIX: dup/dup2 → Win32: DuplicateHandle() */
static inline HANDLE win32_dup(HANDLE fd) {
    HANDLE dup_handle;
    HANDLE proc = GetCurrentProcess();
    if (!DuplicateHandle(proc, fd, proc, &dup_handle, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
        return INVALID_HANDLE_VALUE;
    }
    return dup_handle;
}

static inline int win32_dup2(HANDLE oldfd, HANDLE newfd) {
    /* Close newfd first, then duplicate */
    CloseHandle(newfd);
    return (win32_dup(oldfd) != INVALID_HANDLE_VALUE) ? 0 : -1;
}

/* ==================== Network Sockets ==================== */

/* POSIX: socket() → Win32: socket() [Winsock2]
 * Winsock API is very similar to POSIX sockets, requires WSAStartup()
 */
static inline void win32_winsock_init(void) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
}

static inline void win32_winsock_cleanup(void) {
    WSACleanup();
}

/* Winsock functions mostly match POSIX names:
 * socket(), bind(), listen(), accept(), connect()
 * send(), recv(), sendto(), recvfrom()
 * setsockopt(), getsockopt()
 * NOTE: Use closesocket() instead of close() for sockets
 */

/* ==================== Threading ==================== */

/* POSIX: pthread_create → Win32: CreateThread() / _beginthreadex()
 * _beginthreadex is safer for CRT usage
 */
typedef HANDLE pthread_t;
typedef void* pthread_attr_t;

static inline int win32_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                                        void* (*start_routine)(void*), void *arg) {
    *thread = (HANDLE)_beginthreadex(NULL, 0,
                                     (unsigned (__stdcall*)(void*))start_routine,
                                     arg, 0, NULL);
    return (*thread == 0) ? -1 : 0;
}

/* POSIX: pthread_join → Win32: WaitForSingleObject() */
static inline int win32_pthread_join(pthread_t thread, void **retval) {
    WaitForSingleObject(thread, INFINITE);
    if (retval) {
        DWORD exit_code;
        GetExitCodeThread(thread, &exit_code);
        *retval = (void*)(intptr_t)exit_code;
    }
    CloseHandle(thread);
    return 0;
}

/* POSIX: pthread_mutex → Win32: CRITICAL_SECTION or SRWLOCK
 * CRITICAL_SECTION is lighter weight, SRWLOCK for reader/writer
 */
typedef CRITICAL_SECTION pthread_mutex_t;
typedef void* pthread_mutexattr_t;

static inline int win32_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    InitializeCriticalSection(mutex);
    return 0;
}

static inline int win32_pthread_mutex_destroy(pthread_mutex_t *mutex) {
    DeleteCriticalSection(mutex);
    return 0;
}

static inline int win32_pthread_mutex_lock(pthread_mutex_t *mutex) {
    EnterCriticalSection(mutex);
    return 0;
}

static inline int win32_pthread_mutex_unlock(pthread_mutex_t *mutex) {
    LeaveCriticalSection(mutex);
    return 0;
}

/* POSIX: pthread_cond → Win32: CONDITION_VARIABLE */
typedef CONDITION_VARIABLE pthread_cond_t;
typedef void* pthread_condattr_t;

static inline int win32_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    InitializeConditionVariable(cond);
    return 0;
}

static inline int win32_pthread_cond_destroy(pthread_cond_t *cond) {
    /* No cleanup needed for CONDITION_VARIABLE */
    return 0;
}

static inline int win32_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    return SleepConditionVariableCS(cond, mutex, INFINITE) ? 0 : -1;
}

static inline int win32_pthread_cond_signal(pthread_cond_t *cond) {
    WakeConditionVariable(cond);
    return 0;
}

static inline int win32_pthread_cond_broadcast(pthread_cond_t *cond) {
    WakeAllConditionVariable(cond);
    return 0;
}

/* ==================== File System ==================== */

/* POSIX: mkdir() → Win32: CreateDirectory() */
static inline int win32_mkdir(const char *path, int mode) {
    return CreateDirectoryA(path, NULL) ? 0 : -1;
}

/* POSIX: rmdir() → Win32: RemoveDirectory() */
static inline int win32_rmdir(const char *path) {
    return RemoveDirectoryA(path) ? 0 : -1;
}

/* POSIX: unlink() → Win32: DeleteFile() */
static inline int win32_unlink(const char *path) {
    return DeleteFileA(path) ? 0 : -1;
}

/* POSIX: rename() → Win32: MoveFileEx() */
static inline int win32_rename(const char *oldpath, const char *newpath) {
    return MoveFileExA(oldpath, newpath, MOVEFILE_REPLACE_EXISTING) ? 0 : -1;
}

/* POSIX: opendir/readdir/closedir → Win32: FindFirstFile/FindNextFile/FindClose */
typedef struct {
    HANDLE handle;
    WIN32_FIND_DATAA data;
    int first;
} DIR;

typedef struct {
    char d_name[MAX_PATH];
} dirent;

static inline DIR* win32_opendir(const char *path) {
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s\\*", path);

    DIR *dir = malloc(sizeof(DIR));
    if (!dir) return NULL;

    dir->handle = FindFirstFileA(search_path, &dir->data);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        free(dir);
        return NULL;
    }
    dir->first = 1;
    return dir;
}

static inline dirent* win32_readdir(DIR *dir) {
    static dirent entry;

    if (dir->first) {
        dir->first = 0;
    } else {
        if (!FindNextFileA(dir->handle, &dir->data)) {
            return NULL;
        }
    }

    strncpy(entry.d_name, dir->data.cFileName, MAX_PATH);
    return &entry;
}

static inline int win32_closedir(DIR *dir) {
    if (dir) {
        FindClose(dir->handle);
        free(dir);
    }
    return 0;
}

#endif /* _WIN32 */

#endif /* COSMO_WIN32_H */
