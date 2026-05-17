#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

/* -------------------------------------------------------------------------
 * FILE allocation
 * ------------------------------------------------------------------------- */

static FILE *alloc_file(int fd) {
  FILE *f = malloc(sizeof(FILE));
  if (!f) return NULL;
  f->fd      = fd;
  f->eof     = 0;
  f->err     = 0;
  f->membuf  = NULL;
  f->memsize = 0;
  f->mempos  = 0;
  f->memonly = 0;
  return f;
}

/* -------------------------------------------------------------------------
 * open / close
 * ------------------------------------------------------------------------- */

FILE *fopen(const char *path, const char *mode) {
  int flags;
  if (mode[0] == 'r')
    flags = (mode[1] == '+') ? O_RDWR : O_RDONLY;
  else if (mode[0] == 'w')
    flags = O_WRONLY | O_CREAT | O_TRUNC  | (mode[1] == '+' ? O_RDWR : 0);
  else
    flags = O_WRONLY | O_CREAT | O_APPEND | (mode[1] == '+' ? O_RDWR : 0);

  int fd = open(path, flags, 0666);
  if (fd < 0) return NULL;
  FILE *f = alloc_file(fd);
  if (!f) { close(fd); return NULL; }
  return f;
}

FILE *freopen(const char *path, const char *mode, FILE *stream) {
  if (!stream) return NULL;
  if (!stream->memonly && stream->fd >= 0)
    close(stream->fd);
  stream->fd     = -1;
  stream->eof    = 0;
  stream->err    = 0;
  stream->memonly = 0;

  int flags;
  if (mode[0] == 'r')
    flags = (mode[1] == '+') ? O_RDWR : O_RDONLY;
  else if (mode[0] == 'w')
    flags = O_WRONLY | O_CREAT | O_TRUNC  | (mode[1] == '+' ? O_RDWR : 0);
  else
    flags = O_WRONLY | O_CREAT | O_APPEND | (mode[1] == '+' ? O_RDWR : 0);

  int fd = open(path, flags, 0666);
  if (fd < 0) return NULL;
  stream->fd = fd;
  return stream;
}

FILE *fdopen(int fd, const char *mode) {
  (void)mode;
  return alloc_file(fd);
}

FILE *tmpfile(void) {
  char path[] = "/tmp/os-tmpfile-XXXXXX";
  int fd = mkstemp(path);
  if (fd < 0)
    return NULL;
  unlink(path);
  FILE *f = alloc_file(fd);
  if (!f) {
    close(fd);
    return NULL;
  }
  return f;
}

FILE *fmemopen(void *buf, size_t size, const char *mode) {
  (void)mode;
  FILE *f = alloc_file(-1);
  if (!f) return NULL;
  f->membuf  = (char *)buf;
  f->memsize = size;
  f->mempos  = 0;
  f->memonly = 1;
  return f;
}

int fclose(FILE *stream) {
  if (!stream) return EOF;
  int rc = 0;
  if (!stream->memonly && stream->fd >= 0)
    rc = close(stream->fd);
  /* stdin/stdout/stderr are static — don't free them */
  if (stream != stdin && stream != stdout && stream != stderr)
    free(stream);
  return rc ? EOF : 0;
}

/* -------------------------------------------------------------------------
 * character I/O
 * ------------------------------------------------------------------------- */

int fgetc(FILE *stream) {
  if (!stream || stream->eof) return EOF;
  unsigned char c;
  if (stream->memonly) {
    if (stream->mempos >= stream->memsize) { stream->eof = 1; return EOF; }
    return (unsigned char)stream->membuf[stream->mempos++];
  }
  ssize_t n = read(stream->fd, &c, 1);
  if (n <= 0) { if (n == 0) stream->eof = 1; else stream->err = 1; return EOF; }
  return (unsigned char)c;
}

int fputc(int c, FILE *stream) {
  if (!stream) return EOF;
  unsigned char ch = (unsigned char)c;
  if (stream->memonly) {
    if (stream->mempos >= stream->memsize) return EOF;
    stream->membuf[stream->mempos++] = (char)ch;
    return ch;
  }
  if (write(stream->fd, &ch, 1) == 1) return ch;
  stream->err = 1;
  return EOF;
}

