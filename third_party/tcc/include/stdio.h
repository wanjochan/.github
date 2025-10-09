#ifndef _STDIO_H_
#define _STDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdarg.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef EOF
#define EOF (-1)
#endif

#define BUFSIZ 8192
#define FILENAME_MAX 4096
#define FOPEN_MAX 20
#define TMP_MAX 238328
#define L_tmpnam 20

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct _FILE FILE;
typedef long fpos_t;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* File operations */
FILE *fopen(const char *filename, const char *mode);
FILE *freopen(const char *filename, const char *mode, FILE *stream);
int fclose(FILE *stream);
int fflush(FILE *stream);
void clearerr(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
int fileno(FILE *stream);

/* Character I/O */
int fgetc(FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int fputc(int c, FILE *stream);
int fputs(const char *s, FILE *stream);
int getc(FILE *stream);
int getchar(void);
char *gets(char *s);
int putc(int c, FILE *stream);
int putchar(int c);
int puts(const char *s);
int ungetc(int c, FILE *stream);

/* Formatted I/O */
int fprintf(FILE *stream, const char *format, ...);
int fscanf(FILE *stream, const char *format, ...);
int printf(const char *format, ...);
int scanf(const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int sscanf(const char *str, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list ap);
int vfscanf(FILE *stream, const char *format, va_list ap);
int vprintf(const char *format, va_list ap);
int vscanf(const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
int vsscanf(const char *str, const char *format, va_list ap);

/* Direct I/O */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

/* File positioning */
int fgetpos(FILE *stream, fpos_t *pos);
int fseek(FILE *stream, long int offset, int whence);
int fsetpos(FILE *stream, const fpos_t *pos);
long int ftell(FILE *stream);
void rewind(FILE *stream);

/* Error handling */
void perror(const char *s);

/* Temporary files */
FILE *tmpfile(void);
char *tmpnam(char *s);

/* File management */
int remove(const char *filename);
int rename(const char *old_filename, const char *new_filename);

#ifdef __cplusplus
}
#endif

#endif /* _STDIO_H_ */