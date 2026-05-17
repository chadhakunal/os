#include <time.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

char *tzname[2] = { "UTC", "UTC" };
long  timezone  = 0;
int   daylight  = 0;

void tzset(void) {}

/* nanosleep is implemented in unistd.c via syscall */

int clock_gettime(clockid_t clk, struct timespec *tp) {
  (void)clk;
  tp->tv_sec  = 0;
  tp->tv_nsec = 0;
  return 0;
}

int clock_settime(clockid_t clk, const struct timespec *tp) {
  (void)clk; (void)tp;
  errno = EPERM;
  return -1;
}

int clock_getres(clockid_t clk, struct timespec *res) {
  (void)clk;
  res->tv_sec  = 0;
  res->tv_nsec = 1;
  return 0;
}

int clock_nanosleep(clockid_t clk, int flags, const struct timespec *req,
                    struct timespec *rem) {
  (void)clk; (void)flags; (void)rem;
  return nanosleep(req, rem);
}

int clock_getcpuclockid(int pid, clockid_t *clk) {
  (void)pid;
  *clk = CLOCK_PROCESS_CPUTIME_ID;
  return 0;
}

int timer_create(clockid_t clk, struct sigevent *evp, timer_t *timerid) {
  (void)clk; (void)evp;
  *timerid = (timer_t)1;
  return 0;
}

int timer_delete(timer_t timerid) {
  (void)timerid;
  return 0;
}

int timer_gettime(timer_t timerid, struct itimerspec *val) {
  (void)timerid;
  val->it_value.tv_sec  = 0;
  val->it_value.tv_nsec = 0;
  val->it_interval      = val->it_value;
  return 0;
}

int timer_settime(timer_t timerid, int flags, const struct itimerspec *val,
                  struct itimerspec *old) {
  (void)timerid; (void)flags; (void)val; (void)old;
  return 0;
}

int timer_getoverrun(timer_t timerid) {
  (void)timerid;
  return 0;
}

int timespec_get(struct timespec *ts, int base) {
  if (base != TIME_UTC) return 0;
  clock_gettime(CLOCK_REALTIME, ts);
  return base;
}

clock_t clock(void) {
  return 0;
}

time_t time(time_t *t) {
  if (t) *t = 0;
  return 0;
}

double difftime(time_t t1, time_t t0) {
  return (double)(t1 - t0);
}

/* Days per month (non-leap, leap) */
static const int days_in_month[2][12] = {
  { 31,28,31,30,31,30,31,31,30,31,30,31 },
  { 31,29,31,30,31,30,31,31,30,31,30,31 },
};

static int is_leap(int year) {
  return (year%4==0 && year%100!=0) || year%400==0;
}

static struct tm static_tm;

struct tm *gmtime_r(const time_t *t, struct tm *result) {
  time_t secs = *t;
  int days = (int)(secs / 86400);
  int rem  = (int)(secs % 86400);
  if (rem < 0) { rem += 86400; days--; }

  result->tm_hour = rem / 3600; rem %= 3600;
  result->tm_min  = rem / 60;
  result->tm_sec  = rem % 60;

  /* Jan 1 1970 = Thursday (wday=4) */
  result->tm_wday = (4 + days) % 7;
  if (result->tm_wday < 0) result->tm_wday += 7;

  int year = 1970;
  while (1) {
    int dy = is_leap(year) ? 366 : 365;
    if (days < dy) break;
    days -= dy;
    year++;
  }
  result->tm_year = year - 1900;
  result->tm_yday = days;

  int leap = is_leap(year);
  result->tm_mon = 0;
  for (int m = 0; m < 12; m++) {
    if (days < days_in_month[leap][m]) { result->tm_mon = m; break; }
    days -= days_in_month[leap][m];
  }
  result->tm_mday   = days + 1;
  result->tm_isdst  = 0;
  return result;
}

struct tm *gmtime(const time_t *t) {
  return gmtime_r(t, &static_tm);
}

struct tm *localtime_r(const time_t *t, struct tm *result) {
  return gmtime_r(t, result);
}

struct tm *localtime(const time_t *t) {
  return gmtime_r(t, &static_tm);
}

time_t mktime(struct tm *tm) {
  int year = tm->tm_year + 1900;
  long days = 0;
  for (int y = 1970; y < year; y++)
    days += is_leap(y) ? 366 : 365;
  int leap = is_leap(year);
  for (int m = 0; m < tm->tm_mon; m++)
    days += days_in_month[leap][m];
  days += tm->tm_mday - 1;
  /* recompute wday/yday */
  tm->tm_yday = (int)(days - (/* days to Jan 1 of year */ 0));
  tm->tm_wday = (int)((4 + days) % 7);
  return (time_t)(days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec);
}

static const char *wday_abbr[] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
static const char *mon_abbr[]  = { "Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec" };

static char asctime_buf[26];

