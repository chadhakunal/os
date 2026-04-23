KLANG ?= c
SUBMIT_DIR ?= /submit

CROSS   := riscv64-unknown-elf-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
AR      := $(CROSS)ar
OBJCOPY := $(CROSS)objcopy
CFLAGS  := -gdwarf-4 -Wall -Werror -Os -ffreestanding -fno-builtin -nostdlib -nostdinc -isystem $(shell $(CC) -print-file-name=include) -mcmodel=medany -march=rv64imac_zicsr_zifencei -mabi=lp64 -ffunction-sections -fdata-sections -fno-tree-switch-conversion
RUSTFLAGS := --target riscv64imac-unknown-none-elf -C panic=abort -C linker=$(CC) -C code-model=medium -C link-arg=-nostartfiles -C link-arg=-march=rv64imac -C link-arg=-mabi=lp64

KERN_ASM := $(patsubst %,build/%.o,$(wildcard kernel/*.S))
LIBC_OBJ := $(patsubst %,build/%.o,$(filter-out libc/crt.S,$(wildcard libc/*.c libc/*.S)))
C_BIN    := $(addprefix build/rootfs/bin/,$(shell ls -d bin/*/*.c 2>/dev/null | cut -d/ -f2 | sort -u))
RS_BIN   := $(addprefix build/rootfs/bin/,$(shell ls -d bin/*/*.rs 2>/dev/null | cut -d/ -f2 | sort -u))
USER_BIN := $(C_BIN) $(RS_BIN)

# Language-specific kernel object/library
ifeq ($(KLANG),c)
  KERN_C   := $(patsubst %,build/%.o,$(shell find kernel -name '*.c'))
  KERN_ALL := $(KERN_ASM) $(KERN_C)
else ifeq ($(KLANG),rust)
  KERN_LIB := build/rust/riscv64imac-unknown-none-elf/debug/libsbunix.a
  KERN_ALL := $(KERN_ASM) $(KERN_LIB)
else ifeq ($(KLANG),zig)
  KERN_LIB := zig-out/lib/libsbunix.a
  KERN_ALL := $(KERN_ASM) $(KERN_LIB)
endif

DISK_SIZE ?= 16
DISK_IMG  := build/disk.img

-include Makefile.local

all: build/kernel.elf

build/kernel/%.c.o: kernel/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Ikernel/include -c $< -o $@

build/kernel/%.S.o: kernel/%.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Ikernel/include -c $< -o $@

build/libc/%.c.o: libc/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Ilibc/include -c $< -o $@

build/libc/%.S.o: libc/%.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Ilibc/include -c $< -o $@

build/libc.a: $(LIBC_OBJ)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

# Rust kernel build
build/rust/riscv64imac-unknown-none-elf/debug/libsbunix.a: $(wildcard kernel/*.rs) Cargo.toml
	cargo build

# Zig kernel build
zig-out/lib/libsbunix.a: $(wildcard kernel/*.zig) build.zig
	zig build -Doptimize=ReleaseSafe

.SECONDEXPANSION:
$(C_BIN): build/rootfs/bin/%: $$(wildcard bin/%/*.c) build/libc/crt.S.o build/libc.a
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Ilibc/include build/libc/crt.S.o bin/$*/*.c build/libc.a -Wl,--gc-sections -o $@

$(RS_BIN): build/rootfs/bin/%: $$(wildcard bin/%/*.rs) build/libc/crt.S.o build/libc.a
	@mkdir -p $(@D)
	rustc $(RUSTFLAGS) -C link-arg=build/libc/crt.S.o -C link-arg=build/libc.a bin/$*/$*.rs -o $@

build/tarfs.o: $(USER_BIN)
	cp -a rootfs/. build/rootfs/
	tar cf build/rootfs.tar -C build/rootfs .
	cd build && $(OBJCOPY) -I binary -O elf64-littleriscv -B riscv \
		--rename-section .data=.tarfs \
		--redefine-sym _binary_rootfs_tar_start=_tarfs_start \
		--redefine-sym _binary_rootfs_tar_end=_tarfs_end \
		--redefine-sym _binary_rootfs_tar_size=_tarfs_size \
		rootfs.tar tarfs.o

build/kernel.elf: $(KERN_ALL) build/tarfs.o
	$(LD) -T kernel/kernel.ld $(KERN_ALL) build/tarfs.o -o $@

thirdparty: build/libc/crt.S.o build/libc.a
	$(MAKE) -C thirdparty install
	@rm -f build/tarfs.o
	$(MAKE) build/kernel.elf

build/tools/mkfs: tools/mkfs.c
	@mkdir -p $(@D)
	gcc -Wall -o $@ $<

$(DISK_IMG): build/tools/mkfs
	build/tools/mkfs $@ $(DISK_SIZE)

qemu: build/kernel.elf $(DISK_IMG)
	qemu-system-riscv64 -machine virt -bios default -kernel $< \
		-drive file=$(DISK_IMG),format=raw,if=none,id=hd0 \
		-device virtio-blk-pci-non-transitional,drive=hd0 \
		-device virtio-gpu-pci \
		-device virtio-net-pci-non-transitional,netdev=net0 \
		-netdev user,id=net0 \
		-nographic

clean:
	rm -rf build zig-out .zig-cache

submit:
	@set -e; \
	STMP=$$(mktemp -d /tmp/sbunix-submit.XXXXXX); \
	cleanup() { set +e; chmod -R u+w "$$STMP" 2>/dev/null; rm -rf "$$STMP" 2>/dev/null; }; \
	trap cleanup EXIT; \
	rsync -a \
		--exclude='.git' --exclude='build/' --exclude='zig-out/' \
		--exclude='.zig-cache/' --exclude='.claude/' --exclude='thirdparty/' \
		--max-size=100K \
		. "$$STMP/"; \
	cd "$$STMP"; \
	git init -q; \
	git add -A; \
	git -c user.name=student -c user.email=student@sbunix commit -q -m "submit"; \
	rm -rf $(SUBMIT_DIR)/sbunix.2 2>/dev/null || true; \
	mv $(SUBMIT_DIR)/sbunix.1 $(SUBMIT_DIR)/sbunix.2 2>/dev/null || true; \
	mv $(SUBMIT_DIR)/sbunix $(SUBMIT_DIR)/sbunix.1 2>/dev/null || true; \
	mkdir -p $(SUBMIT_DIR)/sbunix; \
	git archive HEAD | tar -x -C $(SUBMIT_DIR)/sbunix; \
	echo "Submitted to $(SUBMIT_DIR)/sbunix"

.PRECIOUS: build/%.S.o build/%.c.o
.PHONY: all qemu clean thirdparty submit
