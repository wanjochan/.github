#ifndef COSMORUN_H
#define COSMORUN_H

/* Cosmopolitan libc compatibility header for TCC runtime compilation */

/* When using TCC with cosmorun, use auto-generated libc headers */
#ifdef __COSMORUN__

/* va_list definition using TCC builtins */
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

/* Define NULL pointer */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* Standard bool type (C99) */
#ifndef __cplusplus
#ifndef _Bool
#define _Bool int
#endif
typedef _Bool bool;
#define true 1
#define false 0
#endif

/* Basic type definitions for TCC */
typedef unsigned long size_t;
typedef long ssize_t;
typedef long ptrdiff_t;
typedef long time_t;
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef long off_t;
typedef int mode_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned long uintptr_t;
typedef long intptr_t;
typedef int errno_t;
typedef int wchar_t;
typedef unsigned int wint_t;
typedef unsigned int char32_t;
typedef unsigned short char16_t;
typedef int bool32;

/* Special Cosmopolitan types */
struct axdx_t { long ax, dx; };
typedef struct axdx_t axdx_t;

/* stdint.h limits */
#define INT8_MIN   (-128)
#define INT16_MIN  (-32768)
#define INT32_MIN  (-2147483647-1)
#define INT64_MIN  (-9223372036854775807LL-1)
#define INT8_MAX   127
#define INT16_MAX  32767
#define INT32_MAX  2147483647
#define INT64_MAX  9223372036854775807LL
#define UINT8_MAX  255
#define UINT16_MAX 65535
#define UINT32_MAX 4294967295U
#define UINT64_MAX 18446744073709551615ULL
#define SIZE_MAX   UINT64_MAX

/* limits.h constants */
#define LONG_MAX   9223372036854775807L
#define LONG_MIN   (-LONG_MAX - 1L)
#define ULONG_MAX  18446744073709551615UL
#define INT_MAX    2147483647
#define INT_MIN    (-INT_MAX - 1)
#define UINT_MAX   4294967295U
#define CHAR_BIT   8
#define PATH_MAX   4096

