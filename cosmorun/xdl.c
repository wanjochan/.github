#define _GNU_SOURCE
#include "xdl.h"

#if defined(__COSMOPOLITAN__)
#include "libc/dlopen/dlfcn.h"

xdl_handle xdl_open(const char* filename, int flags) {
    return cosmo_dlopen(filename, flags);
}

void* xdl_sym(xdl_handle handle, const char* symbol) {
    return cosmo_dlsym(handle, symbol);
}

int xdl_close(xdl_handle handle) {
    return cosmo_dlclose(handle);
}

const char* xdl_error(void) {
    return cosmo_dlerror();
}

#elif defined(_WIN32)
#include <windows.h>
#include <stdio.h>
#include <string.h>

static __declspec(thread) char g_errbuf[512];

static void set_last_error_from_win(DWORD code) {
    if (!code) {
        g_errbuf[0] = 0;
        return;
    }
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD n = FormatMessageA(flags, NULL, code, 0, g_errbuf, (DWORD)sizeof(g_errbuf), NULL);
    if (!n) {
        snprintf(g_errbuf, sizeof(g_errbuf), "WinErr 0x%lx", (unsigned long)code);
    }
}

xdl_handle xdl_open(const char* filename, int flags) {
    (void)flags;
    HMODULE h = LoadLibraryA(filename);
    if (!h) {
        set_last_error_from_win(GetLastError());
    } else {
        g_errbuf[0] = 0;
    }
    return (xdl_handle)h;
}

void* xdl_sym(xdl_handle handle, const char* symbol) {
    FARPROC p = GetProcAddress((HMODULE)handle, symbol);
    if (!p) {
        set_last_error_from_win(GetLastError());
    } else {
        g_errbuf[0] = 0;
    }
    return (void*)p;
}

int xdl_close(xdl_handle handle) {
    BOOL ok = FreeLibrary((HMODULE)handle);
    if (!ok) {
        set_last_error_from_win(GetLastError());
        return -1;
    }
    g_errbuf[0] = 0;
    return 0;
}

const char* xdl_error(void) {
    return g_errbuf[0] ? g_errbuf : NULL;
}

#else //use weak link to get the inner symbol...

#include <stdint.h>
#include <stddef.h>

__attribute__((weak)) void* __libc_dlopen_mode(const char*, int);
__attribute__((weak)) void* __libc_dlsym(void*, const char*);
__attribute__((weak)) int   __libc_dlclose(void*);
__attribute__((weak)) const char* __libc_dlerror(void);

__attribute__((weak)) void* dlopen(const char*, int);
__attribute__((weak)) void* dlsym(void*, const char*);
__attribute__((weak)) int   dlclose(void*);
__attribute__((weak)) char* dlerror(void);

static inline int have_libc_dl(void) {
    return (__libc_dlopen_mode && __libc_dlsym);
}

static inline int have_std_dl(void) {
    return (dlopen && dlsym);
}

xdl_handle xdl_open(const char* filename, int flags) {
    if (have_libc_dl()) return __libc_dlopen_mode(filename, flags);
    if (have_std_dl()) return dlopen(filename, flags);
    return NULL;
}

void* xdl_sym(xdl_handle handle, const char* symbol) {
    if (have_libc_dl()) return __libc_dlsym(handle, symbol);
    if (have_std_dl()) return dlsym(handle, symbol);
    return NULL;
}

int xdl_close(xdl_handle handle) {
    if (have_libc_dl()) return __libc_dlclose ? __libc_dlclose(handle) : 0;
    if (have_std_dl()) return dlclose(handle);
    return -1;
}

const char* xdl_error(void) {
    if (have_libc_dl()) return __libc_dlerror ? __libc_dlerror() : "unknown";
    if (have_std_dl()) return dlerror();
    return "no dl interface available";
}

#endif
