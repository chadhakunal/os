#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

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
  unsigned long days_since_epoch = (unsigned long)t / 86400;
  /* 1970-01-01 was a Thursday = day 4 in Sun=0 convention, or index 0 in our Thu-first array. */
  unsigned long dow = (days_since_epoch + 4) % 7; /* 0=Thu,1=Fri,2=Sat,3=Sun,4=Mon,5=Tue,6=Wed */

  unsigned long days  = days_since_epoch;

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

  static const char *months[] = {
    "","Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
  };

  /* Check for optional format string: date +FORMAT */
  if (argc >= 2 && argv[1][0] == '+') {
    const char *fmt = argv[1] + 1;
    char out[64];
    int olen = 0;
    for (; *fmt && olen < 63; ) {
      if (*fmt != '%') { out[olen++] = *fmt++; continue; }
      fmt++; /* skip '%' */
      char spec = *fmt++;
      char tmp[16];
      int tlen = 0;
      switch (spec) {
        case 'Y': tlen = snprintf(tmp, sizeof(tmp), "%04lu", y);    break;
        case 'm': tlen = snprintf(tmp, sizeof(tmp), "%02lu", mo);   break;
        case 'd': tlen = snprintf(tmp, sizeof(tmp), "%02lu", d);    break;
        case 'H': tlen = snprintf(tmp, sizeof(tmp), "%02lu", hour); break;
        case 'M': tlen = snprintf(tmp, sizeof(tmp), "%02lu", min);  break;
        case 'S': tlen = snprintf(tmp, sizeof(tmp), "%02lu", sec);  break;
        case 'n': tmp[0] = '\n'; tlen = 1;                          break;
        case '%': tmp[0] = '%';  tlen = 1;                          break;
        default:  tmp[0] = '%'; tmp[1] = spec; tlen = 2;            break;
      }
      for (int i = 0; i < tlen && olen < 63; i++)
        out[olen++] = tmp[i];
    }
    out[olen++] = '\n';
    out[olen] = '\0';
    write(1, out, olen);
    return 0;
  }

  /* Default: "Www Mmm DD HH:MM:SS UTC YYYY" */
  printf("%s %s %02lu %02lu:%02lu:%02lu UTC %04lu\n",
         days_of_week[dow], months[mo], d, hour, min, sec, y);
  return 0;
}
