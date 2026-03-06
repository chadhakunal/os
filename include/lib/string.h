#ifndef STRING_H
#define STRING_H

#include "types.h"

void memcpy(void* dst, const void* src, int len);
void *memset(void *dest, int value, uint64_t n);
void strncpy(char *dst, const char *src, int n);
int strncmp(const char* s1, const char* s2);
int strneq_prefix(const char *s1, const char *s2, int n);

#endif