char *asctime_r(const struct tm *tm, char *buf) {
  /* "Thu Jan  1 00:00:00 1970\n" */
  const char *wd = (tm->tm_wday >= 0 && tm->tm_wday < 7)
                   ? wday_abbr[tm->tm_wday] : "???";
  const char *mo = (tm->tm_mon  >= 0 && tm->tm_mon  < 12)
                   ? mon_abbr[tm->tm_mon]   : "???";
  int year = tm->tm_year + 1900;
  /* format: "Www Mmm DD HH:MM:SS YYYY\n" */
  buf[0]  = wd[0]; buf[1]  = wd[1]; buf[2]  = wd[2]; buf[3]  = ' ';
  buf[4]  = mo[0]; buf[5]  = mo[1]; buf[6]  = mo[2]; buf[7]  = ' ';
  buf[8]  = tm->tm_mday < 10 ? ' ' : '0' + tm->tm_mday / 10;
  buf[9]  = '0' + tm->tm_mday % 10;
  buf[10] = ' ';
  buf[11] = '0' + tm->tm_hour / 10; buf[12] = '0' + tm->tm_hour % 10; buf[13] = ':';
  buf[14] = '0' + tm->tm_min  / 10; buf[15] = '0' + tm->tm_min  % 10; buf[16] = ':';
  buf[17] = '0' + tm->tm_sec  / 10; buf[18] = '0' + tm->tm_sec  % 10; buf[19] = ' ';
  buf[20] = '0' + year / 1000;
  buf[21] = '0' + (year / 100) % 10;
  buf[22] = '0' + (year / 10)  % 10;
  buf[23] = '0' + year % 10;
  buf[24] = '\n'; buf[25] = '\0';
  return buf;
}

char *asctime(const struct tm *tm) {
  return asctime_r(tm, asctime_buf);
}

char *ctime_r(const time_t *t, char *buf) {
  struct tm tmp;
  return asctime_r(gmtime_r(t, &tmp), buf);
}

char *ctime(const time_t *t) {
  return ctime_r(t, asctime_buf);
}

size_t strftime(char *restrict s, size_t max, const char *restrict fmt,
                const struct tm *restrict tm) {
  size_t out = 0;
#define PUT(c) do { if (out+1 >= max) return 0; s[out++] = (c); } while(0)
#define PUTS2(a,b) do { PUT(a); PUT(b); } while(0)
  for (; *fmt; fmt++) {
    if (*fmt != '%') { PUT(*fmt); continue; }
    fmt++;
    switch (*fmt) {
    case 'Y': {
      int y = tm->tm_year + 1900;
      PUT('0'+y/1000); PUT('0'+(y/100)%10); PUT('0'+(y/10)%10); PUT('0'+y%10);
      break;
    }
    case 'm': PUTS2('0'+((tm->tm_mon+1)/10), '0'+((tm->tm_mon+1)%10)); break;
    case 'd': PUTS2('0'+(tm->tm_mday/10),    '0'+(tm->tm_mday%10));    break;
    case 'H': PUTS2('0'+(tm->tm_hour/10),    '0'+(tm->tm_hour%10));    break;
    case 'M': PUTS2('0'+(tm->tm_min/10),     '0'+(tm->tm_min%10));     break;
    case 'S': PUTS2('0'+(tm->tm_sec/10),     '0'+(tm->tm_sec%10));     break;
    case 'A': { const char *w = wday_abbr[tm->tm_wday % 7];
                PUT(w[0]); PUT(w[1]); PUT(w[2]); break; }
    case 'b':
    case 'h': { const char *m = mon_abbr[tm->tm_mon % 12];
                PUT(m[0]); PUT(m[1]); PUT(m[2]); break; }
    case 'n': PUT('\n'); break;
    case 't': PUT('\t'); break;
    case '%': PUT('%'); break;
    default:  PUT('%'); PUT(*fmt); break;
    }
  }
  s[out] = '\0';
  return out;
#undef PUT
#undef PUTS2
}

static int parse_int(const char **s, int width) {
  int val = 0, n = 0;
  while (n < width && **s >= '0' && **s <= '9') {
    val = val * 10 + (**s - '0');
    (*s)++; n++;
  }
  return (n > 0) ? val : -1;
}

char *strptime(const char *s, const char *fmt, struct tm *tm) {
  for (; *fmt; fmt++) {
    if (*fmt != '%') {
      if (*s == *fmt) { s++; continue; }
      return NULL;
    }
    fmt++;
    int val;
    switch (*fmt) {
    case 'Y': val = parse_int(&s, 4); if (val < 0) return NULL; tm->tm_year = val - 1900; break;
    case 'm': val = parse_int(&s, 2); if (val < 0) return NULL; tm->tm_mon  = val - 1;    break;
    case 'd': val = parse_int(&s, 2); if (val < 0) return NULL; tm->tm_mday = val;         break;
    case 'H': val = parse_int(&s, 2); if (val < 0) return NULL; tm->tm_hour = val;         break;
    case 'M': val = parse_int(&s, 2); if (val < 0) return NULL; tm->tm_min  = val;         break;
    case 'S': val = parse_int(&s, 2); if (val < 0) return NULL; tm->tm_sec  = val;         break;
    default:  return NULL;
    }
  }
  return (char *)s;
}
