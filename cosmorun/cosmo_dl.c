/*
 * CosmoRun Dynamic Loader Implementation
 * Cross-platform dlopen/dlsym/dlclose/dlerror implementation
 */

#include "cosmo_dl.h"
#include "cosmo_dl_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* FIX v0.9.6: Use direct system dlopen to avoid circular dependency with xdl.c
 *
 * Previous: cosmo_dlopen → xdl_open → cosmo_dlopen (circular!)
 * Now: cosmo_dlopen → system dlopen (direct)
 *
 * We use weak symbols to call native dlopen functions directly,
 * bypassing the xdl layer to eliminate circular dependency.
 */

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
    #include <dlfcn.h>
#else
    /* Use weak symbols to call system dlopen directly */
    #include <dlfcn.h>
#endif

/* Weak references to system dlopen functions (POSIX systems) */
#if !defined(_WIN32)
__attribute__((weak)) void* __libc_dlopen_mode(const char*, int);
__attribute__((weak)) void* __libc_dlsym(void*, const char*);
__attribute__((weak)) int   __libc_dlclose(void*);
__attribute__((weak)) const char* __libc_dlerror(void);

__attribute__((weak)) void* dlopen(const char*, int);
__attribute__((weak)) void* dlsym(void*, const char*);
__attribute__((weak)) int   dlclose(void*);
__attribute__((weak)) char* dlerror(void);
#endif

/* Thread-local error storage - use static for now to avoid TLS issues in JIT */
static char g_dl_error_buf[DL_ERROR_MAX_LEN] = {0};
static int g_dl_error_set = 0;

/* Global state */
dl_state_t g_dl_state = {0};

/* Mutex for thread-safe operations (simplified for now) */
static int g_dl_mutex_initialized = 0;

/* ========================================================================
 * Error Handling
 * ======================================================================== */

void dl_set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_dl_error_buf, DL_ERROR_MAX_LEN, fmt, args);
    va_end(args);
    g_dl_error_set = 1;
}

void dl_clear_error(void) {
    g_dl_error_buf[0] = '\0';
    g_dl_error_set = 0;
}

/* ========================================================================
 * Platform-Specific Native Wrappers
 * ======================================================================== */

#if defined(_WIN32)

void *dl_native_open(const char *filename, int flags) {
    (void)flags; /* Windows doesn't use RTLD flags */

    if (!filename) {
        return GetModuleHandle(NULL);
    }

    HMODULE handle = LoadLibraryA(filename);
    if (!handle) {
        DWORD err = GetLastError();
        char buf[256];
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, err, 0, buf, sizeof(buf), NULL);
        dl_set_error("LoadLibrary failed: %s", buf);
        return NULL;
    }

    return (void*)handle;
}

void *dl_native_sym(void *handle, const char *symbol) {
    if (!handle || !symbol) {
        dl_set_error("Invalid handle or symbol");
        return NULL;
    }

    FARPROC addr = GetProcAddress((HMODULE)handle, symbol);
    if (!addr) {
        DWORD err = GetLastError();
        dl_set_error("GetProcAddress failed: error %lu", err);
        return NULL;
    }

    return (void*)addr;
}

int dl_native_close(void *handle) {
    if (!handle) {
        dl_set_error("Invalid handle");
        return -1;
    }

    if (!FreeLibrary((HMODULE)handle)) {
        DWORD err = GetLastError();
        dl_set_error("FreeLibrary failed: error %lu", err);
        return -1;
    }

    return 0;
}

const char *dl_native_error(void) {
    return g_dl_error_set ? g_dl_error_buf : NULL;
}

#else /* POSIX (Linux, macOS, etc.) - use system dlopen directly */

void *dl_native_open(const char *filename, int flags) {
    /* Map our flags to native flags */
    int native_flags = 0;

    if (flags & RTLD_NOW) {
        native_flags |= RTLD_NOW;
    } else {
        native_flags |= RTLD_LAZY;
    }

    if (flags & RTLD_GLOBAL) {
        native_flags |= RTLD_GLOBAL;
    } else {
        native_flags |= RTLD_LOCAL;
    }

    /* Call system dlopen directly using weak symbols - avoid xdl circular dependency */
    void *handle = NULL;

    /* Try __libc_dlopen_mode first (glibc internal) */
    if (__libc_dlopen_mode) {
        handle = __libc_dlopen_mode(filename, native_flags);
    }
    /* Fallback to standard dlopen */
    else if (dlopen) {
        handle = dlopen(filename, native_flags);
    }

    if (!handle) {
        const char *err = NULL;
        if (__libc_dlerror) {
            err = __libc_dlerror();
        } else if (dlerror) {
            err = dlerror();
        }
        dl_set_error("%s", err ? err : "Unknown dlopen error");
        return NULL;
    }

    return handle;
}

