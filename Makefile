## Shared Makefile for the bootloader and kernel. They may share some
## common objects, so it makes some sense to share a Makefile, although
## some parts may be repetitive.
##
## The order of sections in this makefile is significant:
##
## 1. General flags: specify defaults
## 2. Build options: builds $(OUT_DIR) and adds to default flags based
##    on flags passed to `make`
## 3. Bootloader/kernel files: This depends on $(OUT_DIR).
## 4. Build rules.
## 5. Special (phony) targets and automatic header dependencies.

################################################################################
# General flags
################################################################################
SRC_DIR:=src

COMMON_SRC_DIR:=$(SRC_DIR)/common
BOOT_SRC_DIR:=$(SRC_DIR)/boot
KERNEL_SRC_DIR:=$(SRC_DIR)/kernel
KERNEL_TEST_SRC_DIR:=$(SRC_DIR)/test

ARCH:=x86

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
# CFLAGS are essentially for bootloader, and CXXFLAGS for the kernel.
CFLAGS:=$(_CFLAGS) -std=c17
CXXFLAGS:=$(_CFLAGS) \
	-std=c++2a \
	-fno-rtti \
	-fno-exceptions \
	-I$(KERNEL_SRC_DIR) \
	-I$(KERNEL_SRC_DIR)/arch/$(ARCH)
QEMU_FLAGS:=-m 4G -M q35

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
	--build-id=none \
	-nostdlib
# This needs to go at the end of the `ld` invocation (at least on GNU ld):
# https://forum.osdev.org/viewtopic.php?p=340465
LDLIBS:=$(LIBGCC)

################################################################################
# Build options
################################################################################

# Parameters that change build flags will also create a new build
# variant (i.e. builds to a different target directory). Each
# combination of (DEBUG, GNU, OPT) flags creates a new build variant.
# Building with debug symbols for GDB also creates a new build
# variant.
OUT_DIR:=out

ifneq ($(TEST),)
TEST_INPUT:=echo " $(TEST)" |
RUN_TARGET=$(BOOTABLE_DISK_TEST)
KERNEL_TARGET_ELF_WITH_SYMBOLS=$(KERNEL_TEST_ELF_WITH_SYMBOLS)
override QEMU_FLAGS+=-display none -serial stdio
else
RUN_TARGET=$(BOOTABLE_DISK)
KERNEL_TARGET_ELF_WITH_SYMBOLS=$(KERNEL_ELF_WITH_SYMBOLS)
# QEMU will write to the console. Exit with `C-a x`.
# override QEMU_FLAGS+=-nographic
override QEMU_FLAGS+=-serial stdio
endif

ifneq ($(DEBUG),)
# Setting DEBUG enables debug assertions. This does not add debug
# symbols (which you get by adding -g).
override OUT_DIR:=$(OUT_DIR).debug
override CFLAGS+=-DDEBUG
override CXXFLAGS+=-DDEBUG
endif

# Make sure to recompile the bootloader and modify the kernel link
# script accordingly after changing this value.
#
# Technically this should also be a different build variant since this
# changes compile flags.
ifneq ($(KERNEL_LOAD_ADDR),)
override CFLAGS+=-DKERNEL_LOAD_ADDR=$(KERNEL_LOAD_ADDR)
endif

# Build with GNU toolchain rather than LLVM. Assembly (*.S files) are
# always built with GNU as b/c there's no LLVM equivalent.
ifneq ($(GNU),)
override CC:=gcc
override CXX:=g++
override LD:=ld
override OUT_DIR:=$(OUT_DIR).gcc
endif

# Custom optimization level.
ifneq ($(OPT),)
override CFLAGS+=-O$(OPT)
override OUT_DIR:=$(OUT_DIR).O$(OPT)
else
override CFLAGS+=-O0
endif

ifneq ($(filter runi,$(MAKECMDGOALS)),)
# If building for gdb, we also need to build with debug symbols. Note
# that the flat binary (which we actually run in QEMU) will not have
# debug symbols, but the generated ELF files for the bootloader/kernel
# (which we load in GDB) will have the generated
override OUT_DIR:=$(OUT_DIR).symbols
override ASFLAGS+=-g
override CFLAGS+=-g
override CXXFLAGS+=-g
else ifneq ($(filter gdb,$(MAKECMDGOALS)),)
# Same thing as above.
override OUT_DIR:=$(OUT_DIR).symbols
endif

