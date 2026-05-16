#pragma once

#include <stddef.h>

void   *memcpy(void *restrict dst, const void *restrict src, size_t n);
void   *memmove(void *dst, const void *src, size_t n);
void   *memset(void *s, int c, size_t n);
int     memcmp(const void *s1, const void *s2, size_t n);
void   *memchr(const void *s, int c, size_t n);

char   *strcpy(char *restrict dst, const char *restrict src);
char   *strncpy(char *restrict dst, const char *restrict src, size_t n);
char   *strcat(char *restrict dst, const char *restrict src);
char   *strncat(char *restrict dst, const char *restrict src, size_t n);
size_t  strlcpy(char *restrict dst, const char *restrict src, size_t size);
size_t  strlcat(char *restrict dst, const char *restrict src, size_t size);
int     strcmp(const char *s1, const char *s2);
int     strncmp(const char *s1, const char *s2, size_t n);
size_t  strlen(const char *s);
size_t  strnlen(const char *s, size_t maxlen);
char   *strchr(const char *s, int c);
char   *strrchr(const char *s, int c);
char   *strstr(const char *haystack, const char *needle);
char   *strpbrk(const char *s, const char *accept);
size_t  strspn(const char *s, const char *accept);
size_t  strcspn(const char *s, const char *reject);
char   *strtok(char *restrict s, const char *restrict sep);
char   *strtok_r(char *restrict s, const char *restrict sep, char **restrict ptr);
char   *strdup(const char *s);
char   *strndup(const char *s, size_t n);
int     strcoll(const char *s1, const char *s2);
size_t  strxfrm(char *restrict dst, const char *restrict src, size_t n);
char   *strerror(int errnum);
int     strerror_r(int errnum, char *buf, size_t buflen);
