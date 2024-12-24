# boot.md

These notes assume x86 booting onto a HDD using the MBR protocol. The
protocol is implemented by the BIOS, and the machine will be in real
mode (a.k.a. 16-bit mode or i8086 mode).

## Master Boot Record (MBR)
The MBR is the first sector (boot sector) of a storage device and is
identified by the "magic bytes" 0x55, 0xAA in byte offsets 0x01FE and
0x01FF respectively (end of a 512-byte "sector", even if the
underlying storage device doesn't have 512-byte
sectors). Additionally, the MBR contains the partition table (four
16-byte entries at 0x01BE through 0x01FD), so we only have 446 bytes
for executable code at the start of the sector.

| Offset | Length | Description |
| --- | --- | --- |
| 0x0000 | 446 | Executable code |
| 0x01BE | 16 | Partition 1 descriptor |
| 0x01CE | 16 | Partition 2 descriptor |
| 0x01DE | 16 | Partition 3 descriptor |
| 0x01EE | 16 | Partition 4 descriptor |
| 0x01FE | 2 | 0x55, 0xAA |

The BIOS will copy the boot sector to memory starting at 0x7C00, so
the bootloader compilation/linking has to be aware of this.

## Enabling A20 line
The A20 line is needed to enable pin 20 of the memory bus. Otherwise
this pin will always be set to 0, thus disabling all odd-megabytes of
memory (i.e., 0x5ABCDE is treated as 0x4ABCDE). There are silly
historical reasons for this (stemming from the silly backcompat with
real mode segments) and even sillier non-standardized ways to
enable/disable the A20 line. The [OSDev wiki on the A20
line](https://wiki.osdev.org/A20_Line) is very thorough.

An annotated version of the `check_a20` function to check if the A20
line is enabled, in which case we don't have to enable it ourselves:

```
es=0, ds=0xFFFF
di=0x0500, si=0x0510

es:di = 0x0000:0x0500 = 0x000500
ds:si = 0xFFFF:0x0510 = 0x100500

These values are 1MB apart. Importantly, neither of these should
overlap bootloader code.

Save old values stored at these locations.
Push them onto the stack.

Overwrite es:di with 0x00.
Overwrite ds:si with 0xFF.

Check if es:di == 0xFF. If so, this means the A20 line isn't enabled.

(Then cleanup by restoring the old values at es:di and ds:si.)
```

It turns out that QEMU already enables the A20 line, which makes
things simpler for us.

## x86 CPU modes
Most of this is all silly and historical, but it's necessary for the
booting process.

x86 starts in 16-bit *real mode*, for compatibility reasons back to
the first x86 cpus (i8086). This does support 32-bit registers and
addressing modes, but instructions are 16-bit, we only have direct
mapping (i.e. we have segment:offset addresses, but no segmentation or
paging). In general we're constrained to a 20-bit address space
(except for unreal mode). In real mode the BIOS (in BIOS or UEFI
compatibility mode) exposes some useful low-level tools via BIOS
interrupts, e.g., simple display and disk read operations.

Then we need to enter *protected mode*, which is the primary CPU mode
since i386 (32-bit x86). This allows for the use of segmentation and
paging, as well as long mode (64-bit mode). We also lose access to the
BIOS interrupts (in exchange for our own interrupt code set up in the
IDT).

BIOS functions are useful for the early boot stages but it's fairly
limiting. For example, we may want to copy the kernel from disk to
memory, but the BIOS can address less than 1MB of memory. This means
we have to copy 1MB chunks of the kernel in real mode, switch to
protected mode to copy this to higher memory, and then switch back to
real mode to repeat. There are two main ways around this: (1) enter
*virtual 8086* (v8086) mode, which is an emulated real-mode within
protected-mode; and (2) use *unreal mode*. Unreal mode is simply real
mode with 32-bit GDT (data) descriptors. It turns out that GDT
descriptors (like most CPU register descriptors) are cached, so
setting a 32-bit protected-mode segment and then switching back to
real mode allows for 32-bit addressing in real mode. This may be
simpler than using v8086 mode since we're already familiar with
writing plain real mode code.

## Disk layout
For this project, at least for now, there is a very simple disk layout
assumed by the bootloader. There's no concept of filesystems, at least
not for now.

| Offset | Length | Description |
| --- | --- | --- |
| 0x0000 | 0x200 | MBR. Loaded into memory by the BIOS at 0x7C00. |
| 0x0200 | 0x7C00 | Bootloader stage 1.5. Loaded into memory by the MBR at 0x7E00. |
| ?? | ?? | Kernel binary file. Stored at the beginning of a partition. |

## GDT layout
We'll use a standard flat model for the GDT. We mostly need 32-bit
descriptors, one for (code, data) x (ring 0, ring 3).

Additionally, we'll have one code segment for "big unreal mode" (with
limit 0xFFFF) to avoid any edge cases with "huge unreal mode" due to
the upper bits of %eip not being set properly on mode switches.