BOOTABLE_DISK:=$(OUT_DIR)/disk.bin
BOOTABLE_DISK_TEST:=$(OUT_DIR)/disk_test.bin

################################################################################
# QEMU options
################################################################################
# These don't change compile options and don't create a new build variant.

ifneq ($(SHOWINT),)
override QEMU_FLAGS+=-d int,cpu_reset -no-reboot
endif

ifneq ($(KVM),)
override QEMU_FLAGS+=-accel kvm
endif

################################################################################
# Bootloader-specific config
################################################################################
BOOT_LINKER_SCRIPT:=$(BOOT_SRC_DIR)/linker.ld
BOOT_LDFLAGS:=$(LDFLAGS) \
	-T$(BOOT_LINKER_SCRIPT)

BOOTLOADER:=$(OUT_DIR)/boot.bin
BOOTLOADER_ELF_WITH_SYMBOLS:=$(OUT_DIR)/boot.elf

BOOT_SRCS:=$(shell find $(BOOT_SRC_DIR) $(COMMON_SRC_DIR) -name *.[cS])
_BOOT_OBJS:=$(BOOT_SRCS:$(SRC_DIR)/%.c=$(OUT_DIR)/%.o)
BOOT_OBJS:=$(_BOOT_OBJS:$(SRC_DIR)/%.S=$(OUT_DIR)/%.o)

################################################################################
# Kernel-specific config
################################################################################
# C++ runtime support for the kernel (specifically global ctors/dtors).
CRTI_OBJ=$(OUT_DIR)/crt/crti.o
CRTN_OBJ=$(OUT_DIR)/crt/crtn.o

KERNEL_LINKER_SCRIPT:=$(KERNEL_SRC_DIR)/linker.ld
KERNEL_LDFLAGS:=$(LDFLAGS) \
	-T$(KERNEL_LINKER_SCRIPT)
KERNEL:=$(OUT_DIR)/kernel.bin
KERNEL_TEST:=$(OUT_DIR)/kernel_test.bin
KERNEL_FS:=$(OUT_DIR)/kernel_fs.bin
KERNEL_FS_TEST:=$(OUT_DIR)/kernel_test_fs.bin
KERNEL_ELF_WITH_SYMBOLS:=$(OUT_DIR)/kernel.elf
KERNEL_TEST_ELF_WITH_SYMBOLS:=$(OUT_DIR)/kernel_test.elf

KERNEL_SRCS:=$(shell \
	find $(KERNEL_SRC_DIR) $(COMMON_SRC_DIR) -name *.[cS] -o -name *.cc)
_KERNEL_OBJS:=$(KERNEL_SRCS:$(SRC_DIR)/%.cc=$(OUT_DIR)/%.o)
__KERNEL_OBJS:=$(_KERNEL_OBJS:$(SRC_DIR)/%.c=$(OUT_DIR)/%.o)
___KERNEL_OBJS:=$(__KERNEL_OBJS:$(SRC_DIR)/%.S=$(OUT_DIR)/%.o)
KERNEL_OBJS:=$(CRTI_OBJ) $(___KERNEL_OBJS) $(CRTN_OBJ)

KERNEL_TEST_SRCS:=\
	$(filter-out %/entry.cc,$(KERNEL_SRCS)) \
	$(shell find $(KERNEL_TEST_SRC_DIR) -name *.[cS] -o -name *.cc)
_KERNEL_TEST_OBJS:=$(KERNEL_TEST_SRCS:$(SRC_DIR)/%.cc=$(OUT_DIR)/%.o)
__KERNEL_TEST_OBJS:=$(_KERNEL_TEST_OBJS:$(SRC_DIR)/%.c=$(OUT_DIR)/%.o)
___KERNEL_TEST_OBJS:=$(__KERNEL_TEST_OBJS:$(SRC_DIR)/%.S=$(OUT_DIR)/%.o)
KERNEL_TEST_OBJS:=$(CRTI_OBJ) $(___KERNEL_TEST_OBJS) $(CRTN_OBJ)