char *fgets(char *s, int n, FILE *stream) {
  if (!s || n <= 0 || !stream || stream->eof) return NULL;
  int i = 0;
  while (i < n - 1) {
    int c = fgetc(stream);
    if (c == EOF) { if (i == 0) return NULL; break; }
    s[i++] = (char)c;
    if (c == '\n') break;
  }
  s[i] = '\0';
  return s;
}

int fputs(const char *s, FILE *stream) {
  if (!s || !stream) return EOF;
  size_t len = strlen(s);
  if (len == 0) return 0;
  if (stream->memonly) {
    size_t avail = stream->memsize - stream->mempos;
    size_t w = len < avail ? len : avail;
    memcpy(stream->membuf + stream->mempos, s, w);
    stream->mempos += w;
    return w == len ? 0 : EOF;
  }
  ssize_t n = write(stream->fd, s, len);
  return (n < 0 || (size_t)n != len) ? EOF : 0;
}

int ungetc(int c, FILE *stream) {
  if (!stream || c == EOF) return EOF;
  if (stream->memonly) {
    if (stream->mempos == 0) return EOF;
    stream->mempos--;
    stream->membuf[stream->mempos] = (char)c;
    stream->eof = 0;
    return c;
  }
  /* no pushback buffer for fd streams yet */
  return EOF;
}

/* -------------------------------------------------------------------------
 * block I/O
 * ------------------------------------------------------------------------- */

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  if (!stream || size == 0 || nmemb == 0) return 0;
  size_t total = size * nmemb;
  if (stream->memonly) {
    size_t avail = stream->memsize - stream->mempos;
    size_t r = total < avail ? total : avail;
    memcpy(ptr, stream->membuf + stream->mempos, r);
    stream->mempos += r;
    if (r < total) stream->eof = 1;
    return r / size;
  }
  ssize_t n = read(stream->fd, ptr, total);
  if (n <= 0) { if (n == 0) stream->eof = 1; else stream->err = 1; return 0; }
  return (size_t)n / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
  if (!stream || size == 0 || nmemb == 0) return 0;
  size_t total = size * nmemb;
  if (stream->memonly) {
    size_t avail = stream->memsize - stream->mempos;
    size_t w = total < avail ? total : avail;
    memcpy(stream->membuf + stream->mempos, ptr, w);
    stream->mempos += w;
    return w / size;
  }
  ssize_t n = write(stream->fd, ptr, total);
  if (n < 0) { stream->err = 1; return 0; }
  return (size_t)n / size;
}

/* -------------------------------------------------------------------------
 * positioning
 * ------------------------------------------------------------------------- */

int fseek(FILE *stream, long offset, int whence) {
  if (!stream) return -1;
  if (stream->memonly) {
    size_t newpos;
    if (whence == SEEK_SET)      newpos = (size_t)offset;
    else if (whence == SEEK_CUR) newpos = (size_t)((long)stream->mempos + offset);
    else                         newpos = (size_t)((long)stream->memsize + offset);
    if (newpos > stream->memsize) return -1;
    stream->mempos = newpos;
    stream->eof = 0;
    return 0;
  }
  off_t r = lseek(stream->fd, (off_t)offset, whence);
  if (r < 0) return -1;
  stream->eof = 0;
  return 0;
}

long ftell(FILE *stream) {
  if (!stream) return -1;
  if (stream->memonly) return (long)stream->mempos;
  return (long)lseek(stream->fd, 0, SEEK_CUR);
}

void rewind(FILE *stream) {
  if (!stream) return;
  fseek(stream, 0, SEEK_SET);
  stream->err = 0;
}

int fgetpos(FILE *stream, fpos_t *pos) {
  if (!stream || !pos) return -1;
  long p = ftell(stream);
  if (p < 0) return -1;
  *pos = (fpos_t)p;
  return 0;
}

int fsetpos(FILE *stream, const fpos_t *pos) {
  if (!stream || !pos) return -1;
  return fseek(stream, (long)*pos, SEEK_SET);
}

/* -------------------------------------------------------------------------
 * status
 * ------------------------------------------------------------------------- */

int feof(FILE *stream)   { return stream ? stream->eof : 1; }
int ferror(FILE *stream) { return stream ? stream->err : 1; }
void clearerr(FILE *stream) { if (stream) { stream->eof = 0; stream->err = 0; } }
int fileno(FILE *stream) { return stream ? stream->fd : -1; }
int fflush(FILE *stream) { (void)stream; return 0; }

