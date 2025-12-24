# HmmOS
Second attempt at writing an OS, this time a little more from scratch
now that I have a slightly better idea of what the bootloader does.

Focusing on x86 first, will maybe update this to be for x86_64 later.

High-level roadmap for HmmOS:
- Bootloader
  - [X] Simple boot protocol with bootloader requests
  - [X] Kernel loader
	- [X] With FAT32 support
	- [ ] With ELF support
- Memory management
  - [X] Page frame allocator
  - [X] vmalloc
  - [ ] Slab allocator
- Userspace processes
  - [X] Kernel threads
  - [X] Thread scheduling
  - [X] ELF loader
  - [ ] Syscalls
  - [ ] Shared libraries
  - Sample programs
    - [ ] Shell
	- [ ] Simple libc implementation
- Filesystem
  - Filesystem drivers
    - [X] FAT32
	- [ ] ext2
  - [X] VFS layer
    - [ ] LRU page cache
- Device drivers (very simple)
  - [X] Serial port
  - [X] BIOS text mode display
  - [X] ACPI
  - [X] AHCI (SATA)
  - [ ] TTY
  - [ ] Keyboard
  - [ ] Graphics
  - [ ] UDP
  - [ ] TCP
- SMP
  - [ ] Locking primitives
- Tooling
  - [X] Debugging in GDB
  - [X] clang+gcc tooling
  - [X] Testing in QEMU w/ optional KVM virtualization
  - [X] Unit test framework
  - [ ] Kernel module system with dependencies

## Setup
This project is mainly built with clang/LLVM toolchains in mind. This
includes `clang`, `lld`, `clangd`, and `clang-format`. You'll need at
least version 14.

GNU `binutils` is needed where LLVM is lacking (i.e. `as` for GNU AT&T
asm syntax + `objdump` and friends). GNU `mtools` is also required to
build the binary.

This will be tested with `qemu` with QEMU and KVM emulation.

Versions:
```
$ as --version
GNU assembler (GNU Binutils for Debian) 2.40
Copyright (C) 2023 Free Software Foundation, Inc.
This program is free software; you may redistribute it under the terms of
the GNU General Public License version 3 or later.
This program has absolutely no warranty.
This assembler was configured for a target of `x86_64-linux-gnu'.
$
$ clang --version
Debian clang version 14.0.6
Target: x86_64-pc-linux-gnu
Thread model: posix
InstalledDir: /usr/bin
$
$ qemu-system-i386 --version
QEMU emulator version 7.2.13 (Debian 1:7.2+dfsg-7+deb12u7)
Copyright (c) 2003-2022 Fabrice Bellard and the QEMU Project developers
$
$ mtools --version
mtools (GNU mtools) 4.0.32
configured with the following options: enable-xdf disable-vold disable-new-vold disable-debug enable-raw-term
$
$ uname -a
Linux jebian 6.1.0-27-amd64 #1 SMP PREEMPT_DYNAMIC Debian 6.1.115-1 (2024-11-01) x86_64 GNU/Linux
```

## Getting started
Build the bootloader and run it in QEMU. Make sure to have `qemu` and
GNU `make` installed, and `doxygen` if you want to generate documentation.
```
$ # Run the kernel
$ make run
$
$ # Run tests
$ make run TEST=all
$ make run TEST=<testselection>
$
$ # Generate documentation
$ make docs
```

### Debugging in GDB
```
$ # Compile with debug symbols enabled and start running in QEMU.
$ # This doesn't actually start until `make gdb` is called.
$ make runi
$
$ # Start gdb and connect to the previous session, loading the debug
$ # symbols compiled from the last run to `make runi`. Make sure to
$ # run this with the same compile options (e.g., OPT=*, DEBUG=*) as
$ # in `make runi`.
$ make gdb
```

## Misc.
Name courtesy of \@Victoooooor.
