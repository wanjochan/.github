/* Modular libc for TCC - Header with feature selection */
#ifndef MODULAR_LIBC_H
#define MODULAR_LIBC_H

#include "tcc_runtime_config.h"
#include "include/stddef.h"

/* Basic types - always included */
typedef long ssize_t;
typedef long off_t;
typedef long time_t;
typedef int sigset_t;
typedef int sem_t;
typedef int jmp_buf[16];

/* Conditional compilation based on features */

#if TCC_MINIMAL_LIBC_STDIO
/* stdio.h equivalents */
int printf(const char *format, ...);
int fprintf(void *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
void *fopen(const char *pathname, const char *mode);
int fclose(void *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, void *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, void *stream);
int fgetc(void *stream);
int fputc(int c, void *stream);
int fputs(const char *s, void *stream);
int fflush(void *stream);
extern void *stdout, *stderr;
#endif

#if TCC_MINIMAL_LIBC_STDLIB
/* stdlib.h equivalents */
void *malloc(size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
int atoi(const char *nptr);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
char *getenv(const char *name);
extern void exit(int status);  /* Provided by runmain.c */
void abort(void);
#endif

#if TCC_MINIMAL_LIBC_STRING
/* string.h equivalents */
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strcat(char *dest, const char *src);
char *strstr(const char *haystack, const char *needle);
char *strpbrk(const char *s, const char *accept);
#endif

#if TCC_MINIMAL_LIBC_MATH
/* math.h equivalents */
long double ldexpl(long double x, int exp);
float strtof(const char *nptr, char **endptr);
double strtod(const char *nptr, char **endptr);
long double strtold(const char *nptr, char **endptr);
#endif

#if TCC_MINIMAL_LIBC_TIME
/* time.h equivalents */
struct timeval { long tv_sec, tv_usec; };
struct timezone { int tz_minuteswest, tz_dsttime; };
struct tm { int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst; };
time_t time(time_t *tloc);
struct tm *localtime(const time_t *timep);
int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

#if TCC_MINIMAL_LIBC_SIGNAL
/* signal.h equivalents */
struct sigaction { void (*sa_handler)(int); sigset_t sa_mask; int sa_flags; };
int sigemptyset(sigset_t *set);
int sigaddset(sigset_t *set, int signum);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int _setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);
#endif

/* Always included - essential functions */
int *__errno_location(void);
char *strerror(int errnum);
void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function);
void __rt_exit(void *frame, int code);

/* File operations - minimal set */
int open(const char *pathname, int flags, ...);
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
off_t lseek(int fd, off_t offset, int whence);
int unlink(const char *pathname);

/* System functions */
long sysconf(int name);
char *getcwd(char *buf, size_t size);
char *realpath(const char *path, char *resolved_path);

/* Sorting */
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

/* Dynamic loading stubs */
void *dlopen(const char *filename, int flag);
void *dlsym(void *handle, const char *symbol);
int dlclose(void *handle);

/* Process control */
int execvp(const char *file, char *const argv[]);

/* Memory protection */
int mprotect(void *addr, size_t len, int prot);

/* Semaphore stubs */
int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_wait(sem_t *sem);
int sem_post(sem_t *sem);

/* Global variables */
extern char **environ;

/* Feature detection macros */
#define TCC_HAS_STDIO    TCC_MINIMAL_LIBC_STDIO
#define TCC_HAS_STDLIB   TCC_MINIMAL_LIBC_STDLIB
#define TCC_HAS_STRING   TCC_MINIMAL_LIBC_STRING
#define TCC_HAS_MATH     TCC_MINIMAL_LIBC_MATH
#define TCC_HAS_TIME     TCC_MINIMAL_LIBC_TIME
#define TCC_HAS_SIGNAL   TCC_MINIMAL_LIBC_SIGNAL

#endif /* MODULAR_LIBC_H */
