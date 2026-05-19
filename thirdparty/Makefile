# Userland ports and tests -> build/rootfs/bin/ (initramfs staging).
ROOT := ..
CC := riscv64-unknown-elf-gcc
CFLAGS := -gdwarf-4 -Wall -Werror -Os -ffreestanding -fno-builtin -nostdlib -nostdinc \
	-isystem $(shell $(CC) -print-file-name=include) -mcmodel=medany \
	-march=rv64imac_zicsr_zifencei -mabi=lp64 -ffunction-sections -fdata-sections \
	-fno-tree-switch-conversion -I$(ROOT)/libc/include

CRT := $(ROOT)/build/libc/crt.S.o
LIBC := $(ROOT)/build/libc.a
BINDIR := $(ROOT)/build/rootfs/bin

RC_TESTS := test_syscalls test_shell test_signals stacktest stacktest2 test_rlimit posix_compliance \
	testjobs testsignal test_dprintf test_long_wait test_mmap test_pipe test_sleep test-kernel \
	test_devnull test_oom

.PHONY: install os-tests-sortix-clone os-tests-build os-tests-install

install: $(LIBC) $(CRT) $(RC_TESTS:%=$(BINDIR)/%) os-tests-install

$(LIBC) $(CRT):
	$(MAKE) -C $(ROOT) build/libc.a build/libc/crt.S.o

$(BINDIR)/test_syscalls:    test_syscalls/test_syscalls.c $(CRT) $(LIBC)
$(BINDIR)/test_shell:       test_shell/test_shell.c $(CRT) $(LIBC)
$(BINDIR)/test_signals:     test_signals/test_signals.c $(CRT) $(LIBC)
$(BINDIR)/stacktest:        stacktest/stacktest.c $(CRT) $(LIBC)
$(BINDIR)/stacktest2:       stacktest2/stacktest.c $(CRT) $(LIBC)
$(BINDIR)/test_rlimit:      test_rlimit/test_rlimit.c $(CRT) $(LIBC)
$(BINDIR)/posix_compliance: posix_compliance.c $(CRT) $(LIBC)
$(BINDIR)/testjobs:         testjobs/testjobs.c $(CRT) $(LIBC)
$(BINDIR)/testsignal:       testsignal/testsignal.c $(CRT) $(LIBC)
$(BINDIR)/test_dprintf:     test_dprintf/test_dprintf.c $(CRT) $(LIBC)
$(BINDIR)/test_long_wait:   test_long_wait/test_long_wait.c $(CRT) $(LIBC)
$(BINDIR)/test_mmap:        test_mmap/test_mmap.c $(CRT) $(LIBC)
$(BINDIR)/test_pipe:        test_pipe/test_pipe.c $(CRT) $(LIBC)
$(BINDIR)/test_sleep:       test_sleep/test_sleep.c $(CRT) $(LIBC)
$(BINDIR)/test-kernel:      test-kernel/test-kernel.c $(CRT) $(LIBC)
$(BINDIR)/test_devnull:     test_devnull/test_devnull.c $(CRT) $(LIBC)
$(BINDIR)/test_oom:         test_oom/test_oom.c $(CRT) $(LIBC)

$(RC_TESTS:%=$(BINDIR)/%):
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $(CRT) $(filter %.c,$^) $(LIBC) -Wl,--gc-sections -o $@

# os-test (Sortix basic suite)
OSTEST_TMP ?= /tmp/os-test
OSTEST_URL ?= https://gitlab.com/sortix/os-test.git
OSTEST_DIR := os-tests-sortix
OSTEST_TESTS ?= stdio sys_wait signal fcntl sys_stat sys_mman unistd stdlib time
#sys_wait signal strings dirent fcntl sys_stat
OSTEST_BASE := $(ROOT)/thirdparty/os-tests-sortix/ostest
OSTEST_INSTALL_DIR := $(ROOT)/build/rootfs/bin/os-tests-sortix

