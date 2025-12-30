## Free-form compile rules for userspace executables, which will be
## loaded by the OS. These compile rules should be more
## straightforward than the OS's rules.
##
## This will be called via recursive make from top-level (`make us`).
##
## TODO: copy args from above makefile here. Can we dedup some of
## the variables here?

SRC_DIR=.
OUT_OBJ_DIR:=$(OUT_DIR)/userspace/obj
OUT_LIB_DIR:=$(OUT_DIR)/userspace/lib
OUT_BIN_DIR:=$(OUT_DIR)/userspace/bin
CXX=clang++
CXXFLAGS:=-ffreestanding -nostdlib -m32 -I$(SRC_DIR)/lib

# TODO: get ld.lld working. Program isn't loaded correctly by the ELF
# loader if I do this.
LD=ld
LDFLAGS:=-m elf_i386 --build-id=none -nostdlib -static -L$(OUT_LIB_DIR)

BINS:=$(shell find $(SRC_DIR)/bin -mindepth 1 -type d -printf "%f\n")
LIBS:=$(shell find $(SRC_DIR)/lib -mindepth 1 -type d -printf "%f\n")
OUT_BINS:=$(BINS:%=$(OUT_BIN_DIR)/%)

ENSURE_DIR=mkdir -p $(dir $@)

.PHONY: all
all: $(OUT_BINS)

$(OUT_OBJ_DIR)/%.o: $(SRC_DIR)/%.cc
	@$(ENSURE_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# BUILD_BIN(program_name, deps...)
#
# Compile a binary into out/bin/<program_name>
define BUILD_BIN
_$(1)_objs:=$$(shell find $$(SRC_DIR)/bin/$(1)/ -name *.cc)
$(1)_objs:=$$(_$(1)_objs:$$(SRC_DIR)/%.cc=$$(OUT_OBJ_DIR)/%.o)
_$(1)_libs:=$(2)
$(1)_libs:=$$(_$(1)_libs:%=-l%)
$$(info LIBS= $$($(1)_libs))
$$(OUT_BIN_DIR)/$(1): $$($(1)_objs) $$($(1)_libs)
	@$$(ENSURE_DIR)
	$$(LD) $$(LDFLAGS) -o $$@ $$^
endef

# BUILD_LIB(library_name)
#
# Compile a static library into out/lib/lib<library_name>.a
define BUILD_LIB
_$(1)_deps:=$$(shell find $$(SRC_DIR)/lib/$(1)/ -name *.cc)
$(1)_deps:=$$(_$(1)_deps:$$(SRC_DIR)/%.cc=$$(OUT_OBJ_DIR)/%.o)
$$(OUT_LIB_DIR)/lib$(1).a: $$($(1)_deps)
	@$$(ENSURE_DIR)
	$$(info DEPS $$@ MORE $$^ DONE)
	ar rcs $$@ $$^

.PHONY: -l$(1)
-l$(1): $$(OUT_LIB_DIR)/lib$(1).a
endef

# List all binaries/libaries to be built here.
# TODO: turn this into a foreach
$(eval $(call BUILD_BIN,init))
$(eval $(call BUILD_BIN,josh,jlibc))
$(eval $(call BUILD_LIB,jlibc))
