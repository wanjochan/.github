/*
 * cosmo_symbol_stubs.c - Symbol Resolution Stub Library
 * P0.1: Provides stub implementations for commonly undefined Cosmopolitan symbols
 * 
 * This file provides weak symbols and no-op stubs for Cosmopolitan internals
 * to enable static linking without pulling in the entire Cosmopolitan library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* ===== Weak Symbol Definitions ===== */

/* Strace/tracing symbols - no-op implementations */
__attribute__((weak)) int strace_enabled = 0;
__attribute__((weak)) void __strace_init(void) {}
__attribute__((weak)) void ftrace_init(void) {}

/* Pthread zombification - stub */
__attribute__((weak)) void _pthread_zombify(void) {}
__attribute__((weak)) void _pthread_mutex_wipe_np(void* mutex) { (void)mutex; }

/* Describe functions - return placeholder strings */
__attribute__((weak)) const char* _DescribeErrno(int err) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "errno=%d", err);
    return buf;
}

__attribute__((weak)) const char* _DescribeTimespec(void* ts) {
    (void)ts;
    return "{timespec}";
}

__attribute__((weak)) const char* _DescribeClockName(int clk) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "clock_%d", clk);
    return buf;
}

__attribute__((weak)) const char* _DescribeSchedPolicy(int policy) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "sched_%d", policy);
    return buf;
}

__attribute__((weak)) const char* _DescribeSchedParam(void* param) {
    (void)param;
    return "{sched_param}";
}

__attribute__((weak)) const char* _DescribeSigaction(void* act) {
    (void)act;
    return "{sigaction}";
}

__attribute__((weak)) const char* _DescribeSigaltstack(void* stack) {
    (void)stack;
    return "{sigaltstack}";
}

__attribute__((weak)) const char* _DescribeBacktrace(void) {
    return "{backtrace}";
}

__attribute__((weak)) const char* _DescribeMapFlags(int flags) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "map_flags=0x%x", flags);
    return buf;
}

__attribute__((weak)) const char* _DescribeProtFlags(int prot) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "prot=0x%x", prot);
    return buf;
}

__attribute__((weak)) const char* _DescribeSigset(void* set) {
    (void)set;
    return "{sigset}";
}

__attribute__((weak)) const char* _DescribeFutexOp(int op) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "futex_op=%d", op);
    return buf;
}

__attribute__((weak)) const char* _DescribeMremapFlags(int flags) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "mremap_flags=0x%x", flags);
    return buf;
}

__attribute__((weak)) const char* _DescribeSockLevel(int level) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "sock_level=%d", level);
    return buf;
}

__attribute__((weak)) const char* _DescribeSockOptname(int opt) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "sockopt=%d", opt);
    return buf;
}

/* Clock constants */
__attribute__((weak)) const int CLOCK_REALTIME = 0;
__attribute__((weak)) const int CLOCK_MONOTONIC = 1;
__attribute__((weak)) const int CLOCK_PROCESS_CPUTIME_ID = 2;
__attribute__((weak)) const int CLOCK_THREAD_CPUTIME_ID = 3;
__attribute__((weak)) const int CLOCK_MONOTONIC_RAW = 4;
__attribute__((weak)) const int CLOCK_REALTIME_COARSE = 5;
__attribute__((weak)) const int CLOCK_MONOTONIC_COARSE = 6;
__attribute__((weak)) const int CLOCK_BOOTTIME = 7;

/* Timespec utility functions - stub implementations */
__attribute__((weak)) uint64_t timespec_fromnanos(uint64_t nanos) {
    return nanos;
}

__attribute__((weak)) uint64_t timespec_tonanos(void* ts) {
    (void)ts;
    return 0;
}

__attribute__((weak)) uint64_t timespec_tomillis(void* ts) {
    (void)ts;
    return 0;
}

__attribute__((weak)) uint64_t timespec_tomicros(void* ts) {
    (void)ts;
    return 0;
}

__attribute__((weak)) void timespec_totimeval(void* tv, void* ts) {
    (void)tv; (void)ts;
}

__attribute__((weak)) uint64_t timespec_frommillis(uint64_t millis) {
    return millis * 1000000ULL;
}

__attribute__((weak)) uint64_t timespec_frommicros(uint64_t micros) {
    return micros * 1000ULL;
}

/* System call stubs */
__attribute__((weak)) int sys_sigprocmask(int how, void* set, void* oldset) {
    (void)how; (void)set; (void)oldset;
    return 0;
}

__attribute__((weak)) int sys_clock_getres(int clk_id, void* res) {
    (void)clk_id; (void)res;
    return -1;  // Not implemented
}

__attribute__((weak)) int sys_clock_nanosleep(int clk, int flags, void* req, void* rem) {
    (void)clk; (void)flags; (void)req; (void)rem;
    return -1;
}

