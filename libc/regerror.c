/*
 * POSIX regerror() for TRE-based regex (messages aligned with common libc text).
 */

#include <regex.h>
#include <string.h>

static const char *reg_errmsg(int err) {
  switch (err) {
    case REG_NOMATCH: return "regexec failed to match";
    case REG_BADPAT: return "invalid regular expression";
    case REG_ECOLLATE: return "invalid collating element";
    case REG_ECTYPE: return "invalid character class";
    case REG_EESCAPE: return "trailing backslash";
    case REG_ESUBREG: return "invalid backreference";
    case REG_EBRACK: return "unmatched bracket";
    case REG_EPAREN: return "unmatched parenthesis";
    case REG_EBRACE: return "unmatched brace";
    case REG_BADBR: return "invalid brace expression";
    case REG_ERANGE: return "invalid character range";
    case REG_ESPACE: return "out of memory";
    case REG_BADRPT: return "invalid repetition";
    default: return "unknown regex error";
  }
}

size_t regerror(int errcode, const regex_t *restrict preg, char *restrict errbuf,
                size_t errbuf_size) {
  (void)preg;
  const char *msg = reg_errmsg(errcode);
  size_t n = strlen(msg) + 1;
  if (errbuf && errbuf_size > 0) {
    size_t cpy = n < errbuf_size ? n : errbuf_size;
    memcpy(errbuf, msg, cpy - 1);
    errbuf[cpy - 1] = '\0';
  }
  return n;
}
