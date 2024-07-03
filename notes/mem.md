# Memory
Specifically focused on the x86 architecture.

## Overview of memory access

### Addressing modes
x86 has 16-bit, 32-bit, and 64-bit addressing modes. These don't
exactly correspond with the active CPU mode ((16-bit) real mode can
use 32-bit registers and addressing, for example). The addressing mode
restricts how you can address memory (e.g., which registers can be
used as a base address, and operand sizes) and is encoded into the
instruction/asm instruction.

### Real mode
x86 computers start in i8086 mode. Each memory access involves one of
the segment registers plus an offset. The segment register is usually
implicit (%cs for code, %ds for data, and %ss for stack) but can
usually be explicitly specified. (segment:offset) pairs are translated
into the physical address (segment*0x10 + offset).

This segmentation method exists historically to extend a 16-bit
address space into a 20-bit one (and still exists for backcompat
reasons), and it comes with many complications (e.g., the A20
line). For the sake of this toy project, all segment registers are
forced to 0 in real mode.

### Protected mode
In protected mode, we still have "segmentation" (i.e., the segment
registers still exist and have meaning), although in a different
way. Segments are described by the Global Descriptor Table (GDT) (and
some other related tables, e.g., the LDT, IDT, TSS), and the segment
registers are only used to select a segment. Segments contain a base
and a limit, some permissions fields, and can be 16-, 32-, or 64-bit.

For the most part, segmentation has been superceded by a different
virtual memory method called paging. I say "for the most part" because
segmentation still exists and is implemented in x86 to translate
segment:offset into virtual addresses (which then get translated to
physical addresses via paging if enabled). Nowadays the GDT mapping is
almost always trivial (offset=0, limit=max) so that paging is the only
memory mapping mechanism.

Paging is a fairly complicated, hierarchical memory mapping scheme
that requires hardware support, so I'm not going to go into
it. Additionally, there's differences between 32-bit and 64-bit paging
(PAE, 4-level vs. 5-level paging, and hugepages make this even more
complicated). Both the OSDev wiki and the Intel SDM are great
resources.

## Paging strategy
In x86_64, the virtual address space (48 for 4-level paging or 57 bit
for 5-level) is usually larger than the physical address space (52
bit) which is vastly larger than the actual physical memory for many
machines (~30-40 bit). This means that we can easily map the entirety
of physical memory into some part of virtual memory, so the kernel can
address any byte in memory (plus some offset). This usually lives at
the start of higher-half memory (0xFFFF800000000000 assuming 4-level
paging) and is referred to as the *higher half direct map*.

In 32-bit x86, we're a little less lucky. It's very possible for the
physical memory to take up the entirety of the addressable space
(4GB), so we can't fit the entire physical memory into the higher
half.  PAE makes this even worse, since it allows a physical address
space (36 bits) larger than the virtual address space.

It's a good idea to keep all kernel virtual addresses in the higher
half, so it's fine to just map enough space for the kernel. If the
"higher half" starts at 0xC0000000 (leaving us 1GB of virtual address
space for the kernel), we can linearly map the first 1GB of
memory. The limitations of this are: (1) the kernel cannot address
memory outside the first GB of physical memory (unless it maps those
pages manually) and (2) userspace only gets 3GB of addressible
memory. We don't have to prevent userspace processes from allocating
page frames from the first 1GB of physical memory, but these will need
to be evicted/paged out if the kernel is running low on memory, or the
kernel will need to manually map (low) memory for its own use. The
kernel will remain fixed for all userspace
processes. [Here's](https://forum.osdev.org/viewtopic.php?p=312121#p312121)
a good description of this paging strategy.

A different approach is to identity-map the entire 32-bit address
space for the kernel, and maintain a completely separate address space
for userspace, but [this complicates
things](https://forum.osdev.org/viewtopic.php?p=312122&sid=a6ce91a6afd755945b629cd7678ab6e8#p312122).

## General kernel memory usage
Just to get a sense of what takes space in the kernel:

- *Text/data region*: The actual kernel code. In the case of this toy
  project, this is negligible.
- *Page tables*: For 2-level (non-PAE) x86, the memory usage can be as
  low as 4 bytes per page, or 1/1024 of the mapped address space. This
  ratio will be higher if userspace processes have sparse page tables.
- *Page frame array*: If we copy Linux and use 64 bytes of metadata
  per page frame, this is probably the largest component of kernel
  memory, taking 1/64 of the physical memory. For 4GB of physical
  memory, this means 64MB dedicated to the page frame array.
- *Kernel stacks*: There will be one one-page page frame per kernel-
  or userspace process.
