/**
 * Centralized authorization / whitelist checks
 */
#ifndef CDP_AUTH_H
#define CDP_AUTH_H

/* Return 1 to allow, 0 to deny */
int cdp_authz_allow(const char *action, const char *target);

#endif /* CDP_AUTH_H */

