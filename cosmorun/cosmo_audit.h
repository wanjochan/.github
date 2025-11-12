/* cosmo_audit.h - Security Audit Logging
 * Structured JSON logging for security-relevant operations
 */

#ifndef COSMO_AUDIT_H
#define COSMO_AUDIT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Audit configuration structure */
typedef struct {
    const char* log_path;       /* Path to audit log file */
    int verbose;                /* Enable detailed syscall logging */
    int use_syslog;             /* Also send to syslog/journald */
    size_t max_file_size;       /* Max log file size in bytes (default: 10MB) */
    int max_rotations;          /* Max number of rotated files to keep (default: 5) */
    int rate_limit;             /* Max events per second (default: 1000) */
} audit_config_t;

/* Event types for audit logging */
#define AUDIT_EVENT_PROGRAM_START    "program_start"
#define AUDIT_EVENT_PROGRAM_END      "program_end"
#define AUDIT_EVENT_FILE_OPEN        "file_open"
#define AUDIT_EVENT_FILE_READ        "file_read"
#define AUDIT_EVENT_FILE_WRITE       "file_write"
#define AUDIT_EVENT_FILE_EXEC        "file_exec"
#define AUDIT_EVENT_SYSCALL_VIOLATION "syscall_violation"
#define AUDIT_EVENT_SECURITY_CHECK_FAIL "security_check_fail"
#define AUDIT_EVENT_PACKAGE_INSTALL  "package_install"
#define AUDIT_EVENT_PACKAGE_PUBLISH  "package_publish"
#define AUDIT_EVENT_DEPENDENCY_RESOLVE "dependency_resolve"

/* Initialize audit logging system
 * Returns 0 on success, -1 on error
 */
int cosmo_audit_init(audit_config_t* config);

/* Log a security event
 * event_type: one of AUDIT_EVENT_* constants
 * details: JSON string with event-specific details (can be NULL)
 */
void cosmo_audit_log_event(const char* event_type, const char* details);

/* Log a formatted event (printf-style)
 * event_type: one of AUDIT_EVENT_* constants
 * fmt: printf-style format string
 * ...: variable arguments
 */
void cosmo_audit_log_eventf(const char* event_type, const char* fmt, ...);

/* Shutdown audit logging system (flush and close files) */
void cosmo_audit_shutdown(void);

/* Flush pending audit log entries to disk */
void cosmo_audit_flush(void);

/* Check if audit logging is enabled */
int cosmo_audit_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* COSMO_AUDIT_H */
