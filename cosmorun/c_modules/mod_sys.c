/*
 * mod_sys.c - System module implementation using System Layer
 */

#include "mod_sys.h"
#include "../cosmorun_system/libc_shim/sys_libc_shim.h"


/* External environ variable */
extern char **environ;

/* ==================== Environment Variables ==================== */

const char* sys_getenv(const char* name) {
    if (!name) return NULL;  /* NULL safety */
    return getenv(name);
}

int sys_setenv(const char* name, const char* value, int overwrite) {
    if (!name || !value) return -1;
    return setenv(name, value, overwrite);
}

int sys_unsetenv(const char* name) {
    if (!name) return -1;
    return unsetenv(name);
}

char** sys_environ(void) {
    return environ;
}

/* ==================== Process Information ==================== */

int sys_getpid(void) {
    return getpid();
}

int sys_getppid(void) {
    return getppid();
}

/* ==================== System Information ==================== */

const SystemInfo* sys_get_system_info(void) {
    /* For mod_sys, we don't use the full system layer's SystemInfo */
    /* Return NULL or implement a simple version if needed */
    return NULL;
}

const char* sys_get_arch_name(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__i386__) || defined(_M_IX86)
    return "i386";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#else
    return "unknown";
#endif
}

const char* sys_get_os_name(void) {
#if defined(__APPLE__)
    return "darwin";
#elif defined(__linux__)
    return "linux";
#elif defined(_WIN32) || defined(_WIN64)
    return "windows";
#else
    return "unknown";
#endif
}

int sys_uname(sys_uname_t* info) {
    if (!info) {
        return -1;
    }

    /* Simplified implementation using compile-time and runtime info */
    const char *sysname = sys_get_os_name();
    const char *machine = sys_get_arch_name();

    /* Fill in system name */
    shim_strncpy(info->sysname, sysname, sizeof(info->sysname) - 1);
    info->sysname[sizeof(info->sysname) - 1] = '\0';

    /* Fill in nodename (hostname) - use "localhost" as fallback */
    const char *hostname = getenv("HOSTNAME");
    if (!hostname) hostname = getenv("COMPUTERNAME");
    if (!hostname) hostname = "localhost";
    shim_strncpy(info->nodename, hostname, sizeof(info->nodename) - 1);
    info->nodename[sizeof(info->nodename) - 1] = '\0';

    /* Fill in release and version - use "unknown" as fallback */
    shim_strncpy(info->release, "unknown", sizeof(info->release) - 1);
    info->release[sizeof(info->release) - 1] = '\0';

    shim_strncpy(info->version, "unknown", sizeof(info->version) - 1);
    info->version[sizeof(info->version) - 1] = '\0';

    /* Fill in machine architecture */
    shim_strncpy(info->machine, machine, sizeof(info->machine) - 1);
    info->machine[sizeof(info->machine) - 1] = '\0';

    return 0;
}

/* ==================== Signal Handling ==================== */

sys_signal_handler_t sys_signal(int signum, sys_signal_handler_t handler) {
    return (sys_signal_handler_t)signal(signum, (void(*)(int))handler);
}

int sys_kill(int pid, int sig) {
    return kill(pid, sig);
}

int sys_raise(int sig) {
    return raise(sig);
}

/* ==================== Resource Limits ==================== */

int sys_getrlimit(int resource, sys_rlimit_t *rlim) {
    if (!rlim) return -1;
    struct rlimit r;
    int ret = getrlimit(resource, &r);
    if (ret == 0) {
        rlim->soft = r.rlim_cur;
        rlim->hard = r.rlim_max;
    }
    return ret;
}

int sys_setrlimit(int resource, const sys_rlimit_t *rlim) {
    if (!rlim) return -1;
    struct rlimit r;
    r.rlim_cur = rlim->soft;
    r.rlim_max = rlim->hard;
    return setrlimit(resource, &r);
}

/* ==================== User and Group Information ==================== */

int sys_getuid(void) {
    return getuid();
}

int sys_geteuid(void) {
    return geteuid();
}

int sys_getgid(void) {
    return getgid();
}

int sys_getegid(void) {
    return getegid();
}

const char* sys_getusername(void) {
    /* Try USER first (Unix), then USERNAME (Windows) */
    const char *user = getenv("USER");
    if (!user) user = getenv("USERNAME");
    return user;
}

const char* sys_gethomedir(void) {
    /* Try HOME first (Unix), then USERPROFILE (Windows) */
    const char *home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    return home;
}

/* ==================== System Load Information ==================== */

int sys_getloadavg(double loadavg[3]) {
    if (!loadavg) return -1;
#ifdef __APPLE__
    return getloadavg(loadavg, 3) >= 0 ? 0 : -1;
#else
    /* On Linux, getloadavg returns number of samples, we want 0 on success */
    int n = getloadavg(loadavg, 3);
    return (n > 0) ? 0 : -1;
#endif
}

/* ==================== Signal Blocking ==================== */

int sys_sigblock(int signum) {
    /* Stub implementation - signal blocking requires platform-specific headers
     * that may not be available in TinyCC runtime environment */
    (void)signum;  /* Suppress unused warning */
    return -1;  /* Not implemented */
}

int sys_sigunblock(int signum) {
    /* Stub implementation - signal blocking requires platform-specific headers
     * that may not be available in TinyCC runtime environment */
    (void)signum;  /* Suppress unused warning */
    return -1;  /* Not implemented */
}

/* ==================== Module Initialization ==================== */

void mod_sys_init(void) {
    /* Currently nothing to initialize, but kept for future use */
    /* Note: The system layer's sys_init() is called separately */
}
