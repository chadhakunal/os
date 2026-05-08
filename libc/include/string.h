#pragma once

#include <types.h>

void *memcpy(void *dst, const void *src, size_t len);
void *memset(void *dest, int value, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