__attribute__((weak)) int sys_settimeofday(void* tv, void* tz) {
    (void)tv; (void)tz;
    return -1;
}

__attribute__((weak)) int sys_clock_settime(int clk, void* ts) {
    (void)clk; (void)ts;
    return -1;
}

__attribute__((weak)) int sys_sched_get_priority_min(int policy) {
    (void)policy;
    return 0;
}

__attribute__((weak)) int sys_sched_getparam(int pid, void* param) {
    (void)pid; (void)param;
    return -1;
}

__attribute__((weak)) int sys_sched_getscheduler(int pid) {
    (void)pid;
    return 0;
}

__attribute__((weak)) int sys_sched_getscheduler_netbsd(int pid) {
    (void)pid;
    return 0;
}

__attribute__((weak)) int sys_sched_setparam(int pid, void* param) {
    (void)pid; (void)param;
    return -1;
}

__attribute__((weak)) int sys_sched_setscheduler(int pid, int policy, void* param) {
    (void)pid; (void)policy; (void)param;
    return -1;
}

__attribute__((weak)) int sys_clock_gettime(int clk, void* ts) {
    (void)clk; (void)ts;
    return -1;
}

__attribute__((weak)) int sys_clock_gettime_nt(int clk, void* ts) {
    (void)clk; (void)ts;
    return -1;
}

__attribute__((weak)) int sys_clock_gettime_xnu(int clk, void* ts) {
    (void)clk; (void)ts;
    return -1;
}

__attribute__((weak)) int sys_sched_yield(void) {
    return 0;
}

__attribute__((weak)) int sys_gettid(void) {
    return 1;  // Fake TID
}

__attribute__((weak)) int sys_sigaltstack(void* ss, void* old_ss) {
    (void)ss; (void)old_ss;
    return -1;
}

__attribute__((weak)) int sys_bsdthread_register(void) {
    return -1;
}

__attribute__((weak)) int sys_sched_getaffinity(int pid, size_t cpusetsize, void* mask) {
    (void)pid; (void)cpusetsize; (void)mask;
    return -1;
}

__attribute__((weak)) int sys_mmap_metal(void* addr, size_t len, int prot, int flags, int fd, off_t offset) {
    (void)addr; (void)len; (void)prot; (void)flags; (void)fd; (void)offset;
    return -1;
}

__attribute__((weak)) int __sys_mprotect(void* addr, size_t len, int prot) {
    (void)addr; (void)len; (void)prot;
    return -1;
}

__attribute__((weak)) int sys_umtx_timedwait_uint(void* addr, unsigned val, int flags, void* timeout) {
    (void)addr; (void)val; (void)flags; (void)timeout;
    return -1;
}

__attribute__((weak)) int sys_getsockopt(int fd, int level, int optname, void* optval, void* optlen) {
    (void)fd; (void)level; (void)optname; (void)optval; (void)optlen;
    return -1;
}

__attribute__((weak)) int __sys_getsockname(int fd, void* addr, void* addrlen) {
    (void)fd; (void)addr; (void)addrlen;
    return -1;
}

__attribute__((weak)) int __sys_getpeername(int fd, void* addr, void* addrlen) {
    (void)fd; (void)addr; (void)addrlen;
    return -1;
}

/* Memory/signal platform stubs */
__attribute__((weak)) void __sigenter_wsl(void) {}
__attribute__((weak)) void __sigenter_netbsd(void) {}
__attribute__((weak)) void __sigenter_freebsd(void) {}
__attribute__((weak)) void __sigenter_openbsd(void) {}

/* NR syscall numbers */
__attribute__((weak)) const int __NR_sigaction = 13;
__attribute__((weak)) const int __NR_exit_group = 231;

/* nsync semaphore stubs */
__attribute__((weak)) void nsync_sem_wait_with_cancel_(void* sem) { (void)sem; }
__attribute__((weak)) void* cosmo_futex_thunk = NULL;

__attribute__((weak)) void nsync_mu_semaphore_init_sem(void* s) { (void)s; }
__attribute__((weak)) void nsync_mu_semaphore_init_futex(void* s) { (void)s; }
__attribute__((weak)) void nsync_mu_semaphore_destroy_sem(void* s) { (void)s; }
__attribute__((weak)) void nsync_mu_semaphore_destroy_futex(void* s) { (void)s; }
__attribute__((weak)) void nsync_mu_semaphore_p_sem(void* s) { (void)s; }
__attribute__((weak)) void nsync_mu_semaphore_p_futex(void* s) { (void)s; }
__attribute__((weak)) int nsync_mu_semaphore_p_with_deadline_sem(void* s, void* t) { (void)s; (void)t; return 0; }
__attribute__((weak)) int nsync_mu_semaphore_p_with_deadline_futex(void* s, void* t) { (void)s; (void)t; return 0; }
__attribute__((weak)) void nsync_mu_semaphore_v_sem(void* s) { (void)s; }
__attribute__((weak)) void nsync_mu_semaphore_v_futex(void* s) { (void)s; }

