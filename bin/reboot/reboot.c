#include <stdio.h>
#include <unistd.h>

int main(void) {
  printf("Rebooting...\n");
  reboot(RB_AUTOBOOT);
  return 0;
}
