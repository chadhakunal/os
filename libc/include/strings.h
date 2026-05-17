#pragma once

#include <stddef.h>

int     ffs(int i);
int     ffsl(long i);
int     ffsll(long long i);
int     bcmp(const void *s1, const void *s2, size_t n);
void    bcopy(const void *src, void *dest, size_t n);
void    bzero(void *s, size_t n);
int     strcasecmp(const char *s1, const char *s2);
int     strncasecmp(const char *s1, const char *s2, size_t n);
void    explicit_bzero(void *s, size_t n);

#define index  strchr
#define rindex strrchr