/* CXA thread finalization */
__attribute__((weak)) void __cxa_thread_finalize(void) {}

/* Debug functions */
__attribute__((weak)) void* FindDebugBinary(void) { return NULL; }

/* Misc stubs */
__attribute__((weak)) void* __sig_mask = NULL;
__attribute__((weak)) const int AT_SYSINFO_EHDR = 33;
__attribute__((weak)) int __get_minsigstksz(void) { return 8192; }

/* File I/O unlocked versions - forward to locked versions */
__attribute__((weak)) int vfprintf_unlocked(FILE* stream, const char* format, va_list ap) {
    return vfprintf(stream, format, ap);
}

__attribute__((weak)) size_t fwrite_unlocked(const void* ptr, size_t size, size_t n, FILE* stream) {
    return fwrite(ptr, size, n, stream);
}

__attribute__((weak)) int fflush_unlocked(FILE* stream) {
    return fflush(stream);
}

__attribute__((weak)) int fgetc_unlocked(FILE* stream) {
    return fgetc(stream);
}

__attribute__((weak)) int fputc_unlocked(int c, FILE* stream) {
    return fputc(c, stream);
}

/* Malloc/memory stubs */
__attribute__((weak)) void __dlmalloc_abort(void) {
    abort();
}

__attribute__((weak)) size_t __get_safe_size(size_t size, int extra) {
    (void)extra;
    return size;
}

/* Map flags */
__attribute__((weak)) const int MAP_FIXED_NOREPLACE = 0x100000;
__attribute__((weak)) const int MAP_SHARED_VALIDATE = 0x03;

/* Error codes */
__attribute__((weak)) const int EPROTONOSUPPORT = 93;
__attribute__((weak)) const int ESOCKTNOSUPPORT = 94;
__attribute__((weak)) const int ENOTRECOVERABLE = 131;
__attribute__((weak)) const int _POSIX_VDISABLE = 0;

/* Platform-specific stubs for Win32 functions */
#ifdef _WIN32
// Real Win32 symbols will be used on Windows
#else
// Provide weak stubs for non-Windows platforms
__attribute__((weak)) void* GetCurrentThread(void) { return NULL; }
__attribute__((weak)) int DuplicateHandle(void* a, void* b, void* c, void** d, int e, int f, int g) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g;
    return 0;
}
__attribute__((weak)) int GetThreadContext(void* thread, void* context) {
    (void)thread; (void)context;
    return 0;
}
__attribute__((weak)) int SetThreadContext(void* thread, void* context) {
    (void)thread; (void)context;
    return 0;
}
__attribute__((weak)) void* AddVectoredExceptionHandler(int first, void* handler) {
    (void)first; (void)handler;
    return NULL;
}
__attribute__((weak)) int SetConsoleCtrlHandler(void* handler, int add) {
    (void)handler; (void)add;
    return 0;
}
__attribute__((weak)) void* CreateFileMapping(void* file, void* attr, int protect, int high, int low, void* name) {
    (void)file; (void)attr; (void)protect; (void)high; (void)low; (void)name;
    return NULL;
}
__attribute__((weak)) int UnmapViewOfFile(void* addr) {
    (void)addr;
    return 0;
}
__attribute__((weak)) void* MapViewOfFileEx(void* mapping, int access, int high, int low, size_t bytes, void* addr) {
    (void)mapping; (void)access; (void)high; (void)low; (void)bytes; (void)addr;
    return NULL;
}
__attribute__((weak)) int VirtualProtect(void* addr, size_t size, int newprot, void* oldprot) {
    (void)addr; (void)size; (void)newprot; (void)oldprot;
    return 0;
}
__attribute__((weak)) int GetConsoleMode(void* handle, void* mode) {
    (void)handle; (void)mode;
    return 0;
}
__attribute__((weak)) void* VirtualAllocEx(void* proc, void* addr, size_t size, int type, int protect) {
    (void)proc; (void)addr; (void)size; (void)type; (void)protect;
    return NULL;
}
__attribute__((weak)) int DeviceIoControl(void* dev, int code, void* in, int insize, void* out, int outsize, void* ret, void* overlap) {
    (void)dev; (void)code; (void)in; (void)insize; (void)out; (void)outsize; (void)ret; (void)overlap;
    return 0;
}
__attribute__((weak)) int GetFileInformationByHandle(void* file, void* info) {
    (void)file; (void)info;
    return 0;
}
__attribute__((weak)) void WindowsTimeToTimeSpec(void* ts, uint64_t wintime) {
    (void)ts; (void)wintime;
}
__attribute__((weak)) int GetVolumeInformationByHandle(void* file, void* name, int namelen, void* serial, void* maxlen, void* flags, void* fsname, int fsnamelen) {
    (void)file; (void)name; (void)namelen; (void)serial; (void)maxlen; (void)flags; (void)fsname; (void)fsnamelen;
    return 0;
}
__attribute__((weak)) int GetFileInformationByHandleEx(void* file, int class, void* info, int size) {
    (void)file; (void)class; (void)info; (void)size;
    return 0;
}
__attribute__((weak)) int GetOverlappedResult(void* file, void* overlap, void* bytes, int wait) {
    (void)file; (void)overlap; (void)bytes; (void)wait;
    return 0;
}
__attribute__((weak)) int WaitForMultipleObjects(int count, void** handles, int all, int timeout) {
    (void)count; (void)handles; (void)all; (void)timeout;
    return 0;
}
__attribute__((weak)) void WakeByAddressAll(void* addr) {
    (void)addr;
}
__attribute__((weak)) void WakeByAddressSingle(void* addr) {
    (void)addr;
}
__attribute__((weak)) void TerminateThisProcess(int code) {
    exit(code);
}
__attribute__((weak)) int GetCurrentProcessorNumberEx(void* proc_number) {
    (void)proc_number;
    return 0;
}
__attribute__((weak)) int GetMaximumProcessorCount(int group) {
    (void)group;
    return 1;
}

