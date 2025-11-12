/* TCC-compatible stdint.h types
 *
 * IMPORTANT: This file must be included BEFORE any standard library headers
 * (stdio.h, stdlib.h, string.h, etc.) to avoid type redefinition errors.
 *
 * TCC has internal type definitions that conflict with user typedefs if
 * standard headers are included first.
 */

/* Define types directly using C basic types - no typedefs to avoid conflicts */
#define int8_t signed char
#define uint8_t unsigned char
#define int16_t short
#define uint16_t unsigned short
#define int32_t int
#define uint32_t unsigned int
#define int64_t long long
#define uint64_t unsigned long long