/* NULL definition */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* From libc/stdio/stdio.h */
struct FILE;
typedef struct FILE FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;
errno_t ferror(FILE *);
void clearerr(FILE *);
int feof(FILE *);
int getc(FILE *);
int putc(int, FILE *);
int fflush(FILE *);
int fpurge(FILE *);
int fgetc(FILE *);
char *fgetln(FILE *, size_t *);
int ungetc(int, FILE *);
int fileno(FILE *);
int fputc(int, FILE *);
int fputs(const char *, FILE *);
int fputws(const wchar_t *, FILE *);
void flockfile(FILE *);
void funlockfile(FILE *);
int ftrylockfile(FILE *);
char *fgets(char *, int, FILE *);
wchar_t *fgetws(wchar_t *, int, FILE *);
wint_t putwc(wchar_t, FILE *);
wint_t fputwc(wchar_t, FILE *);
wint_t putwchar(wchar_t);
wint_t getwchar(void);
wint_t getwc(FILE *);
wint_t fgetwc(FILE *);
wint_t ungetwc(wint_t, FILE *);
int getchar(void);
int putchar(int);
int puts(const char *);
ssize_t getline(char **, size_t *, FILE *);
ssize_t getdelim(char **, size_t *, int, FILE *);
FILE *fopen(const char *, const char *);
FILE *fdopen(int, const char *);
FILE *fmemopen(void *, size_t, const char *);
FILE *freopen(const char *, const char *, FILE *);
size_t fread(void *, size_t, size_t, FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
int fclose(FILE *);
int fseek(FILE *, long, int);
long ftell(FILE *);
int fseeko(FILE *, int64_t, int);
int64_t ftello(FILE *);
void rewind(FILE *);
int fopenflags(const char *);
void setlinebuf(FILE *);
void setbuf(FILE *, char *);
void setbuffer(FILE *, char *, size_t);
int setvbuf(FILE *, char *, int, size_t);
int pclose(FILE *);
char *ctermid(char *);
void perror(const char *);
typedef uint64_t fpos_t;
char *gets(char *);
int fgetpos(FILE *, fpos_t *);
int fsetpos(FILE *, const fpos_t *);
FILE *tmpfile(void);
char *tmpnam(char *);
char *tmpnam_r(char *);
FILE *popen(const char *, const char *);
int printf(const char *, ...);
int vprintf(const char *, va_list);
int fprintf(FILE *, const char *, ...);
int vfprintf(FILE *, const char *, va_list);
int scanf(const char *, ...);
int vscanf(const char *, va_list);
int fscanf(FILE *, const char *, ...);
int vfscanf(FILE *, const char *, va_list);
int snprintf(char *, size_t, const char *, ...);
int vsnprintf(char *, size_t, const char *, va_list);
int sprintf(char *, const char *, ...);
int vsprintf(char *, const char *, va_list);
int fwprintf(FILE *, const wchar_t *, ...);
int fwscanf(FILE *, const wchar_t *, ...);
int swprintf(wchar_t *, size_t, const wchar_t *, ...);
int swscanf(const wchar_t *, const wchar_t *, ...);
int vfwprintf(FILE *, const wchar_t *, va_list);
int vfwscanf(FILE *, const wchar_t *, va_list);
int vswprintf(wchar_t *, size_t, const wchar_t *, va_list);
int vswscanf(const wchar_t *, const wchar_t *, va_list);
int vwprintf(const wchar_t *, va_list);
int vwscanf(const wchar_t *, va_list);
int wprintf(const wchar_t *, ...);
int wscanf(const wchar_t *, ...);
int fwide(FILE *, int);
int sscanf(const char *, const char *, ...);
int vsscanf(const char *, const char *, va_list);
int asprintf(char **, const char *, ...);
int vasprintf(char **, const char *, va_list);
int getc_unlocked(FILE *);
int puts_unlocked(const char *);
int getchar_unlocked(void);
int putc_unlocked(int, FILE *);
int putchar_unlocked(int);
void clearerr_unlocked(FILE *);
int feof_unlocked(FILE *);
int ferror_unlocked(FILE *);
int fileno_unlocked(FILE *);
int fflush_unlocked(FILE *);
int fgetc_unlocked(FILE *);
int fputc_unlocked(int, FILE *);
size_t fread_unlocked(void *, size_t, size_t, FILE *);
size_t fwrite_unlocked(const void *, size_t, size_t, FILE *);
char *fgets_unlocked(char *, int, FILE *);
int fputs_unlocked(const char *, FILE *);
wint_t getwc_unlocked(FILE *);
wint_t getwchar_unlocked(void);
wint_t fgetwc_unlocked(FILE *);
wint_t fputwc_unlocked(wchar_t, FILE *);
wint_t putwc_unlocked(wchar_t, FILE *);
wint_t putwchar_unlocked(wchar_t);
wchar_t *fgetws_unlocked(wchar_t *, int, FILE *);
int fputws_unlocked(const wchar_t *, FILE *);
wint_t ungetwc_unlocked(wint_t, FILE *);
int ungetc_unlocked(int, FILE *);
int fseek_unlocked(FILE *, int64_t, int);
ssize_t getdelim_unlocked(char **, size_t *, int, FILE *);
int fprintf_unlocked(FILE *, const char *, ...);
int vfprintf_unlocked(FILE *, const char *, va_list);

/* From libc/str/str.h */
void *memset(void *, int, size_t);
void *memmove(void *, const void *, size_t);
void *memcpy(void *, const void *, size_t);
char *hexpcpy(char *, const void *, size_t);
int memcmp(const void *, const void *, size_t);
int timingsafe_bcmp(const void *, const void *, size_t);
int timingsafe_memcmp(const void *, const void *, size_t);
size_t strlen(const char *);
size_t strnlen(const char *, size_t);
size_t strnlen_s(const char *, size_t);
char *strchr(const char *, int);
void *memchr(const void *, int, size_t);
void *rawmemchr(const void *, int);
size_t wcslen(const wchar_t *);
size_t wcsnlen(const wchar_t *, size_t);
size_t wcsnlen_s(const wchar_t *, size_t);
wchar_t *wcschr(const wchar_t *, wchar_t);
wchar_t *wmemchr(const wchar_t *, wchar_t, size_t);
wchar_t *wcschrnul(const wchar_t *, wchar_t);
char *strstr(const char *, const char *);
wchar_t *wcsstr(const wchar_t *, const wchar_t *);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);
int wcscmp(const wchar_t *, const wchar_t *);
int wcsncmp(const wchar_t *, const wchar_t *, size_t);
int wmemcmp(const wchar_t *, const wchar_t *, size_t);
int strcasecmp(const char *, const char *);
int wcscasecmp(const wchar_t *, const wchar_t *);
int strncasecmp(const char *, const char *, size_t);
int wcsncasecmp(const wchar_t *, const wchar_t *, size_t);
char *strrchr(const char *, int);
wchar_t *wcsrchr(const wchar_t *, wchar_t);
char *strpbrk(const char *, const char *);
wchar_t *wcspbrk(const wchar_t *, const wchar_t *);
size_t strspn(const char *, const char *);
size_t wcsspn(const wchar_t *, const wchar_t *);
size_t strcspn(const char *, const char *);
size_t wcscspn(const wchar_t *, const wchar_t *);
void *memfrob(void *, size_t);
int strcoll(const char *, const char *);
char *stpcpy(char *, const char *);
char *stpncpy(char *, const char *, size_t);
char *strcat(char *, const char *);
wchar_t *wcscat(wchar_t *, const wchar_t *);
size_t strxfrm(char *, const char *, size_t);
char *strcpy(char *, const char *);
wchar_t *wcscpy(wchar_t *, const wchar_t *);
char *strncat(char *, const char *, size_t);
wchar_t *wcsncat(wchar_t *, const wchar_t *, size_t);
char *strncpy(char *, const char *, size_t);
char *strtok(char *, const char *);
char *strtok_r(char *, const char *, char **);
wchar_t *wcstok(wchar_t *, const wchar_t *, wchar_t **);
wchar_t *wmemset(wchar_t *, wchar_t, size_t);
wchar_t *wmemcpy(wchar_t *, const wchar_t *, size_t);
wchar_t *wmemmove(wchar_t *, const wchar_t *, size_t);
ssize_t strfmon(char *, size_t, const char *, ...);
long a64l(const char *);
char *l64a(long);
typedef unsigned mbstate_t;
wchar_t *wcsncpy(wchar_t *, const wchar_t *, size_t);
int mbtowc(wchar_t *, const char *, size_t);
size_t mbrtowc(wchar_t *, const char *, size_t, mbstate_t *);
size_t mbsrtowcs(wchar_t *, const char **, size_t, mbstate_t *);
size_t mbstowcs(wchar_t *, const char *, size_t);
size_t wcrtomb(char *, wchar_t, mbstate_t *);
size_t c32rtomb(char *, char32_t, mbstate_t *);
size_t mbrtoc32(char32_t *, const char *, size_t, mbstate_t *);
size_t c16rtomb(char *, char16_t, mbstate_t *);
size_t mbrtoc16(char16_t *, const char *, size_t, mbstate_t *);
size_t mbrlen(const char *, size_t, mbstate_t *);
size_t mbsnrtowcs(wchar_t *, const char **, size_t, size_t, mbstate_t *);
size_t wcsnrtombs(char *, const wchar_t **, size_t, size_t, mbstate_t *);
size_t wcsrtombs(char *, const wchar_t **, size_t, mbstate_t *);
size_t wcstombs(char *, const wchar_t *, size_t);
int mbsinit(const mbstate_t *);
int mblen(const char *, size_t);
int wctomb(char *, wchar_t);
int wctob(wint_t);
wint_t btowc(int);
int getsubopt(char **, char *const *, char **);
char *strsignal(int);
char *strerror(int);
errno_t strerror_r(int, char *, size_t);
char *__xpg_strerror_r(int, char *, size_t);
int bcmp(const void *, const void *, size_t);
void bcopy(const void *, void *, size_t);
void bzero(void *, size_t);
char *index(const char *, int);
char *rindex(const char *, int);
void *memccpy(void *, const void *, int, size_t);
char *strsep(char **, const char *);
void explicit_bzero(void *, size_t);
size_t strlcpy(char *, const char *, size_t);
size_t strlcat(char *, const char *, size_t);
int strverscmp(const char *, const char *);
char *strchrnul(const char *, int);
char *strcasestr(const char *, const char *);
void *memmem(const void *, size_t, const void *, size_t);
void *memrchr(const void *, int, size_t);
void *mempcpy(void *, const void *, size_t);
wchar_t *wmempcpy(wchar_t *, const wchar_t *, size_t);
uint64_t tpenc(uint32_t);
char *chomp(char *);
wchar_t *wchomp(wchar_t *);
uint64_t __fnv(const void *, size_t);
bool32 startswith(const char *, const char *);
bool32 startswithi(const char *, const char *);
bool32 endswith(const char *, const char *);
bool32 istext(const void *, size_t);
bool32 isutf8(const void *, size_t);
void *wmemrchr(const wchar_t *, wchar_t, size_t);
const char *strsignal_r(int, char[21]);
char16_t *chomp16(char16_t *);
size_t strlen16(const char16_t *);
size_t strnlen16(const char16_t *, size_t);
char16_t *strchr16(const char16_t *, int);
void *memchr16(const void *, int, size_t);
char16_t *strchrnul16(const char16_t *, int);
void *rawmemchr16(const void *, int);
char16_t *strstr16(const char16_t *, const char16_t *);
int memcasecmp(const void *, const void *, size_t);
int strcmp16(const char16_t *, const char16_t *);
int strncmp16(const char16_t *, const char16_t *, size_t);
int strcasecmp16(const char16_t *, const char16_t *);
int strncasecmp16(const char16_t *, const char16_t *, size_t);
char16_t *strrchr16(const char16_t *, int);
void *memrchr16(const void *, int, size_t);
char16_t *strpbrk16(const char16_t *, const char16_t *);
size_t strspn16(const char16_t *, const char16_t *);
size_t strcspn16(const char16_t *, const char16_t *);
char16_t *strcat16(char16_t *, const char16_t *);
char16_t *strcpy16(char16_t *, const char16_t *);
char16_t *strncat16(char16_t *, const char16_t *, size_t);
char16_t *memset16(char16_t *, char16_t, size_t);
bool32 startswith16(const char16_t *, const char16_t *);
bool32 endswith16(const char16_t *, const char16_t *);
axdx_t tprecode8to16(char16_t *, size_t, const char *);
axdx_t tprecode16to8(char *, size_t, const char16_t *);
bool32 wcsstartswith(const wchar_t *, const wchar_t *);
bool32 wcsendswith(const wchar_t *, const wchar_t *);
char *__join_paths(char *, size_t, const char *, const char *);
int __mkntpathat(int, const char *, int, char16_t[78]);

