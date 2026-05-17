/*
 * Minimal wide-character classification for TRE (regcomp/regexec).
 * ASCII semantics for code points U+0000..U+007F; sufficient for os-tests.
 */

#include <wctype.h>
#include <ctype.h>
#include <string.h>

enum {
  WCT_ALNUM = 1,
  WCT_ALPHA,
  WCT_BLANK,
  WCT_CNTRL,
  WCT_DIGIT,
  WCT_GRAPH,
  WCT_LOWER,
  WCT_PRINT,
  WCT_PUNCT,
  WCT_SPACE,
  WCT_UPPER,
  WCT_XDIGIT,
};

static int wct_ascii(wint_t wc) {
  return wc >= 0 && wc <= 0x7f;
}

int iswalnum(wint_t wc) {
  return wct_ascii(wc) && isalnum((int)wc);
}

int iswalpha(wint_t wc) {
  return wct_ascii(wc) && isalpha((int)wc);
}

int iswblank(wint_t wc) {
  return wc == L' ' || wc == L'\t';
}

int iswcntrl(wint_t wc) {
  return wct_ascii(wc) && iscntrl((int)wc);
}

int iswdigit(wint_t wc) {
  return wct_ascii(wc) && isdigit((int)wc);
}

int iswgraph(wint_t wc) {
  return wct_ascii(wc) && isgraph((int)wc);
}

int iswlower(wint_t wc) {
  return wct_ascii(wc) && islower((int)wc);
}

int iswprint(wint_t wc) {
  return wct_ascii(wc) && isprint((int)wc);
}

int iswpunct(wint_t wc) {
  return wct_ascii(wc) && ispunct((int)wc);
}

int iswspace(wint_t wc) {
  return wct_ascii(wc) && isspace((int)wc);
}

int iswupper(wint_t wc) {
  return wct_ascii(wc) && isupper((int)wc);
}

int iswxdigit(wint_t wc) {
  return wct_ascii(wc) && isxdigit((int)wc);
}

wint_t towlower(wint_t wc) {
  return wct_ascii(wc) ? (wint_t)tolower((int)wc) : wc;
}

wint_t towupper(wint_t wc) {
  return wct_ascii(wc) ? (wint_t)toupper((int)wc) : wc;
}

wctype_t wctype(const char *name) {
  if (!name)
    return 0;
  if (!strcmp(name, "alnum"))
    return WCT_ALNUM;
  if (!strcmp(name, "alpha"))
    return WCT_ALPHA;
  if (!strcmp(name, "blank"))
    return WCT_BLANK;
  if (!strcmp(name, "cntrl"))
    return WCT_CNTRL;
  if (!strcmp(name, "digit"))
    return WCT_DIGIT;
  if (!strcmp(name, "graph"))
    return WCT_GRAPH;
  if (!strcmp(name, "lower"))
    return WCT_LOWER;
  if (!strcmp(name, "print"))
    return WCT_PRINT;
  if (!strcmp(name, "punct"))
    return WCT_PUNCT;
  if (!strcmp(name, "space"))
    return WCT_SPACE;
  if (!strcmp(name, "upper"))
    return WCT_UPPER;
  if (!strcmp(name, "xdigit"))
    return WCT_XDIGIT;
  return 0;
}

int iswctype(wint_t wc, wctype_t desc) {
  switch (desc) {
    case WCT_ALNUM: return iswalnum(wc);
    case WCT_ALPHA: return iswalpha(wc);
    case WCT_BLANK: return iswblank(wc);
    case WCT_CNTRL: return iswcntrl(wc);
    case WCT_DIGIT: return iswdigit(wc);
    case WCT_GRAPH: return iswgraph(wc);
    case WCT_LOWER: return iswlower(wc);
    case WCT_PRINT: return iswprint(wc);
    case WCT_PUNCT: return iswpunct(wc);
    case WCT_SPACE: return iswspace(wc);
    case WCT_UPPER: return iswupper(wc);
    case WCT_XDIGIT: return iswxdigit(wc);
    default: return 0;
  }
}
