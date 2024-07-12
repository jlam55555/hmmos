# Shared Makefile for the bootloader and kernel. They may share some
# common objects, so it makes some sense to share a Makefile, although
# some parts may be repetitive.
SRC_DIR:=src
OUT_DIR:=out

BOOT_SRC_DIR:=$(SRC_DIR)/boot
KERNEL_SRC_DIR:=$(SRC_DIR)/kernel
COMMON_SRC_DIR:=$(SRC_DIR)/common

AS:=as
ASFLAGS:=--32 --fatal-warnings
CC:=clang
CXX:=clang++
# Flags common to C/C++. These should match the flags in .clangd.
_CFLAGS:=\
	-m32 \
	-ffreestanding \
	-fno-pie \
	-Werror \
	-MMD \
	-MP \
	-I$(COMMON_SRC_DIR)
CFLAGS:=$(_CFLAGS) -std=c17
CXXFLAGS:=$(_CFLAGS) -std=c++2a -fno-rtti
QEMU_FLAGS:=-m 4G

# libgcc contains some useful logic that may be used implicitly by
# gcc/clang (e.g., __divdi3 for unsigned long long operations on a
# 32-bit architecture that doesn't support these operations
# natively). It isn't linked by default if `-ffreestanding` or
# `-nostdlib` is provided
#
# libgcc.a is in a location known to the compiler but not the linker,
# so we need to query the compiler for it.
LIBGCC:=$(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

LD:=ld.lld
LDFLAGS:=\
	-m elf_i386 \
	--oformat=binary \
	--build-id=none \
	-nostdlib
# This needs to go at the end of the `ld` invocation (at least on GNU ld):
# https://forum.osdev.org/viewtopic.php?p=340465&sid=963812bf6b3cd15a63c74d35d7d368fd#p340465
LDLIBS:=$(LIBGCC)

################################################################################
# Bootloader-specific config
################################################################################
BOOT_LINKER_SCRIPT:=$(BOOT_SRC_DIR)/linker.ld
BOOT_LDFLAGS:=$(LDFLAGS) \
	-T$(BOOT_LINKER_SCRIPT)

BOOTLOADER:=$(OUT_DIR)/boot.bin
BOOTABLE_DISK:=$(OUT_DIR)/disk.bin

BOOT_SRCS:=$(shell find $(BOOT_SRC_DIR) $(COMMON_SRC_DIR) -name *.[cS])
_BOOT_OBJS:=$(BOOT_SRCS:$(SRC_DIR)/%.c=$(OUT_DIR)/%.o)
BOOT_OBJS:=$(_BOOT_OBJS:$(SRC_DIR)/%.S=$(OUT_DIR)/%.o)

################################################################################
# Kernel-specific config
################################################################################
KERNEL_LINKER_SCRIPT:=$(KERNEL_SRC_DIR)/linker.ld
KERNEL_LDFLAGS:=$(LDFLAGS) \
	-T$(KERNEL_LINKER_SCRIPT)
KERNEL:=$(OUT_DIR)/kernel.bin

KERNEL_SRCS:=$(shell \
	find $(KERNEL_SRC_DIR) $(COMMON_SRC_DIR) -name *.[cS] -o -name *.cc)
_KERNEL_OBJS:=$(KERNEL_SRCS:$(SRC_DIR)/%.cc=$(OUT_DIR)/%.o)
__KERNEL_OBJS:=$(_KERNEL_OBJS:$(SRC_DIR)/%.c=$(OUT_DIR)/%.o)
KERNEL_OBJS:=$(__KERNEL_OBJS:$(SRC_DIR)/%.S=$(OUT_DIR)/%.o)

################################################################################
# Build options
################################################################################
ifneq ($(DEBUG),)
override QEMU_FLAGS+=-no-reboot -no-shutdown
endif

ifneq ($(SHOWINT),)
override QEMU_FLAGS+=-d int,cpu_reset
endif

# Make sure to recompile the bootloader and modify the kernel link
# script accordingly after changing this value.
ifneq ($(KERNEL_LOAD_ADDR),)
override CFLAGS+=-DKERNEL_LOAD_ADDR=$(KERNEL_LOAD_ADDR)
endif

# Build with GNU toolchain rather than LLVM. Assembly (*.S files) are
# always built with GNU as b/c there's no LLVM equivalent.
ifneq ($(GNU),)
override CC:=gcc
override CXX:=g++
override LD:=ld
endif

# Custom optimization level.
ifneq ($(OPT),)
override CFLAGS+=-O$(OPT)
else
override CFLAGS+=-O0
endif

################################################################################
# Build rules
################################################################################
all: $(BOOTABLE_DISK)

ENSURE_DIR=mkdir -p $(dir $@)

$(OUT_DIR)/%.o: $(SRC_DIR)/%.S
	@$(ENSURE_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(OUT_DIR)/%.o: $(SRC_DIR)/%.c
	@$(ENSURE_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_DIR)/%.o: $(SRC_DIR)/%.cc
	@$(ENSURE_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BOOTLOADER): $(BOOT_OBJS) $(BOOT_LINKER_SCRIPT)
	$(LD) $(BOOT_LDFLAGS) $(BOOT_OBJS) -o $@ $(LDLIBS)

$(KERNEL): $(KERNEL_OBJS) $(KERNEL_LINKER_SCRIPT)
	$(LD) $(KERNEL_LDFLAGS) $(KERNEL_OBJS) -o $@ $(LDLIBS)

$(BOOTABLE_DISK): $(BOOTLOADER) $(KERNEL)
	scripts/install_bootloader.py -b $(BOOTLOADER) -k $(KERNEL) -o $@

.PHONY: run
run: $(BOOTABLE_DISK)
	qemu-system-i386 $(QEMU_FLAGS) -drive format=raw,file=$<

.PHONY: clean
clean:
	rm -rf $(OUT_DIR)

DEPS:=$(BOOT_OBJS:.o=.d) $(KERNEL_OBJS:.o=.d)
-include $(DEPS)