| GDT descriptor | Description |
| --- | --- |
| 0 | Null descriptor |
| 1 | Ring 0 code (32-bit) |
| 2 | Ring 0 data (32-bit) |
| 3 | Ring 3 code (32-bit) |
| 4 | Ring 3 data (32-bit) |
| 5 | Ring 0 code (16-bit) |

We don't need 16-bit descriptors for switching back to real
mode. Indeed, the whole point of unreal mode is to switch back to real
mode using cached 32-bit GDT descriptors.

## Bootloading strategy
In lieu of reading filesystems to get the kernel ELF file (a la GRUB
stage 1.5/2), a simpler method is to write the kernel to a fixed
location on the disk (known at installation time) and have the
bootloader load this fixed location image. LILO does something
similar.

For now, this very simple method means: write the kernel to a
"partition" and write the partition details to the MBR. Probably with
some magic bytes so it's easy to identify.

A very simple script is provided to do this. It takes as input the
compiled bootloader binary and the kernel binary:
```
$ python3 scripts/install_bootloader.py \
	-b out/boot.bin \
	-k out/kernel.bin \
	-o out/disk.bin
```

The resulting partition details can be read by `fdisk`. If the kernel
file fits in a single sector, this would look like the following:
```
$ /sbin/fdisk -l out/disk.bin
Disk out/disk.bin: 1 MiB, 1049088 bytes, 2049 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0x00000000

Device        Boot Start   End Sectors  Size Id Type
out/disk.bin1 *     2048  2048       1  512B ff BBT
```

The kernel is expected to be a flat binary file that will be loaded at
the configurable `KERNEL_LOAD_ADDR`. Later we can parse and load an
ELF file, but this is simpler to get started.

## Bootloader stages

i.e., navigating the bootloader code

1. MBR: first sector, operates in real-mode
2. stage1.5: remaining real-mode code, loaded into memory by the MBR
3. stage2: (32-bit) protected-mode code, can call into C code
   (may jump into unreal-mode to call BIOS functions)

## Bootloader TODO
Rough order, may need additional steps. Roughly based on the list from
[Rolling your own
bootloader](https://wiki.osdev.org/Rolling_Your_Own_Bootloader).

Now all complete!

- [X] Set up MBR
- [X] Set up basic printing
	- [X] BIOS 0x10 function
	- [X] Basic printing functions
- [X] Copy rest of bootloader (stage 1.5) to memory
- [X] Enable A20 line
- [X] Enter protected mode
	- [X] Load GDTR
	- [X] Enter unreal mode
	- [X] Enter protected mode again (for real this time)
- [X] Set up paging
	- [X] HHDM (0xC0000000 through `KERNEL_LOAD_ADDR`: ~1GB low memory
          linear map)
	- [X] Direct map (only needed for the bootstrap process): first
          1MB of memory
	- [X] Kernel map (starting at `KERNEL_LOAD_ADDR`)
- [X] Prepare kernel
	- [X] Load kernel to memory at fixed load address
	- [X] Jump to kernel (build a simple toy kernel)
	- [X] Report memory map and other capabilities to kernel
