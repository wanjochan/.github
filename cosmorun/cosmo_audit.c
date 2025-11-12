/* cosmo_audit.c - Security Audit Logging Implementation
 * Structured JSON logging with rotation, rate limiting, and syslog support
 */

#include "cosmo_audit.h"
#include "cosmo_libc.h"
#include <stdarg.h>

/* Platform-specific includes */
#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    #include <syslog.h>
    #include <pthread.h>
    #define HAVE_SYSLOG 1
    #define HAVE_PTHREAD 1
#else
    #define HAVE_SYSLOG 0
    #define HAVE_PTHREAD 0
#endif

/* Default configuration values */
#define AUDIT_DEFAULT_MAX_SIZE      (10 * 1024 * 1024)  /* 10MB */
#define AUDIT_DEFAULT_MAX_ROTATIONS  5
#define AUDIT_DEFAULT_RATE_LIMIT     1000  /* events/sec */
#define AUDIT_BUFFER_SIZE            4096

/* Global audit state */
static struct {
    int enabled;
    FILE* log_file;
    char* log_path;
    int verbose;
    int use_syslog;
    size_t max_file_size;
    int max_rotations;
    int rate_limit;

    /* Rate limiting state */
    time_t rate_window_start;
    int events_in_window;

    /* Thread safety */
#if HAVE_PTHREAD
    pthread_mutex_t lock;
#endif

    /* Stats */
    size_t current_file_size;
    unsigned long total_events;
    unsigned long dropped_events;
} g_audit = {0};

/* Thread-safe logging macros */
#if HAVE_PTHREAD
    #define AUDIT_LOCK()   pthread_mutex_lock(&g_audit.lock)
    #define AUDIT_UNLOCK() pthread_mutex_unlock(&g_audit.lock)
#else
    #define AUDIT_LOCK()   ((void)0)
    #define AUDIT_UNLOCK() ((void)0)
#endif

/* Get current timestamp in ISO 8601 format */
static void get_iso8601_timestamp(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);
    if (tm_info) {
        strftime(buffer, size, "%Y-%m-%dT%H:%M:%SZ", tm_info);
    } else {
        snprintf(buffer, size, "1970-01-01T00:00:00Z");
    }
}

/* Get current username */
static const char* get_username(void) {
#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    const char* user = getenv("USER");
    if (!user) user = getenv("LOGNAME");
    if (!user) user = "unknown";
    return user;
#else
    const char* user = getenv("USERNAME");
    if (!user) user = "unknown";
    return user;
#endif
}

/* Escape JSON string (basic implementation) */
static void json_escape(const char* input, char* output, size_t output_size) {
    size_t i = 0, j = 0;

    while (input[i] && j < output_size - 2) {
        char c = input[i];
        if (c == '"' || c == '\\') {
            output[j++] = '\\';
            output[j++] = c;
        } else if (c == '\n') {
            output[j++] = '\\';
            output[j++] = 'n';
        } else if (c == '\r') {
            output[j++] = '\\';
            output[j++] = 'r';
        } else if (c == '\t') {
            output[j++] = '\\';
            output[j++] = 't';
        } else if ((unsigned char)c < 32) {
            /* Skip control characters */
            i++;
            continue;
        } else {
            output[j++] = c;
        }
        i++;
    }
    output[j] = '\0';
}

