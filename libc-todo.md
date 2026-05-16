# libc roadmap — Python-ready (no threads)

Goal: grow **sbunix libc** until upstream **CPython** (or a trimmed static build) can be **compiled and linked** against `libc.a` on this OS. Programs are built from **source** with `riscv64-unknown-elf-gcc`; we are **not** pulling in musl/glibc binaries.

**Out of scope for this list:** `pthread`, `futex`, `clone`, TLS for per-thread `errno`, `threading` module support.

**Toolchain note:** kernel/userspace target `rv64imac` (no FPU). Plan for soft-float / `libgcc` math or a Python build with `--disable-float` / limited `math` module.

---

## Infrastructure & layout

- [ ] Reorganize libc into `src/` subdirs (string, stdio, stdlib, unistd, …) while keeping `build/libc.a`
- [ ] Add `libc/arch/riscv64/syscall.h` — single list of syscall numbers (must match `kernel/include/arch/riscv64/syscalls/syscalls.h`)
- [ ] Add `libc/internal/syscall_ret.c` — map kernel negative returns to `errno` and return `-1` to caller
- [ ] Add `libc/errno.c` — define `int errno` and use it from all syscall wrappers
- [ ] Move syscall macros out of `include/unistd.h` into `arch/riscv64/syscall.h` (keep public headers clean)
- [ ] Document `struct stat` layout in one place (`include/sys/stat.h`) and keep kernel `vfs_stat` in sync
- [ ] Add `include/limits.h` (`PATH_MAX`, `NAME_MAX`, `LINE_MAX`, `OPEN_MAX`, …)
- [ ] Add `include/stdbool.h`, `include/stdint.h` (extend if needed), `include/inttypes.h`
- [ ] Add `include/assert.h` (`assert` macro)
- [ ] Add `include/ctype.h` + `src/ctype/is*.c` or macro-only implementation
- [ ] Add `include/strings.h` (`bcopy`, `bzero`, `strcasecmp`, `strncasecmp`, `index`/`rindex` aliases optional)
- [ ] Add `include/float.h`, `include/limits.h` FP constants (even if soft-float)
- [ ] Add `include/setjmp.h` + `arch/riscv64/setjmp.S` (CPython and gcc use longjmp in places)
- [ ] Add `include/stdalign.h`, `include/stdnoreturn.h` stubs if compiler expects them
- [ ] Add `include/uchar.h` / minimal wide stubs (or configure Python `--disable-wide-char`)

---

## Errno & syscall wrappers

- [ ] Wire every existing `unistd.c` / `open.c` / `mman.c` wrapper through `syscall_ret`
- [ ] Return `-1` and set `errno` on all failure paths (stop returning raw negative errno to app code)
- [ ] Add `include/string.h` → `strerror()` / `strerror_r()` using errno table
- [ ] Add `perror()` / `perror.c` in stdio
- [ ] Expand `errno.h` with codes Python/stdlib expect (`EWOULDBLOCK`, `ENOTSUP`, `EOVERFLOW`, `EADDRINUSE`, …)

---

## String & memory (`string.c` → `src/string/`)

- [ ] `memmove`
- [ ] `memcmp`
- [ ] `memchr`
- [ ] `strlen` — already present; audit `strnlen`
- [ ] `strdup` / `strndup`
- [ ] `strcat` / `strncat` (or document ban; Python may not need if snprintf used)
- [ ] `strlcpy` / `strlcat` (BSD; some tarballs use them)
- [ ] `strcoll` / `strxfrm` — stub to `strcmp` / memcpy for `"C"` locale
- [ ] `strerror` (see errno)
- [ ] `strpbrk`, `strspn`, `strcspn`
- [ ] `strstr`
- [ ] `strtok` / `strtok_r`
- [ ] `strchr` — present; add `strrchr`
- [ ] `strcasecmp` / `strncasecmp` in `strings.h`

---

