SRC_DIR:=src
OUT_DIR:=out

AS:=as
ASFLAGS:=--32 --fatal-warnings
LD:=ld.lld
CC:=clang

# 16-bit mode for now. It's better if we switch to C after we enter
# protected mode so all C code can be 32-bit mode.
CFLAGS:=-m32 -ffreestanding -O0 -fno-pie -Werror
LDFLAGS:=--oformat=binary \
	--build-id=none \
	-T$(SRC_DIR)/boot/linker.ld

BOOTLOADER:=$(OUT_DIR)/boot.bin

# This is pretty standard for C but I'm not sure if it makes much
# sense for asm files.
AS_SRCS:=$(wildcard $(SRC_DIR)/**/*.S)
C_SRCS:=$(wildcard $(SRC_DIR)/**/*.c)
C_OBJS:=$(patsubst $(SRC_DIR)/%.S,$(OUT_DIR)/%.o,$(AS_SRCS))	\
	$(patsubst $(SRC_DIR)/%.c,$(OUT_DIR)/%.o,$(C_SRCS))

QEMU_FLAGS:=-m 4G

ifneq ($(DEBUG),)
	override QEMU_FLAGS+=-no-reboot -no-shutdown
endif

ifneq ($(SHOWINT),)
	override QEMU_FLAGS+=-d int,cpu_reset
endif

all: $(BOOTLOADER)

$(OUT_DIR)/%.o: $(SRC_DIR)/%.S
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $^ -o $@

# TODO: support different build variants (e.g., with different
# optimization levels and with gcc) to make sure things work outside
# of the default build.
$(OUT_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $^ -o $@

$(BOOTLOADER): $(AS_OBJS) $(C_OBJS)
	mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $^ -o $@

.PHONY: run
run: $(BOOTLOADER)
	qemu-system-i386 $(QEMU_FLAGS) -drive format=raw,file=$<

.PHONY: clean
clean:
	rm -r $(OUT_DIR)
