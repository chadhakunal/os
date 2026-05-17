#include <wchar.h>

wchar_t *wcscpy(wchar_t *restrict dest, const wchar_t *restrict src) {
  wchar_t *d = dest;
  while ((*d++ = *src++) != L'\0')
    ;
  return dest;
}

int wcscmp(const wchar_t *s1, const wchar_t *s2) {
  while (*s1 == *s2 && *s1 != L'\0') {
    s1++;
    s2++;
  }
  return *s1 - *s2;
}

size_t wcslen(const wchar_t *s) {
  size_t n = 0;
  while (s[n] != L'\0')
    n++;
  return n;
}

int mbtowc(wchar_t *restrict pwc, const char *restrict s, size_t n) {
  if (!s)
    return 0;
  if (n == 0)
    return 0;
  if (*s == '\0') {
    if (pwc)
      *pwc = 0;
    return 0;
  }
  unsigned char c = (unsigned char)s[0];
  if (c >= 0x80)
    return -1;
  if (pwc)
    *pwc = (wchar_t)c;
  return 1;
}
