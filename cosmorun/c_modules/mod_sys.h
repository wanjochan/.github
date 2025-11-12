/*
 * mod_sys.h - System module interface
 * Provides system-level functionality using System Layer
 * 
 * v1 System Layer Integration:
 * - 使用 System Layer API (sys_*)
 * - 不再直接包含系统头文件
 * - 跨平台兼容 (Windows/Linux/macOS)
 */

#ifndef MOD_SYS_H
#define MOD_SYS_H

#include "../cosmorun_system/cosmo_system.h"

/* ==================== Environment Variables ==================== */

/* Get environment variable value */
const char* sys_getenv(const char* name);

/* Set environment variable (0 on success, -1 on error) */
int sys_setenv(const char* name, const char* value, int overwrite);

/* Unset environment variable (0 on success, -1 on error) */
int sys_unsetenv(const char* name);

/* ==================== Process Information ==================== */

/* Get process ID */
int sys_getpid(void);

/* Get parent process ID */
int sys_getppid(void);

/* ==================== System Information ==================== */

/* Get system information */
const SystemInfo* sys_get_system_info(void);

/* Get architecture name */
const char* sys_get_arch_name(void);

/* Get OS name */
const char* sys_get_os_name(void);

/* ==================== Signal Handling ==================== */

/* Signal handler type */
typedef void (*sys_signal_handler_t)(int);

/* Set signal handler */
sys_signal_handler_t sys_signal(int signum, sys_signal_handler_t handler);

/* Send signal to process */
int sys_kill(int pid, int sig);

/* Raise signal */
int sys_raise(int sig);

/* Signal constants */
#define SYS_SIGINT  2
#define SYS_SIGTERM 15
#define SYS_SIGKILL 9
#define SYS_SIGHUP  1

/* ==================== System Information ==================== */

/* System information structure (similar to struct utsname) */
typedef struct {
    char sysname[65];    /* Operating system name (e.g., "Linux") */
    char nodename[65];   /* Network node hostname */
    char release[65];    /* Operating system release */
    char version[65];    /* Operating system version */
    char machine[65];    /* Hardware identifier */
} sys_uname_t;

/* Get system information (returns 0 on success, -1 on error) */
int sys_uname(sys_uname_t* info);

/* ==================== Resource Limits ==================== */

/* Resource limit structure */
typedef struct {
    long soft;  /* Soft limit (rlim_cur) */
    long hard;  /* Hard limit (rlim_max) */
} sys_rlimit_t;

/* Get resource limits */
int sys_getrlimit(int resource, sys_rlimit_t *rlim);

/* Set resource limits */
int sys_setrlimit(int resource, const sys_rlimit_t *rlim);

/* Resource constants */
#define SYS_RLIMIT_CPU     0
#define SYS_RLIMIT_FSIZE   1
#define SYS_RLIMIT_DATA    2
#define SYS_RLIMIT_STACK   3
#define SYS_RLIMIT_CORE    4
#define SYS_RLIMIT_NOFILE  7  /* Max number of open files */
#define SYS_RLIMIT_AS      9  /* Address space limit */

/* ==================== User and Group Information ==================== */

/* Get user ID */
int sys_getuid(void);

/* Get effective user ID */
int sys_geteuid(void);

/* Get group ID */
int sys_getgid(void);

/* Get effective group ID */
int sys_getegid(void);

/* Get username (returns NULL on error) */
const char* sys_getusername(void);

/* Get home directory (returns NULL on error) */
const char* sys_gethomedir(void);

/* ==================== System Load Information ==================== */

/* Get system load average (returns 0 on success, -1 on error) */
int sys_getloadavg(double loadavg[3]);

/* ==================== Environment ==================== */

/* Get full environment array */
char** sys_environ(void);

/* ==================== Signal Blocking ==================== */

/* Block signal (returns 0 on success, -1 on error) */
int sys_sigblock(int signum);

/* Unblock signal (returns 0 on success, -1 on error) */
int sys_sigunblock(int signum);

/* ==================== Module Initialization ==================== */

/* Initialize module (optional, for compatibility) */
void mod_sys_init(void);

#endif /* MOD_SYS_H */