/* From libc/mem/mem.h */
void free(void *);
void *malloc(size_t);
void *calloc(size_t, size_t);
void *memalign(size_t, size_t);
void *realloc(void *, size_t);
void *realloc_in_place(void *, size_t);
void *reallocarray(void *, size_t, size_t);
void *valloc(size_t);
void *pvalloc(size_t);
char *strdup(const char *);
char *strndup(const char *, size_t);
void *aligned_alloc(size_t, size_t);
int posix_memalign(void **, size_t, size_t);
int mallopt(int, int);
int malloc_trim(size_t);
size_t bulk_free(void **, size_t);
size_t malloc_usable_size(void *);
void **independent_calloc(size_t, size_t, void **);
void **independent_comalloc(size_t, size_t *, void **);
wchar_t *wcsdup(const wchar_t *);
struct mallinfo {
    size_t arena;
    size_t ordblks;
    size_t smblks;
    size_t hblks;
    size_t hblkhd;
    size_t usmblks;
    size_t fsmblks;
    size_t uordblks;
    size_t fordblks;
    size_t keepcost;
};
struct mallinfo mallinfo(void);
size_t malloc_footprint(void);
size_t malloc_max_footprint(void);
size_t malloc_footprint_limit(void);
size_t malloc_set_footprint_limit(size_t);
void malloc_inspect_all(void (*)(void *, void *, size_t, void *), void *);