# Dropped after vendor — not applicable to sbunix (see thirdparty/OSTEST_STATUS.md).
OSTEST_PRUNE_DIRS := \
	pthread threads spawn semaphore sys_sem \
	arpa_inet netinet_in net_if netdb sys_socket \
	math complex fenv \
	grp pwd utmpx \
	aio libintl dlfcn \
	fmtmsg fnmatch ftw glob iconv \
	langinfo locale monetary nl_types \
	mqueue ndbm \
	sched search \
	sys_ipc sys_msg sys_shm \
	sys_resource sys_statvfs sys_times sys_utsname sys_uio \
	syslog termios \
	uchar wchar wctype wordexp \
	devctl endian \
	strings/strcasecmp_l.c strings/strncasecmp_l.c \
	signal/sigaltstack.c \
	signal/psiginfo.c \
	signal/pthread_kill.c \
	signal/pthread_sigmask.c \
	signal/sig2str.c \
	signal/sigqueue.c \
	signal/sigtimedwait.c \
	signal/sigwaitinfo.c \
	signal/str2sig.c \
	time/timer_create.c \
	time/timer_delete.c \
	time/timer_getoverrun.c \
	time/timer_gettime.c \
	time/timer_settime.c \
	stdio/ctermid.c \
	stdio/setvbuf.c \
	stdio/tmpnam.c \
	stdio/fflush.c \
	stdlib/atof.c \
	stdlib/drand48.c \
	stdlib/erand48.c \
	stdlib/grantpt.c \
	stdlib/jrand48.c \
	stdlib/lcong48.c \
	stdlib/lrand48.c \
	stdlib/mrand48.c \
	stdlib/nrand48.c \
	stdlib/posix_openpt.c \
	stdlib/ptsname.c \
	stdlib/ptsname_r.c \
	stdlib/seed48.c \
	stdlib/srand48.c \
	stdlib/strtod.c \
	stdlib/strtof.c \
	stdlib/strtold.c \
	stdlib/unlockpt.c \
	sys_mman/mlock.c \
	sys_mman/mlockall.c \
	sys_mman/munlock.c \
	sys_mman/munlockall.c \
	sys_mman/shm_open.c \
	sys_mman/shm_unlink.c \
	sys_stat/futimens.c \
	sys_stat/mkfifo.c \
	sys_stat/mkfifoat.c \
	sys_stat/mknod.c \
	sys_stat/mknodat.c \
	sys_stat/utimensat.c \
	time/difftime.c \
	time/getdate.c \
	unistd/access.c \
	unistd/alarm.c \
	unistd/chown.c \
	unistd/crypt.c \
	unistd/encrypt.c \
	unistd/faccessat.c \
	unistd/fchown.c \
	unistd/fchownat.c \
	unistd/getgroups.c \
	unistd/getlogin.c \
	unistd/getlogin_r.c \
	unistd/getresgid.c \
	unistd/getresuid.c \
	unistd/getsid.c \
	unistd/lchown.c \
	unistd/lockf.c \
	unistd/nice.c \
	unistd/setegid.c \
	unistd/seteuid.c \
	unistd/setgid.c \
	unistd/setregid.c \
	unistd/setresgid.c \
	unistd/setresuid.c \
	unistd/setreuid.c \
	unistd/setsid.c \
	unistd/setuid.c \
	unistd/tcgetpgrp.c \
	unistd/tcsetpgrp.c \
	unistd/ttyname.c \
	unistd/ttyname_r.c \
	sys_time/utimes.c

# Clone upstream (if needed) and vendor basic/ + misc/ (gitignored under os-tests-sortix/).
os-tests-sortix-clone:
	@if [ ! -d "$(OSTEST_TMP)/.git" ]; then \
		git clone "$(OSTEST_URL)" "$(OSTEST_TMP)"; \
	fi
	rm -rf "$(OSTEST_DIR)/basic" "$(OSTEST_DIR)/misc"
	cp -a "$(OSTEST_TMP)/basic" "$(OSTEST_TMP)/misc" "$(OSTEST_DIR)/"
	@for d in $(OSTEST_PRUNE_DIRS); do rm -rf "$(OSTEST_DIR)/basic/$$d"; done
	@echo "os-tests-sortix-clone: pruned $(words $(OSTEST_PRUNE_DIRS)) dirs (see OSTEST_STATUS.md)"

# Cross-compile: OSTEST_TESTS empty = all basic/; else subdir or path, e.g. OSTEST_TESTS="string"
os-tests-build: os-tests-sortix-clone $(LIBC) $(CRT)
	@test -d "$(OSTEST_DIR)/basic" || (echo "run: make -C thirdparty os-tests-sortix"; exit 1)
	chmod +x "$(OSTEST_DIR)/build-basic.sh"
	ROOT="$(abspath $(ROOT))" OSTEST_SRC="$(abspath $(OSTEST_DIR))" \
		OSTEST_BASE="$(abspath $(OSTEST_BASE))" OSTEST_TESTS="$(OSTEST_TESTS)" \
		sh "$(OSTEST_DIR)/build-basic.sh"

# Install last successful build -> build/rootfs/bin/os-tests-sortix/
os-tests-install: os-tests-build
	chmod +x "$(OSTEST_DIR)/install-basic.sh"
	ROOT="$(abspath $(ROOT))" OSTEST_SRC="$(abspath $(OSTEST_DIR))" \
		OSTEST_BASE="$(abspath $(OSTEST_BASE))" \
		OSTEST_INSTALL_DIR="$(abspath $(OSTEST_INSTALL_DIR))" \
		sh "$(OSTEST_DIR)/install-basic.sh"
	chmod +x gen-run-tests.sh
	ROOT="$(abspath $(ROOT))" OSTEST_BASE="$(abspath $(OSTEST_BASE))" \
		OSTEST_INSTALL_DIR="$(abspath $(OSTEST_INSTALL_DIR))" \
		sh gen-run-tests.sh
