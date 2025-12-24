## Free-form compile rules for userspace executables, which will be
## loaded by the OS. These compile rules should be more
## straightforward than the OS's rules.
##
## This will be called via recursive make from top-level (`make us`).
##
## TODO: copy args from above makefile here. Can we dedup some of
## the variables here?

OUT_DIR:=../../out/userspace
CXX=clang++
CXXFLAGS:=-ffreestanding -nostdlib -static -m32

all: $(OUT_DIR)/init

# Init process.
$(OUT_DIR)/init: init.cc
	@mkdir -p $(OUT_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@
