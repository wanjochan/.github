/* unistd.h compatibility layer for Windows */
#ifndef _UNISTD_H
#define _UNISTD_H

#include <io.h>
#include <process.h>
#include <direct.h>

#define R_OK    4       /* Test for read permission.  */
#define W_OK    2       /* Test for write permission.  */
#define X_OK    1       /* Test for execute permission.  */
#define F_OK    0       /* Test for existence.  */

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* ssize_t可能在其他地方已定义，所以这里不再定义 */

#define access _access
#define dup _dup
#define dup2 _dup2
#define close _close
#define read _read
#define write _write
#define lseek _lseek
#define isatty _isatty
#define chdir _chdir
#define getcwd _getcwd
#define unlink _unlink

#define sleep(x) Sleep((x) * 1000)

#endif /* _UNISTD_H */ 