/* From libc/calls/calls.h */
typedef int sig_atomic_t;
bool32 isatty(int);
char *getcwd(char *, size_t);
char *realpath(const char *, char *);
char *ttyname(int);
int access(const char *, int);
int chdir(const char *);
int chmod(const char *, unsigned);
int chown(const char *, unsigned, unsigned);
int chroot(const char *);
int close(int);
int close_range(unsigned, unsigned, unsigned);
int closefrom(int);
int creat(const char *, unsigned);
int dup(int);
int dup2(int, int);
int dup3(int, int, int);
int execl(const char *, const char *, ...);
int execle(const char *, const char *, ...);
int execlp(const char *, const char *, ...);
int execv(const char *, char *const[]);
int execve(const char *, char *const[], char *const[]);
int execvp(const char *, char *const[]);
int faccessat(int, const char *, int, int);
int fchdir(int);
int fchmod(int, unsigned);
int fchmodat(int, const char *, unsigned, int);
int fchown(int, unsigned, unsigned);
int fchownat(int, const char *, unsigned, unsigned, int);
int fcntl(int, int, ...);
int fdatasync(int);
int fexecve(int, char *const[], char *const[]);
int flock(int, int);
int fork(void);
int fsync(int);
int ftruncate(int, int64_t);
int getdomainname(char *, size_t);
int getgroups(int, unsigned[]);
int gethostname(char *, size_t);
int getloadavg(double *, int);
int getpgid(int);
int getpgrp(void);
int getpid(void);
int getppid(void);
int getpriority(int, unsigned);
int getsid(int);
int ioctl(int, unsigned long, ...);
int issetugid(void);
int kill(int, int);
int killpg(int, int);
int lchmod(const char *, unsigned);
int lchown(const char *, unsigned, unsigned);
int link(const char *, const char *);
int linkat(int, const char *, int, const char *, int);
int mincore(void *, size_t, unsigned char *);
int mkdir(const char *, unsigned);
int mkdirat(int, const char *, unsigned);
int mknod(const char *, unsigned, uint64_t);
int nice(int);
int open(const char *, int, ...);
int openat(int, const char *, int, ...);
int pause(void);
int pipe(int[2]);
int pipe2(int[2], int);
int posix_fadvise(int, int64_t, int64_t, int);
int posix_madvise(void *, uint64_t, int);
int raise(int);
int reboot(int);
int remove(const char *);
int rename(const char *, const char *);
int renameat(int, const char *, int, const char *);
int rmdir(const char *);
int sched_yield(void);
int setegid(unsigned);
int seteuid(unsigned);
int setfsgid(unsigned);
int setfsuid(unsigned);
int setgid(unsigned);
int setgroups(size_t, const unsigned[]);
int setpgid(int, int);
int setpgrp(void);
int setpriority(int, unsigned, int);
int setregid(unsigned, unsigned);
int setreuid(unsigned, unsigned);
int setsid(void);
int setuid(unsigned);
int shm_open(const char *, int, unsigned);
int shm_unlink(const char *);
int sigignore(int);
int siginterrupt(int, int);
int symlink(const char *, const char *);
int symlinkat(const char *, int, const char *);
int tcgetpgrp(int);
int tcsetpgrp(int, int);
int truncate(const char *, int64_t);
int ttyname_r(int, char *, size_t);
int unlink(const char *);
int unlinkat(int, const char *, int);
int usleep(uint64_t);
int vfork(void);
int wait(int *);
int waitpid(int, int *, int);
int64_t clock(void);
int64_t time(int64_t *);
ssize_t copy_file_range(int, long *, int, long *, size_t, unsigned);
ssize_t lseek(int, int64_t, int);
ssize_t pread(int, void *, size_t, int64_t);
ssize_t pwrite(int, const void *, size_t, int64_t);
ssize_t read(int, void *, size_t);
ssize_t readlink(const char *, char *, size_t);
ssize_t readlinkat(int, const char *, char *, size_t);
ssize_t write(int, const void *, size_t);
unsigned alarm(unsigned);
unsigned getegid(void);
unsigned geteuid(void);
unsigned getgid(void);
unsigned getuid(void);
unsigned sleep(unsigned);
unsigned ualarm(unsigned, unsigned);
unsigned umask(unsigned);
void sync(void);
int syncfs(int);
int prctl(int, ...);
int gettid(void);
int setresgid(unsigned, unsigned, unsigned);
int setresuid(unsigned, unsigned, unsigned);
int getresgid(unsigned *, unsigned *, unsigned *);
int getresuid(unsigned *, unsigned *, unsigned *);
char *get_current_dir_name(void);
ssize_t splice(int, int64_t *, int, int64_t *, size_t, unsigned);
int memfd_create(const char *, unsigned int);
int execvpe(const char *, char *const[], char *const[]);
int euidaccess(const char *, int);
int eaccess(const char *, int);
int madvise(void *, uint64_t, int);
int getcpu(unsigned *, unsigned *);
bool32 fdexists(int);
bool32 fileexists(const char *);
bool32 ischardev(int);
bool32 isdirectory(const char *);
bool32 isexecutable(const char *);
bool32 isregularfile(const char *);
bool32 issymlink(const char *);
char *commandv(const char *, char *, size_t);
int __getcwd(char *, size_t);
int clone(void *, void *, size_t, int, void *, void *, void *, void *);
int fadvise(int, uint64_t, uint64_t, int);
int makedirs(const char *, unsigned);
int pivot_root(const char *, const char *);
int pledge(const char *, const char *);
int seccomp(unsigned, unsigned, void *);
int sys_iopl(int);
int sys_ioprio_get(int, int);
int sys_ioprio_set(int, int, int);
int sys_mlock(const void *, size_t);
int sys_mlock2(const void *, size_t, int);
int sys_mlockall(int);
int sys_munlock(const void *, size_t);
int sys_munlockall(void);
int sys_personality(uint64_t);
int sys_ptrace(int, ...);
int sysctl(int *, unsigned, void *, size_t *, void *, size_t);
int sysctlbyname(const char *, void *, size_t *, void *, size_t);
int sysctlnametomib(const char *, int *, size_t *);
int tmpfd(void);
int touch(const char *, unsigned);
int unveil(const char *, const char *);
long ptrace(int, ...);
ssize_t copyfd(int, int, size_t);
ssize_t readansi(int, char *, size_t);
ssize_t tinyprint(int, const char *, ...);
void shm_path_np(const char *, char[78]);
int system(const char *);
int __wifstopped(int);
int __wifcontinued(int);
int __wifsignaled(int);

