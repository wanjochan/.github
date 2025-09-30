/**
 * CDP Logging helpers
 */

#include "cdp_internal.h"
#include "cdp_log.h"
#include <stdarg.h>
#include <stdio.h>

extern int verbose;

void cdp_log(cdp_log_level_t level, const char *module, const char *fmt, ...) {
    if (!verbose && level == CDP_LOG_DEBUG) return;
    const char *lvl = level==CDP_LOG_DEBUG?"DEBUG": level==CDP_LOG_INFO?"INFO": level==CDP_LOG_WARN?"WARN":"ERR";
    fprintf(stderr, "[%s]%s%s ", lvl, module?"[":"", module?module:"");
    if (module) fputc(']', stderr);
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