/* __imp_ prefixed stubs */
__attribute__((weak)) void* __imp_GetCurrentThreadId = NULL;
__attribute__((weak)) void* __imp_WakeByAddressAll = NULL;
__attribute__((weak)) void* __imp_GetEnvironmentVariableW = NULL;
__attribute__((weak)) void* __imp_DuplicateHandle = NULL;
__attribute__((weak)) void* __imp_GetOverlappedResult = NULL;
__attribute__((weak)) void* __imp_VirtualProtectEx = NULL;

#endif /* !_WIN32 */

/* OpenMP/KMP stubs - for C++ parallel support */
__attribute__((weak)) int __kmp_wait_64(void) { return 0; }
__attribute__((weak)) int __kmp_release_64(void) { return 0; }
__attribute__((weak)) void* __kmp_env_get(const char* name) { (void)name; return NULL; }
__attribute__((weak)) void __kmp_threadprivate_resize_cache(int size) { (void)size; }
__attribute__((weak)) void __kmp_common_destroy_gtid(int gtid) { (void)gtid; }
__attribute__((weak)) void __kmp_cleanup_threadprivate_caches(void) {}
__attribute__((weak)) void __kmp_common_initialize(void) {}
__attribute__((weak)) void __kmp_env_blk_init(void) {}
__attribute__((weak)) void* __kmp_env_blk_var(int idx) { (void)idx; return NULL; }
__attribute__((weak)) void __kmp_env_blk_free(void) {}
__attribute__((weak)) void __kmp_env_blk_sort(void) {}

/* Networking stubs */
__attribute__((weak)) int TCP_FASTOPEN_CONNECT = 30;
__attribute__((weak)) int SIOCGIFNETMASK = 0x891b;
__attribute__((weak)) int SIOCGIFBRDADDR = 0x8919;
__attribute__((weak)) int IFF_POINTOPOINT = 0x10;
__attribute__((weak)) int SIOCGIFDSTADDR = 0x8917;
__attribute__((weak)) int AT_SYMLINK_NOFOLLOW = 0x100;

__attribute__((weak)) void sockaddr2linux(void* linux_addr, void* addr, int addrlen) {
    (void)linux_addr; (void)addr; (void)addrlen;
}

__attribute__((weak)) int sys_sendfile_xnu(int out_fd, int in_fd, off_t* offset, size_t count) {
    (void)out_fd; (void)in_fd; (void)offset; (void)count;
    return -1;
}

__attribute__((weak)) int sys_sendfile_freebsd(int out_fd, int in_fd, off_t* offset, size_t count) {
    (void)out_fd; (void)in_fd; (void)offset; (void)count;
    return -1;
}

/* Utility stubs */
__attribute__((weak)) uint32_t _Cz_crc32_sse42_simd_(uint32_t crc, const void* buf, size_t len) {
    (void)crc; (void)buf; (void)len;
    return 0;
}

__attribute__((weak)) uint32_t crc32_avx512_simd_(uint32_t crc, const void* buf, size_t len) {
    (void)crc; (void)buf; (void)len;
    return 0;
}

__attribute__((weak)) int gethostbyaddr_r(const void* addr, int len, int type, void* result, char* buf, size_t buflen, void** res, int* h_err) {
    (void)addr; (void)len; (void)type; (void)result; (void)buf; (void)buflen; (void)res; (void)h_err;
    return -1;
}

__attribute__((weak)) void* gethostbyname2(const char* name, int af) {
    (void)name; (void)af;
    return NULL;
}

__attribute__((weak)) const char* GetHostsTxtPath(void) {
    return "/etc/hosts";
}

