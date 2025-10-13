/* cosmo_trampoline.c - Cross-platform function call trampolines
 *
 * Extracted from cosmorun_tiny.c
 *
 * Two independent trampoline systems:
 * 1. Windows x86_64: System V -> Microsoft x64 calling convention
 * 2. ARM64: Variadic function argument marshalling
 */

#include "cosmo_libc.h"
#include "cosmo_trampoline.h"
#include "xdl.h"

// ============================================================================
// Windows x86_64 Calling Convention Trampolines
// ============================================================================
#ifdef __x86_64__

extern void __sysv2nt14(void);

typedef struct {
    void *orig;
    void *stub;
} WinThunkEntry;

#define COSMORUN_MAX_WIN_THUNKS 256

static WinThunkEntry g_win_thunks[COSMORUN_MAX_WIN_THUNKS];
static size_t g_win_thunk_count = 0;
static void *g_win_host_module = NULL;
static int g_win_initialized = 0;

static bool windows_address_is_executable(const void *addr) {
    struct NtMemoryBasicInformation info;
    if (!addr) return false;
    if (!VirtualQuery(addr, &info, sizeof(info))) {
        return false;
    }
    unsigned prot = info.Protect & 0xffu;
    switch (prot) {
        case kNtPageExecute:
        case kNtPageExecuteRead:
        case kNtPageExecuteReadwrite:
        case kNtPageExecuteWritecopy:
            return true;
        default:
            return false;
    }
}

static void *windows_make_trampoline(void *func) {
    static const unsigned char kTemplate[] = {
        0x55,                               /* push %rbp */
        0x48, 0x89, 0xE5,                   /* mov %rsp,%rbp */
        0x48, 0xB8,                         /* movabs $func,%rax */
        0, 0, 0, 0, 0, 0, 0, 0,             /* placeholder */
        0x49, 0xBA,                         /* movabs $__sysv2nt14,%r10 */
        0, 0, 0, 0, 0, 0, 0, 0,             /* placeholder */
        0x41, 0xFF, 0xE2                    /* jmp *%r10 */
    };

    void *mem = mmap(NULL, sizeof(kTemplate),
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        return NULL;
    }

    memcpy(mem, kTemplate, sizeof(kTemplate));
    memcpy((unsigned char *)mem + 6, &func, sizeof(void *));
    void *bridge = (void *)__sysv2nt14;
    memcpy((unsigned char *)mem + 16, &bridge, sizeof(void *));
    __builtin___clear_cache((char *)mem, (char *)mem + sizeof(kTemplate));
    return mem;
}

void cosmo_trampoline_win_init(void *host_module) {
    g_win_host_module = host_module;
    g_win_thunk_count = 0;
    g_win_initialized = 1;
}

static inline void ensure_win_initialized(void) {
    if (!g_win_initialized) {
        cosmo_trampoline_win_init(NULL);
    }
}

void *cosmo_trampoline_win_wrap(void *module, void *addr) {
    if (!addr) return NULL;
    if (!IsWindows()) return addr;

    ensure_win_initialized();

    if (!module || module == g_win_host_module) return addr;
    if (!windows_address_is_executable(addr)) return addr;

    // Check if we already have a trampoline for this address
    for (size_t i = 0; i < g_win_thunk_count; ++i) {
        if (g_win_thunks[i].orig == addr) {
            void *stub = g_win_thunks[i].stub;
            return stub ? stub : addr;
        }
    }

    // Create new trampoline
    void *stub = windows_make_trampoline(addr);
    if (stub) {
        if (g_win_thunk_count < COSMORUN_MAX_WIN_THUNKS) {
            g_win_thunks[g_win_thunk_count].orig = addr;
            g_win_thunks[g_win_thunk_count].stub = stub;
            ++g_win_thunk_count;
        }
    }
    return stub ? stub : addr;
}

size_t cosmo_trampoline_win_count(void) {
    return g_win_thunk_count;
}

#endif // __x86_64__

// ============================================================================
// ARM64 Variadic Function Trampolines
// ============================================================================
#ifdef __aarch64__

// macOS MAP_JIT flag for JIT code generation
#ifndef MAP_JIT
  #ifdef __APPLE__
    #define MAP_JIT 0x0800
  #else
    #define MAP_JIT 0
  #endif
#endif

#define ARM64_MAX_VARARGS_TRAMPOLINES 64

typedef struct {
    void *orig;          // Original variadic function
    void *stub;          // Our generated trampoline
    const char *name;    // For debugging
} ARM64VarargEntry;