## Stdlib (`stdlib.h`, `malloc.c`, `atoi.c`)

- [ ] `abort()` — raise `SIGABRT` or `_exit`
- [ ] `atexit()` / `__cxa_atexit` minimal (CPython registers cleanups)
- [ ] `exit()` — present; ensure `stdio` flush_all on exit
- [ ] `getenv` / `setenv` / `unsetenv` / `putenv` (read `execve` envp; store in libc for later)
- [ ] `system()` — `fork` + `execve` + `wait` (optional if disabled in Python)
- [ ] `strtol`, `strtoul`, `strtoll`, `strtoull` (base 0, errno `ERANGE`)
- [ ] `strtod`, `strtof`, `strtold` (soft-float or link libgcc)
- [ ] `atof`, `atol`, `atoll`
- [ ] `rand` / `srand` (simple LCG ok for bootstrap)
- [ ] `qsort`, `bsearch`
- [ ] `abs`, `labs`, `llabs`
- [ ] `div`, `ldiv`, `lldiv`
- [ ] `realpath` (wrap `getcwd` + path resolution or syscall)
- [ ] `canonicalize_file_name` (GNU; alias or stub)
- [ ] `posix_memalign` / `aligned_alloc` (optional; malloc alignment may suffice)
- [ ] `getpagesize()` / `sysconf(_SC_PAGESIZE)`
- [ ] `sysconf()` — stub constants (`_SC_CLK_TCK`, `_SC_OPEN_MAX`, …)
- [ ] Harden `realloc` (NULL, zero size, failure preserves old block)
- [ ] `calloc` overflow check
- [ ] Document max heap / sbrk+ mmap arena policy for Python memory hunger

---

## Stdio (`printf.c` → `src/stdio/`)

Python needs real `FILE *` streams, not only `printf`.

- [ ] Define `FILE` struct (`stdio_impl.h`: fd, flags, buffer, bufpos, buflen)
- [ ] `stdin` / `stdout` / `stderr` as global `FILE` objects
- [ ] `fopen` / `freopen` / `fdopen`
- [ ] `fclose` / `fflush`
- [ ] `fread` / `fwrite`
- [ ] `fgetc` / `getc` / `fputc` / `putc`
- [ ] `ungetc`
- [ ] `fgets` / `fputs`
- [ ] `fseek` / `ftell` / `rewind` / `fgetpos` / `fsetpos`
- [ ] `feof` / `ferror` / `clearerr`
- [ ] `setvbuf` / `setbuf`
- [ ] `fprintf` / `vfprintf` / `vprintf`
- [ ] `sprintf` / `vsprintf` (unsafe but headers expected)
- [ ] `snprintf` / `vsnprintf` — extend existing `snprintf`
- [ ] `sscanf` / `fscanf` / `scanf` (large; required for some code paths — or stub and disable Python features)
- [ ] `perror`
- [ ] `fileno`
- [ ] `dprintf` (optional)
- [ ] `tmpfile` / `tmpnam` — stub `-ENOSYS` or temp dir on sbfs
- [ ] `popen` / `pclose` (pipe + fork + exec) for `os.popen` if enabled
- [ ] Locking: no-op macros for single-threaded build (`flockfile` stubs)
- [ ] Flush all open streams from `exit()`

---

## Math (`include/math.h`, `src/math/`)

- [ ] Full `math.h` declarations
- [ ] Implement via compiler RT / `libgcc` soft-float, or vendor minimal `libm`
- [ ] `fenv.h` stubs (`feclearexcept`, …) if math.h references them
- [ ] Python: expect to build with external libm or limited `math` module on `rv64imac`

---

## Time (`time.h`, `src/time/`)

