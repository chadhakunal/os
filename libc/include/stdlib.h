#pragma once

#include <stddef.h>
#include <wchar.h>

#ifndef MB_CUR_MAX
#define MB_CUR_MAX 4
#endif

typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;

void exit(int);
void _Exit(int);
void quick_exit(int status);
void abort(void);
int  atexit(void (*func)(void));
int  at_quick_exit(void (*func)(void));
int  system(const char *command);
int  atoi(const char *str);

long strtol(const char *restrict nptr, char **restrict endptr, int base);
unsigned long strtoul(const char *restrict nptr, char **restrict endptr, int base);
long long strtoll(const char *restrict nptr, char **restrict endptr, int base);
unsigned long long strtoull(const char *restrict nptr, char **restrict endptr, int base);
long atol(const char *s);
long long atoll(const char *s);

double strtod(const char *restrict nptr, char **restrict endptr);
float strtof(const char *restrict nptr, char **restrict endptr);
long double strtold(const char *restrict nptr, char **restrict endptr);
double atof(const char *s);

long   a64l(const char *s);
char  *l64a(long v);
int    getsubopt(char **optionp, char *const *keys, char **valuep);
void   setkey(const char *key);

int    mblen(const char *s, size_t n);
int    wctomb(char *s, wchar_t wc);
size_t mbstowcs(wchar_t *restrict dst, const char *restrict src, size_t len);
size_t wcstombs(char *restrict dst, const wchar_t *restrict src, size_t len);

int abs(int n);
long labs(long n);
long long llabs(long long n);
div_t div(int num, int den);
ldiv_t ldiv(long num, long den);
lldiv_t lldiv(long long num, long long den);

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void *reallocarray(void *ptr, size_t nmemb, size_t size);
void  free(void *ptr);

void *aligned_alloc(size_t alignment, size_t size);
int   posix_memalign(void **memptr, size_t alignment, size_t size);

#define RAND_MAX 2147483647

int   rand(void);
void  srand(unsigned seed);

double drand48(void);
double erand48(unsigned short xsubi[3]);
long   jrand48(unsigned short xsubi[3]);
long   lrand48(void);
long   mrand48(void);
long   nrand48(unsigned short xsubi[3]);
unsigned short *seed48(unsigned short xseed[3]);
void   srand48(long seedval);
void   lcong48(unsigned short param[7]);

long   random(void);
void   srandom(unsigned seed);
char  *initstate(unsigned seed, char *state, size_t n);
char  *setstate(char *state);

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));
void qsort_r(void *base, size_t nmemb, size_t size,
             int (*compar)(const void *, const void *, void *), void *arg);

char *getenv(const char *name);
char *secure_getenv(const char *name);
int   setenv(const char *name, const char *value, int overwrite);
int   unsetenv(const char *name);
int   putenv(char *string);

char *realpath(const char *path, char *resolved_path);
char *mkdtemp(char *tmpl);
int   mkstemp(char *tmpl);
int   mkostemp(char *tmpl, int flags);