void *dl_native_sym(void *handle, const char *symbol) {
    if (!symbol) {
        dl_set_error("Invalid symbol name");
        return NULL;
    }

    /* Clear any existing error */
    if (__libc_dlerror) {
        __libc_dlerror();
    } else if (dlerror) {
        dlerror();
    }

    /* Call system dlsym directly */
    void *addr = NULL;
    if (__libc_dlsym) {
        addr = __libc_dlsym(handle, symbol);
    } else if (dlsym) {
        addr = dlsym(handle, symbol);
    }

    /* Check for error - NULL can be a valid symbol address */
    const char *err = NULL;
    if (__libc_dlerror) {
        err = __libc_dlerror();
    } else if (dlerror) {
        err = dlerror();
    }
    if (err) {
        dl_set_error("%s", err);
        return NULL;
    }

    return addr;
}

int dl_native_close(void *handle) {
    if (!handle) {
        dl_set_error("Invalid handle");
        return -1;
    }

    int result = -1;
    if (__libc_dlclose) {
        result = __libc_dlclose(handle);
    } else if (dlclose) {
        result = dlclose(handle);
    }

    if (result != 0) {
        const char *err = NULL;
        if (__libc_dlerror) {
            err = __libc_dlerror();
        } else if (dlerror) {
            err = dlerror();
        }
        dl_set_error("%s", err ? err : "Unknown dlclose error");
        return -1;
    }

    return 0;
}

const char *dl_native_error(void) {
    return g_dl_error_set ? g_dl_error_buf : NULL;
}

#endif /* Platform-specific natives */

/* ========================================================================
 * Handle Management
 * ======================================================================== */

int dl_init_state(void) {
    if (g_dl_state.initialized) {
        return 0;
    }

    g_dl_state.handles = NULL;
    g_dl_state.handle_count = 0;
    /* Note: cosmopolitan/xdl doesn't support NULL (main executable) handle */
    g_dl_state.main_handle = NULL;
    g_dl_state.initialized = 1;

    return 0;
}

void dl_cleanup_state(void) {
    if (!g_dl_state.initialized) {
        return;
    }

    /* Close all handles */
    dl_handle_t *h = g_dl_state.handles;
    while (h) {
        dl_handle_t *next = h->next;
        dl_destroy_handle(h);
        h = next;
    }

    g_dl_state.handles = NULL;
    g_dl_state.handle_count = 0;
    g_dl_state.initialized = 0;
}

dl_handle_t *dl_create_handle(const char *filename, int flags) {
    dl_handle_t *h = (dl_handle_t*)malloc(sizeof(dl_handle_t));
    if (!h) {
        dl_set_error("Out of memory");
        return NULL;
    }

    memset(h, 0, sizeof(dl_handle_t));

    if (filename) {
        h->filename = strdup(filename);
        if (!h->filename) {
            free(h);
            dl_set_error("Out of memory");
            return NULL;
        }
    }

    h->flags = flags;
    h->refcount = 1;
    h->next = NULL;

    return h;
}

void dl_destroy_handle(dl_handle_t *h) {
    if (!h) {
        return;
    }

    /* Call fini_array destructors */
    if (h->fini_array) {
        for (size_t i = 0; i < h->fini_array_size; i++) {
            void (*fini)(void) = (void (*)(void))h->fini_array[i];
            if (fini) {
                fini();
            }
        }
        free(h->fini_array);
    }

    if (h->init_array) {
        free(h->init_array);
    }

    if (h->filename) {
        free(h->filename);
    }

    /* Close native handle */
    if (h->native_handle) {
        dl_native_close(h->native_handle);
    }

    free(h);
}

dl_handle_t *dl_find_handle(void *handle) {
    if (!handle) {
        return NULL;
    }

    for (dl_handle_t *h = g_dl_state.handles; h; h = h->next) {
        if (h == handle || h->native_handle == handle) {
            return h;
        }
    }

    return NULL;
}

/* ========================================================================
 * Public API Implementation
 * ======================================================================== */

void *cosmo_dlopen(const char *filename, int flags) {
    dl_clear_error();

    /* Initialize state if needed */
    if (!g_dl_state.initialized) {
        dl_init_state();
    }

    /* Check if NULL filename (main executable) */
    if (!filename) {
        if (g_dl_state.main_handle) {
            return g_dl_state.main_handle;
        }
        dl_set_error("Main executable handle not available");
        return NULL;
    }

    /* Check for RTLD_NOLOAD - don't load, just check */
    if (flags & RTLD_NOLOAD) {
        for (dl_handle_t *h = g_dl_state.handles; h; h = h->next) {
            if (h->filename && strcmp(h->filename, filename) == 0) {
                h->refcount++;
                return h;
            }
        }
        dl_set_error("Library not already loaded");
        return NULL;
    }

    /* Check if already loaded */
    for (dl_handle_t *h = g_dl_state.handles; h; h = h->next) {
        if (h->filename && strcmp(h->filename, filename) == 0) {
            h->refcount++;
            dl_clear_error();
            return h;
        }
    }

    /* Load the library */
    void *native_handle = dl_native_open(filename, flags);
    if (!native_handle) {
        return NULL;
    }

    /* Create handle structure */
    dl_handle_t *h = dl_create_handle(filename, flags);
    if (!h) {
        dl_native_close(native_handle);
        return NULL;
    }

    h->native_handle = native_handle;

    /* Add to list */
    h->next = g_dl_state.handles;
    g_dl_state.handles = h;
    g_dl_state.handle_count++;

    dl_clear_error();
    return h;
}

