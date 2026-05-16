#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
  const char *path = (argc >= 2) ? argv[1] : "/";

  struct statfs st;
  if (statfs(path, &st) < 0) {
    printf("df: statfs failed for '%s'\n", path);
    return 1;
  }

  long block_size  = st.f_bsize ? st.f_bsize : 512;
  long total_kb    = (st.f_blocks * block_size) / 1024;
  long free_kb     = (st.f_bfree  * block_size) / 1024;
  long used_kb     = total_kb - free_kb;
  int  use_pct     = total_kb > 0 ? (int)((used_kb * 100) / total_kb) : 0;

  printf("%-20s %10s %10s %10s %5s %s\n",
         "Filesystem", "1K-blocks", "Used", "Available", "Use%", "Mounted on");
  printf("%-20s %10ld %10ld %10ld %4d%% %s\n",
         "sbfs", total_kb, used_kb, free_kb, use_pct, path);

  printf("\nInodes:\n");
  printf("  Total : %ld\n", st.f_files);
  printf("  Free  : %ld\n", st.f_ffree);
  printf("  Used  : %ld\n", st.f_files - st.f_ffree);

  return 0;
}
