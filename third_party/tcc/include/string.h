#ifndef _STRING_H_
#define _STRING_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

/* String functions */
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
int strcoll(const char *s1, const char *s2);
size_t strxfrm(char *dest, const char *src, size_t n);

/* Search functions */
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
size_t strcspn(const char *s1, const char *s2);
size_t strspn(const char *s1, const char *s2);
char *strpbrk(const char *s1, const char *s2);
char *strstr(const char *haystack, const char *needle);
char *strtok(char *str, const char *delim);

/* Length function */
size_t strlen(const char *s);

/* Error function */
char *strerror(int errnum);

/* Memory functions */
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memchr(const void *s, int c, size_t n);
void *memset(void *s, int c, size_t n);

/* Non-standard but commonly used extensions */
char *strdup(const char *s);
char *strndup(const char *s, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* _STRING_H_ */