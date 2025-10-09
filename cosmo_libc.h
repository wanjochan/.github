#ifndef COSMORUN_H
#define COSMORUN_H

/* Cosmopolitan libc compatibility header for TCC runtime compilation */

/* When using TCC with cosmorun, use auto-generated libc headers */
#ifdef __COSMORUN__

/* va_list must be defined before including cosmorun_libc.h */
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

/* Define NULL before cosmorun_libc.h to prevent redefinition */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* Standard bool type (C99) - must be defined before cosmorun_libc.h */
#ifndef __cplusplus
#ifndef _Bool
#define _Bool int
#endif
typedef _Bool bool;
#define true 1
#define false 0
#endif

/* Include auto-generated Cosmopolitan libc headers */
#include "cosmorun_libc.h"

/* Fix NULL redefinition from cosmorun_libc.h */
#ifdef NULL
#undef NULL
#endif
#define NULL ((void*)0)

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

/* lseek whence values (already in cosmorun_libc.h) */

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

/* jmp_buf for setjmp/longjmp (from cosmorun_libc.h) */
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

/* String/memory functions - declarations from cosmorun_libc.h */
extern size_t strspn(const char *s, const char *accept);
extern size_t strcspn(const char *s, const char *reject);
extern char *strdup(const char *s);

/* Math functions - declarations from cosmorun_libc.h */
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
