ARCH 			= aarch64
ARCHDIR 		= arch/$(ARCH)
CROSS			= aarch64-elf-

CC      		= $(CROSS)gcc
LD      		= $(CROSS)ld
OBJCOPY 		= $(CROSS)objcopy
AR 				= $(CROSS)ar

BUILD			= build/$(ARCH)
CFLAGS  		= -O3 -ffreestanding -nostdlib -Iinclude/arch/$(ARCH) -Iinclude -Wall -Wextra -fPIC -mgeneral-regs-only # -Werror
LDFLAGS 		= -T $(ARCHDIR)/linker.ld
OBJCOPY_FORMAT	= elf64-littleaarch64
OBJCOPY_ARCH	= aarch64

TARGET			= $(BUILD)/kernel.elf

KERNEL_C_SRCS 	:= $(shell find kernel -name "*.c")
PLATFORM_C_SRCS := $(shell find platform -name "*.c")
LIB_C_SRCS		:= $(shell find lib -name "*.c")
ARCH_C_SRCS 	:= $(shell find $(ARCHDIR) -name "*.c")
ARCH_S_SRCS		:= $(shell find $(ARCHDIR) -name "*.S")

ALL_C_SRCS 		:= $(KERNEL_C_SRCS) $(PLATFORM_C_SRCS) $(ARCH_C_SRCS) $(LIB_C_SRCS)

OBJS 			:= $(patsubst %.c,$(BUILD)/%.o,$(ALL_C_SRCS))
OBJS 			+= $(patsubst %.S,$(BUILD)/%.o,$(ARCH_S_SRCS))

LIBC_C_SRCS		:= $(wildcard libc/*.c)
LIBC_S_SRCS		:= $(wildcard libc/arch/$(ARCH)/*.S)
LIBC_OBJ		:= $(patsubst libc/%,$(BUILD)/libc/%.o,$(LIBC_C_SRCS)) \
				   $(patsubst libc/arch/$(ARCH)/%,$(BUILD)/libc/%.o,$(LIBC_S_SRCS))

BIN_DIRS		:= $(wildcard bin/*)
BIN_NAMES		:= $(notdir $(BIN_DIRS))
USER_BINS		:= $(addprefix $(BUILD)/rootfs/bin/,$(BIN_NAMES))

all: $(TARGET) $(USER_BINS)

$(BUILD):
	mkdir -p $(BUILD)

# Compile C
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile ASM
$(BUILD)/%.o: %.S | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@

$(BUILD)/libc/%.c.o: libc/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Ilibc/include -c $< -o $@

$(BUILD)/libc/%.S.o: libc/%.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Ilibc/include -c $< -o $@

$(BUILD)/libc/%.S.o: libc/arch/$(ARCH)/%.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Ilibc/include -c $< -o $@

$(BUILD)/libc.a: $(LIBC_OBJ)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

.SECONDEXPANSION:
$(BUILD)/rootfs/bin/%: $$(wildcard bin/%/*.c) $(BUILD)/libc/crt.S.o $(BUILD)/libc.a
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Ilibc/include $(BUILD)/libc/crt.S.o $< $(BUILD)/libc.a -o $@

$(BUILD)/tarfs.o: $(USER_BINS)
	cp -a rootfs/. $(BUILD)/rootfs/
	COPYFILE_DISABLE=1 tar --format=ustar -cf $(BUILD)/rootfs.tar -C $(BUILD)/rootfs .
	cd $(BUILD) && $(OBJCOPY) -I binary -O $(OBJCOPY_FORMAT) -B $(OBJCOPY_ARCH) \
		--rename-section .data=.tarfs \
		--redefine-sym _binary_rootfs_tar_start=_tarfs_start \
		--redefine-sym _binary_rootfs_tar_end=_tarfs_end \
		--redefine-sym _binary_rootfs_tar_size=_tarfs_size \
		rootfs.tar tarfs.o

# Link
$(TARGET): $(OBJS) $(BUILD)/tarfs.o
	$(LD) $(LDFLAGS) $(OBJS) $(BUILD)/tarfs.o -o $@

virt.dtb: 
	qemu-system-aarch64 \
		-machine virt,dumpdtb=virt.dtb \
		-cpu cortex-a72 \
		-m 1024

run: all virt.dtb
	qemu-system-aarch64 \
		-machine virt \
		-cpu cortex-a72 \
		-m 1024 \
		-kernel $(TARGET) \
		-device loader,file=virt.dtb,addr=0x41000000 \
		-nographic

.PRECIOUS: $(BUILD)/libc/crt.S.o

clean:
	rm -rf $(BUILD)
	rm -rf virt.dtb