static ARM64VarargEntry g_arm64_vararg_trampolines[ARM64_MAX_VARARGS_TRAMPOLINES];
static size_t g_arm64_vararg_count = 0;

// ============================================================================
// Universal Trampoline Template - Pre-compiled machine code
// ============================================================================
static const uint32_t g_trampoline_template[] = {
    0xa9bf7bfd,  // [0]  stp x29, x30, [sp, #-16]!
    0x910003fd,  // [1]  mov x29, sp
    0xd10103ff,  // [2]  sub sp, sp, #64
    0xf90003e1,  // [3]  str x1, [sp, #0]   - Will patch to nop if not needed
    0xf90007e2,  // [4]  str x2, [sp, #8]   - Will patch to nop if not needed
    0xf9000be3,  // [5]  str x3, [sp, #16]  - Will patch to nop if not needed
    0xf9000fe4,  // [6]  str x4, [sp, #24]
    0xf90013e5,  // [7]  str x5, [sp, #32]
    0xf90017e6,  // [8]  str x6, [sp, #40]
    0xf9001be7,  // [9]  str x7, [sp, #48]
    0x910003e3,  // [10] mov x3, sp        - Will patch register number
    0xd2800009,  // [11] movz x9, #0x0000  - Will patch vfunc bits [15:0]
    0xf2a00009,  // [12] movk x9, #0x0000, lsl #16 - Will patch [31:16]
    0xf2c00009,  // [13] movk x9, #0x0000, lsl #32 - Will patch [47:32]
    0xf2e00009,  // [14] movk x9, #0x0000, lsl #48 - Will patch [63:48]
    0xd63f0120,  // [15] blr x9
    0x910103ff,  // [16] add sp, sp, #64
    0xa8c17bfd,  // [17] ldp x29, x30, [sp], #16
    0xd65f03c0,  // [18] ret
};
#define TEMPLATE_SIZE sizeof(g_trampoline_template)

