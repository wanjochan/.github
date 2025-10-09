/* compat.h - Windows兼容层 */
#ifndef _COMPAT_H
#define _COMPAT_H

#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>

/* strtoll实现 */
static long long strtoll(const char *nptr, char **endptr, int base)
{
    return _strtoi64(nptr, endptr, base);
}

/* strtoull实现 */
static unsigned long long strtoull(const char *nptr, char **endptr, int base)
{
    return _strtoui64(nptr, endptr, base);
}

#endif /* _COMPAT_H */ 