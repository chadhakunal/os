#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>

#define EOF (-1)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef long fpos_t;

typedef struct FILE {
  int  fd;
  int  eof;
  int  err;
  /* fmemopen state */
  char       *membuf;
  size_t      memsize;
  size_t      mempos;
  int         memonly;
} FILE;

extern FILE __stdin_file;
extern FILE __stdout_file;
extern FILE __stderr_file;

#define stdin  (&__stdin_file)
#define stdout (&__stdout_file)
#define stderr (&__stderr_file)

/* open / close */
FILE   *fopen(const char *path, const char *mode);
FILE   *fdopen(int fd, const char *mode);
FILE   *tmpfile(void);
FILE   *fmemopen(void *buf, size_t size, const char *mode);
int     fclose(FILE *stream);

/* character I/O */
int     fgetc(FILE *stream);
int     fputc(int c, FILE *stream);
char   *fgets(char *s, int n, FILE *stream);
int     fputs(const char *s, FILE *stream);
int     ungetc(int c, FILE *stream);

/* block I/O */
size_t  fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t  fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

/* positioning */
int     fseek(FILE *stream, long offset, int whence);
long    ftell(FILE *stream);
void    rewind(FILE *stream);
int     fgetpos(FILE *stream, fpos_t *pos);
int     fsetpos(FILE *stream, const fpos_t *pos);

/* status */
int     feof(FILE *stream);
int     ferror(FILE *stream);
void    clearerr(FILE *stream);
int     fileno(FILE *stream);
int     fflush(FILE *stream);

/* locking stubs */
void    flockfile(FILE *stream);
void    funlockfile(FILE *stream);
int     ftrylockfile(FILE *stream);

/* formatted I/O */
int     printf(const char *fmt, ...);
int     fprintf(FILE *stream, const char *fmt, ...);
int     vfprintf(FILE *stream, const char *fmt, va_list ap);
int     vprintf(const char *fmt, va_list ap);
int     dprintf(int fd, const char *fmt, ...);
int     vdprintf(int fd, const char *fmt, va_list ap);
int     snprintf(char *str, size_t size, const char *fmt, ...);
int     vsnprintf(char *str, size_t size, const char *fmt, va_list ap);
int     sprintf(char *str, const char *fmt, ...);
int     vsprintf(char *str, const char *fmt, va_list ap);
int     asprintf(char **strp, const char *fmt, ...);
int     vasprintf(char **strp, const char *fmt, va_list ap);
int     fscanf(FILE *stream, const char *fmt, ...);
int     vfscanf(FILE *stream, const char *fmt, va_list ap);
int     scanf(const char *fmt, ...);
int     vscanf(const char *fmt, va_list ap);
int     sscanf(const char *str, const char *fmt, ...);
int     vsscanf(const char *str, const char *fmt, va_list ap);

#endif