/* -------------------------------------------------------------------------
 * locking — single-threaded, all no-ops
 * ------------------------------------------------------------------------- */

void flockfile(FILE *stream)    { (void)stream; }
void funlockfile(FILE *stream)  { (void)stream; }
int  ftrylockfile(FILE *stream) { (void)stream; return 0; }

/* -------------------------------------------------------------------------
 * character I/O convenience wrappers
 * ------------------------------------------------------------------------- */

int getchar(void)           { return fgetc(stdin); }
int putchar(int c)          { return fputc(c, stdout); }
int puts(const char *s)     { if (fputs(s, stdout) < 0) return EOF; return fputc('\n', stdout); }
int getc(FILE *f)           { return fgetc(f); }
int putc(int c, FILE *f)    { return fputc(c, f); }
int getc_unlocked(FILE *f)           { return fgetc(f); }
int putc_unlocked(int c, FILE *f)    { return fputc(c, f); }
int getchar_unlocked(void)           { return fgetc(stdin); }
int putchar_unlocked(int c)          { return fputc(c, stdout); }

/* -------------------------------------------------------------------------
 * off_t positioning
 * ------------------------------------------------------------------------- */

int   fseeko(FILE *f, off_t offset, int whence) { return fseek(f, (long)offset, whence); }
off_t ftello(FILE *f)                           { return (off_t)ftell(f); }

/* -------------------------------------------------------------------------
 * buffering — single-threaded, no real buffering, stubs only
 * ------------------------------------------------------------------------- */

void setbuf(FILE *f, char *buf)                        { (void)f; (void)buf; }
int  setvbuf(FILE *f, char *buf, int mode, size_t sz)  { (void)f; (void)buf; (void)mode; (void)sz; return 0; }

/* -------------------------------------------------------------------------
 * line reading
 * ------------------------------------------------------------------------- */

ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *f) {
  if (!lineptr || !n || !f) { return -1; }
  if (!*lineptr || *n == 0) {
    *n = 128;
    *lineptr = malloc(*n);
    if (!*lineptr) return -1;
  }
  size_t len = 0;
  int c;
  while ((c = fgetc(f)) != EOF) {
    if (len + 1 >= *n) {
      size_t newn = *n * 2;
      char *newp = realloc(*lineptr, newn);
      if (!newp) return -1;
      *lineptr = newp;
      *n = newn;
    }
    (*lineptr)[len++] = (char)c;
    if (c == delim) break;
  }
  if (len == 0) return -1;
  (*lineptr)[len] = '\0';
  return (ssize_t)len;
}

ssize_t getline(char **lineptr, size_t *n, FILE *f) {
  return getdelim(lineptr, n, '\n', f);
}

/* -------------------------------------------------------------------------
 * memory stream (write-only)
 * ------------------------------------------------------------------------- */

FILE *open_memstream(char **ptr, size_t *sizeloc) {
  FILE *f = fmemopen(NULL, 128, "w");
  if (!f) return NULL;
  /* store pointers so caller can retrieve buffer — best-effort stub */
  (void)ptr; (void)sizeloc;
  return f;
}

/* -------------------------------------------------------------------------
 * pipe streams — not supported, return NULL/error
 * ------------------------------------------------------------------------- */

FILE *popen(const char *cmd, const char *type) { (void)cmd; (void)type; return NULL; }
int   pclose(FILE *f)                          { (void)f; return -1; }

/* -------------------------------------------------------------------------
 * error / file ops
 * ------------------------------------------------------------------------- */

void perror(const char *s) {
  if (s && *s) { fputs(s, stderr); fputs(": ", stderr); }
  fputs("error\n", stderr);
}

int remove(const char *path) {
  return unlink(path);
}

static char tmpnam_buf[L_tmpnam];
char *tmpnam(char *s) {
  const char *name = "/tmp/tmp000000";
  char *dst = s ? s : tmpnam_buf;
  int i = 0;
  while ((dst[i] = name[i])) i++;
  return dst;
}

static char ctermid_buf[L_ctermid];
char *ctermid(char *s) {
  const char *name = "/dev/tty";
  char *dst = s ? s : ctermid_buf;
  int i = 0;
  while ((dst[i] = name[i])) i++;
  return dst;
}
