/*
 * mod_os.c - Operating system utilities implementation
 */

/* Include cosmo_libc.h first to get proper type and macro definitions */
#include "src/cosmo_libc.h"

/* For non-TinyCC, include system headers to get POSIX types like pid_t */
#ifndef __TINYC__
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <sys/mman.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <dirent.h>
#endif

#include "mod_os.h"

/* External declarations for functions not in cosmo_libc.h */
extern char **environ;
extern int mkstemp(char *template);
extern char *mkdtemp(char *template);

/* madvise() constants - define if not available */
#ifndef MADV_NORMAL
#define MADV_NORMAL 0
#endif
#ifndef MADV_RANDOM
#define MADV_RANDOM 1
#endif
#ifndef MADV_SEQUENTIAL
#define MADV_SEQUENTIAL 2
#endif
#ifndef MADV_WILLNEED
#define MADV_WILLNEED 3
#endif
#ifndef MADV_DONTNEED
#define MADV_DONTNEED 4
#endif

/* msync() constants - define if not available */
#ifndef MS_ASYNC
#define MS_ASYNC 1
#endif
#ifndef MS_SYNC
#define MS_SYNC 4
#endif
#ifndef MS_INVALIDATE
#define MS_INVALIDATE 2
#endif

/* ==================== Process Management ==================== */

os_process_t* os_exec(const char *command, char **args, char **env) {
    if (!command) return NULL;

    os_process_t *proc = (os_process_t*)shim_malloc(sizeof(os_process_t));
    if (!proc) return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        shim_free(proc);
        return NULL;
    }

    if (pid == 0) {
        // Child process
        if (env) {
            execve(command, args, env);
        } else {
            execv(command, args);
        }
        _exit(127);  // exec failed
    }

    // Parent process
    proc->pid = pid;
    proc->exit_code = -1;
    proc->running = 1;

    return proc;
}

int os_wait(os_process_t *proc, int timeout_ms) {
    if (!proc || !proc->running) return -1;

    int status;
    int options = timeout_ms == 0 ? WNOHANG : 0;

    pid_t result = waitpid(proc->pid, &status, options);

    if (result == proc->pid) {
        proc->running = 0;
        if (WIFEXITED(status)) {
            proc->exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            proc->exit_code = -WTERMSIG(status);
        }
        return proc->exit_code;
    } else if (result == 0) {
        return -2;  // Still running (timeout)
    }

    return -1;  // Error
}

int os_kill(os_process_t *proc, int signal) {
    if (!proc || !proc->running) return -1;
    return kill(proc->pid, signal);
}

void os_process_free(os_process_t *proc) {
    if (proc) shim_free(proc);
}

int os_getpid(void) {
    return getpid();
}

int os_getppid(void) {
    return getppid();
}

/* ==================== File System ==================== */

int os_exists(const char *path) {
    if (!path) return 0;
    return access(path, F_OK) == 0;
}

int os_is_file(const char *path) {
    if (!path) return 0;

    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}

