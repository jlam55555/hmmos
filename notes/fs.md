# filesystem and disk drivers

at first I thought this could wait until after implementing userspace
processes, but then I realized that it's quite annoying to load
practical userspace programs that are stored in the kernel binary
itself and have a purely in-memory layout.

The initial goals for the HmmOS filesystem are, in order:
1. write the HmmOS kernel and userspace programs to a FAT filesystem
2. implement basic FAT support in the bootloader (to find the kernel)
3. write a basic disk driver that can read/write one block at a time
4. implement FAT support in the kernel to load userspace programs
5. (later) write an ELF loader and store the binaries in ELF form
   rather than flat binaries

### preparing the filesystem disk

The process is not very different than before. Rather than storing the
flat kernel binary at a particular offset within the raw disk image,
we'll store a filesystem image there instead:

1. Create a filesystem image (using `mkfs.fat`).
2. Write the kernel and other necessary files to the filesystem image
   (using `mtools`).
3. Write the bootloader at the start of the raw disk image.
4. Write the filesystem image at the partition offset (instead of the
   flat kernel binary).

```
$ # Generate the empty filesystem image (FAT32 minimum size is 32MB).
$ dd if=/dev/zero of=filesystem.img bs=1M count=33
$
$ # Generate the filesystem.
$ /sbin/mkfs.fat -F32 filesystem.img
$
$ # Write kernel image to `/KERNEL.BIN`.
$ mcopy -i filesystem.img out/kernel_test.bin ::KERNEL.BIN
$ mdir -i filesystem.img ::
 Volume in drive : has no label
 Volume Serial Number is C72B-1CDB
Directory for ::/

KERNEL   BIN    123416 2024-12-23  14:46
        1 file              123 416 bytes
                         33 929 728 bytes free
$
$ # Install filesystem image just as we had installed the kernel bin.
$ scripts/install_bootloader.py \
$     -b bootloader.img -k filesystem.img -o disk.img
```

The bootloader code lives in the sectors immediately following the
MBR, and does not live on the partition containing the kernel and
userspace code (i.e. no executable code in the VBR). If the bootloader
grows too large, we can possibly store later stages of it in the
filesystem similar to the kernel to be dynamically loaded at runtime.

### finding and loading the kernel image from the partition

Let's assume the kernel image is written in the root directory with
name "KERNEL.BIN". We need to be able to locate and read this file
into memory. For simplicity, the bootloader will maintain its own
minimal FAT32 implementation for the sole purpose of loading the
kernel binary.

## disk interfaces

(may split this out into another notes file if this gets too long)

- **SATA (Serial AT Attachment)**: common physical interface for hard
  drives. Can be accessed using the ATA (older) or AHCI (newer)
  software interfaces.
- **PATA (Parallel AT Attachment)**: older than SATA, can be accessed
  using the ATA interface.
- **IDE (Integrated Drive Electronics)**: original name for PATA
- **ATAPI (ATA Packet Interface)**: extensions to ATA to support more
  devices other than hard drives (e.g. optical drives, SCSI devices)
- **AHCI (Advanced Host Controller Interface)**: encapsulates SATA
  devices and provides a standard PCI interface. Supports all of the
  ATA functionality and more.
- **PCI (Peripheral Component Interconnect)**: a high-performance bus
  for peripherals; can be used to enumerate devices.
- **HBA (Host Bus Adapter)**: the AHCI controller. Usually this refers
  to a physical extension card or onboard chip that connects external
  storage to a system bus. NICs are similar for networking.
- **FIS (Frame Information Structure)**: The packets used to
  communicate between the CPU and the HBA.

For HmmOS, we'll start with a simple AHCI driver. We can start with
PIO-mode (slow but simple) and then implement DMA.

### TODO
- [X] write the HmmOS kernel to a FAT32 filesystem
- [X] implement basic FAT32 support in the bootloader
- [ ] write a basic AHCI driver in the kernel
- [ ] implement FAT32 support in the kernel to load userspace programs

Further TODO items
- [ ] write an ELF loader and store the binaries in ELF format
- [ ] VFS layer
- [ ] ext2 support
- [ ] NVMe support
