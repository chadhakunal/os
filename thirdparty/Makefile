# Userland ports and tests installed into build/rootfs/bin/ (initramfs staging).
# Use: make thirdparty
ROOT := ..
CC := riscv64-unknown-elf-gcc
CFLAGS := -gdwarf-4 -Wall -Werror -Os -ffreestanding -fno-builtin -nostdlib -nostdinc \
	-isystem $(shell $(CC) -print-file-name=include) -mcmodel=medany \
	-march=rv64imac_zicsr_zifencei -mabi=lp64 -ffunction-sections -fdata-sections \
	-fno-tree-switch-conversion -I$(ROOT)/libc/include

CRT := $(ROOT)/build/libc/crt.S.o
LIBC := $(ROOT)/build/libc.a
BINDIR := $(ROOT)/build/rootfs/bin

# Programs run from rootfs/etc/rc at boot.
RC_TESTS := test_syscalls test_shell test_signals stacktest stacktest2

.PHONY: install
install: $(LIBC) $(CRT) $(RC_TESTS:%=$(BINDIR)/%)

$(LIBC) $(CRT):
	$(MAKE) -C $(ROOT) build/libc.a build/libc/crt.S.o

$(BINDIR)/test_syscalls: test_syscalls/test_syscalls.c $(CRT) $(LIBC)
$(BINDIR)/test_shell: test_shell/test_shell.c $(CRT) $(LIBC)
$(BINDIR)/test_signals: test_signals/test_signals.c $(CRT) $(LIBC)
$(BINDIR)/stacktest: stacktest/stacktest.c $(CRT) $(LIBC)
$(BINDIR)/stacktest2: stacktest2/stacktest.c $(CRT) $(LIBC)

$(RC_TESTS:%=$(BINDIR)/%):
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $(CRT) $(filter %.c,$^) $(LIBC) -Wl,--gc-sections -o $@
