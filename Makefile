ARCH 			= aarch64
ARCHDIR 		= arch/$(ARCH)
CC      		= aarch64-elf-gcc
LD      		= aarch64-elf-ld

BUILD			= build/$(ARCH)
CFLAGS  		= -O3 -ffreestanding -nostdlib -Iinclude -Wall -Wextra # -Werror
LDFLAGS 		= -T $(ARCHDIR)/linker.ld

TARGET			= $(BUILD)/kernel.elf

KERNEL_C_SRCS 	:= $(shell find kernel -name "*.c")
PLATFORM_C_SRCS := $(shell find platform -name "*.c")
ARCH_C_SRCS 	:= $(shell find $(ARCHDIR) -name "*.c")
ARCH_S_SRCS		:= $(shell find $(ARCHDIR) -name "*.S")

ALL_C_SRCS 		:= $(KERNEL_C_SRCS) $(PLATFORM_C_SRCS) $(ARCH_C_SRCS)

OBJS 			:= $(patsubst %.c,$(BUILD)/%.o,$(ALL_C_SRCS))
OBJS 			+= $(patsubst %.S,$(BUILD)/%.o,$(ARCH_S_SRCS)) 

all: $(TARGET)

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

# Link
$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

run: all
	qemu-system-aarch64 -machine virt,dumpdtb=virt.dtb -nographic
	qemu-system-aarch64 \
		-machine virt \
		-cpu cortex-a72 \
		-m 1024 \
		-kernel $(TARGET) \
		-device loader,file=virt.dtb,addr=0x41000000 \
		-nographic

clean:
	rm -rf $(BUILD)
