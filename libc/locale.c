#include <locale.h>
#include <string.h>

static struct __locale default_locale;

locale_t duplocale(locale_t loc) {
  (void)loc;
  return &default_locale;
}

locale_t newlocale(int category_mask, const char *locale, locale_t base) {
  (void)category_mask;
  (void)locale;
  (void)base;
  return &default_locale;
}

void freelocale(locale_t loc) {
  (void)loc;
}

int strcoll_l(const char *s1, const char *s2, locale_t loc) {
  (void)loc;
  return strcoll(s1, s2);
}

size_t strxfrm_l(char *restrict dst, const char *restrict src, size_t n,
                 locale_t loc) {
  (void)loc;
  return strxfrm(dst, src, n);
}

char *strerror_l(int errnum, locale_t loc) {
  (void)loc;
  return strerror(errnum);
}
