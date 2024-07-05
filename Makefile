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
LD:=ld.lld
CC:=clang
CFLAGS:=-m32 -ffreestanding -O0 -fno-pie -Werror -I$(COMMON_SRC_DIR)
QEMU_FLAGS:=-m 4G

################################################################################
# Bootloader-specific config
################################################################################
BOOT_LINKER_SCRIPT:=$(BOOT_SRC_DIR)/linker.ld
BOOT_LDFLAGS:=--oformat=binary \
	--build-id=none \
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
KERNEL_LDFLAGS:=--oformat=binary \
	--build-id=none \
	-T$(KERNEL_LINKER_SCRIPT)
KERNEL:=$(OUT_DIR)/kernel.bin

KERNEL_SRCS:=$(shell find $(KERNEL_SRC_DIR) $(COMMON_SRC_DIR) -name *.[cS])
_KERNEL_OBJS:=$(KERNEL_SRCS:$(SRC_DIR)/%.c=$(OUT_DIR)/%.o)
KERNEL_OBJS:=$(_KERNEL_OBJS:$(SRC_DIR)/%.S=$(OUT_DIR)/%.o)

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

################################################################################
# Build rules
################################################################################
all: $(BOOTABLE_DISK)

$(OUT_DIR)/%.o: $(SRC_DIR)/%.S
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $^ -o $@

# TODO: support different build variants (e.g., with different
# optimization levels and with gcc) to make sure things work outside
# of the default build.
#
# TODO: support automatic header dependencies
$(OUT_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $^ -o $@

$(BOOTLOADER): $(BOOT_OBJS) $(BOOT_LINKER_SCRIPT)
	mkdir -p $(dir $@)
	$(LD) $(BOOT_LDFLAGS) $(BOOT_OBJS) -o $@

$(KERNEL): $(KERNEL_OBJS) $(KERNEL_LINKER_SCRIPT)
	mkdir -p $(dir $@)
	$(LD) $(KERNEL_LDFLAGS) $(KERNEL_OBJS) -o $@

$(BOOTABLE_DISK): $(BOOTLOADER) $(KERNEL)
	scripts/install_bootloader.py -b $(BOOTLOADER) -k $(KERNEL) -o $@

.PHONY: run
run: $(BOOTABLE_DISK)
	qemu-system-i386 $(QEMU_FLAGS) -drive format=raw,file=$<

.PHONY: clean
clean:
	rm -r $(OUT_DIR)
