#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

typedef __int128 int128_t;

static double bits_to_double(uint64_t bits) {
  union {
    uint64_t u;
    double d;
  } x;
  x.u = bits;
  return x.d;
}

static int bitlen64(uint64_t x) {
  int n = 0;

  while (x) {
    x >>= 1;
    n++;
  }
  return n;
}

static uint64_t dec_to_bits(uint64_t mant, int exp10, int sign) {
  uint64_t n = mant;
  uint64_t d = 1;
  uint64_t sig;
  int bn;
  int bd;
  int shift;
  int exp;

  if (!mant)
    return (uint64_t)sign << 63;

  if (exp10 > 0) {
    while (exp10-- > 0)
      n *= 10;
  } else if (exp10 < 0) {
    while (exp10++ < 0)
      d *= 10;
  }

  bn = bitlen64(n);
  bd = bitlen64(d);
  shift = 52 + bd - bn;

  if (shift >= 64)
    return (uint64_t)sign << 63;
  if (shift > 0)
    n <<= shift;
  else if (shift < 0)
    n >>= -shift;

  sig = n / d;
  if ((n % d) * 2 >= d)
    sig++;

  exp = 1023 + bn - bd;

  while (sig >= (1ULL << 53)) {
    sig >>= 1;
    exp--;
  }
  while (sig < (1ULL << 52)) {
    sig <<= 1;
    exp++;
  }

  return ((uint64_t)sign << 63) | ((uint64_t)(exp & 0x7ff) << 52) |
         (sig - (1ULL << 52));
}

static double dec_to_double(int128_t mant, int exp10, int sign) {
  if (mant == 0)
    return bits_to_double(sign ? (1ULL << 63) : 0);
  if ((int128_t)(uint64_t)mant != mant)
    return bits_to_double((uint64_t)sign << 63);
  return bits_to_double(dec_to_bits((uint64_t)mant, exp10, sign));
}

static const char *parse_dec(const char *s, int128_t *mant, int *exp10) {
  int128_t a = 0;
  int dp = 0;
  int got = 0;

  while (isdigit((unsigned char)*s)) {
    got = 1;
    a = a * 10 + (*s - '0');
    s++;
  }

  if (*s == '.') {
    s++;
    while (isdigit((unsigned char)*s)) {
      got = 1;
      if (a < ((int128_t)1 << 120))
        a = a * 10 + (*s - '0');
      dp++;
      s++;
    }
  }

  if (*s == 'e' || *s == 'E') {
    int esign = 1;
    int e = 0;
    s++;
    if (*s == '+')
      s++;
    else if (*s == '-') {
      esign = -1;
      s++;
    }
    if (!isdigit((unsigned char)*s))
      return s;
    while (isdigit((unsigned char)*s)) {
      e = e * 10 + (*s - '0');
      s++;
    }
    dp -= esign * e;
  }

  if (!got) {
    errno = EINVAL;
    return s;
  }

  *mant = a;
  *exp10 = -dp;
  return s;
}

double strtod(const char *restrict nptr, char **restrict endptr) {
  const char *s = nptr;
  const char *start;
  int sign = 0;
  int128_t mant = 0;
  int exp10 = 0;

  if (!nptr) {
    errno = EINVAL;
    return 0.0;
  }

  while (isspace((unsigned char)*s))
    s++;

  start = s;
  if (*s == '-') {
    sign = 1;
    s++;
  } else if (*s == '+') {
    s++;
  }

  if (!isdigit((unsigned char)*s) && *s != '.') {
    if (endptr)
      *endptr = (char *)nptr;
    return 0.0;
  }

  s = parse_dec(s, &mant, &exp10);
  if (endptr)
    *endptr = (char *)(s > start ? s : nptr);

  return dec_to_double(mant, exp10, sign);
}

float strtof(const char *restrict nptr, char **restrict endptr) {
  return (float)strtod(nptr, endptr);
}

long double strtold(const char *restrict nptr, char **restrict endptr) {
  return (long double)strtod(nptr, endptr);
}

double atof(const char *s) {
  return strtod(s, NULL);
}
