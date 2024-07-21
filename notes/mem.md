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

Note that the kernel also needs to be loaded at a fixed address in
high memory. For simplicity I choose this to be a region of hardcoded
size (starting at `KERNEL_LOAD_ADDR`) at the top of the virtual
address space. This will eat away from the 1GB HHDM, but the kernel
size should be much smaller than 1GB.

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

## Paging extensions
All of the paging extensions can be queried via the `cpuid`
instruction.

### x86
The standard paging involves a two-tier hierarchy. *Page tables*
comprise 1024 4KB pages, and *page directories* comprise 1024 page
tables (4MB).

These paging extensions do not increase the virtual address space
(which cannot be extended past 32 bits). PSE-36 and PAE extend the
physical address space to 36 bits.

- *Page size extension* (PSE) enables 4MB huge pages in the page
  directory. These page directory entries point to 4MB pages rather
  than page tables.
- *PSE-36* enables 36-bit physical addresses via a modified page table
  structure. It still uses the two-hierarchy page table structure (and
  is thus simpler than PAE). However, it only allows huge pages for
  addresses above 4GB. This was a stopgap measure before PAE was
  introduced, and PAE is considered a better solution now.
- *Physical address extension* (PAE) sets up 3-level paging with
  8-byte page table entries (512 entries per page). The page table
  addresses 512 pages (2MB), the page directory 512 page tables (1GB),
  and the new *page directory pointer table* (terrible name, I know)
  contains 512 page directories (512GB). This theoretically enables up
  to 12+9+9+9=39 bits of physical address space, but usually only 36
  bits of physical address space were allowed/implemented, same as
  PSE-36.

*Page attribute table* (PAT) is another 3-letter acronym starting with
'P' that relates to paging. I'm not going to go into this.

### x86-64
The standard 64-bit paging involves a four-tier hierarchy, based on
PAE. One additional level (the *page map level 4*, or PML4) was
added. This extends the physical address space to 48 bits (256TB).

However, this physical address space isn't enough even for some
large-scale servers, so another page table level (*PML5*) was added to
support up to 57 bits (128PB) of virtual address space. This is (as of
the time of wrting) a pretty new feature (only supported by Intel Ice
Lake/AMD Ryzen 7000 and newer).

## Kernel memory management

### Physical memory management
There are two levels of physical memory allocation: page frames and
smaller chunks.

Firstly, we need a `page frame table`, which is an array of `page
frame descriptors`s. There exists one page frame descriptor for each
page frame (4KB physical page) in RAM. This stores metadata about the
page frame and easily is the kernel's largest data structure (in Linux
this has an overhead of 1/64 of the total installed RAM).

After the page frame table is initialized, we need a `page frame
allocator`. This should provide a very simple interface:
```
void *alloc_page_frames(int num_page_frames);
void free_page_frames(void *start, int num_page_frames);
```
There are [multiple common approaches to
this](https://wiki.osdev.org/Page_Frame_Allocation), from being as
simple as a linear scan over an auxiliary bitmap, using a stack data
structure to keep track of freepages for fast allocation/freeing, and
using multiple bitmaps ("buddy allocator") for fast allocation of
different sizes of contiguous memory.

Once we have the ability to allocate kernel pages, there should be a
mechanism to allocate smaller pieces of memory to allow for
`kmalloc()`/`kfree()`-like functionality. This probably means some
sort of slab allocator (e.g., Linux has different slab allocator
implementations called SLAB/SLUB/SLOB).

### Virtual memory management
This is mostly relevant for userspace, since we'll be using the linear
HHDM for kernel purposes. (For now, it's simplest if we use this
3GB/1GB as a hard division of userspace/kernel code, but later we can
be more lenient and allow userspace to allocate from the low 1GB if
needed.)

Virtual memory will be managed via `demand paging` (i.e. lazily). Upon
a `mmap()` request, the new `virtual memory area` (VMA) will be added
to the processes' VMA list. The actual page(s) will be mapped in upon
a page fault, which will first check that the memory is valid (in the
VMA) before allocating a physical page and performing the virtual
memory mapping.

Upon a userspace process exit, all physical pages mapped to that
process will be freed. The virtual mappings will automatically get
discarded once the process's page mapping is discarded (this should
also be included in the set of physical pages mapped to that process).
