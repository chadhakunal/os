#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>

/* -------------------------------------------------------------------------
 * Static buffers for stdin/stdout/stderr
 * ------------------------------------------------------------------------- */

static char stdout_buf[BUFSIZ];
static char stderr_buf[BUFSIZ];

FILE __stdin_file  = { .fd = 0, .bufmode = _IONBF };
FILE __stdout_file = { .fd = 1, .bufmode = _IOLBF,
                       .wbuf = stdout_buf, .wbuf_cap = BUFSIZ, .wbuf_owned = 0 };
FILE __stderr_file = { .fd = 2, .bufmode = _IONBF,
                       .wbuf = stderr_buf, .wbuf_cap = BUFSIZ, .wbuf_owned = 0 };

/* -------------------------------------------------------------------------
 * Internal: flush write buffer to fd
 * ------------------------------------------------------------------------- */

static int _flush(FILE *f) {
  if (!f || f->memonly || !f->wbuf || f->wbuf_len == 0) return 0;
  size_t total = f->wbuf_len;
  size_t sent = 0;
  while (sent < total) {
    ssize_t n = write(f->fd, f->wbuf + sent, total - sent);
    if (n < 0) { f->err = 1; return EOF; }
    sent += (size_t)n;
  }
  f->wbuf_len = 0;
  return 0;
}

/* Internal: buffer a byte, flushing if needed */
static int _wbuf_putc(FILE *f, unsigned char c) {
  if (f->bufmode == _IONBF) {
    if (write(f->fd, &c, 1) != 1) { f->err = 1; return EOF; }
    return c;
  }
  /* ensure buffer exists */
  if (!f->wbuf) {
    f->wbuf = malloc(BUFSIZ);
    if (!f->wbuf) { f->err = 1; return EOF; }
    f->wbuf_cap = BUFSIZ;
    f->wbuf_owned = 1;
  }
  f->wbuf[f->wbuf_len++] = (char)c;
  /* flush if full, or on newline for line-buffered */
  if (f->wbuf_len >= f->wbuf_cap ||
      (f->bufmode == _IOLBF && c == '\n')) {
    if (_flush(f) == EOF) return EOF;
  }
  return c;
}

/* Internal: buffer a block */
static ssize_t _wbuf_write(FILE *f, const void *buf, size_t len) {
  if (f->bufmode == _IONBF) {
    ssize_t n = write(f->fd, buf, len);
    if (n < 0) { f->err = 1; return -1; }
    return n;
  }
  /* ensure buffer exists */
  if (!f->wbuf) {
    f->wbuf = malloc(BUFSIZ);
    if (!f->wbuf) { f->err = 1; return -1; }
    f->wbuf_cap = BUFSIZ;
    f->wbuf_owned = 1;
  }
  const char *p = (const char *)buf;
  size_t remaining = len;
  while (remaining > 0) {
    size_t space = f->wbuf_cap - f->wbuf_len;
    size_t chunk = remaining < space ? remaining : space;
    /* for line-buffered, find last newline and split there */
    if (f->bufmode == _IOLBF) {
      size_t nl = chunk;
      for (size_t i = 0; i < chunk; i++) {
        if (p[i] == '\n') { nl = i + 1; break; }
      }
      chunk = nl;
    }
    memcpy(f->wbuf + f->wbuf_len, p, chunk);
    f->wbuf_len += chunk;
    p += chunk;
    remaining -= chunk;
    if (f->wbuf_len >= f->wbuf_cap ||
        (f->bufmode == _IOLBF && f->wbuf_len > 0 &&
         f->wbuf[f->wbuf_len-1] == '\n')) {
      if (_flush(f) == EOF) return (ssize_t)(len - remaining) > 0
                                   ? (ssize_t)(len - remaining) : -1;
    }
  }
  return (ssize_t)len;
}

/* -------------------------------------------------------------------------
 * FILE allocation
 * ------------------------------------------------------------------------- */

static FILE *alloc_file(int fd) {
  FILE *f = malloc(sizeof(FILE));
  if (!f) return NULL;
  f->fd        = fd;
  f->eof       = 0;
  f->err       = 0;
  f->membuf    = NULL;
  f->memsize   = 0;
  f->mempos    = 0;
  f->memonly   = 0;
  f->wbuf      = NULL;
  f->wbuf_cap  = 0;
  f->wbuf_len  = 0;
  f->wbuf_owned = 1;
  f->bufmode   = _IOFBF;
  f->ms_ptr     = NULL;
  f->ms_sizeloc = NULL;
  return f;
}

