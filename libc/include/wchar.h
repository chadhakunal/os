#ifndef WCHAR_H
#define WCHAR_H

#include <stddef.h>

typedef int wchar_t;
typedef int wint_t;

#ifndef WEOF
#define WEOF ((wint_t)-1)
#endif

#ifndef MB_LEN_MAX
#define MB_LEN_MAX 4
#endif

wchar_t *wcscpy(wchar_t *restrict dest, const wchar_t *restrict src);
int      wcscmp(const wchar_t *s1, const wchar_t *s2);
size_t   wcslen(const wchar_t *s);
int      mbtowc(wchar_t *restrict pwc, const char *restrict s, size_t n);
int      wctomb(char *s, wchar_t wc);
int      mblen(const char *s, size_t n);
size_t   mbstowcs(wchar_t *restrict dst, const char *restrict src, size_t len);
size_t   wcstombs(char *restrict dst, const wchar_t *restrict src, size_t len);

#endif
