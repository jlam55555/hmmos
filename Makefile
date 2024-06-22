SRC_DIR:=src
OUT_DIR:=out

AS:=as
ASFLAGS:=--32
LD:=ld.lld
LDFLAGS:=--oformat=binary \
	--build-id=none \
	-T$(SRC_DIR)/boot/linker.ld

BOOTLOADER:=$(OUT_DIR)/boot.bin

# This is pretty standard for C but I'm not sure if it makes much
# sense for asm files.
AS_SRCS:=$(wildcard $(SRC_DIR)/**/*.S)
AS_OBJS:=$(patsubst $(SRC_DIR)/%.S,$(OUT_DIR)/%.o,$(AS_SRCS))

all: $(BOOTLOADER)

$(OUT_DIR)/%.o: $(SRC_DIR)/%.S
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $^ -o $@

$(BOOTLOADER): $(AS_OBJS)
	mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $^ -o $@

.PHONY: run
run: $(BOOTLOADER)
	qemu-system-i386 -drive format=raw,file=$<

.PHONY: clean
clean:
	rm -r $(OUT_DIR)
