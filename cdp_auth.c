/**
 * Centralized authorization / whitelist checks
 * Minimal env-based policy until full integration
 */
#include "cdp_auth.h"
#include <string.h>
#include <stdlib.h>

static int env_enabled(const char *name) {
    const char *v = getenv(name);
    return (v && (*v=='1' || strcasecmp(v, "true")==0 || strcasecmp(v, "yes")==0));
}

int cdp_authz_allow(const char *action, const char *target) {
    (void)target;
    if (!action) return 0;
    /* Simple policy gates by env flags */
    if (strncmp(action, "system:", 7) == 0 || strncmp(action, "shell:", 6) == 0) {
        return env_enabled("CDP_ALLOW_SYSTEM");
    }
    if (strncmp(action, "file:", 5) == 0) {
        return env_enabled("CDP_ALLOW_FILE");
    }
    if (strncmp(action, "notify:", 7) == 0) {
        return env_enabled("CDP_ALLOW_NOTIFY") || env_enabled("CDP_ALLOW_SYSTEM");
    }
    /* Allow others by default; future: integrate domain whitelist */
    return 1;
}