/* From libc/runtime/runtime.h */
typedef long jmp_buf[8];
typedef long sigjmp_buf[11];
void mcount(void);
int daemon(int, int);
unsigned long getauxval(unsigned long);
int setjmp(jmp_buf);
void longjmp(jmp_buf, int);
int _setjmp(jmp_buf);
int sigsetjmp(sigjmp_buf, int);
void siglongjmp(sigjmp_buf, int);
void _longjmp(jmp_buf, int);
void exit(int);
void _exit(int);
void _Exit(int);
void quick_exit(int);
void abort(void);
int atexit(void (*)(void));
char *getenv(const char *);
int putenv(char *);
int setenv(const char *, const char *, int);
int unsetenv(const char *);
int clearenv(void);
void fpreset(void);
void *mmap(void *, uint64_t, int32_t, int32_t, int32_t, int64_t);
void *mremap(void *, size_t, size_t, int, ...);
int munmap(void *, uint64_t);
int mprotect(void *, uint64_t, int);
int msync(void *, size_t, int);
int mlock(const void *, size_t);
int munlock(const void *, size_t);
long gethostid(void);
int sethostid(long);
char *getlogin(void);
int getlogin_r(char *, size_t);
int login_tty(int);
int getpagesize(void);
int getgransize(void);
int vhangup(void);
int getdtablesize(void);
int sethostname(const char *, size_t);
int acct(const char *);
extern char **environ;
char *secure_getenv(const char *);
extern int __argc;
extern char **__argv;
extern char **__envp;
extern int __pagesize;
extern int __gransize;
extern unsigned long *__auxv;
extern intptr_t __oldstack;
extern char *__program_executable_name;
extern uint64_t __nosync;
extern int __strace;
extern int __ftrace;
extern uint64_t __syscount;
extern uint64_t kStartTsc;
extern const char kNtSystemDirectory[];
extern const char kNtWindowsDirectory[];
extern size_t __virtualmax;
extern size_t __stackmax;
extern bool32 __isworker;
void _intsort(int *, size_t);
void _longsort(long *, size_t);
void ShowCrashReports(void);
int ftrace_install(void);
int ftrace_enabled(int);
int strace_enabled(int);
void __print_maps(size_t);
void __print_maps_win32(int64_t, const char *, size_t);
void __printargs(const char *);
int _cocmd(int, char **, char **);
char *GetProgramExecutableName(void);
char *GetInterpreterExecutableName(char *, size_t);
int __open_executable(void);
int verynice(void);
void __warn_if_powersave(void);
void _Exit1(int);
void __paginate(int, const char *);
void __paginate_file(int, const char *);
void _weakfree(void *);
void *_mapanon(size_t);
void *_mapshared(size_t);
void CheckForFileLeaks(void);
void __oom_hook(size_t);
void __morph_begin(void);
void __morph_end(void);
void __jit_begin(void);
void __jit_end(void);
void __clear_cache(void *, void *);
bool32 IsGenuineBlink(void);
bool32 IsCygwin(void);
const char *GetCpuidOs(void);
const char *GetCpuidEmulator(void);
void GetCpuidBrand(char[13], uint32_t);
long __get_rlimit(int);
const char *__describe_os(void);
long __get_sysctl(int, int);
int __get_arg_max(void);
int __get_cpu_count(void);
long __get_avphys_pages(void);
long __get_phys_pages(void);
long __get_minsigstksz(void);
long __get_safe_size(long, long);
char *__get_tmpdir(void);
static inline int __trace_disabled(int x) { return 0; }