__attribute__((weak)) void* __lookup_ipliteral(void* buf, const char* name, int family) {
    (void)buf; (void)name; (void)family;
    return NULL;
}

__attribute__((weak)) const char* GetServicesTxtPath(void) {
    return "/etc/services";
}

__attribute__((weak)) int getservbyname_r(const char* name, const char* proto, void* result, char* buf, size_t buflen, void** res) {
    (void)name; (void)proto; (void)result; (void)buf; (void)buflen; (void)res;
    return -1;
}

__attribute__((weak)) int getservbyport_r(int port, const char* proto, void* result, char* buf, size_t buflen, void** res) {
    (void)port; (void)proto; (void)result; (void)buf; (void)buflen; (void)res;
    return -1;
}

/* Time conversion stubs */
__attribute__((weak)) long __year_to_secs(long long year, int* is_leap) {
    (void)year; (void)is_leap;
    return 0;
}

__attribute__((weak)) int __month_to_secs(int month, int is_leap) {
    (void)month; (void)is_leap;
    return 0;
}

/* Crypto stub */
__attribute__((weak)) int __crypt_blowfish(const char* key, const char* setting, char* output) {
    (void)key; (void)setting; (void)output;
    return -1;
}

/* Misc platform stubs */
__attribute__((weak)) void* kNtIsInheritable = NULL;
__attribute__((weak)) void* _vga_font_default_direct = NULL;

__attribute__((weak)) void* kEscapeAuthority = NULL;
__attribute__((weak)) void* kEscapeFragment = NULL;
__attribute__((weak)) void* kEscapeSegment = NULL;

__attribute__((weak)) void* RegisterEventSource(void* server, const char* source) {
    (void)server; (void)source;
    return NULL;
}

__attribute__((weak)) int DeregisterEventSource(void* source) {
    (void)source;
    return 0;
}

/* Memory copy (usually provided by compiler/libc, but add weak stub) */
__attribute__((weak)) void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

/* C++ symbols - weak vtables and typeinfo */
__attribute__((weak)) void _ZTVN10__cxxabiv117__class_type_infoE(void) {}
__attribute__((weak)) void _ZTVN10__cxxabiv121__vmi_class_type_infoE(void) {}
__attribute__((weak)) void _ZTVN10__cxxabiv120__si_class_type_infoE(void) {}
__attribute__((weak)) void __gxx_personality_v0(void) {}

/* C++ exception/runtime stubs */
__attribute__((weak)) void __cxa_throw_bad_array_new_length(void) {
    abort();
}

__attribute__((weak)) void* __dynamic_cast(void* sub, void* src, void* dst, long src2dst_offset) {
    (void)sub; (void)src; (void)dst; (void)src2dst_offset;
    return NULL;
}

/* Standard C++ exception types */
__attribute__((weak)) void _ZNSt8bad_castD2Ev(void) {}
__attribute__((weak)) void _ZNSt8bad_castC1Ev(void) {}
__attribute__((weak)) void _ZNSt8bad_castD1Ev(void) {}
__attribute__((weak)) void* _ZTISt8bad_cast = NULL;

__attribute__((weak)) void _ZNSt9exceptionD2Ev(void) {}
__attribute__((weak)) void* _ZTISt9exception = NULL;
__attribute__((weak)) const char* _ZNKSt9exception4whatEv(void) { return "exception"; }

__attribute__((weak)) void _ZNSt11logic_errorD2Ev(void) {}
__attribute__((weak)) void* _ZTISt11logic_error = NULL;
__attribute__((weak)) const char* _ZNKSt11logic_error4whatEv(void) { return "logic_error"; }

__attribute__((weak)) void _ZNSt13runtime_errorD2Ev(void) {}
__attribute__((weak)) void _ZNSt13runtime_errorD1Ev(void) {}
__attribute__((weak)) void* _ZTISt13runtime_error = NULL;
__attribute__((weak)) const char* _ZNKSt13runtime_error4whatEv(void) { return "runtime_error"; }

__attribute__((weak)) void _ZNSt9bad_allocC1Ev(void) {}
__attribute__((weak)) void _ZNSt9bad_allocD1Ev(void) {}
__attribute__((weak)) void* _ZTISt9bad_alloc = NULL;

__attribute__((weak)) void* _ZTVSt11logic_error = NULL;
__attribute__((weak)) void* _ZTVSt13runtime_error = NULL;
__attribute__((weak)) void* _ZTVSt14overflow_error = NULL;
__attribute__((weak)) void _ZNSt14overflow_errorD1Ev(void) {}
__attribute__((weak)) void* _ZTISt14overflow_error = NULL;

__attribute__((weak)) void* _ZTVSt12length_error = NULL;
__attribute__((weak)) void _ZNSt12length_errorD1Ev(void) {}
__attribute__((weak)) void* _ZTISt12length_error = NULL;

