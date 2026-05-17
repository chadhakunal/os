#include <inttypes.h>
#include <stdlib.h>
#include <wchar.h>

intmax_t imaxabs(intmax_t j) {
  return j < 0 ? -j : j;
}

imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom) {
  imaxdiv_t r;
  r.quot = numer / denom;
  r.rem = numer % denom;
  return r;
}

intmax_t strtoimax(const char *restrict nptr, char **restrict endptr, int base) {
  return (intmax_t)strtoll(nptr, endptr, base);
}

uintmax_t strtoumax(const char *restrict nptr, char **restrict endptr, int base) {
  return (uintmax_t)strtoull(nptr, endptr, base);
}

static char *narrow_wcs(const wchar_t *ws, char *buf, size_t bufsz) {
  size_t i = 0;
  while (ws[i] != L'\0' && i + 1 < bufsz) {
    buf[i] = (char)ws[i];
    i++;
  }
  buf[i] = '\0';
  return buf;
}

intmax_t wcstoimax(const wchar_t *restrict nptr, wchar_t **restrict endptr, int base) {
  char narrow[128];
  char *nend = NULL;
  narrow_wcs(nptr, narrow, sizeof(narrow));
  intmax_t v = strtoimax(narrow, &nend, base);
  if (endptr) {
    if (nend == narrow)
      *endptr = (wchar_t *)nptr;
    else
      *endptr = (wchar_t *)(nptr + (nend - narrow));
  }
  return v;
}

uintmax_t wcstoumax(const wchar_t *restrict nptr, wchar_t **restrict endptr, int base) {
  char narrow[128];
  char *nend = NULL;
  narrow_wcs(nptr, narrow, sizeof(narrow));
  uintmax_t v = strtoumax(narrow, &nend, base);
  if (endptr) {
    if (nend == narrow)
      *endptr = (wchar_t *)nptr;
    else
      *endptr = (wchar_t *)(nptr + (nend - narrow));
  }
  return v;
}
