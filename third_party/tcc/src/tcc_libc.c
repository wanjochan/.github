/* Modular minimal libc for TCC self-compilation */

#include "modular_libc.h"

/* All types and structures are defined in modular_libc.h */

/* Memory functions */
void *memcpy(void *d, const void *s, size_t n) { char *cd=d; const char *cs=s; while(n--) *cd++=*cs++; return d; }
void *memmove(void *d, const void *s, size_t n) { return memcpy(d,s,n); }
void *memset(void *s, int c, size_t n) { char *p=s; while(n--) *p++=c; return s; }
int memcmp(const void *s1, const void *s2, size_t n) { const char *p1=s1,*p2=s2; while(n--) if(*p1!=*p2) return *p1-*p2; else p1++,p2++; return 0; }

/* String functions */
size_t strlen(const char *s) { size_t l=0; while(*s++) l++; return l; }
char *strcpy(char *d, const char *s) { char *r=d; while((*d++=*s++)); return r; }
char *strncpy(char *d, const char *s, size_t n) { char *r=d; while(n--&&(*d++=*s++)); while(n--) *d++=0; return r; }
int strcmp(const char *s1, const char *s2) { while(*s1&&*s1==*s2) s1++,s2++; return *s1-*s2; }
int strncmp(const char *s1, const char *s2, size_t n) { while(n--&&*s1&&*s1==*s2) s1++,s2++; return n?*s1-*s2:0; }
char *strchr(const char *s, int c) { while(*s&&*s!=c) s++; return *s==c?(char*)s:0; }
char *strrchr(const char *s, int c) { char *l=0; while(*s) if(*s==c) l=(char*)s; s++; return l; }
char *strcat(char *d, const char *s) { char *r=d+strlen(d); while((*r++=*s++)); return d; }
char *strstr(const char *h, const char *n) { size_t nl=strlen(n); while(*h) if(!strncmp(h,n,nl)) return (char*)h; else h++; return 0; }
char *strpbrk(const char *s, const char *a) { while(*s) if(strchr(a,*s)) return (char*)s; else s++; return 0; }

/* Conversion functions */
int atoi(const char *s) { int r=0,sign=1; while(*s==' ') s++; if(*s=='-') sign=-1,s++; else if(*s=='+') s++; while(*s>='0'&&*s<='9') r=r*10+(*s-'0'),s++; return sign*r; }
long strtol(const char *s, char **e, int b) { if(e) *e=(char*)s; return atoi(s); }
unsigned long strtoul(const char *s, char **e, int b) { if(e) *e=(char*)s; return atoi(s); }
unsigned long long strtoull(const char *s, char **e, int b) { if(e) *e=(char*)s; return atoi(s); }
long long strtoll(const char *s, char **e, int b) { if(e) *e=(char*)s; return atoi(s); }
float strtof(const char *s, char **e) { if(e) *e=(char*)s; return 0.0f; }
double strtod(const char *s, char **e) { if(e) *e=(char*)s; return 0.0; }
long double strtold(const char *s, char **e) { if(e) *e=(char*)s; return 0.0L; }

/* Math */
long double ldexpl(long double x, int e) { return x; }

/* I/O stubs */
int printf(const char *f, ...) { return 0; }
int fprintf(void *s, const char *f, ...) { return 0; }
int sprintf(char *s, const char *f, ...) { return 0; }
int snprintf(char *s, size_t n, const char *f, ...) { return 0; }
int vprintf(const char *f, void *a) { return 0; }
int vfprintf(void *s, const char *f, void *a) { return 0; }
int vsnprintf(char *s, size_t n, const char *f, void *a) { return 0; }
void *fopen(const char *p, const char *m) { return 0; }
int fclose(void *s) { return 0; }
size_t fread(void *p, size_t s, size_t n, void *f) { return 0; }
size_t fwrite(const void *p, size_t s, size_t n, void *f) { return 0; }
int fgetc(void *s) { return -1; }
int fputc(int c, void *s) { return c; }
int fputs(const char *s, void *f) { return 0; }
int fflush(void *s) { return 0; }
int fseek(void *s, long o, int w) { return 0; }
long ftell(void *s) { return 0; }
void *fdopen(int fd, const char *m) { return 0; }

/* File ops */
int open(const char *p, int f, ...) { return -1; }
int close(int fd) { return 0; }
ssize_t read(int fd, void *b, size_t c) { return 0; }
off_t lseek(int fd, off_t o, int w) { return 0; }
int unlink(const char *p) { return 0; }
int remove(const char *p) { return 0; }

/* Memory */
void *malloc(size_t s) { return 0; }
void *realloc(void *p, size_t s) { return 0; }
void free(void *p) { }

/* Environment */
char *getenv(const char *n) { return 0; }
char *getcwd(char *b, size_t s) { return 0; }
char *realpath(const char *p, char *r) { return 0; }

/* Process */
void exit(int code) {
    // Use system call to exit directly
    asm volatile("movl %0, %%edi\n\t"
                 "movl $60, %%eax\n\t"  // sys_exit
                 "syscall"
                 :
                 : "r" (code)
                 : "rdi", "rax");
    __builtin_unreachable();
}
void abort(void) { exit(1); }
int execvp(const char *f, char *const a[]) { return -1; }

/* Time */
time_t time(time_t *t) { return 0; }
struct tm *localtime(const time_t *t) { return 0; }
int gettimeofday(struct timeval *tv, struct timezone *tz) { return 0; }

/* Error */
int *__errno_location(void) { static int e=0; return &e; }
char *strerror(int e) { return "Error"; }

/* System */
long sysconf(int n) { return 0; }

/* Sort */
void qsort(void *b, size_t n, size_t s, int (*c)(const void*,const void*)) { }

/* Signals */
int sigemptyset(sigset_t *s) { return 0; }
int sigaddset(sigset_t *s, int n) { return 0; }
int sigprocmask(int h, const sigset_t *s, sigset_t *o) { return 0; }
int sigaction(int n, const struct sigaction *a, struct sigaction *o) { return 0; }

/* Jump */
int _setjmp(jmp_buf e) { return 0; }
void longjmp(jmp_buf e, int v) { }

/* Memory protection */
int mprotect(void *a, size_t l, int p) { return 0; }

/* Assert */
void __assert_fail(const char *a, const char *f, unsigned int l, const char *fn) { exit(1); }

/* Semaphore */
int sem_init(sem_t *s, int p, unsigned int v) { return 0; }
int sem_wait(sem_t *s) { return 0; }
int sem_post(sem_t *s) { return 0; }

/* Dynamic loading stubs */
void *dlopen(const char *f, int flag) { return 0; }
void *dlsym(void *h, const char *s) { return 0; }
int dlclose(void *h) { return 0; }

/* TCC-specific runtime function */
void __rt_exit(void *frame, int code) {
    // Use system call to exit directly, avoiding recursion
    asm volatile("movl %0, %%edi\n\t"
                 "movl $60, %%eax\n\t"  // sys_exit
                 "syscall"
                 :
                 : "r" (code)
                 : "rdi", "rax");
    __builtin_unreachable();
}

/* Global variables */
void *stdout = 0;
void *stderr = 0;
char **environ = 0;
