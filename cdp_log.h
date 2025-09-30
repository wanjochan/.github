/**
 * CDP Logging helpers
 */
#ifndef CDP_LOG_H
#define CDP_LOG_H

typedef enum {
    CDP_LOG_DEBUG = 0,
    CDP_LOG_INFO  = 1,
    CDP_LOG_WARN  = 2,
    CDP_LOG_ERR   = 3
} cdp_log_level_t;

void cdp_log(cdp_log_level_t level, const char *module, const char *fmt, ...);

#endif /* CDP_LOG_H */