- [ ] Kernel: `clock_gettime` syscall (RTC + monotonic tick)
- [ ] `clock_gettime`, `clock_getres`
- [ ] `gettimeofday` / `settimeofday` (optional)
- [ ] `time()` 
- [ ] `timespec_get` (C11)
- [ ] `nanosleep` — present; handle `rem` on `EINTR`
- [ ] `sleep` / `usleep` — present in unistd; move or alias
- [ ] `clock()` — stub from monotonic ms
- [ ] `struct tm`, `gmtime`, `localtime`, `mktime` (simplified epoch math ok)
- [ ] `strftime` / `strptime` (strftime required for `time` module formatting)
- [ ] `tzset` / `timezone` — stub UTC-only
- [ ] `include/sys/time.h` (`struct timeval`, `gettimeofday`)
- [ ] `include/time.h` — `CLOCK_REALTIME`, `CLOCK_MONOTONIC`, `timespec`

---

## Stat & mode (`include/sys/stat.h`, `src/stat/`)

- [ ] Move `struct stat` out of `unistd.h` into `sys/stat.h`
- [ ] Add `S_IFMT`, `S_ISREG`, `S_ISDIR`, `S_ISLNK`, `S_ISCHR`, … macros
- [ ] Add `S_IRUSR` … permission macros (match kernel `mode.h`)
- [ ] `chmod` / `fchmod` wrapper (kernel `fchmod` if missing)
- [ ] `mkdir` mode bits
- [ ] `umask()` — kernel syscall + libc wrapper
- [ ] `mkfifo` (optional; stub if no FIFO open path)
- [ ] `lutimes` / `utimes` / `utime` — stubs ok for bootstrap

---

## Directory & filesystem (`dirent.h`, unistd)

- [ ] `opendir` / `readdir` / `closedir` / `rewinddir` on top of `getdents`
- [ ] `scandir` / `alphasort` (optional; Python `os.scandir` uses lower level)
- [ ] `dirfd` (optional)
- [ ] `access()` — kernel `faccessat` + wrapper
- [ ] `fcntl()` — `F_GETFL`, `F_SETFL`, `O_NONBLOCK`, `F_DUPFD`, `FD_CLOEXEC`
- [ ] `dup()` / `dup3()` — kernel + libc (only `dup2` today)
- [ ] `pipe2()` with `O_CLOEXEC` flag
- [ ] `creat()` wrapper
- [ ] `pathconf` / `fpathconf` — stubs
- [ ] `statvfs` / `fstatvfs` — optional (`statfs` exists)
- [ ] `sync` / `fsync` — `fsync` present
- [ ] `link` / `symlink` / `readlink` — mostly present via unistd
- [ ] `rename` — present
- [ ] `truncate` / `ftruncate` — present
- [ ] `chown` / `fchown` / `lchown` — stub success (root owns all)
- [ ] `getcwd` / `chdir` — present
- [ ] `getdents` — keep custom layout or migrate; document in `dirent.h`

---

## Process & wait (`unistd`, `sys/wait.h`)

- [ ] Add `include/sys/wait.h` — `WIFEXITED`, `WEXITSTATUS`, `WIFSIGNALED`, `WTERMSIG`, `WIFSTOPPED`, `WCOREDUMP`
- [ ] `WNOHANG`, `WUNTRACED` constants
- [ ] Kernel: `waitpid` accept `options` (`WNOHANG` at minimum)
- [ ] Kernel: `waitpid` accept `pid == 0` (any child in group) if shells/python subprocess code need it
- [ ] `wait`, `waitpid`, `waitid` — stub `waitid`
- [ ] `fork`, `execve`, `execv`, `execl`, … wrappers
- [ ] `_exit` / quick exit syscall path
- [ ] `getpid`, `getppid`, `getpgid`, `setpgid` — mostly present
- [ ] `kill`, `raise`
- [ ] `alarm` — stub
- [ ] `pause`
- [ ] `getuid` / `geteuid` / `getgid` / … — stub return 0
- [ ] `sys/types.h` — `pid_t`, `uid_t`, `gid_t`, `off_t`, `ssize_t`, `mode_t`, `dev_t`, `ino_t`, `nlink_t`
- [ ] `include/unistd.h` — trim to declarations; split impl into `src/unistd/*.c`

