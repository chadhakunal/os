#pragma once

#include <stddef.h>

void exit(int);
void _Exit(int);
int  atexit(void (*func)(void));
int  atoi(const char *str);

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);

char *getenv(const char *name);
int   mkstemp(char *tmpl);
