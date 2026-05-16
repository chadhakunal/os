#include <stdio.h>
#include <unistd.h>

int main(void) {
  printf("Shutting down...\n");
  reboot(RB_POWER_OFF);
  return 0;
}