int os_is_dir(const char *path) {
    if (!path) return 0;

    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

os_fileinfo_t* os_stat(const char *path) {
    if (!path) return NULL;

    struct stat st;
    if (stat(path, &st) != 0) return NULL;

    os_fileinfo_t *info = (os_fileinfo_t*)shim_malloc(sizeof(os_fileinfo_t));
    if (!info) return NULL;

    info->path = shim_strdup(path);
    info->is_dir = S_ISDIR(st.st_mode);
    info->is_file = S_ISREG(st.st_mode);
    info->size = st.st_size;
    info->mtime = st.st_mtime;

    return info;
}

void os_fileinfo_free(os_fileinfo_t *info) {
    if (!info) return;
    if (info->path) shim_free(info->path);
    shim_free(info);
}

int os_mkdir(const char *path) {
    if (!path) return -1;
    return mkdir(path, 0755);
}

int os_rmdir(const char *path) {
    if (!path) return -1;
    return rmdir(path);
}

int os_remove(const char *path) {
    if (!path) return -1;
    return unlink(path);
}

int os_rename(const char *old_path, const char *new_path) {
    if (!old_path || !new_path) return -1;
    return rename(old_path, new_path);
}

/* Simple dynamic array for directory entries (no dependency on mod_std) */
static char** dirlist_entries = NULL;
static size_t dirlist_count = 0;
static size_t dirlist_capacity = 0;

os_dirlist_t* os_listdir(const char *path) {
    if (!path) return NULL;

    DIR *dir = (DIR*)opendir(path);
    if (!dir) return NULL;

    os_dirlist_t *list = (os_dirlist_t*)shim_malloc(sizeof(os_dirlist_t));
    if (!list) {
        closedir(dir);
        return NULL;
    }

    /* Initialize simple dynamic array */
    size_t capacity = 16;
    char **entries = (char**)shim_malloc(sizeof(char*) * capacity);
    if (!entries) {
        shim_free(list);
        closedir(dir);
        return NULL;
    }

    size_t count = 0;
    struct dirent *entry;
    while ((entry = (struct dirent*)readdir(dir)) != NULL) {
        // Skip . and ..
        if (shim_strcmp(entry->d_name, ".") == 0 || shim_strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Grow array if needed */
        if (count >= capacity) {
            capacity *= 2;
            char **new_entries = (char**)shim_realloc(entries, sizeof(char*) * capacity);
            if (!new_entries) {
                for (size_t i = 0; i < count; i++) shim_free(entries[i]);
                shim_free(entries);
                shim_free(list);
                closedir(dir);
                return NULL;
            }
            entries = new_entries;
        }

        entries[count++] = shim_strdup(entry->d_name);
    }

    closedir(dir);

    /* Store in list structure (repurpose entries field as char**) */
    list->entries = (void*)entries;
    /* Use capacity field to store count */
    *(size_t*)&list->entries = count;  /* Store count in first word */

    /* Actually we need a better approach - redefine the structure */
    /* For now, create a wrapper */
    dirlist_entries = entries;
    dirlist_count = count;
    dirlist_capacity = capacity;

    return list;
}

void os_dirlist_free(os_dirlist_t *list) {
    if (!list) return;

    if (dirlist_entries) {
        for (size_t i = 0; i < dirlist_count; i++) {
            if (dirlist_entries[i]) shim_free(dirlist_entries[i]);
        }
        shim_free(dirlist_entries);
        dirlist_entries = NULL;
        dirlist_count = 0;
        dirlist_capacity = 0;
    }

    shim_free(list);
}

/* ==================== Path Utilities ==================== */

char* os_path_join(const char *base, const char *name) {
    if (!base || !name) return NULL;

    size_t base_len = shim_strlen(base);
    size_t name_len = shim_strlen(name);
    int need_slash = (base_len > 0 && base[base_len - 1] != '/') ? 1 : 0;

    char *result = (char*)shim_malloc(base_len + need_slash + name_len + 1);
    if (!result) return NULL;

    shim_memcpy(result, base, base_len);
    if (need_slash) {
        result[base_len] = '/';
        shim_memcpy(result + base_len + 1, name, name_len);
        result[base_len + 1 + name_len] = '\0';
    } else {
        shim_memcpy(result + base_len, name, name_len);
        result[base_len + name_len] = '\0';
    }

    return result;
}

char* os_path_dirname(const char *path) {
    if (!path) return NULL;

    const char *last_slash = shim_strrchr(path, '/');
    if (!last_slash) {
        return shim_strdup(".");
    }

    if (last_slash == path) {
        return shim_strdup("/");
    }

    size_t len = last_slash - path;
    char *result = (char*)shim_malloc(len + 1);
    if (!result) return NULL;

    shim_memcpy(result, path, len);
    result[len] = '\0';

    return result;
}

char* os_path_basename(const char *path) {
    if (!path) return NULL;

    const char *last_slash = shim_strrchr(path, '/');
    if (!last_slash) {
        return shim_strdup(path);
    }

    return shim_strdup(last_slash + 1);
}

char* os_path_abs(const char *path) {
    if (!path) return NULL;

    char resolved[4096];
    if (!realpath(path, resolved)) {
        return NULL;
    }

    return shim_strdup(resolved);
}

char* os_getcwd(void) {
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) {
        return NULL;
    }
    return shim_strdup(cwd);
}

int os_chdir(const char *path) {
    if (!path) return -1;
    return chdir(path);
}

/* ==================== Environment Variables ==================== */

const char* os_getenv(const char *name) {
    if (!name) return NULL;
    return getenv(name);
}

int os_setenv(const char *name, const char *value) {
    if (!name || !value) return -1;
    return setenv(name, value, 1);
}

int os_unsetenv(const char *name) {
    if (!name) return -1;
    return unsetenv(name);
}

/* Simple hashmap implementation (structure defined in mod_os.h) */
struct env_entry {
    char *key;
    char *value;
    struct env_entry *next;
};

static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

simple_hashmap_t* os_environ(void) {
    simple_hashmap_t *map = (simple_hashmap_t*)shim_malloc(sizeof(simple_hashmap_t));
    if (!map) return NULL;

    map->size = 256;  /* Fixed size for simplicity */
    map->buckets = (env_entry_t**)shim_calloc(map->size, sizeof(env_entry_t*));
    if (!map->buckets) {
        shim_free(map);
        return NULL;
    }

    for (char **ep = environ; *ep != NULL; ep++) {
        char *eq = shim_strchr(*ep, '=');
        if (!eq) continue;

        size_t key_len = eq - *ep;
        char *key = (char*)shim_malloc(key_len + 1);
        if (!key) continue;

        shim_memcpy(key, *ep, key_len);
        key[key_len] = '\0';

        env_entry_t *entry = (env_entry_t*)shim_malloc(sizeof(env_entry_t));
        if (!entry) {
            shim_free(key);
            continue;
        }

        entry->key = key;
        entry->value = shim_strdup(eq + 1);
        unsigned int bucket = hash_string(key) % map->size;
        entry->next = map->buckets[bucket];
        map->buckets[bucket] = entry;
    }

    return map;
}

void os_environ_free(simple_hashmap_t *map) {
    if (!map) return;
    if (map->buckets) {
        for (size_t i = 0; i < map->size; i++) {
            env_entry_t *entry = map->buckets[i];
            while (entry) {
                env_entry_t *next = entry->next;
                shim_free(entry->key);
                shim_free(entry->value);
                shim_free(entry);
                entry = next;
            }
        }
        shim_free(map->buckets);
    }
    shim_free(map);
}

/* ==================== System Info ==================== */

os_sysinfo_t* os_sysinfo(void) {
    os_sysinfo_t *info = (os_sysinfo_t*)shim_malloc(sizeof(os_sysinfo_t));
    if (!info) return NULL;

    // OS type
#if defined(__linux__)
    info->os_type = shim_strdup("linux");
#elif defined(__APPLE__)
    info->os_type = shim_strdup("darwin");
#elif defined(_WIN32)
    info->os_type = shim_strdup("windows");
#elif defined(__FreeBSD__)
    info->os_type = shim_strdup("freebsd");
#else
    info->os_type = shim_strdup("unknown");
#endif

    // Hostname
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        info->hostname = shim_strdup(hostname);
    } else {
        info->hostname = shim_strdup("unknown");
    }

    // CPU count
    long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    info->num_cpus = (int)(ncpus > 0 ? ncpus : 1);

    return info;
}

