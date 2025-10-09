/* TCC Runtime Configuration - Dual libc Strategy */
#ifndef TCC_RUNTIME_CONFIG_H
#define TCC_RUNTIME_CONFIG_H

/* Runtime mode selection */
#ifndef TCC_RUNTIME_MODE
#define TCC_RUNTIME_MODE_SYSTEM    1  /* Use system libc (default) */
#define TCC_RUNTIME_MODE_MINIMAL   2  /* Use our minimal libc */
#define TCC_RUNTIME_MODE_HYBRID    3  /* Try system, fallback to minimal */

/* Default mode - can be overridden at compile time */
#define TCC_RUNTIME_MODE TCC_RUNTIME_MODE_HYBRID
#endif

/* Feature flags for minimal libc */
#define TCC_MINIMAL_LIBC_STDIO     1  /* Include stdio functions */
#define TCC_MINIMAL_LIBC_STDLIB    1  /* Include stdlib functions */
#define TCC_MINIMAL_LIBC_STRING    1  /* Include string functions */
#define TCC_MINIMAL_LIBC_MATH      1  /* Include basic math */
#define TCC_MINIMAL_LIBC_TIME      1  /* Include time functions */
#define TCC_MINIMAL_LIBC_SIGNAL    1  /* Include signal handling */

/* System detection */
#ifdef __linux__
#define TCC_SYSTEM_LINUX 1
#endif

#ifdef _WIN32
#define TCC_SYSTEM_WINDOWS 1
#endif

#ifdef __APPLE__
#define TCC_SYSTEM_MACOS 1
#endif

/* Runtime library paths */
#define TCC_SYSTEM_LIBC_PATH "/lib:/usr/lib:/usr/local/lib"
#define TCC_MINIMAL_LIBC_NAME "libtcc_minimal.a"

#endif /* TCC_RUNTIME_CONFIG_H */
