#include "lib/string.h"
#include "kernel/drivers/uart.h"

void memcpy(void* dst, const void* src, int len) {
    // TODO: Optimize
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (int i = 0; i < len; i++) {
        d[i] = s[i];
    }
}

void *memset(void *dest, int value, uint64_t n) {
    // TODO: Optimize
    unsigned char *d = dest;

    for (uint64_t i = 0; i < n; i++) {
        d[i] = (unsigned char)value;
    }

    return dest;
}

void strncpy(char *dst, const char *src, int n) {
    int i;

    for (i = 0; i < n - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }

    dst[i] = '\0';
}

int strncmp(const char* s1, const char* s2) {
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

uint64_t parse_octal(const char *str, uint64_t max_len) {
    uint64_t value = 0;

    for (uint64_t i = 0; i < max_len && str[i] != '\0' && str[i] != ' '; i++) {
        if (str[i] == ' ') continue;
        if (str[i] < '0' || str[i] > '7') break;
        value = (value * 8) + (str[i] - '0');
    }

    return value;
}

int str_tok(const char **src, char *dst, char delim, int max_len) {
    const char *s = *src;
    int i = 0;

    while (i < max_len - 2 && s[i] != '\0' && s[i] != delim) {
        dst[i] = s[i];
        i++;
    }

    if (s[i] == delim) {
        dst[i] = delim;
        i++;
        *src = &s[i];
    } else {
        *src = &s[i];
    }

    dst[i] = '\0';

    return i;
}

