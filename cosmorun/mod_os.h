#ifndef COSMORUN_OS_H
#define COSMORUN_OS_H

/*
 * mod_os - Operating system utilities for cosmorun
 *
 * Provides cross-platform OS abstractions:
 * - Process management
 * - File system operations
 * - Environment variables
 * - Path utilities
 */

#include "cosmo_libc.h"
#include "mod_std.h"

/* ==================== Process Management ==================== */

typedef struct {
    int pid;
    int exit_code;
    int running;
} os_process_t;

/* Process execution */
os_process_t* os_exec(const char *command, char **args, char **env);
int os_wait(os_process_t *proc, int timeout_ms);
int os_kill(os_process_t *proc, int signal);
void os_process_free(os_process_t *proc);

/* Current process info */
int os_getpid(void);
int os_getppid(void);

/* ==================== File System ==================== */

typedef struct {
    char *path;
    int is_dir;
    int is_file;
    size_t size;
    long mtime;
} os_fileinfo_t;

/* File operations */
int os_exists(const char *path);
int os_is_file(const char *path);
int os_is_dir(const char *path);
os_fileinfo_t* os_stat(const char *path);
void os_fileinfo_free(os_fileinfo_t *info);

/* Directory operations */
int os_mkdir(const char *path);
int os_rmdir(const char *path);
int os_remove(const char *path);
int os_rename(const char *old_path, const char *new_path);

/* Directory listing */
typedef struct {
    std_vector_t *entries;  // vector of char*
} os_dirlist_t;

os_dirlist_t* os_listdir(const char *path);
void os_dirlist_free(os_dirlist_t *list);

/* ==================== Path Utilities ==================== */

/* Path manipulation (caller must free returned strings) */
char* os_path_join(const char *base, const char *name);
char* os_path_dirname(const char *path);
char* os_path_basename(const char *path);
char* os_path_abs(const char *path);

/* Working directory (caller must free returned string) */
char* os_getcwd(void);
int os_chdir(const char *path);

/* ==================== Environment Variables ==================== */

/* Environment access */
const char* os_getenv(const char *name);
int os_setenv(const char *name, const char *value);
int os_unsetenv(const char *name);

/* Simple hashmap for environment variables */
typedef struct env_entry env_entry_t;
typedef struct {
    env_entry_t **buckets;
    size_t size;
} simple_hashmap_t;

/* Get all environment variables (caller must free with os_environ_free) */
simple_hashmap_t* os_environ(void);
void os_environ_free(simple_hashmap_t *map);

/* ==================== System Info ==================== */

typedef struct {
    char *os_type;      // "linux", "windows", "darwin", etc.
    char *hostname;
    int num_cpus;
} os_sysinfo_t;

os_sysinfo_t* os_sysinfo(void);
void os_sysinfo_free(os_sysinfo_t *info);

/* ==================== Memory Mapping ==================== */

typedef struct {
    void *addr;         // Mapped memory address
    size_t size;        // Size of mapped region
    int fd;             // File descriptor
    char *filename;     // File path (for debugging)
    int prot;           // Protection flags
    int flags;          // Mapping flags
    int is_mapped;      // Whether currently mapped
} os_mmap_t;

/* Memory mapping flags */
#define OS_MMAP_READ     0x1    // PROT_READ
#define OS_MMAP_WRITE    0x2    // PROT_WRITE
#define OS_MMAP_EXEC     0x4    // PROT_EXEC
#define OS_MMAP_PRIVATE  0x10   // MAP_PRIVATE
#define OS_MMAP_SHARED   0x20   // MAP_SHARED

/* Memory mapping operations */
os_mmap_t* os_mmap_file(const char *filename, int prot, int flags);
os_mmap_t* os_mmap_create(size_t size, int prot, int flags);
int os_mmap_sync(os_mmap_t *map);
void os_mmap_free(os_mmap_t *map);

/* Memory mapping utilities */
int os_mmap_advise(os_mmap_t *map, int advice);
size_t os_mmap_get_pagesize(void);

/* Memory mapping advice flags */
#define OS_MMAP_NORMAL     0    // MADV_NORMAL
#define OS_MMAP_RANDOM     1    // MADV_RANDOM
#define OS_MMAP_SEQUENTIAL 2    // MADV_SEQUENTIAL
#define OS_MMAP_WILLNEED   3    // MADV_WILLNEED
#define OS_MMAP_DONTNEED   4    // MADV_DONTNEED

/* ==================== Temporary Files ==================== */

/* Create temporary file/directory (caller must free returned strings) */
char* os_tmpfile(void);
char* os_tmpdir(void);

#endif /* COSMORUN_OS_H */