/* Additional definitions needed for QuickJS/wasm3 compilation */

/* errno */
extern int errno;
#define EPERM 1
#define ENOENT 2
#define EINTR 4
#define EIO 5
#define EBADF 9
#define EAGAIN 11
#define EWOULDBLOCK EAGAIN
#define ENOMEM 12
#define EACCES 13
#define EFAULT 14
#define EBUSY 16
#define EEXIST 17
#define ENODEV 19
#define ENOTDIR 20
#define EISDIR 21
#define EINVAL 22
#define ENOSPC 28
#define EPIPE 32
#define ERANGE 34
#define ENAMETOOLONG 36
#define ENOSYS 38
#define ETIMEDOUT 60
#define EOVERFLOW 84

/* Standard file descriptors */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* File flags */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_NONBLOCK  0x0004
#define O_APPEND    0x0008
#define O_CREAT     0x0200
#define O_TRUNC     0x0400
#define O_EXCL      0x0800
#define O_DIRECTORY 0x100000
#define O_NOFOLLOW  0x0100

/* fcntl commands */
#define F_GETFL     3
#define F_SETFL     4

/* lseek whence values  */

/* Signal constants */
#define SIGINT      2
#define SIGTERM     15
#define SIG_DFL     ((void (*)(int))0)
#define SIG_IGN     ((void (*)(int))1)
typedef void (*sighandler_t)(int);
extern sighandler_t signal(int signum, sighandler_t handler);

/* Signal numbers */
#define SIGHUP  1
#define SIGQUIT 3
#define SIGILL  4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGFPE  8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22

/* stdio.h constants */
#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2
#define EOF      -1

/* inttypes.h format macros */
#define PRId64 "lld"
#define PRIu64 "llu"
#define PRIx64 "llx"
#define PRIX64 "llX"
#define PRId32 "d"
#define PRIu32 "u"
#define PRIx32 "x"

/* Math constants */
#define NAN       (0.0/0.0)
#define INFINITY  (1.0/0.0)
#define HUGE_VAL  1.7976931348623157e+308

/* Math type checking macros */
#define isnan(x) __builtin_isnan(x)
#define isfinite(x) __builtin_isfinite(x)
#define signbit(x) __builtin_signbit(x)

/* fenv.h functions - constants defined later with correct values */
int fesetround(int rounding_mode);

/* stdlib.h functions */
int rand(void);
void srand(unsigned int seed);
int abs(int n);
long labs(long n);
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

/* Additional types and structures for QuickJS/wasm3 */

/* Socket/network types */
typedef unsigned int socklen_t;
typedef unsigned short sa_family_t;
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr { in_addr_t s_addr; };
struct sockaddr { sa_family_t sa_family; char sa_data[14]; };
struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    unsigned char sin_zero[8];
};

/* termios structure */
typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;
struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_cc[20];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

/* winsize structure for ioctl */
struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

/* ioctl constants */
#define TIOCGWINSZ 0x5413

/* dirent structures */
typedef struct __DIR DIR;
struct dirent {
    unsigned long d_ino;
    unsigned long d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12
#define DT_WHT     14

/* time structures */
typedef long time_t;
struct timeval { long tv_sec; long tv_usec; };
struct timespec { long tv_nsec; long tv_sec; };
struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
    long tm_gmtoff;
    const char *tm_zone;
};

extern struct tm *localtime_r(const time_t *timep, struct tm *result);
extern struct tm *gmtime_r(const time_t *timep, struct tm *result);
extern time_t mktime(struct tm *tm);
extern int gettimeofday(struct timeval *tv, void *tz);

/* utsname structure for uname() */
/* utsname structure - must match Cosmopolitan's definition */
#define SYS_NMLN 150
struct utsname {
    char sysname[SYS_NMLN];    /* name of os */
    char nodename[SYS_NMLN];   /* name of network node */
    char release[SYS_NMLN];    /* release level */
    char version[SYS_NMLN];    /* version level */
    char machine[SYS_NMLN];    /* hardware type */
    char domainname[SYS_NMLN]; /* domain name (Cosmopolitan has this!) */
};