---

## Signals (`signal.c`, `signal.h`)

- [ ] Implement `sigprocmask` — kernel `rt_sigprocmask` syscall
- [ ] `sigpending` — stub empty set
- [ ] `sigsuspend` — stub or implement with mask
- [ ] `sigwait` — stub `-ENOSYS` (no threads)
- [ ] `signal()` — thin wrapper over `sigaction`
- [ ] `siginterrupt` — stub
- [ ] `sigemptyset` … — present
- [ ] `raise()`
- [ ] `kill()` — present
- [ ] Python: build with signals enabled; ensure `EINTR` restarts or `SA_RESTART` where needed

---

## Memory mapping (`mman.c`, `sys/mman.h`)

- [ ] `mprotect()` — kernel syscall + libc wrapper (CPython W^X for code objects / JIT off)
- [ ] `msync()` — stub success
- [ ] `madvise()` — stub success
- [ ] `posix_madvise` — stub
- [ ] `mmap` — present; audit `MAP_FIXED`, `MAP_PRIVATE|SHARED`, file-backed maps
- [ ] `munmap` — present
- [ ] `brk` / `sbrk` — present; coordinate with malloc
- [ ] Large mapping support for Python heap + arena

---

## I/O vectors & poll (no threads)

- [ ] `include/sys/uio.h` — `struct iovec`, `readv`, `writev`
- [ ] Kernel + libc: `readv` / `writev` syscalls
- [ ] `include/poll.h` — `poll`, `struct pollfd`, `POLLIN`, `POLLOUT`, …
- [ ] Kernel + libc: `poll` syscall (for `selectors` / `select` module without threads)
- [ ] `select()` — implement via `poll` if needed
- [ ] `ppoll` / `pselect6` — optional

---

## Locale & wchar (stubs acceptable)

- [ ] `include/locale.h` — `setlocale` returns `"C"`, `localeconv` static struct
- [ ] `nl_langinfo` — stub
- [ ] `mbstowcs` / `wcstombs` / `mbrtowc` — minimal UTF-8 if wide char enabled
- [ ] `include/wchar.h` — stub types and no-op functions if Python wide build disabled

---

## Networking (optional — enable `import socket` later)

- [ ] `include/sys/socket.h`, `netinet/in.h`, `arpa/inet.h` — headers
- [ ] `socket`, `bind`, `listen`, `accept`, `connect`, `send`, `recv`, `shutdown`
- [ ] `getaddrinfo` / `freeaddrinfo` / `getnameinfo` — large; defer or stub
- [ ] Kernel network stack or hosted offload — separate project

---

## Dynamic linking (optional — static Python first)

- [ ] `include/dlfcn.h` — `dlopen`, `dlsym`, `dlclose`, `dlerror`
- [ ] Requires kernel loader relocations + shared libs — **defer**; ship static `python` only
- [ ] Python: `--disable-shared`, no extension modules or builtin-only modules

---

## Regex / glob / fnmatch (optional ports)

- [ ] `fnmatch.c` — useful for make/busybox, less for Python core
- [ ] `glob.c` — optional
- [ ] Python ships own `re` engine; system regex not required

---

## Crypt / hash (optional)

- [ ] `crypt.h` / `crypt()` — not required for Python 3 core
- [ ] `md5` / `sha` — Python uses own implementations

---

## Kernel syscalls to add (libc depends on these)

Implement in `kernel/syscalls/*.c` and register in `syscalls.c` as libc grows:

