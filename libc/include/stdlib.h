#pragma once

#include <stddef.h>

void exit(int);
void _Exit(int);
int  atexit(void (*func)(void));
int  atoi(const char *str);
long long strtoll(const char *restrict nptr, char **restrict endptr, int base);
unsigned long long strtoull(const char *restrict nptr, char **restrict endptr, int base);

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));

char *getenv(const char *name);
int   mkstemp(char *tmpl);