/* stat structure */
struct stat {
    unsigned long st_dev;
    unsigned long st_ino;
    unsigned int st_mode;
    unsigned int st_nlink;
    unsigned int st_uid;
    unsigned int st_gid;
    unsigned long st_rdev;
    long st_size;
    long st_blksize;
    long st_blocks;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    long st_atime;
    long st_mtime;
    long st_ctime;
};

#define S_IFMT   0170000
#define S_IFREG  0100000
#define S_IFDIR  0040000
#define S_IFLNK  0120000
#define S_IFIFO  0010000
#define S_IFCHR  0020000
#define S_IFBLK  0060000
#define S_IFSOCK 0140000
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000
#define S_IXUSR  0000100
#define S_IRUSR  0000400
#define S_IWUSR  0000200
#define S_IRWXU  0000700
#define S_IRGRP  0000040
#define S_IWGRP  0000020
#define S_IXGRP  0000010
#define S_IRWXG  0000070
#define S_IROTH  0000004
#define S_IWOTH  0000002
#define S_IXOTH  0000001
#define S_IRWXO  0000007

/* wait() options and macros */
#define WNOHANG 1
#define WUNTRACED 2
#define WIFEXITED(status)   (((status) & 0x7F) == 0)
#define WEXITSTATUS(status) (((status) & 0xFF00) >> 8)
#define WIFSIGNALED(status) (((status) & 0x7F) > 0 && ((status) & 0x7F) < 0x7F)
#define WTERMSIG(status)    ((status) & 0x7F)
#define WIFSTOPPED(status)  (((status) & 0xFF) == 0x7F)
#define WSTOPSIG(status)    (((status) & 0xFF00) >> 8)

/* rlimit structure for setrlimit/getrlimit */
struct rlimit {
    unsigned long rlim_cur;
    unsigned long rlim_max;
};
#define RLIMIT_AS 9

/* clock constants */
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

/* jmp_buf for setjmp/longjmp  */
typedef int sig_atomic_t;

/* getopt structures */
struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};
#define no_argument       0
#define required_argument 1
#define optional_argument 2
extern char *optarg;
extern int optind, opterr, optopt;

/* environ global */
extern char **environ;

/* sysconf constants */
#define _SC_OPEN_MAX 4
#define _SC_PAGESIZE 30
#define _SC_NPROCESSORS_ONLN 84

/* File access modes for access() */
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

/* pthread types - simplified for TCC */
typedef struct { int __reserved; } pthread_mutex_t;
typedef struct { int __reserved; } pthread_cond_t;
typedef struct { unsigned long __reserved; } pthread_t;
typedef struct { int __reserved; } pthread_attr_t;
typedef struct { int __reserved; } pthread_mutexattr_t;

#define PTHREAD_MUTEX_INITIALIZER {0}
#define PTHREAD_CREATE_DETACHED 1
#define PTHREAD_CREATE_JOINABLE 0

/* sigaction structure */
typedef struct {
    unsigned long __bits[16];
} sigset_t;

struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
    void (*sa_restorer)(void);
};

#define SA_RESTART  0x10000000
#define SA_NOCLDSTOP 1

/* Socket constants */
#define AF_INET     2
#define SOCK_STREAM 1
#define IPPROTO_TCP 6
#define SOL_SOCKET  0xFFFF
#define SO_REUSEADDR 0x0004
#define SO_RCVTIMEO  0x1006
#define SO_SNDTIMEO  0x1005
#define INADDR_LOOPBACK 0x7f000001
#define INADDR_ANY      0x00000000

/* termios flags */
#define TCSANOW     0
#define TCSADRAIN   1
#define TCSAFLUSH   2
#define ECHO        0x00000008
#define ICANON      0x00000100
#define VMIN        16
#define VTIME       17

/* termios input flags */
#define IGNBRK  0x00000001
#define BRKINT  0x00000002
#define IGNPAR  0x00000004
#define PARMRK  0x00000008
#define INPCK   0x00000010
#define ISTRIP  0x00000020
#define INLCR   0x00000040
#define IGNCR   0x00000080
#define ICRNL   0x00000100
#define IXON    0x00000400
#define IXOFF   0x00001000
#define IXANY   0x00000800

/* termios output flags */
#define OPOST   0x00000001
#define ONLCR   0x00000002

/* termios control flags */
#define CSIZE   0x00000300
#define CS5     0x00000000
#define CS6     0x00000100
#define CS7     0x00000200
#define CS8     0x00000300
#define CSTOPB  0x00000400
#define CREAD   0x00000800
#define PARENB  0x00001000
#define PARODD  0x00002000
#define HUPCL   0x00004000
#define CLOCAL  0x00008000