- [ ] `clock_gettime`
- [ ] `gettimeofday` (if not folding into clock_gettime only)
- [ ] `faccessat` / `access`
- [ ] `fcntl`
- [ ] `dup` / `dup3`
- [ ] `pipe2`
- [ ] `mprotect`
- [ ] `rt_sigprocmask`
- [ ] `umask`
- [ ] `readv` / `writev`
- [ ] `poll`
- [ ] `getrandom` (or libc PRNG fallback for `os.urandom`)
- [ ] `exit_group` — alias of `exit` ok
- [ ] `waitpid` — `WNOHANG` and broader `pid` matching
- [ ] `fchmod` / `fchmodat` (if `fchmod` wrapper added)
- [ ] `lseek` — verify 64-bit offsets for large files
- [ ] Unknown syscall → `-ENOSYS` (not `-1`)

---

## Headers inventory (add if missing)

- [ ] `sys/stat.h`
- [ ] `sys/wait.h`
- [ ] `sys/types.h`
- [ ] `sys/time.h`
- [ ] `sys/times.h`
- [ ] `sys/resource.h` (`getrusage`, `getrlimit`)
- [ ] `sys/uio.h`
- [ ] `sys/ioctl.h` (expand TTY ioctls)
- [ ] `sys/mman.h` — extend
- [ ] `poll.h`
- [ ] `fcntl.h` — extend `open` flags, `AT_*` constants
- [ ] `dirent.h` — DIR type + API
- [ ] `termios.h` (for `isatty` / ttyname patterns)
- [ ] `unistd.h` — POSIX.1 declarations cleanup
- [ ] `math.h`
- [ ] `locale.h`
- [ ] `stdint.h` / `inttypes.h`
- [ ] `stdarg.h` — present
- [ ] `setjmp.h`
- [ ] `signal.h` — present; complete impl
- [ ] `stdio.h` — expand heavily
- [ ] `stdlib.h` — expand heavily
- [ ] `string.h` — expand
- [ ] `time.h` — expand
- [ ] `errno.h` — present; extend
- [ ] `limits.h`
- [ ] `float.h`
- [ ] `iso646.h`, `stdalign.h` — optional stubs

---

## Python port checklist (application-level, not libc.c)

- [ ] Vendor CPython sources under `third_party/python-3.x/`
- [ ] Cross-compile / native compile with `-I libc/include -L build -lc`
- [ ] `./configure` flags: `--without-threads`, `--disable-shared`, `--disable-loadable-extensions` (initially)
- [ ] Provide `os.py` platform def or patch `pyconfig.h` for `sbunix`
- [ ] Heap: ensure sufficient RAM + mmap for interpreter (~8–32 MiB minimum)
- [ ] `rv64imac`: enable soft-float ABI consistently in kernel, libc, and Python build
- [ ] Smoke tests: `python -c "print(1)"`, `import os`, `open/read/write`, `time.time()`, `signal.signal`
- [ ] Consider **MicroPython** as intermediate milestone (smaller libc bar) before full CPython

---

## Suggested implementation order

1. **errno + syscall_ret** — everything else becomes debuggable  
2. **sys/stat.h + wait.h + time (clock_gettime)**  
3. **stdio FILE layer**  
4. **stdlib strto* + getenv + atexit**  
5. **access, fcntl, dup, mprotect, sigprocmask** (kernel + libc)  
6. **poll + readv** (for select module, no threads)  
7. **math/libm** strategy for `rv64imac`  
8. **strftime + locale stubs**  
9. **Attempt CPython static build** and fill gaps from linker / runtime errors  

---

## Already present (do not re-implement from scratch)

- Syscall macros and most POSIX I/O in `unistd.c` / `open.c`
- `malloc` / `calloc` / `realloc` / `free`
- Basic `string` + `printf` / `snprintf`
- `mmap` / `munmap` / `brk`
- `fork`, `execve`, `waitpid`, `kill`, `pipe`, `dup2`
- `stat` / `fstat` / `lstat`, `chmod`, `truncate` / `ftruncate`
- `*at` VFS operations, `symlink`, `readlink`, `getdents` (custom layout)
- `sigaction`, sigset helpers, `nanosleep`, `getopt`
- `crt.S` entry (`_start` → `main` → `exit`)