void os_sysinfo_free(os_sysinfo_t *info) {
    if (!info) return;
    if (info->os_type) shim_free(info->os_type);
    if (info->hostname) shim_free(info->hostname);
    shim_free(info);
}

/* ==================== Temporary Files ==================== */

char* os_tmpfile(void) {
    char template[] = "/tmp/cosmorun_XXXXXX";
    int fd = mkstemp(template);
    if (fd < 0) return NULL;

    close(fd);
    return shim_strdup(template);
}

char* os_tmpdir(void) {
    char template[] = "/tmp/cosmorun_XXXXXX";
    if (!mkdtemp(template)) return NULL;

    return shim_strdup(template);
}

/* ==================== Memory Mapping Implementation ==================== */

// #include <sys/mman.h>

/* Convert OS flags to system flags */
static int convert_prot_flags(int os_prot) {
    int prot = 0;
    if (os_prot & OS_MMAP_READ)  prot |= PROT_READ;
    if (os_prot & OS_MMAP_WRITE) prot |= PROT_WRITE;
    if (os_prot & OS_MMAP_EXEC)  prot |= PROT_EXEC;
    return prot;
}

static int convert_map_flags(int os_flags) {
    int flags = 0;
    if (os_flags & OS_MMAP_PRIVATE) flags |= MAP_PRIVATE;
    if (os_flags & OS_MMAP_SHARED)  flags |= MAP_SHARED;
    return flags;
}