__attribute__((weak)) void* _ZTVSt12out_of_range = NULL;
__attribute__((weak)) void _ZNSt12out_of_rangeD1Ev(void) {}
__attribute__((weak)) void* _ZTISt12out_of_range = NULL;

__attribute__((weak)) void* _ZTVSt16invalid_argument = NULL;
__attribute__((weak)) void _ZNSt16invalid_argumentD1Ev(void) {}
__attribute__((weak)) void* _ZTISt16invalid_argument = NULL;

__attribute__((weak)) void* _ZTISt20bad_array_new_length = NULL;
__attribute__((weak)) void _ZNSt20bad_array_new_lengthC1Ev(void) {}
__attribute__((weak)) void _ZNSt20bad_array_new_lengthD1Ev(void) {}

/* C++ new/delete operators */
__attribute__((weak)) void* _Znwm(size_t size) {
    return malloc(size);
}

__attribute__((weak)) void _ZdlPvm(void* ptr, size_t size) {
    (void)size;
    free(ptr);
}

__attribute__((weak)) void _ZdaPv(void* ptr) {
    free(ptr);
}

__attribute__((weak)) void* _ZnamSt11align_val_t(size_t size, size_t align) {
    (void)align;
    return malloc(size);
}

__attribute__((weak)) void _ZdaPvSt11align_val_t(void* ptr, size_t align) {
    (void)align;
    free(ptr);
}

/* End of cosmo_symbol_stubs.c */

/* ===== Part 2: Additional Stubs for Remaining Symbols ===== */

/* ACPI symbols */
__attribute__((weak)) int _AcpiBootFlags = 0;
__attribute__((weak)) void* _AcpiIoApics = NULL;
__attribute__((weak)) int _AcpiMadtFlags = 0;
__attribute__((weak)) int _AcpiNumIoApics = 0;
__attribute__((weak)) void* _AcpiXsdtEntries = NULL;
__attribute__((weak)) int _AcpiXsdtNumEntries = 0;

/* AT auxiliary vector constants */
__attribute__((weak)) const int AT_BASE_PLATFORM = 24;
__attribute__((weak)) const int AT_DCACHEBSIZE = 19;
__attribute__((weak)) const int AT_ICACHEBSIZE = 20;
__attribute__((weak)) const int AT_MINSIGSTKSZ = 51;
__attribute__((weak)) const int AT_PAGESIZESLEN = 28;
__attribute__((weak)) const int AT_UCACHEBSIZE = 21;

/* File control constants */
__attribute__((weak)) const int F_BARRIERFSYNC = 85;
__attribute__((weak)) const int F_DUPFD_CLOEXEC = 1030;
__attribute__((weak)) const int F_GETNOSIGPIPE = 74;
__attribute__((weak)) const int F_SETNOSIGPIPE = 75;

/* Additional _Describe functions */
__attribute__((weak)) const char* _DescribeDnotifyFlags(int flags) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "dnotify_flags=0x%x", flags);
    return buf;
}

__attribute__((weak)) const char* _DescribeFcntlCmd(int cmd) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "fcntl_cmd=%d", cmd);
    return buf;
}

__attribute__((weak)) const char* _DescribeFlockType(int type) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "flock_type=%d", type);
    return buf;
}

__attribute__((weak)) const char* _DescribeGidList(void* gids, int count) {
    (void)gids; (void)count;
    return "{gid_list}";
}

__attribute__((weak)) const char* _DescribeInOutInt64(void* ptr) {
    (void)ptr;
    return "{int64}";
}

__attribute__((weak)) const char* _DescribeItimer(int which) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "itimer=%d", which);
    return buf;
}

__attribute__((weak)) const char* _DescribeItimerval(void* it) {
    (void)it;
    return "{itimerval}";
}

__attribute__((weak)) const char* _DescribeMapping(void* mapping) {
    (void)mapping;
    return "{mapping}";
}

__attribute__((weak)) const char* _DescribeMsyncFlags(int flags) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "msync_flags=0x%x", flags);
    return buf;
}

__attribute__((weak)) const char* _DescribeNtConsoleInFlags(int flags) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "console_in_flags=0x%x", flags);
    return buf;
}

__attribute__((weak)) const char* _DescribeNtConsoleOutFlags(int flags) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "console_out_flags=0x%x", flags);
    return buf;
}

__attribute__((weak)) const char* _DescribeOpenFlags(int flags) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "open_flags=0x%x", flags);
    return buf;
}

__attribute__((weak)) const char* _DescribeOpenMode(int mode) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "mode=0%o", mode);
    return buf;
}

__attribute__((weak)) const char* _DescribePollFds(void* fds, int nfds) {
    (void)fds; (void)nfds;
    return "{pollfds}";
}

__attribute__((weak)) const char* _DescribePtrace(int request) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "ptrace=%d", request);
    return buf;
}

__attribute__((weak)) const char* _DescribeRlimit(void* rlim) {
    (void)rlim;
    return "{rlimit}";
}

