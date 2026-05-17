#include <stdlib.h>

int abs(int n)
{
  if (n < 0)
    return -n;
  return n;
}

long labs(long n)
{
  if (n < 0)
    return -n;
  return n;
}

long long llabs(long long n)
{
  if (n < 0)
    return -n;
  return n;
}

div_t div(int num, int den)
{
  div_t r;
  r.quot = num / den;
  r.rem = num % den;
  return r;
}

ldiv_t ldiv(long num, long den)
{
  ldiv_t r;
  r.quot = num / den;
  r.rem = num % den;
  return r;
}

lldiv_t lldiv(long long num, long long den)
{
  lldiv_t r;
  r.quot = num / den;
  r.rem = num % den;
  return r;
}