/* Rotate log files */
static int rotate_log_files(void) {
    if (!g_audit.log_path) return -1;

    char old_path[AUDIT_BUFFER_SIZE];
    char new_path[AUDIT_BUFFER_SIZE];

    /* Close current file */
    if (g_audit.log_file) {
        fclose(g_audit.log_file);
        g_audit.log_file = NULL;
    }

    /* Remove oldest rotation if exists */
    snprintf(old_path, sizeof(old_path), "%s.%d", g_audit.log_path, g_audit.max_rotations);
    remove(old_path);

    /* Rotate existing files */
    for (int i = g_audit.max_rotations - 1; i >= 1; i--) {
        snprintf(old_path, sizeof(old_path), "%s.%d", g_audit.log_path, i);
        snprintf(new_path, sizeof(new_path), "%s.%d", g_audit.log_path, i + 1);
        rename(old_path, new_path);
    }

    /* Rotate current file to .1 */
    snprintf(new_path, sizeof(new_path), "%s.1", g_audit.log_path);
    rename(g_audit.log_path, new_path);

    /* Open new file */
    g_audit.log_file = fopen(g_audit.log_path, "a");
    if (!g_audit.log_file) {
        return -1;
    }

    g_audit.current_file_size = 0;
    return 0;
}

/* Check and enforce rate limiting
 * Returns 1 if event should be logged, 0 if dropped
 */
static int check_rate_limit(void) {
    if (g_audit.rate_limit <= 0) {
        return 1;  /* No rate limiting */
    }

    time_t now = time(NULL);

    /* Reset window if a second has passed */
    if (now != g_audit.rate_window_start) {
        g_audit.rate_window_start = now;
        g_audit.events_in_window = 0;
    }

    /* Check if we're over the limit */
    if (g_audit.events_in_window >= g_audit.rate_limit) {
        g_audit.dropped_events++;
        return 0;  /* Drop this event */
    }

    g_audit.events_in_window++;
    return 1;
}

/* Write log entry to file */
static void write_log_entry(const char* json_entry) {
    if (!g_audit.log_file) return;

    size_t len = strlen(json_entry);

    /* Check if rotation needed */
    if (g_audit.current_file_size + len > g_audit.max_file_size) {
        rotate_log_files();
        if (!g_audit.log_file) return;
    }

    /* Write entry */
    fprintf(g_audit.log_file, "%s\n", json_entry);
    g_audit.current_file_size += len + 1;
    g_audit.total_events++;
}

/* Send to syslog if enabled */
static void send_to_syslog(const char* event_type, const char* message) {
#if HAVE_SYSLOG
    if (!g_audit.use_syslog) return;

    int priority = LOG_AUTHPRIV | LOG_INFO;

    /* Adjust priority based on event type */
    if (strstr(event_type, "violation") || strstr(event_type, "fail")) {
        priority = LOG_AUTHPRIV | LOG_WARNING;
    }

    syslog(priority, "cosmorun[audit]: %s: %s", event_type, message);
#endif
}

/* Initialize audit logging */
int cosmo_audit_init(audit_config_t* config) {
    if (!config || !config->log_path) {
        return -1;
    }

    AUDIT_LOCK();

    /* Already initialized? */
    if (g_audit.enabled) {
        AUDIT_UNLOCK();
        return 0;
    }

    /* Store configuration */
    g_audit.log_path = strdup(config->log_path);
    if (!g_audit.log_path) {
        AUDIT_UNLOCK();
        return -1;
    }

    g_audit.verbose = config->verbose;
    g_audit.use_syslog = config->use_syslog;
    g_audit.max_file_size = config->max_file_size > 0 ? config->max_file_size : AUDIT_DEFAULT_MAX_SIZE;
    g_audit.max_rotations = config->max_rotations > 0 ? config->max_rotations : AUDIT_DEFAULT_MAX_ROTATIONS;
    g_audit.rate_limit = config->rate_limit > 0 ? config->rate_limit : AUDIT_DEFAULT_RATE_LIMIT;

    /* Open log file */
    g_audit.log_file = fopen(g_audit.log_path, "a");
    if (!g_audit.log_file) {
        free(g_audit.log_path);
        g_audit.log_path = NULL;
        AUDIT_UNLOCK();
        return -1;
    }

    /* Get initial file size */
    fseek(g_audit.log_file, 0, SEEK_END);
    g_audit.current_file_size = ftell(g_audit.log_file);

    /* Initialize rate limiting */
    g_audit.rate_window_start = time(NULL);
    g_audit.events_in_window = 0;

    /* Initialize syslog if requested */
#if HAVE_SYSLOG
    if (g_audit.use_syslog) {
        openlog("cosmorun", LOG_PID | LOG_CONS, LOG_AUTHPRIV);
    }
#endif

    g_audit.enabled = 1;

    AUDIT_UNLOCK();
    return 0;
}

