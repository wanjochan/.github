/* Minimal libc implementation for TCC self-compilation */

#include <stddef.h>

/* Basic type definitions */
typedef long ssize_t;
typedef long off_t;
typedef long time_t;
typedef int sigset_t;
typedef int sem_t;
typedef int jmp_buf[16];

struct timeval {
    long tv_sec;
    long tv_usec;
};

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year;
    int tm_wday, tm_yday, tm_isdst;
};

struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
};

/* Memory functions */
void *memcpy(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    return memcpy(dest, src, n); // Simplified
}

void *memset(void *s, int c, size_t n) {
    char *p = s;
    while (n--) *p++ = c;
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const char *p1 = s1, *p2 = s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

/* String functions */
size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (n-- && (*d++ = *src++));
    while (n--) *d++ = 0;
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *s1 - *s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n-- && *s1 && *s1 == *s2) { s1++; s2++; }
    return n ? *s1 - *s2 : 0;
}

char *strchr(const char *s, int c) {
    while (*s && *s != c) s++;
    return *s == c ? (char*)s : 0;
}

char *strrchr(const char *s, int c) {
    char *last = 0;
    while (*s) {
        if (*s == c) last = (char*)s;
        s++;
    }
    return last;
}

char *strcat(char *dest, const char *src) {
    char *d = dest + strlen(dest);
    while ((*d++ = *src++));
    return dest;
}

char *strstr(const char *haystack, const char *needle) {
    size_t needle_len = strlen(needle);
    while (*haystack) {
        if (strncmp(haystack, needle, needle_len) == 0)
            return (char*)haystack;
        haystack++;
    }
    return 0;
}

char *strpbrk(const char *s, const char *accept) {
    while (*s) {
        if (strchr(accept, *s)) return (char*)s;
        s++;
    }
    return 0;
}

/* Conversion functions */
int atoi(const char *nptr) {
    int result = 0, sign = 1;
    while (*nptr == ' ') nptr++;
    if (*nptr == '-') { sign = -1; nptr++; }
    else if (*nptr == '+') nptr++;
    while (*nptr >= '0' && *nptr <= '9') {
        result = result * 10 + (*nptr - '0');
        nptr++;
    }
    return sign * result;
}

long strtol(const char *nptr, char **endptr, int base) {
    return atoi(nptr); // Simplified
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    return atoi(nptr); // Simplified
}

unsigned long long strtoull(const char *nptr, char **endptr, int base) {
    return atoi(nptr); // Simplified
}

long long strtoll(const char *nptr, char **endptr, int base) {
    return atoi(nptr); // Simplified
}

float strtof(const char *nptr, char **endptr) {
    return 0.0f; // Simplified
}

double strtod(const char *nptr, char **endptr) {
    return 0.0; // Simplified
}

long double strtold(const char *nptr, char **endptr) {
    return 0.0L; // Simplified
}

/* Math functions */
long double ldexpl(long double x, int exp) {
    return x; // Simplified
}

/* I/O functions - minimal stubs */
int printf(const char *format, ...) { return 0; }
int fprintf(void *stream, const char *format, ...) { return 0; }
int sprintf(char *str, const char *format, ...) { return 0; }
int snprintf(char *str, size_t size, const char *format, ...) { return 0; }
int vprintf(const char *format, void *ap) { return 0; }
int vfprintf(void *stream, const char *format, void *ap) { return 0; }
int vsnprintf(char *str, size_t size, const char *format, void *ap) { return 0; }

void *fopen(const char *pathname, const char *mode) { return 0; }
int fclose(void *stream) { return 0; }
size_t fread(void *ptr, size_t size, size_t nmemb, void *stream) { return 0; }
size_t fwrite(const void *ptr, size_t size, size_t nmemb, void *stream) { return 0; }
int fgetc(void *stream) { return -1; }
int fputc(int c, void *stream) { return c; }
int fputs(const char *s, void *stream) { return 0; }
int fflush(void *stream) { return 0; }
int fseek(void *stream, long offset, int whence) { return 0; }
long ftell(void *stream) { return 0; }
void *fdopen(int fd, const char *mode) { return 0; }

/* File operations */
int open(const char *pathname, int flags, ...) { return -1; }
int close(int fd) { return 0; }
ssize_t read(int fd, void *buf, size_t count) { return 0; }
off_t lseek(int fd, off_t offset, int whence) { return 0; }
int unlink(const char *pathname) { return 0; }
int remove(const char *pathname) { return 0; }

/* Memory allocation */
void *malloc(size_t size) { return 0; }
void *realloc(void *ptr, size_t size) { return 0; }
void free(void *ptr) { }

/* Environment */
char *getenv(const char *name) { return 0; }
char *getcwd(char *buf, size_t size) { return 0; }
char *realpath(const char *path, char *resolved_path) { return 0; }

/* Process control */
void exit(int status) { while(1); } // Infinite loop for now
void abort(void) { exit(1); }
int execvp(const char *file, char *const argv[]) { return -1; }

/* Time functions */
time_t time(time_t *tloc) { return 0; }
struct tm *localtime(const time_t *timep) { return 0; }
int gettimeofday(struct timeval *tv, struct timezone *tz) { return 0; }

/* Error handling */
int *__errno_location(void) { static int errno_val = 0; return &errno_val; }
char *strerror(int errnum) { return "Error"; }

/* System functions */
long sysconf(int name) { return 0; }

/* Sorting */
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) { }

/* Signal handling - minimal stubs */
int sigemptyset(sigset_t *set) { return 0; }
int sigaddset(sigset_t *set, int signum) { return 0; }
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) { return 0; }
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) { return 0; }

/* Jump functions */
int _setjmp(jmp_buf env) { return 0; }
void longjmp(jmp_buf env, int val) { }

/* Memory protection */
int mprotect(void *addr, size_t len, int prot) { return 0; }

/* Assert */
void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    exit(1);
}

/* Semaphore stubs */
int sem_init(sem_t *sem, int pshared, unsigned int value) { return 0; }
int sem_wait(sem_t *sem) { return 0; }
int sem_post(sem_t *sem) { return 0; }

/* Global variables */
void *stdout = 0;
void *stderr = 0;
char **environ = 0;

/* TCC-specific runtime functions */
void __va_arg(void) { }
void __floatundixf(void) { }
void __fixunsxfdi(void) { }
