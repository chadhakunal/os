#ifndef INTTYPES_H
#define INTTYPES_H

#include <stdint.h>

#define PRId8   "d"
#define PRId16  "d"
#define PRId32  "d"
#define PRId64  "lld"
#define PRIdMAX "lld"
#define PRIdPTR "lld"

#define PRIi8   "i"
#define PRIi16  "i"
#define PRIi32  "i"
#define PRIi64  "lli"
#define PRIiMAX "lli"

#define PRIo8   "o"
#define PRIo16  "o"
#define PRIo32  "o"
#define PRIo64  "llo"
#define PRIoMAX "llo"

#define PRIu8   "u"
#define PRIu16  "u"
#define PRIu32  "u"
#define PRIu64  "llu"
#define PRIuMAX "llu"
#define PRIuPTR "llu"

#define PRIx8   "x"
#define PRIx16  "x"
#define PRIx32  "x"
#define PRIx64  "llx"
#define PRIxMAX "llx"

#define SCNd8   "hhd"
#define SCNd16  "hd"
#define SCNd32  "d"
#define SCNd64  "lld"
#define SCNdMAX "lld"

#define SCNu8   "hhu"
#define SCNu16  "hu"
#define SCNu32  "u"
#define SCNu64  "llu"
#define SCNuMAX "llu"

typedef struct {
  intmax_t quot;
  intmax_t rem;
} imaxdiv_t;

intmax_t imaxabs(intmax_t j);
imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom);

intmax_t strtoimax(const char *restrict nptr, char **restrict endptr, int base);
uintmax_t strtoumax(const char *restrict nptr, char **restrict endptr, int base);

#include <wchar.h>

intmax_t wcstoimax(const wchar_t *restrict nptr, wchar_t **restrict endptr, int base);
uintmax_t wcstoumax(const wchar_t *restrict nptr, wchar_t **restrict endptr, int base);

#endif
