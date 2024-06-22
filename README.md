# HmmOS
Second attempt at writing an OS, this time a little more from scratch
now that I have a slightly better idea of what the bootloader does.

Focusing on x86 first, will maybe update this to be for x86_64 later.

## Setup
This project is mainly built with clang/LLVM toolchains in mind. This
includes `clang`, `lld`, `clangd`, and `clang-format`.

GNU `binutils` is needed where LLVM is lacking (i.e. `as` for GNU AT&T
asm syntax + `objdump` and friends).

This will be tested with `qemu`, but any (x86) emulator should be fine.

Versions:
```
$ clang --version
clang version 17.0.6
Target: x86_64-pc-linux-gnu
Thread model: posix
InstalledDir: /usr/bin
$ as --version
GNU assembler (GNU Binutils) 2.42.0
Copyright (C) 2024 Free Software Foundation, Inc.
This program is free software; you may redistribute it under the terms of
the GNU General Public License version 3 or later.
This program has absolutely no warranty.
This assembler was configured for a target of `x86_64-pc-linux-gnu'.
```

## Getting started
Build the bootloader and run it in QEMU. Make sure to have `qemu` and
GNU `make` installed.
```
$ make run
```

## Misc.
Name courtesy of @Victoooooor.
