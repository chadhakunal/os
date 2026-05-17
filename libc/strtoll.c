#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

static int digit_val(int c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'z')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'Z')
    return c - 'A' + 10;
  return -1;
}

static const char *skip_space(const char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\v' || *s == '\f' || *s == '\r')
    s++;
  return s;
}

static int parse_base(const char *s, const char **end, int base) {
  if (base != 0)
    return base;
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    *end = s + 2;
    return 16;
  }
  if (s[0] == '0') {
    *end = s + 1;
    return 8;
  }
  *end = s;
  return 10;
}

long long strtoll(const char *restrict nptr, char **restrict endptr, int base) {
  const char *s = skip_space(nptr);
  int sign = 1;
  int overflow = 0;

  if (*s == '-') {
    sign = -1;
    s++;
  } else if (*s == '+') {
    s++;
  }

  const char *basep = s;
  base = parse_base(s, &basep, base);
  if (basep != s)
    s = basep;

  if (base < 2 || base > 36) {
    errno = EINVAL;
    if (endptr)
      *endptr = (char *)nptr;
    return 0;
  }

  unsigned long long acc = 0;
  int any = 0;

  while (1) {
    int d = digit_val((unsigned char)*s);
    if (d < 0 || d >= base)
      break;
    any = 1;
    if (acc > (ULLONG_MAX - (unsigned long long)d) / (unsigned long long)base)
      overflow = 1;
    acc = acc * (unsigned long long)base + (unsigned long long)d;
    s++;
  }

  if (endptr)
    *endptr = (char *)(any ? s : nptr);

  if (!any) {
    errno = EINVAL;
    return 0;
  }

  if (overflow) {
    errno = ERANGE;
    return sign < 0 ? LLONG_MIN : LLONG_MAX;
  }

  long long limit = sign < 0 ? (long long)LLONG_MIN : (long long)LLONG_MAX;
  unsigned long long ulim = sign < 0 ? (unsigned long long)LLONG_MIN
                                     : (unsigned long long)LLONG_MAX;
  if (acc > ulim) {
    errno = ERANGE;
    return limit;
  }

  errno = 0;
  return sign < 0 ? -(long long)acc : (long long)acc;
}

unsigned long long strtoull(const char *restrict nptr, char **restrict endptr, int base) {
  const char *s = skip_space(nptr);
  int sign = 1;

  if (*s == '-') {
    sign = -1;
    s++;
  } else if (*s == '+') {
    s++;
  }

  const char *basep = s;
  base = parse_base(s, &basep, base);
  if (basep != s)
    s = basep;

  if (base < 2 || base > 36) {
    errno = EINVAL;
    if (endptr)
      *endptr = (char *)nptr;
    return 0;
  }

  unsigned long long acc = 0;
  int any = 0;
  int overflow = 0;

  while (1) {
    int d = digit_val((unsigned char)*s);
    if (d < 0 || d >= base)
      break;
    any = 1;
    if (acc > (ULLONG_MAX - (unsigned long long)d) / (unsigned long long)base)
      overflow = 1;
    acc = acc * (unsigned long long)base + (unsigned long long)d;
    s++;
  }

  if (endptr)
    *endptr = (char *)(any ? s : nptr);

  if (!any) {
    errno = EINVAL;
    return 0;
  }

  if (overflow) {
    errno = ERANGE;
    return ULLONG_MAX;
  }

  if (sign < 0)
    acc = 0ULL - acc;

  errno = 0;
  return acc;
}

long strtol(const char *restrict nptr, char **restrict endptr, int base)
{
  long long v = strtoll(nptr, endptr, base);
  if (errno == ERANGE)
    return v < 0 ? LONG_MIN : LONG_MAX;
  if (v < (long long)LONG_MIN || v > (long long)LONG_MAX) {
    errno = ERANGE;
    return v < 0 ? LONG_MIN : LONG_MAX;
  }
  return (long)v;
}

unsigned long strtoul(const char *restrict nptr, char **restrict endptr, int base)
{
  unsigned long long v = strtoull(nptr, endptr, base);
  if (errno == ERANGE)
    return ULONG_MAX;
  if (v > (unsigned long long)ULONG_MAX) {
    errno = ERANGE;
    return ULONG_MAX;
  }
  return (unsigned long)v;
}