__attribute__((weak)) const char* _DescribeRlimitName(int resource) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "rlimit_%d", resource);
    return buf;
}

__attribute__((weak)) const char* _DescribeSeccompOperation(int op) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "seccomp_op=%d", op);
    return buf;
}

__attribute__((weak)) const char* _DescribeSiCode(int code) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "si_code=%d", code);
    return buf;
}

__attribute__((weak)) const char* _DescribeSiginfo(void* si) {
    (void)si;
    return "{siginfo}";
}

__attribute__((weak)) const char* _DescribeSocketFamily(int family) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "family=%d", family);
    return buf;
}

__attribute__((weak)) const char* _DescribeSocketProtocol(int proto) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "proto=%d", proto);
    return buf;
}

__attribute__((weak)) const char* _DescribeSocketType(int type) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "type=%d", type);
    return buf;
}

__attribute__((weak)) const char* _DescribeStatfs(void* stat) {
    (void)stat;
    return "{statfs}";
}

__attribute__((weak)) const char* _DescribeStringList(char** list) {
    (void)list;
    return "{string_list}";
}

__attribute__((weak)) const char* _DescribeTermios(void* termios) {
    (void)termios;
    return "{termios}";
}

__attribute__((weak)) const char* _DescribeTimeval(void* tv) {
    (void)tv;
    return "{timeval}";
}

__attribute__((weak)) const char* _DescribeWhence(int whence) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "whence=%d", whence);
    return buf;
}

__attribute__((weak)) const char* _DescribeWhichPrio(int which) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "which_prio=%d", which);
    return buf;
}

__attribute__((weak)) const char* _DescribeWinsize(void* ws) {
    (void)ws;
    return "{winsize}";
}

/* Unlocked stdio functions */
__attribute__((weak)) void clearerr_unlocked(FILE* stream) {
    clearerr(stream);
}

__attribute__((weak)) int ferror_unlocked(FILE* stream) {
    return ferror(stream);
}

__attribute__((weak)) int fileno_unlocked(FILE* stream) {
    return fileno(stream);
}

__attribute__((weak)) char* fgets_unlocked(char* s, int n, FILE* stream) {
    return fgets(s, n, stream);
}