/* Sync open_memstream buffer to caller's *ptr / *sizeloc. */
static void _memstream_sync(FILE *f) {
  if (!f->ms_ptr || !f->ms_sizeloc) return;
  /* mempos is the number of bytes written so far */
  size_t len = f->mempos;
  /* grow if needed (+1 for NUL) */
  if (f->membuf == NULL || f->memsize < len + 1) {
    size_t newcap = len + 1 < 64 ? 64 : len + 2;
    char *nb = realloc(f->membuf, newcap);
    if (!nb) { f->err = 1; return; }
    f->membuf  = nb;
    f->memsize = newcap;
  }
  f->membuf[len] = '\0';
  *f->ms_ptr     = f->membuf;
  *f->ms_sizeloc = len;
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
  _flush(stream);
  if (!stream->memonly && stream->fd >= 0)
    close(stream->fd);
  stream->fd     = -1;
  stream->eof    = 0;
  stream->err    = 0;
  stream->memonly = 0;
  stream->wbuf_len = 0;

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
  if (fd < 0) return NULL;
  unlink(path);
  FILE *f = alloc_file(fd);
  if (!f) { close(fd); return NULL; }
  return f;
}

FILE *fmemopen(void *buf, size_t size, const char *mode) {
  (void)mode;
  FILE *f = alloc_file(-1);
  if (!f) return NULL;
  if (buf) {
    f->membuf = (char *)buf;
  } else {
    f->membuf = malloc(size);
    if (!f->membuf) { free(f); return NULL; }
  }
  f->memsize = size;
  f->mempos  = 0;
  f->memonly = 1;
  return f;
}

int fclose(FILE *stream) {
  if (!stream) return EOF;
  _memstream_sync(stream);
  _flush(stream);
  int rc = 0;
  if (!stream->memonly && stream->fd >= 0)
    rc = close(stream->fd);
  if (stream != stdin && stream != stdout && stream != stderr) {
    /* For open_memstream, membuf is handed off to the caller — don't free it. */
    if (stream->memonly && stream->ms_ptr) stream->membuf = NULL;
    if (stream->wbuf_owned && stream->wbuf) free(stream->wbuf);
    free(stream);
  }
  return rc ? EOF : 0;
}

/* -------------------------------------------------------------------------
 * character I/O
 * ------------------------------------------------------------------------- */

int fgetc(FILE *stream) {
  if (!stream || stream->eof) return EOF;
  if (stream->memonly) {
    if (stream->mempos >= stream->memsize) { stream->eof = 1; return EOF; }
    return (unsigned char)stream->membuf[stream->mempos++];
  }
  unsigned char c;
  ssize_t n = read(stream->fd, &c, 1);
  if (n <= 0) { if (n == 0) stream->eof = 1; else stream->err = 1; return EOF; }
  return (unsigned char)c;
}

int fputc(int c, FILE *stream) {
  if (!stream) return EOF;
  unsigned char ch = (unsigned char)c;
  if (stream->memonly) {
    /* open_memstream: grow buffer on demand (+1 reserved for NUL) */
    if (stream->ms_ptr && stream->mempos + 1 >= stream->memsize) {
      size_t newcap = stream->memsize * 2;
      char *nb = realloc(stream->membuf, newcap);
      if (!nb) { stream->err = 1; return EOF; }
      stream->membuf  = nb;
      stream->memsize = newcap;
    } else if (stream->mempos >= stream->memsize) {
      return EOF;
    }
    stream->membuf[stream->mempos++] = (char)ch;
    return ch;
  }
  return _wbuf_putc(stream, ch);
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
  ssize_t n = _wbuf_write(stream, s, len);
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
    /* open_memstream: grow to fit (+1 for NUL) */
    if (stream->ms_ptr) {
      size_t needed = stream->mempos + total + 1;
      if (needed > stream->memsize) {
        size_t newcap = stream->memsize * 2;
        while (newcap < needed) newcap *= 2;
        char *nb = realloc(stream->membuf, newcap);
        if (!nb) { stream->err = 1; return 0; }
        stream->membuf  = nb;
        stream->memsize = newcap;
      }
    }
    size_t avail = stream->memsize - stream->mempos;
    size_t w = total < avail ? total : avail;
    memcpy(stream->membuf + stream->mempos, ptr, w);
    stream->mempos += w;
    return w / size;
  }
  ssize_t n = _wbuf_write(stream, ptr, total);
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
  _flush(stream);
  off_t r = lseek(stream->fd, (off_t)offset, whence);
  if (r < 0) return -1;
  stream->eof = 0;
  return 0;
}

