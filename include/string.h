#ifndef STRING_H
#define STRING_H

#include "types.h"

void memcpy_local(void* dst, const void* src, int len) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (int i = 0; i < len; i++) {
        d[i] = s[i];
    }
}

void strncpy_local(char* dst, const char* src, int n) {
    int i = 0;
    while (i < n && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    if (i < n) {
        dst[i] = '\0';
    }
}

int strcmp_local(const char* s1, const char* s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strneq_prefix(const char *s1, const char *s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i])
            return 0;
    }
    return 1;
}


#endif
