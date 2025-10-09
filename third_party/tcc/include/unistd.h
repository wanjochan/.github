#ifndef _UNISTD_H_
#define _UNISTD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

/* File descriptor values */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Access modes */
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

/* Seek whence values */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* Standard types - moved before use */
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef long ssize_t;
typedef long off_t;
typedef unsigned int useconds_t;

/* Process functions */
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
gid_t getgid(void);
uid_t geteuid(void);
gid_t getegid(void);

/* File operations */
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
off_t lseek(int fd, off_t offset, int whence);
int access(const char *pathname, int mode);
int unlink(const char *pathname);
int rmdir(const char *pathname);

/* Directory operations */
int chdir(const char *path);
char *getcwd(char *buf, size_t size);

/* Process control */
int execl(const char *path, const char *arg, ...);
int execle(const char *path, const char *arg, ...);
int execlp(const char *file, const char *arg, ...);
int execv(const char *path, char *const argv[]);
int execve(const char *path, char *const argv[], char *const envp[]);
int execvp(const char *file, char *const argv[]);

pid_t fork(void);
unsigned int sleep(unsigned int seconds);
int usleep(useconds_t usec);

#ifdef __cplusplus
}
#endif

#endif /* _UNISTD_H_ */