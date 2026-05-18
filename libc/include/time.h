#pragma once

#include <stddef.h>
#include <stdint.h>
#include <types.h>
#include <locale.h>

typedef long clock_t;
typedef long time_t;
typedef int clockid_t;
typedef void *timer_t;

#define CLOCKS_PER_SEC  1000000L

#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3

#define TIMER_ABSTIME 1
#define TIME_UTC      1

struct timespec {
  time_t tv_sec;
  long   tv_nsec;
};

struct itimerspec {
  struct timespec it_interval;
  struct timespec it_value;
};

struct tm {
  int tm_sec;
  int tm_mon;
  int tm_mday;
  int tm_hour;
  int tm_min;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
};

struct sigevent;

/* nanosleep */
int nanosleep(const struct timespec *req, struct timespec *rem);

/* POSIX clocks */
int clock_gettime(clockid_t clk, struct timespec *tp);
int clock_settime(clockid_t clk, const struct timespec *tp);
int clock_getres(clockid_t clk, struct timespec *res);
int clock_nanosleep(clockid_t clk, int flags, const struct timespec *req,
                    struct timespec *rem);
int clock_getcpuclockid(int pid, clockid_t *clk);

/* POSIX timers */
int timer_create(clockid_t clk, struct sigevent *evp, timer_t *timerid);
int timer_delete(timer_t timerid);
int timer_gettime(timer_t timerid, struct itimerspec *val);
int timer_settime(timer_t timerid, int flags, const struct itimerspec *val,
                  struct itimerspec *old);
int timer_getoverrun(timer_t timerid);

/* C11 timespec_get */
int timespec_get(struct timespec *ts, int base);

/* C/POSIX time functions */
clock_t  clock(void);
time_t   time(time_t *t);
double   difftime(time_t t1, time_t t0);
time_t   mktime(struct tm *tm);
struct tm *gmtime(const time_t *t);
struct tm *gmtime_r(const time_t *t, struct tm *result);
struct tm *localtime(const time_t *t);
struct tm *localtime_r(const time_t *t, struct tm *result);
char     *asctime(const struct tm *tm);
char     *asctime_r(const struct tm *tm, char *buf);
char     *ctime(const time_t *t);
char     *ctime_r(const time_t *t, char *buf);
size_t    strftime(char *restrict s, size_t max, const char *restrict fmt,
                   const struct tm *restrict tm);
size_t    strftime_l(char *restrict s, size_t max, const char *restrict fmt,
                     const struct tm *restrict tm, locale_t loc);
char     *strptime(const char *s, const char *fmt, struct tm *tm);
void      tzset(void);

extern char *tzname[2];
extern long  timezone;
extern int   daylight;
