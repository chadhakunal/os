#include <stdio.h>
#include <time.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) < 0) {
    fprintf(stderr, "date: cannot get time\n");
    return 1;
  }

  time_t t = ts.tv_sec;

  /* Decompose seconds-since-epoch into date/time components.
   * Valid for 1970-2099 (no leap-second handling). */
  unsigned long sec   = (unsigned long)t % 60;
  unsigned long min   = ((unsigned long)t / 60) % 60;
  unsigned long hour  = ((unsigned long)t / 3600) % 24;
  unsigned long days  = (unsigned long)t / 86400;

  /* Shift to a March-based year for simpler leap-year math. */
  days += 719468UL; /* days from 0000-03-01 to 1970-01-01 */
  unsigned long era  = days / 146097;
  unsigned long doe  = days - era * 146097;
  unsigned long yoe  = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
  unsigned long y    = yoe + era * 400;
  unsigned long doy  = doe - (365*yoe + yoe/4 - yoe/100);
  unsigned long mp   = (5*doy + 2) / 153;
  unsigned long d    = doy - (153*mp + 2)/5 + 1;
  unsigned long mo   = mp < 10 ? mp + 3 : mp - 9;
  y += (mo <= 2);

  static const char *days_of_week[] = {
    "Thu","Fri","Sat","Sun","Mon","Tue","Wed"
  };
  /* days since epoch; 1970-01-01 was a Thursday (day 4). */
  unsigned long dow = ((unsigned long)t / 86400 + 4) % 7;

  static const char *months[] = {
    "","Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
  };

  /* Check for optional format string: date +FORMAT */
  if (argc >= 2 && argv[1][0] == '+') {
    const char *fmt = argv[1] + 1;
    for (; *fmt; ) {
      if (*fmt != '%') { putchar(*fmt++); continue; }
      fmt++; /* skip '%' */
      char spec = *fmt++;
      switch (spec) {
        case 'Y': printf("%04lu", y);    break;
        case 'm': printf("%02lu", mo);   break;
        case 'd': printf("%02lu", d);    break;
        case 'H': printf("%02lu", hour); break;
        case 'M': printf("%02lu", min);  break;
        case 'S': printf("%02lu", sec);  break;
        case 'n': putchar('\n');         break;
        case '%': putchar('%');          break;
        default:  putchar('%'); putchar(spec); break;
      }
    }
    putchar('\n');
    return 0;
  }

  /* Default: "Www Mmm DD HH:MM:SS UTC YYYY" */
  printf("%s %s %02lu %02lu:%02lu:%02lu UTC %04lu\n",
         days_of_week[dow], months[mo], d, hour, min, sec, y);
  return 0;
}