// Runtime patching of universal template
static void *arm64_make_vararg_trampoline(void *vfunc, int variadic_type) {
    uint64_t vfunc_addr = (uint64_t)vfunc;
    int first_var_reg = 4 - variadic_type;  // 3, 2, or 1

    // Allocate writable+executable memory
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef __APPLE__
    flags |= MAP_JIT;
#endif

    void *mem = mmap(NULL, TEMPLATE_SIZE, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (mem == MAP_FAILED) {
        return NULL;
    }

    uint32_t *code = (uint32_t*)mem;
    memcpy(code, g_trampoline_template, TEMPLATE_SIZE);

    // PATCH 1: Disable unneeded str instructions (x1, x2, x3)
    for (int reg = 1; reg < first_var_reg; reg++) {
        code[3 + (reg - 1)] = 0xd503201f;  // nop
    }

    // PATCH 2: Adjust offsets for remaining str instructions
    int offset = 0;
    for (int reg = first_var_reg; reg <= 7; reg++) {
        int idx = 3 + (reg - 1);
        code[idx] = 0xf90003e0 | reg | ((offset / 8) << 10);
        offset += 8;
    }

    // PATCH 3: Set va_list register
    code[10] = 0x910003e0 | first_var_reg;  // mov xN, sp

    // PATCH 4: Set vfunc address
    code[11] = 0xd2800009 | ((vfunc_addr & 0xffff) << 5);
    code[12] = 0xf2a00009 | (((vfunc_addr >> 16) & 0xffff) << 5);
    code[13] = 0xf2c00009 | (((vfunc_addr >> 32) & 0xffff) << 5);
    code[14] = 0xf2e00009 | (((vfunc_addr >> 48) & 0xffff) << 5);

    if (mprotect(mem, TEMPLATE_SIZE, PROT_READ | PROT_EXEC) != 0) {
        munmap(mem, TEMPLATE_SIZE);
        return NULL;
    }

    __builtin___clear_cache(mem, (char*)mem + TEMPLATE_SIZE);

    return mem;
}

void *cosmo_trampoline_arm64_vararg(void *vfunc, int variadic_type, const char *name) {
    if (!vfunc) return NULL;

    // Check if we already have a trampoline for this function
    for (size_t i = 0; i < g_arm64_vararg_count; ++i) {
        if (g_arm64_vararg_trampolines[i].orig == vfunc) {
            return g_arm64_vararg_trampolines[i].stub;
        }
    }

    // Create new trampoline
    void *stub = arm64_make_vararg_trampoline(vfunc, variadic_type);
    if (stub && g_arm64_vararg_count < ARM64_MAX_VARARGS_TRAMPOLINES) {
        g_arm64_vararg_trampolines[g_arm64_vararg_count].orig = vfunc;
        g_arm64_vararg_trampolines[g_arm64_vararg_count].stub = stub;
        g_arm64_vararg_trampolines[g_arm64_vararg_count].name = name;
        ++g_arm64_vararg_count;
    }

    return stub ? stub : vfunc;
}

size_t cosmo_trampoline_arm64_count(void) {
    return g_arm64_vararg_count;
}

#endif // __aarch64__

// ============================================================================
// Generic Interface
// ============================================================================

void cosmo_trampoline_init(void *host_module) {
#ifdef __x86_64__
    if (!g_win_initialized) {
        cosmo_trampoline_win_init(host_module);
    }
#else
    (void)host_module;
#endif
}

void *cosmo_trampoline_wrap(void *module, void *addr) {
    if (!addr) return NULL;

#ifdef __x86_64__
    // Windows CC conversion on x86_64
    // IsWindows(); should do better
    return cosmo_trampoline_win_wrap(module, addr);
#else
    // No automatic wrapping on other platforms
    (void)module;
    return addr;
#endif
}

// ============================================================================
// Libc Function Resolution with Automatic Trampoline
// ============================================================================

// Forward declarations for dlopen/dlsym
// Global library handles
static void* g_libc = NULL;
static void* g_libm = NULL;
static int g_libc_init = 0;

// Wrapper for dlsym with trampoline
static inline void *cosmorun_dlsym(void *handle, const char *symbol) {
    void *addr = xdl_sym(handle, symbol);
    return cosmo_trampoline_wrap(handle, addr);
}

// Wrapper for dlopen
static inline void *cosmorun_dlopen(const char *filename, int flags) {
    return xdl_open(filename, flags);
}

void cosmo_trampoline_libc_init(void) {
    if (g_libc_init) return;

    int dlopen_flag_win = 0;
    int dlopen_flag_unx = 0x101;  // RTLD_LAZY | RTLD_GLOBAL

    if (IsWindows()) {
        // Windows: load msvcrt.dll
        g_libc = cosmorun_dlopen("msvcrt.dll", dlopen_flag_win);
        g_libm = g_libc;  // Windows: math functions in msvcrt
    }
    else if (IsLinux()) {
        // Linux: load libc.so.6 + libm.so.6
        g_libc = cosmorun_dlopen("libc.so.6", dlopen_flag_unx);
        if (!g_libc) {
            g_libc = cosmorun_dlopen("libc.so", dlopen_flag_unx);
        }

        g_libm = cosmorun_dlopen("libm.so.6", dlopen_flag_unx);
        if (!g_libm) {
            g_libm = cosmorun_dlopen("libm.so", dlopen_flag_unx);
        }
    }
    else {
        // macOS/other Unix: load libSystem.B.dylib
        g_libc = cosmorun_dlopen("libSystem.B.dylib", dlopen_flag_unx);
        g_libm = g_libc;  // macOS: unified libSystem
    }

    g_libc_init = 1;
}

void *cosmo_trampoline_libc_resolve(const char *name, int variadic_type) {
    // Lazy initialization
    if (!g_libc_init) {
        cosmo_trampoline_libc_init();
    }

    // Resolve from libc/libm
    void* addr = NULL;
    if (g_libc) {
        addr = cosmorun_dlsym(g_libc, name);  // Already applies Windows CC conversion
    }
    if (!addr && g_libm) {
        addr = cosmorun_dlsym(g_libm, name);  // Already applies Windows CC conversion
    }

    if (!addr) {
        return NULL;
    }

#ifdef __aarch64__
    // ARM64: Create trampoline for variadic functions
    if (variadic_type) {
        // For variadic functions, resolve the v* variant (vsnprintf, vsprintf, vprintf)
        char vname[64];
        snprintf(vname, sizeof(vname), "v%s", name);
        void *vfunc = cosmorun_dlsym(g_libc, vname);
        if (!vfunc) {
            // Fallback to original function if v* variant not found
            return addr;
        }

        // Create trampoline for va_list marshalling
        void* trampoline = cosmo_trampoline_arm64_vararg(vfunc, variadic_type, name);
        if (trampoline) {
            return trampoline;
        }
        // Fallback: if trampoline creation failed, use direct call
    }
#else
    (void)variadic_type;  // Unused on non-ARM64 platforms
#endif

    return addr;
}

bool cosmo_trampoline_libc_is_initialized(void) {
    return g_libc_init != 0;
}
