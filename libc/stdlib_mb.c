#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <errno.h>

int mblen(const char *s, size_t n) {
  if (!s)
    return 0;
  return mbtowc(NULL, s, n);
}

int wctomb(char *s, wchar_t wc) {
  if (!s)
    return 0;
  if ((unsigned)wc >= 0x80) {
    errno = EILSEQ;
    return -1;
  }
  s[0] = (char)wc;
  return 1;
}

size_t mbstowcs(wchar_t *restrict dst, const char *restrict src, size_t len) {
  size_t i = 0;

  if (!src)
    return 0;

  while (src[i]) {
    wchar_t wc;
    int r;

    if (dst && i >= len)
      break;
    r = mbtowc(dst ? &wc : NULL, src + i, MB_LEN_MAX);
    if (r <= 0)
      return (size_t)-1;
    if (dst)
      dst[i] = wc;
    i++;
  }
  if (dst && i < len)
    dst[i] = L'\0';
  return i;
}

size_t wcstombs(char *restrict dst, const wchar_t *restrict src, size_t len) {
  size_t i = 0;
  size_t out = 0;

  if (!src)
    return 0;

  while (src[i]) {
    char buf[MB_LEN_MAX];
    int r;

    r = wctomb(buf, src[i]);
    if (r < 0)
      return (size_t)-1;
    if (dst) {
      if (out + (size_t)r > len)
        break;
      memcpy(dst + out, buf, (size_t)r);
    }
    out += (size_t)r;
    i++;
  }
  if (dst && out < len)
    dst[out] = '\0';
  return out;
}