void *cosmo_dlsym(void *handle, const char *symbol) {
    dl_clear_error();

    if (!symbol) {
        dl_set_error("Invalid symbol name");
        return NULL;
    }

    /* Handle special values */
    if (handle == RTLD_DEFAULT) {
        /* Search all loaded libraries in order */
        void *addr = NULL;

        /* Try main executable first */
        if (g_dl_state.main_handle) {
            addr = dl_native_sym(g_dl_state.main_handle, symbol);
            if (addr) {
                dl_clear_error();
                return addr;
            }
        }

        /* Search loaded libraries */
        for (dl_handle_t *h = g_dl_state.handles; h; h = h->next) {
            if (h->native_handle) {
                addr = dl_native_sym(h->native_handle, symbol);
                if (addr) {
                    dl_clear_error();
                    return addr;
                }
            }
        }

        dl_set_error("Symbol '%s' not found", symbol);
        return NULL;
    }

    if (handle == RTLD_NEXT) {
        /* RTLD_NEXT is complex - simplified implementation */
        dl_set_error("RTLD_NEXT not fully implemented");
        return NULL;
    }

    /* Regular handle */
    dl_handle_t *h = dl_find_handle(handle);
    if (!h) {
        /* Try as native handle directly */
        void *addr = dl_native_sym(handle, symbol);
        if (addr) {
            dl_clear_error();
        }
        return addr;
    }

    /* Look up symbol in the library */
    void *addr = dl_native_sym(h->native_handle, symbol);
    if (addr) {
        dl_clear_error();
    }

    return addr;
}

int cosmo_dlclose(void *handle) {
    dl_clear_error();

    if (!handle) {
        dl_set_error("Invalid handle");
        return -1;
    }

    /* Don't close main executable */
    if (handle == g_dl_state.main_handle) {
        return 0;
    }

    /* Find handle */
    dl_handle_t *h = dl_find_handle(handle);
    if (!h) {
        dl_set_error("Invalid handle");
        return -1;
    }

    /* Decrement reference count */
    h->refcount--;

    /* Don't actually close if RTLD_NODELETE or refcount > 0 */
    if ((h->flags & RTLD_NODELETE) || h->refcount > 0) {
        return 0;
    }

    /* Remove from list */
    if (g_dl_state.handles == h) {
        g_dl_state.handles = h->next;
    } else {
        for (dl_handle_t *prev = g_dl_state.handles; prev; prev = prev->next) {
            if (prev->next == h) {
                prev->next = h->next;
                break;
            }
        }
    }

    g_dl_state.handle_count--;

    /* Destroy handle */
    dl_destroy_handle(h);

    return 0;
}

char *cosmo_dlerror(void) {
    if (!g_dl_error_set) {
        return NULL;
    }

    /* Return error buffer directly and clear flag (per POSIX spec) */
    /* Note: Caller should not free this buffer */
    char *result = g_dl_error_buf;
    g_dl_error_set = 0;  /* Clear flag but keep message for this call */

    return result;
}

int cosmo_dladdr(const void *addr, Dl_info *info) {
    if (!info) {
        return 0;
    }

    memset(info, 0, sizeof(Dl_info));

#if defined(_WIN32)
    /* Windows implementation */
    HMODULE hModule;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)addr, &hModule)) {
        return 0;
    }

    char filename[MAX_PATH];
    if (GetModuleFileNameA(hModule, filename, MAX_PATH)) {
        info->dli_fname = filename;
        info->dli_fbase = (void*)hModule;
    }

    return 1;

#elif defined(__APPLE__) || defined(__linux__)
    /* POSIX implementation */
    Dl_info native_info;
    if (dladdr(addr, &native_info) == 0) {
        return 0;
    }

    info->dli_fname = native_info.dli_fname;
    info->dli_fbase = native_info.dli_fbase;
    info->dli_sname = native_info.dli_sname;
    info->dli_saddr = native_info.dli_saddr;

    return 1;
#else
    return 0;
#endif
}

/* Note: Standard names (dlopen, dlsym, etc.) are registered in cosmo_run.c
 * via tcc_add_symbol to avoid symbol conflicts in the JIT environment */
