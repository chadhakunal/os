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

RC_TESTS := test_syscalls test_shell test_signals stacktest stacktest2 test_rlimit

.PHONY: install os-tests-sortix-clone os-tests-build os-tests-install

install: $(LIBC) $(CRT) $(RC_TESTS:%=$(BINDIR)/%) os-tests-install

$(LIBC) $(CRT):
	$(MAKE) -C $(ROOT) build/libc.a build/libc/crt.S.o

$(BINDIR)/test_syscalls: test_syscalls/test_syscalls.c $(CRT) $(LIBC)
$(BINDIR)/test_shell: test_shell/test_shell.c $(CRT) $(LIBC)
$(BINDIR)/test_signals: test_signals/test_signals.c $(CRT) $(LIBC)
$(BINDIR)/stacktest: stacktest/stacktest.c $(CRT) $(LIBC)
$(BINDIR)/stacktest2: stacktest2/stacktest.c $(CRT) $(LIBC)
$(BINDIR)/test_rlimit: test_rlimit/test_rlimit.c $(CRT) $(LIBC)

$(RC_TESTS:%=$(BINDIR)/%):
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $(CRT) $(filter %.c,$^) $(LIBC) -Wl,--gc-sections -o $@

# os-test (Sortix basic suite)
OSTEST_TMP ?= /tmp/os-test
OSTEST_URL ?= https://gitlab.com/sortix/os-test.git
OSTEST_DIR := os-tests-sortix
OSTEST_TESTS ?= time
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
	poll sys_select sys_time \
	sched search \
	sys_ipc sys_msg sys_shm \
	sys_resource sys_statvfs sys_times sys_utsname sys_uio \
	syslog termios \
	uchar wchar wctype wordexp \
	devctl endian \
	strings/strcasecmp_l strings/strncasecmp_l \
	signal/sigaltstack \
	signal/psiginfo \
	signal/pthread_kill \
	signal/pthread_sigmask \
	signal/sig2str \
	signal/sigqueue \
	signal/sigtimedwait \
	signal/sigwaitinfo \
	signal/str2sig \
	time/timer_getoverrun

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