__attribute__((weak)) int fprintf_unlocked(FILE* stream, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

__attribute__((weak)) int fputs_unlocked(const char* s, FILE* stream) {
    return fputs(s, stream);
}

__attribute__((weak)) size_t fread_unlocked(void* ptr, size_t size, size_t n, FILE* stream) {
    return fread(ptr, size, n, stream);
}

__attribute__((weak)) int fseek_unlocked(FILE* stream, long offset, int whence) {
    return fseek(stream, offset, whence);
}

/* Wide character stdio stubs */
__attribute__((weak)) int fgetwc_unlocked(FILE* stream) {
    (void)stream;
    return -1;
}

__attribute__((weak)) int* fgetws_unlocked(int* ws, int n, FILE* stream) {
    (void)ws; (void)n; (void)stream;
    return NULL;
}

__attribute__((weak)) int fputwc_unlocked(int wc, FILE* stream) {
    (void)wc; (void)stream;
    return -1;
}

__attribute__((weak)) int fputs_unlocked(const int* ws, FILE* stream) {
    (void)ws; (void)stream;
    return -1;
}

__attribute__((weak)) ssize_t getdelim_unlocked(char** lineptr, size_t* n, int delimiter, FILE* stream) {
    (void)lineptr; (void)n; (void)delimiter; (void)stream;
    return -1;
}

/* Critbit tree stubs */
__attribute__((weak)) void critbit0_clear(void* tree) {
    (void)tree;
}

__attribute__((weak)) int critbit0_emplace(void* tree, const char* key) {
    (void)tree; (void)key;
    return 0;
}

/* CXA exception handling */
__attribute__((weak)) void* __cxa_get_globals(void) {
    return NULL;
}

__attribute__((weak)) void* __cxa_get_globals_fast(void) {
    return NULL;
}

__attribute__((weak)) void* __cxa_new_handler = NULL;
__attribute__((weak)) void* __cxa_terminate_handler = NULL;
__attribute__((weak)) void* __cxa_unexpected_handler = NULL;

/* Ftrace/strace stubs */
__attribute__((weak)) int ftrace_enabled = 0;
__attribute__((weak)) int ftrace_stackdigs = 0;

/* Socket utilities */
__attribute__((weak)) void __fixupnewsockfd(int fd) {
    (void)fd;
}

/* GetElfXXX functions */
__attribute__((weak)) void* GetElfSectionAddress(void* elf, const char* name) {
    (void)elf; (void)name;
    return NULL;
}

__attribute__((weak)) void* GetElfSymbolTable(void* elf) {
    (void)elf;
    return NULL;
}

/* System info functions */
__attribute__((weak)) long __get_avphys_pages(void) {
    return 1024;  // Fake value
}

__attribute__((weak)) int getdomainname_linux(char* name, size_t len) {
    (void)name; (void)len;
    return -1;
}

__attribute__((weak)) int gethostname_bsd(char* name, size_t len) {
    (void)name; (void)len;
    return -1;
}

/* Additional Win32 stubs */
#ifndef _WIN32
__attribute__((weak)) int AdjustTokenPrivileges(void* token, int disable, void* newstate, int buflen, void* prevstate, void* retlen) {
    (void)token; (void)disable; (void)newstate; (void)buflen; (void)prevstate; (void)retlen;
    return 0;
}

__attribute__((weak)) int ClearCommBreak(void* file) {
    (void)file;
    return 0;
}

__attribute__((weak)) int CreateDirectory(const char* path, void* security) {
    (void)path; (void)security;
    return 0;
}

__attribute__((weak)) int CreateHardLink(const char* new_path, const char* existing_path, void* security) {
    (void)new_path; (void)existing_path; (void)security;
    return 0;
}

__attribute__((weak)) void* CreateNamedPipe(const char* name, int open_mode, int pipe_mode, int max_instances, int out_buffer, int in_buffer, int timeout, void* security) {
    (void)name; (void)open_mode; (void)pipe_mode; (void)max_instances; (void)out_buffer; (void)in_buffer; (void)timeout; (void)security;
    return NULL;
}

__attribute__((weak)) char* __create_pipe_name(char* buf, size_t len) {
    (void)buf; (void)len;
    return NULL;
}

__attribute__((weak)) int CreateSymbolicLink(const char* link, const char* target, int flags) {
    (void)link; (void)target; (void)flags;
    return 0;
}

__attribute__((weak)) void* CreateWaitableTimer(void* security, int manual_reset, const char* name) {
    (void)security; (void)manual_reset; (void)name;
    return NULL;
}

__attribute__((weak)) int DeleteProcThreadAttributeList(void* list) {
    (void)list;
    return 0;
}

__attribute__((weak)) int DuplicateToken(void* existing, int level, void** duplicate) {
    (void)existing; (void)level; (void)duplicate;
    return 0;
}

__attribute__((weak)) int FlushConsoleInputBuffer(void* console) {
    (void)console;
    return 0;
}

__attribute__((weak)) int FlushFileBuffers(void* file) {
    (void)file;
    return 0;
}

__attribute__((weak)) int FlushViewOfFile(void* addr, size_t bytes) {
    (void)addr; (void)bytes;
    return 0;
}

__attribute__((weak)) int GetAdaptersAddresses(int family, int flags, void* reserved, void* addresses, void* size_ptr) {
    (void)family; (void)flags; (void)reserved; (void)addresses; (void)size_ptr;
    return -1;
}

__attribute__((weak)) int GetComputerNameEx(int type, char* buffer, void* size) {
    (void)type; (void)buffer; (void)size;
    return 0;
}

__attribute__((weak)) int GetCurrentDirectory(int buflen, char* buf) {
    (void)buflen; (void)buf;
    return 0;
}

__attribute__((weak)) int GetCurrentProcessId(void) {
    return 1;  // Fake PID
}

__attribute__((weak)) int GetExitCodeProcess(void* process, void* exit_code) {
    (void)process; (void)exit_code;
    return 0;
}

__attribute__((weak)) int GetFileAttributes(const char* filename) {
    (void)filename;
    return -1;
}

__attribute__((weak)) int GetFileAttributesEx(const char* filename, int info_level, void* file_info) {
    (void)filename; (void)info_level; (void)file_info;
    return 0;
}

__attribute__((weak)) int GetFileSecurity(const char* filename, int requested_info, void* security_desc, int length, void* length_needed) {
    (void)filename; (void)requested_info; (void)security_desc; (void)length; (void)length_needed;
    return 0;
}

__attribute__((weak)) int GetFinalPathNameByHandle(void* file, char* path, int path_len, int flags) {
    (void)file; (void)path; (void)path_len; (void)flags;
    return 0;
}
#endif /* !_WIN32 */

/* Audio/DSP path symbols - treat as data */
__attribute__((weak)) const char dsp_audio_cosmoaudio_cosmoaudio_c[] = "dsp/audio/cosmoaudio/cosmoaudio.c";
__attribute__((weak)) const char dsp_audio_cosmoaudio_cosmoaudio_dll[] = "dsp/audio/cosmoaudio/cosmoaudio.dll";
__attribute__((weak)) const char dsp_audio_cosmoaudio_cosmoaudio_h[] = "dsp/audio/cosmoaudio/cosmoaudio.h";
__attribute__((weak)) const char dsp_audio_cosmoaudio_miniaudio_h[] = "dsp/audio/cosmoaudio/miniaudio.h";

/* End of Part 2 */
