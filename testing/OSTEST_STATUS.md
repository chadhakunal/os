# os-tests-sortix status

Directories are either **pruned** (not worth implementing), **done** (header + implementation exist), or **partial** (header exists, implementation incomplete).

---

## Done — header and implementation present

| Directory    | Notes |
|--------------|-------|
| sys_uio      | readv/writev — sys/uio.h + kernel syscalls |
| sys_times    | times() — sys/times.h + kernel syscall |
| sys_resource | getrlimit/setrlimit — sys/resource.h |
| endian       | endian.h — htobe/le macros, no-op for little-endian |
| strings      | strings.h — ffs, bcmp, bcopy |
| setjmp       | setjmp.h |
| inttypes     | inttypes.h |
| regex        | regex.h — regcomp/regexec (tre backend) |
| libgen       | libgen.h — basename/dirname |
| ctype        | ctype.h |
| sched        | sched.h — sched_yield |
| poll         | poll.h — poll/ppoll kernel + libc |
| signal       | signal.h — sigaction, sigprocmask, kill, sigwait, sigwaitinfo, sigtimedwait, SA_RESTART |
| dirent       | dirent.h — opendir/readdir/rewinddir/seekdir/scandir |
| fcntl        | fcntl.h — open, fcntl, dup, dup2 |
| stdio        | stdio.h — printf family, fopen, fread, fwrite, fseek |
| stdlib       | stdlib.h — malloc, qsort, atoi, strtol, etc |
| time         | time.h — clock_gettime, nanosleep, gmtime, strftime |
| unistd       | unistd.h — read, write, readv, writev, lseek, fork, exec, pipe |
| sys_mman     | sys/mman.h — mmap, munmap, mprotect |
| sys_stat     | sys/stat.h — stat, fstat, mkdir, chmod |
| sys_wait     | sys/wait.h — waitpid, waitid |

---

## Partial — header exists, implementation incomplete or untested

| Directory  | Notes |
|------------|-------|
| wchar      | wchar.h exists; most functions are stubs |
| wctype     | wctype.h exists; classification functions stub |
| select     | sys/select.h — select/pselect kernel exists, libc wired |

---

## Pruned — not applicable or too large to implement

| Directory   | Reason |
|-------------|--------|
| aio         | POSIX async I/O — not implemented |
| termios     | terminal I/O control — no header, not implemented |
| fnmatch     | pattern matching — no header |
| glob        | pathname expansion — no header |
| sys_statvfs | statvfs — no header |
| sys_utsname | uname() struct — no header (uname syscall exists) |
| sys_ipc     | SysV IPC — no header, not implemented |
| sys_msg     | SysV message queues — not implemented |
| sys_shm     | SysV shared memory — not implemented |
| syslog      | syslog — no header |
| libintl     | gettext/i18n — not implemented |
| dlfcn       | dynamic linking — static freestanding OS |
| fmtmsg      | fmtmsg() — no header |
| iconv       | character encoding — not implemented |
| langinfo    | nl_langinfo — locale not implemented |
| locale      | setlocale — stub only |
| monetary    | strfmon — not implemented |
| nl_types    | catopen/catgets — not implemented |
| mqueue      | POSIX message queues — not implemented |
| search      | hsearch/tsearch — no header |
| sched       | advanced scheduler APIs (only sched_yield done) |
| grp         | group DB — not implemented |
| pwd         | password DB — not implemented |
| utmpx       | login records — not implemented |
| math        | math.h — not implemented |
| complex     | complex.h — not implemented |
| fenv        | fenv.h — not implemented |
| uchar       | C11 char16_t/char32_t — not implemented |
| wordexp     | wordexp() — no header |
| devctl      | posix_devctl — Sortix-specific |