/* termios local flags */
#define ISIG    0x00000001
#define ECHOE   0x00000010
#define ECHOK   0x00000020
#define ECHONL  0x00000040
#define NOFLSH  0x00000080
#define TOSTOP  0x00000100
#define ECHOCTL 0x00000200
#define ECHOPRT 0x00000400
#define ECHOKE  0x00000800
#define FLUSHO  0x00001000
#define PENDIN  0x00004000
#define IEXTEN  0x00008000

/* termios control characters indices */
#define VINTR   0
#define VQUIT   1
#define VERASE  2
#define VKILL   3
#define VEOF    4
#define VEOL    5
#define VEOL2   6
#define VSTART  7
#define VSTOP   8
#define VSUSP   9
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT  15

/* dlfcn.h dynamic loading constants */
#define RTLD_NOW    2
#define RTLD_LAZY   1
#define RTLD_GLOBAL 256
#define RTLD_LOCAL  0

/* Cosmopolitan dynamic loading functions */
extern void *cosmo_dlopen(const char *filename, int flags);
extern void *cosmo_dlsym(void *handle, const char *symbol);
extern int cosmo_dlclose(void *handle);
extern char *cosmo_dlerror(void);

/* fenv.h floating-point environment (x86_64/aarch64 values) */
#define FE_TONEAREST  0x0000
#define FE_DOWNWARD   0x0400
#define FE_UPWARD     0x0800
#define FE_TOWARDZERO 0x0c00
typedef int fenv_t;
typedef int fexcept_t;
extern int fesetround(int round);
extern int fegetround(void);

/* Atomic types (simplified for TCC) */
typedef int atomic_int;
#define ATOMIC_VAR_INIT(value) (value)
#define atomic_load(ptr) (*(ptr))
#define atomic_store(ptr, val) (*(ptr) = (val))
#define atomic_fetch_add(ptr, val) __sync_fetch_and_add(ptr, val)
#define atomic_fetch_sub(ptr, val) __sync_fetch_and_sub(ptr, val)

/* File descriptor sets for select() */
#define FD_SETSIZE 1024
typedef struct { long fds_bits[16]; } fd_set;

#define FD_ZERO(set) do { \
    int __i; \
    for (__i = 0; __i < 16; __i++) \
        ((fd_set*)(set))->fds_bits[__i] = 0; \
} while(0)

#define FD_SET(fd, set) do { \
    ((fd_set*)(set))->fds_bits[(fd)/64] |= (1UL << ((fd) % 64)); \
} while(0)

#define FD_ISSET(fd, set) \
    (((fd_set*)(set))->fds_bits[(fd)/64] & (1UL << ((fd) % 64)))

/* String/memory functions */
extern size_t strspn(const char *s, const char *accept);
extern size_t strcspn(const char *s, const char *reject);
extern char *strdup(const char *s);

/* Math functions */
extern double pow(double x, double y);
extern double sqrt(double x);
extern double floor(double x);
extern double ceil(double x);
extern double fabs(double x);
extern double fmod(double x, double y);
extern double sin(double x);
extern double cos(double x);
extern double tan(double x);
extern double asin(double x);
extern double acos(double x);
extern double atan(double x);
extern double atan2(double y, double x);
extern double exp(double x);
extern double log(double x);
extern double log10(double x);
extern double trunc(double x);
extern double round(double x);
extern double sinh(double x);
extern double cosh(double x);
extern double tanh(double x);
extern double asinh(double x);
extern double acosh(double x);
extern double atanh(double x);
extern double exp2(double x);
extern double expm1(double x);
extern double log1p(double x);
extern double log2(double x);
extern double cbrt(double x);
extern double hypot(double x, double y);
extern double copysign(double x, double y);
extern double nextafter(double x, double y);
extern int isnan(double x);
extern int isinf(double x);
extern int isfinite(double x);
extern int signbit(double x);
extern double strtod(const char *str, char **endptr);
extern long lrint(double x);

#else /* !__COSMORUN__ */

/* Normal compilation (including cosmocc): use system headers */
#if defined(__APPLE__) || defined(__linux__) || defined(__COSMOPOLITAN__)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <setjmp.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <dirent.h>
#include <strings.h>
#include <sched.h>
#include <poll.h>
#include <netdb.h>
#include <sys/ioctl.h>

#ifdef __COSMOPOLITAN__

#ifndef _COSMO_SOURCE
//@see third_party/cosmopolitan/, important for dce.h
#define _COSMO_SOURCE
#endif
#include "libc/dce.h" //IsWindows,IsXnu
#include "libc/log/backtrace.internal.h"
#include "libc/nt/memory.h"
#include "libc/nt/enum/pageflags.h"

#endif

#endif

#ifdef _WIN32
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define sleep(x) Sleep((x) * 1000)
#define usleep(x) Sleep((x) / 1000)
#ifndef ssize_t
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#endif

#define _STDBOOL_H
#define _STDINT_H

#ifndef _STDDEF_H
#define _STDDEF_H
#endif
#ifndef _STDDEF_H_
#define _STDDEF_H_
#endif

#endif /* __COSMORUN__ */


#ifndef _GCC_WRAP_STDINT_H
#define _GCC_WRAP_STDINT_H
#endif

#endif /* COSMORUN_H */