long ftell(FILE *stream) {
  if (!stream) return -1;
  if (stream->memonly) return (long)stream->mempos;
  _flush(stream);
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

int fflush(FILE *stream) {
  if (!stream) {
    /* flush all open streams — we don't track them, just flush the std ones */
    int r = 0;
    if (_flush(stdout) == EOF) r = EOF;
    if (_flush(stderr) == EOF) r = EOF;
    return r;
  }
  _memstream_sync(stream);
  return _flush(stream);
}

/* -------------------------------------------------------------------------
 * locking — single-threaded, all no-ops
 * ------------------------------------------------------------------------- */

void flockfile(FILE *stream)    { (void)stream; }
void funlockfile(FILE *stream)  { (void)stream; }
int  ftrylockfile(FILE *stream) { (void)stream; return 0; }

/* -------------------------------------------------------------------------
 * buffering
 * ------------------------------------------------------------------------- */

int setvbuf(FILE *f, char *buf, int mode, size_t sz) {
  if (!f) return -1;
  _flush(f);
  if (f->wbuf_owned && f->wbuf) { free(f->wbuf); f->wbuf = NULL; }
  f->bufmode = mode;
  f->wbuf_len = 0;
  if (mode == _IONBF) {
    f->wbuf = NULL; f->wbuf_cap = 0; f->wbuf_owned = 0;
  } else if (buf) {
    f->wbuf = buf; f->wbuf_cap = sz; f->wbuf_owned = 0;
  } else {
    f->wbuf = malloc(sz ? sz : BUFSIZ);
    if (!f->wbuf) return -1;
    f->wbuf_cap = sz ? sz : BUFSIZ;
    f->wbuf_owned = 1;
  }
  return 0;
}

void setbuf(FILE *f, char *buf) {
  if (buf) setvbuf(f, buf, _IOFBF, BUFSIZ);
  else     setvbuf(f, NULL, _IONBF, 0);
}

/* -------------------------------------------------------------------------
 * character I/O convenience wrappers
 * ------------------------------------------------------------------------- */

int getchar(void)              { return fgetc(stdin); }
int putchar(int c)             { return fputc(c, stdout); }
int puts(const char *s)        { if (fputs(s, stdout) < 0) return EOF; return fputc('\n', stdout); }
int getc(FILE *f)              { return fgetc(f); }
int putc(int c, FILE *f)       { return fputc(c, f); }
int getc_unlocked(FILE *f)     { return fgetc(f); }
int putc_unlocked(int c, FILE *f) { return fputc(c, f); }
int getchar_unlocked(void)     { return fgetc(stdin); }
int putchar_unlocked(int c)    { return fputc(c, stdout); }

/* -------------------------------------------------------------------------
 * off_t positioning
 * ------------------------------------------------------------------------- */

int   fseeko(FILE *f, off_t offset, int whence) { return fseek(f, (long)offset, whence); }
off_t ftello(FILE *f)                           { return (off_t)ftell(f); }

/* -------------------------------------------------------------------------
 * line reading
 * ------------------------------------------------------------------------- */

ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *f) {
  if (!lineptr || !n || !f) return -1;
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
 * memory stream (write-only, grows dynamically)
 * ------------------------------------------------------------------------- */

FILE *open_memstream(char **ptr, size_t *sizeloc) {
  if (!ptr || !sizeloc) return NULL;
  FILE *f = alloc_file(-1);
  if (!f) return NULL;
  /* Start with a small buffer; it grows on demand in fputc/fwrite. */
  f->membuf  = malloc(64);
  if (!f->membuf) { free(f); return NULL; }
  f->memsize = 64;
  f->mempos  = 0;
  f->memonly = 1;
  f->ms_ptr     = ptr;
  f->ms_sizeloc = sizeloc;
  /* Initialise caller's pointers immediately (POSIX requirement). */
  f->membuf[0] = '\0';
  *ptr     = f->membuf;
  *sizeloc = 0;
  return f;
}

/* -------------------------------------------------------------------------
 * pipe streams — not supported
 * ------------------------------------------------------------------------- */

FILE *popen(const char *cmd, const char *type) { (void)cmd; (void)type; errno = ENOSYS; return NULL; }
int   pclose(FILE *f)                          { (void)f; errno = ENOSYS; return -1; }

/* -------------------------------------------------------------------------
 * error / file ops
 * ------------------------------------------------------------------------- */

void perror(const char *s) {
  if (s && *s) { fputs(s, stderr); fputs(": ", stderr); }
  fputs("error\n", stderr);
}

int remove(const char *path) { return unlink(path); }

static char tmpnam_buf[L_tmpnam];
char *tmpnam(char *s) {
  const char *name = "/tmp/tmp000000";
  char *dst = s ? s : tmpnam_buf;
  int i = 0; while ((dst[i] = name[i])) i++;
  return dst;
}

static char ctermid_buf[L_ctermid];
char *ctermid(char *s) {
  const char *name = "/dev/tty";
  char *dst = s ? s : ctermid_buf;
  int i = 0; while ((dst[i] = name[i])) i++;
  return dst;
}
