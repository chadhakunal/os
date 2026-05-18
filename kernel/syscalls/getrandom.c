#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/user_data_access.h"
#include "errno.h"
#include "types.h"

static uint64_t rng_state;

void getrandom_init(void) {
  uint64_t t;
  asm volatile("csrr %0, time" : "=r"(t));
  rng_state = t ^ (uint64_t)&rng_state;
  if (rng_state == 0) rng_state = 0xdeadbeefcafe1234ULL;
}

static uint64_t xorshift64star(void) {
  rng_state ^= rng_state >> 12;
  rng_state ^= rng_state << 25;
  rng_state ^= rng_state >> 27;
  return rng_state * 0x2545F4914F6CDD1DULL;
}

DEFINE_SYSCALL3(getrandom, void *, buf, size_t, count, unsigned int, flags)
{
  (void)flags;
  if (buf == NULL) return -EFAULT;

  size_t done = 0;
  while (done < count) {
    uint64_t val = xorshift64star();
    size_t chunk = count - done;
    if (chunk > sizeof(val)) chunk = sizeof(val);
    if (copy_to_user((char *)buf + done, &val, chunk) != 0) return -EFAULT;
    done += chunk;
  }
  return (int64_t)count;
}
