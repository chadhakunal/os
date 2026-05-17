#ifndef ASSERT_H
#define ASSERT_H

#undef assert
#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) ((void)((expr) ? 0 : (__assert_fail(#expr, __FILE__, __LINE__), 0)))
void __assert_fail(const char *expr, const char *file, int line);
#endif

#endif