static int convert_advice_flags(int os_advice) {
    switch (os_advice) {
        case OS_MMAP_NORMAL:     return MADV_NORMAL;
        case OS_MMAP_RANDOM:     return MADV_RANDOM;
        case OS_MMAP_SEQUENTIAL: return MADV_SEQUENTIAL;
        case OS_MMAP_WILLNEED:   return MADV_WILLNEED;
        case OS_MMAP_DONTNEED:   return MADV_DONTNEED;
        default:                 return MADV_NORMAL;
    }
}

/* Map a file into memory */
os_mmap_t* os_mmap_file(const char *filename, int prot, int flags) {
    if (!filename) return NULL;

    os_mmap_t *map = (os_mmap_t*)shim_malloc(sizeof(os_mmap_t));
    if (!map) return NULL;

    // Initialize structure
    shim_memset(map, 0, sizeof(os_mmap_t));
    map->fd = -1;
    map->prot = prot;
    map->flags = flags;

    // Open file
    int open_flags = O_RDONLY;
    if (prot & OS_MMAP_WRITE) {
        open_flags = O_RDWR;
    }

    map->fd = open(filename, open_flags);
    if (map->fd < 0) {
        shim_free(map);
        return NULL;
    }

    // Get file size
    struct stat st;
    if (fstat(map->fd, &st) < 0) {
        close(map->fd);
        shim_free(map);
        return NULL;
    }
    map->size = st.st_size;

    // Memory map the file
    int sys_prot = convert_prot_flags(prot);
    int sys_flags = convert_map_flags(flags);

    map->addr = mmap(NULL, map->size, sys_prot, sys_flags, map->fd, 0);
    if (map->addr == MAP_FAILED) {
        close(map->fd);
        shim_free(map);
        return NULL;
    }

    // Store filename for debugging
    map->filename = shim_strdup(filename);
    map->is_mapped = 1;

    return map;
}

/* Create anonymous memory mapping */
os_mmap_t* os_mmap_create(size_t size, int prot, int flags) {
    if (size == 0) return NULL;

    os_mmap_t *map = (os_mmap_t*)shim_malloc(sizeof(os_mmap_t));
    if (!map) return NULL;

    // Initialize structure
    shim_memset(map, 0, sizeof(os_mmap_t));
    map->fd = -1;
    map->size = size;
    map->prot = prot;
    map->flags = flags;

    // Create anonymous mapping
    int sys_prot = convert_prot_flags(prot);
    int sys_flags = convert_map_flags(flags) | MAP_ANONYMOUS;

    map->addr = mmap(NULL, size, sys_prot, sys_flags, -1, 0);
    if (map->addr == MAP_FAILED) {
        shim_free(map);
        return NULL;
    }

    map->is_mapped = 1;
    return map;
}

/* Synchronize memory mapping to disk */
int os_mmap_sync(os_mmap_t *map) {
    if (!map || !map->is_mapped) return -1;

    return msync(map->addr, map->size, MS_SYNC);
}

/* Free memory mapping */
void os_mmap_free(os_mmap_t *map) {
    if (!map) return;

    if (map->is_mapped && map->addr != MAP_FAILED) {
        munmap(map->addr, map->size);
    }

    if (map->fd >= 0) {
        close(map->fd);
    }

    if (map->filename) {
        shim_free(map->filename);
    }

    shim_free(map);
}

/* Provide memory access advice */
int os_mmap_advise(os_mmap_t *map, int advice) {
    if (!map || !map->is_mapped) return -1;

    int sys_advice = convert_advice_flags(advice);
    return madvise(map->addr, map->size, sys_advice);
}

/* Get system page size */
size_t os_mmap_get_pagesize(void) {
    return getpagesize();
}
