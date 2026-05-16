# os-tests-sortix status

Directories are either **pruned** (removed from vendor, not worth fixing now) or
**to fix** (header exists in libc/include, failures are compile/link errors we can address).

---

## Pruned — no header / not applicable to sbunix

| Directory   | Reason |
|-------------|--------|
| aio         | POSIX async I/O — not implemented |
| libintl     | gettext/i18n — not implemented |
| dlfcn       | dynamic linking — static freestanding OS |
| fmtmsg      | fmtmsg() — no header |
| fnmatch     | fnmatch() — no header |
| ftw         | file tree walk — no header |
| glob        | glob() — no header |
| iconv       | character encoding conversion — no header |
| langinfo    | nl_langinfo — locale stack not implemented |
| locale      | setlocale/localeconv — locale stack not implemented |
| monetary    | strfmon — locale stack not implemented |
| nl_types    | catopen/catgets — locale stack not implemented |
| mqueue      | POSIX message queues — not implemented |
| ndbm        | legacy key-value DB — no header |
| poll        | poll/ppoll — no poll stack |
| sys_select  | select/pselect — no poll stack |
| sys_time    | utimes/select — no header |
| sched       | scheduler control APIs — not implemented |
| search      | hsearch/tsearch — no header |
| sys_ipc     | SysV IPC ftok — no header |
| sys_msg     | SysV message queues — no header |
| sys_shm     | SysV shared memory — no header |
| sys_resource| getrlimit/getrusage — no header |
| sys_statvfs | statvfs — no header |
| sys_times   | times() — no header |
| sys_utsname | uname() — no header |
| sys_uio     | readv/writev — no header |
| syslog      | syslog — no header |
| termios     | terminal I/O — no header |
| uchar       | C11 char16_t/char32_t — no header |
| wchar       | wide char — no header |
| wctype      | wide char classification — no header |
| wordexp     | wordexp() — no header |
| devctl      | posix_devctl — Sortix-specific, no header |
| endian      | be16toh etc. — no <endian.h> |
| math        | math.h functions — not implemented |
| complex     | complex.h — not implemented |
| fenv        | fenv.h — not implemented |
| grp         | group DB — not implemented |
| pwd         | password DB — not implemented |
| utmpx       | login records — not implemented |

---

## To fix — header exists, failures are incomplete declarations/definitions

| Directory  | Header(s)          | Failures | Notes |
|------------|--------------------|----------|-------|
| ctype      | ctype.h (missing)  | missing_header | need to add ctype.h |
| dirent     | dirent.h           | unknown_type, undeclared | DIR, dirent struct incomplete |
| fcntl      | fcntl.h            | compile_error, undeclared | openat, posix_fadvise, posix_fallocate |
| inttypes   | inttypes.h (missing)| missing_header | need to add inttypes.h |
| libgen     | libgen.h (missing) | missing_header | basename/dirname |
| regex      | regex.h (missing)  | missing_header | regcomp/regexec |
| setjmp     | setjmp.h (missing) | missing_header | need to add setjmp.h |
| signal     | signal.h           | unknown_type, compile_error | siginfo_t, sigset_t incomplete |
| stdio      | stdio.h            | compile_error | FILE, printf family incomplete |
| stdlib     | stdlib.h           | compile_error, unknown_type | div_t, ldiv_t, wchar types missing |
| strings    | strings.h          | compile_error | ffs/ffsl/ffsll, locale variants |
| sys_mman   | sys/mman.h         | compile_error | mmap, mprotect, shm_open |
| sys_stat   | sys/stat.h         | compile_error, undeclared | chmod, fstatat, utimensat |
| sys_wait   | sys/wait.h         | compile_error | wait, waitpid, waitid |
| time       | time.h             | compile_error, unknown_type | struct tm, clockid_t, timer_t |
| unistd     | unistd.h           | compile_error, undeclared | large — most POSIX unistd functions |
