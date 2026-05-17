#include <stdlib.h>
#include <errno.h>
#include <string.h>

static const char b64_digits[] =
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

long a64l(const char *s) {
  long v = 0;
  long mult = 1;
  int i;

  if (!s)
    return 0;

  for (i = 0; i < 6 && s[i]; i++) {
    const char *p = strchr(b64_digits, s[i]);
    if (!p)
      break;
    v += (long)(p - b64_digits) * mult;
    mult *= 64;
  }
  return v;
}

char *l64a(long v) {
  static char buf[8];
  char *s = buf;

  if (v < 0)
    v = -v;

  if (v == 0) {
    buf[0] = b64_digits[0];
    buf[1] = '\0';
    return buf;
  }

  while (v > 0) {
    *s++ = b64_digits[v & 0x3f];
    v >>= 6;
  }
  *s = '\0';
  return buf;
}

int getsubopt(char **optionp, char *const *keys, char **valuep) {
  char *s = *optionp;
  char *comma;
  char *eq;
  int i;

  if (!s || !*s)
    return -1;

  comma = strchr(s, ',');
  if (comma) {
    *comma = '\0';
    *optionp = comma + 1;
  } else {
    *optionp = s + strlen(s);
  }

  eq = strchr(s, '=');
  if (valuep) {
    if (eq) {
      *eq = '\0';
      *valuep = eq + 1;
    } else {
      *valuep = NULL;
    }
  }

  for (i = 0; keys[i]; i++) {
    if (strcmp(s, keys[i]) == 0)
      return i;
  }

  return -1;
}

void setkey(const char *key) {
  (void)key;
}
