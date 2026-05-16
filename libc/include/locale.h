#pragma once

#include <stddef.h>

typedef struct __locale {
  int dummy;
} *__locale_t;
typedef __locale_t locale_t;

#define LC_ALL_MASK     0x7fffffff
#define LC_COLLATE_MASK 1
#define LC_CTYPE_MASK   2
#define LC_MESSAGES_MASK 4
#define LC_MONETARY_MASK 8
#define LC_NUMERIC_MASK  16
#define LC_TIME_MASK     32

locale_t newlocale(int category_mask, const char *locale, locale_t base);
void freelocale(locale_t loc);

int strcoll_l(const char *s1, const char *s2, locale_t loc);
size_t strxfrm_l(char *restrict dst, const char *restrict src, size_t n,
                 locale_t loc);
char *strerror_l(int errnum, locale_t loc);