################################################################################
# Build rules
################################################################################
all: $(BOOTABLE_DISK) $(BOOTABLE_DISK_TEST)

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
	$(LD) $(BOOT_LDFLAGS) --oformat=binary $(BOOT_OBJS) -o $@ $(LDLIBS)
$(KERNEL): $(KERNEL_OBJS) $(KERNEL_LINKER_SCRIPT)
	$(LD) $(KERNEL_LDFLAGS) --oformat=binary $(KERNEL_OBJS) -o $@ $(LDLIBS)
$(KERNEL_TEST): $(KERNEL_TEST_OBJS) $(KERNEL_LINKER_SCRIPT)
	$(LD) $(KERNEL_LDFLAGS) --oformat=binary $(KERNEL_TEST_OBJS) -o $@ $(LDLIBS)

$(BOOTLOADER_ELF_WITH_SYMBOLS): $(BOOT_OBJS) $(BOOT_LINKER_SCRIPT)
	$(LD) $(BOOT_LDFLAGS) $(BOOT_OBJS) -o $@ $(LDLIBS)
$(KERNEL_ELF_WITH_SYMBOLS): $(KERNEL_OBJS) $(KERNEL_LINKER_SCRIPT)
	$(LD) $(KERNEL_LDFLAGS) $(KERNEL_OBJS) -o $@ $(LDLIBS)
$(KERNEL_TEST_ELF_WITH_SYMBOLS): $(KERNEL_TEST_OBJS) $(KERNEL_LINKER_SCRIPT)
	$(LD) $(KERNEL_LDFLAGS) $(KERNEL_TEST_OBJS) -o $@ $(LDLIBS)

# This generates the FAT32 filesystem image containing the kernel and
# userspace executables (KERNEL_FS, KERNEL_FS_TEST).
$(OUT_DIR)/%_fs.bin: $(OUT_DIR)/%.bin
	dd if=/dev/zero of=$@ bs=1M count=33
	/sbin/mkfs.fat -F32 $@
	mcopy -i $@ $< ::KERNEL.BIN

$(BOOTABLE_DISK): $(BOOTLOADER) $(KERNEL_FS)
	scripts/install_bootloader.py -b $(BOOTLOADER) -k $(KERNEL_FS) -o $@
$(BOOTABLE_DISK_TEST): $(BOOTLOADER) $(KERNEL_FS_TEST)
	scripts/install_bootloader.py -b $(BOOTLOADER) -k $(KERNEL_FS_TEST) -o $@

.PHONY: docs run runi gdb clean cleanall
docs:
	doxygen

run: $(RUN_TARGET)
	$(TEST_INPUT) qemu-system-i386 $(QEMU_FLAGS) -drive format=raw,file=$<

# We built the kernel and bootloader ELF with symbols but we don't
# actually run this in QEMU. These will be used by gdb. Run and wait
# for gdb to attach.
runi: $(RUN_TARGET) $(BOOTLOADER_ELF_WITH_SYMBOLS) $(KERNEL_TARGET_ELF_WITH_SYMBOLS)
	$(TEST_INPUT) qemu-system-i386 $(QEMU_FLAGS) -drive format=raw,file=$< -no-reboot -no-shutdown -S -s

# Note that this doesn't build the elf files but we should check that
# they exist. They should be built from the last call to `make runi`
# so that it is in sync with the executable.
gdb:
	gdb -ex 'target remote localhost:1234' \
	    -ex 'set confirm off' \
	    -ex 'add-symbol-file $(BOOTLOADER_ELF_WITH_SYMBOLS)' \
	    -ex 'add-symbol-file $(KERNEL_TARGET_ELF_WITH_SYMBOLS)' \
	    -ex 'set confirm on' \
	    -ex 'set print asm-demangle on'

# Note that `make clean` will only clean the build directory for the
# current build variant. To remove all build variants, use `make
# cleanall`.
clean:
	rm -rf $(OUT_DIR)
cleanall:
	rm -rf out* docs

# Automatic header dependencies.
DEPS:=$(BOOT_OBJS:.o=.d) $(KERNEL_OBJS:.o=.d) $(KERNEL_TEST_OBJS:.o=.d)
-include $(DEPS)
