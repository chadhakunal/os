#include <stdlib.h>

long atol(const char *s)
{
  return strtol(s, (char **)0, 10);
}

long long atoll(const char *s)
{
  return strtoll(s, (char **)0, 10);
}
