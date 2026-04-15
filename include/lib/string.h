#ifndef STRING_H
#define STRING_H

#include "types.h"

void memcpy(void* dst, const void* src, int len);
void *memset(void *dest, int value, uint64_t n);
void strncpy(char *dst, const char *src, int n);
int strncmp(const char* s1, const char* s2);
int strneq_prefix(const char *s1, const char *s2, int n);

/* Parse an octal string to uint64_t (for tar format) */
uint64_t parse_octal(const char *str, uint64_t max_len);

/* Extract token before delimiter, advance src pointer past delimiter */
int str_tok(const char **src, char *dst, char delim, int max_len);

/* Extract token before delimiter (without delimiter), advance src pointer past delimiter */
int str_tok_no_delim(const char **src, char *dst, char delim, int max_len);

int str_len(const char *src, uint64_t max_len);

#endif
