#include <ctype.h>
#include <locale.h>

static int is_uc(unsigned char c) {
  return c >= 'A' && c <= 'Z';
}

static int is_lc(unsigned char c) {
  return c >= 'a' && c <= 'z';
}

static int is_dig(unsigned char c) {
  return c >= '0' && c <= '9';
}

static int is_xdig(unsigned char c) {
  return is_dig(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static unsigned char to_uc(unsigned char c) {
  if (is_lc(c))
    return (unsigned char)(c - ('a' - 'A'));
  return c;
}

static unsigned char to_lc(unsigned char c) {
  if (is_uc(c))
    return (unsigned char)(c + ('a' - 'A'));
  return c;
}

int isalpha(int c) {
  if ((unsigned)c > 127)
    return 0;
  return is_uc((unsigned char)c) || is_lc((unsigned char)c);
}

int isdigit(int c) {
  if ((unsigned)c > 127)
    return 0;
  return is_dig((unsigned char)c);
}

int isalnum(int c) {
  return isalpha(c) || isdigit(c);
}

int isblank(int c) {
  return c == ' ' || c == '\t';
}

int iscntrl(int c) {
  if (c < 0)
    return 0;
  if ((unsigned)c > 127)
    return 0;
  return (unsigned char)c < 0x20 || (unsigned char)c == 0x7f;
}

int isgraph(int c) {
  if ((unsigned)c > 127)
    return 0;
  unsigned char u = (unsigned char)c;
  return u >= 0x21 && u <= 0x7e;
}

int islower(int c) {
  if ((unsigned)c > 127)
    return 0;
  return is_lc((unsigned char)c);
}

int isupper(int c) {
  if ((unsigned)c > 127)
    return 0;
  return is_uc((unsigned char)c);
}

int isprint(int c) {
  if ((unsigned)c > 127)
    return 0;
  unsigned char u = (unsigned char)c;
  return u >= 0x20 && u <= 0x7e;
}

int ispunct(int c) {
  return isgraph(c) && !isalnum(c);
}

int isspace(int c) {
  switch (c) {
    case ' ':
    case '\f':
    case '\n':
    case '\r':
    case '\t':
    case '\v':
      return 1;
    default:
      return 0;
  }
}

int isxdigit(int c) {
  if ((unsigned)c > 127)
    return 0;
  return is_xdig((unsigned char)c);
}

int tolower(int c) {
  if ((unsigned)c > 127)
    return c;
  return (int)to_lc((unsigned char)c);
}

int toupper(int c) {
  if ((unsigned)c > 127)
    return c;
  return (int)to_uc((unsigned char)c);
}

#define CTYPE_L(name, call) \
  int name##_l(int c, locale_t loc) { \
    (void)loc; \
    return call(c); \
  }

CTYPE_L(isalnum, isalnum)
CTYPE_L(isalpha, isalpha)
CTYPE_L(isblank, isblank)
CTYPE_L(iscntrl, iscntrl)
CTYPE_L(isdigit, isdigit)
CTYPE_L(isgraph, isgraph)
CTYPE_L(islower, islower)
CTYPE_L(isprint, isprint)
CTYPE_L(ispunct, ispunct)
CTYPE_L(isspace, isspace)
CTYPE_L(isupper, isupper)
CTYPE_L(isxdigit, isxdigit)
CTYPE_L(tolower, tolower)
CTYPE_L(toupper, toupper)