/* Log an event */
void cosmo_audit_log_event(const char* event_type, const char* details) {
    if (!g_audit.enabled || !event_type) {
        return;
    }

    AUDIT_LOCK();

    /* Check rate limiting */
    if (!check_rate_limit()) {
        AUDIT_UNLOCK();
        return;
    }

    /* Build JSON entry */
    char timestamp[64];
    char json_buffer[AUDIT_BUFFER_SIZE];
    char escaped_details[AUDIT_BUFFER_SIZE / 2];

    get_iso8601_timestamp(timestamp, sizeof(timestamp));

    if (details) {
        json_escape(details, escaped_details, sizeof(escaped_details));
        snprintf(json_buffer, sizeof(json_buffer),
                "{\"timestamp\":\"%s\",\"event\":\"%s\",\"pid\":%d,\"user\":\"%s\",\"details\":\"%s\"}",
                timestamp, event_type, getpid(), get_username(), escaped_details);
    } else {
        snprintf(json_buffer, sizeof(json_buffer),
                "{\"timestamp\":\"%s\",\"event\":\"%s\",\"pid\":%d,\"user\":\"%s\"}",
                timestamp, event_type, getpid(), get_username());
    }

    /* Write to file */
    write_log_entry(json_buffer);

    /* Send to syslog if enabled */
    send_to_syslog(event_type, details ? details : "");

    AUDIT_UNLOCK();
}

/* Log a formatted event */
void cosmo_audit_log_eventf(const char* event_type, const char* fmt, ...) {
    if (!g_audit.enabled || !event_type || !fmt) {
        return;
    }

    char details[AUDIT_BUFFER_SIZE];
    va_list args;

    va_start(args, fmt);
    vsnprintf(details, sizeof(details), fmt, args);
    va_end(args);

    cosmo_audit_log_event(event_type, details);
}

/* Flush audit log */
void cosmo_audit_flush(void) {
    if (!g_audit.enabled) return;

    AUDIT_LOCK();
    if (g_audit.log_file) {
        fflush(g_audit.log_file);
    }
    AUDIT_UNLOCK();
}

/* Shutdown audit logging */
void cosmo_audit_shutdown(void) {
    if (!g_audit.enabled) return;

    AUDIT_LOCK();

    /* Log final stats if there were dropped events */
    if (g_audit.dropped_events > 0) {
        char stats_msg[256];
        snprintf(stats_msg, sizeof(stats_msg),
                "total_events=%lu, dropped_events=%lu (rate_limit=%d/sec)",
                g_audit.total_events, g_audit.dropped_events, g_audit.rate_limit);

        /* Temporarily bypass rate limiting for shutdown message */
        int saved_rate_limit = g_audit.rate_limit;
        g_audit.rate_limit = 0;
        AUDIT_UNLOCK();

        cosmo_audit_log_event("audit_shutdown", stats_msg);

        AUDIT_LOCK();
        g_audit.rate_limit = saved_rate_limit;
    }

    /* Close log file */
    if (g_audit.log_file) {
        fclose(g_audit.log_file);
        g_audit.log_file = NULL;
    }

    /* Free log path */
    if (g_audit.log_path) {
        free(g_audit.log_path);
        g_audit.log_path = NULL;
    }

    /* Close syslog */
#if HAVE_SYSLOG
    if (g_audit.use_syslog) {
        closelog();
    }
#endif

    g_audit.enabled = 0;

    AUDIT_UNLOCK();
}

/* Check if audit logging is enabled */
int cosmo_audit_is_enabled(void) {
    return g_audit.enabled;
}
