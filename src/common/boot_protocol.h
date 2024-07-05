#pragma once
/// A set of definitions that the bootloader and kernel need to agree on.
#include "memdefs.h"

#ifndef KERNEL_LOAD_ADDR
/// Save 32MB at the top of the virtual address space for the kernel
/// load address. This also dictates the maximum size of the kernel
/// binary, so this will need to be overridden (and the linker script
/// modified) if the kernel binary size exceeds this.
///
/// This is a compromise due to the limited size of the 32-bit address
/// space, and the fact that we want the kernel to be loaded at a
/// fixed address (i.e., nothing fancy like PIE). The larger this
/// section is, the more space we take away from the 1GB HHDM, so the
/// kernel will be able to address less memory without creating its
/// own memory mappings.
#define KERNEL_LOAD_ADDR (4 * GB - 32 * MB)
#endif
#define KERNEL_MAP_SZ (4 * GB - KERNEL_LOAD_ADDR)

_Static_assert(HUGEPG_ALIGNED(KERNEL_LOAD_ADDR),
               "KERNEL_LOAD_ADDR must be hugepage-aligned");
