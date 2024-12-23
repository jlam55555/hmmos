# filesystem and disk drivers

at first I thought this could wait until after implementing userspace
processes, but then I realized that it's quite annoying to load
practical userspace programs that are stored in the kernel binary
itself and have a purely in-memory layout.

The initial goals for the HmmOS filesystem are, in order:
1. write the HmmOS kernel and userspace programs to a FAT filesystem
2. implement basic FAT support in the bootloader (to find the kernel)
3. write a basic SATA driver that can read/write one block at a time
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
into memory.

### TODO
- [X] write the HmmOS kernel to a FAT filesystem
- [ ] implement basic FAT support in the bootloader
- [ ] write a basic SATA driver that can read/write one block at a
      time
- [ ] implement FAT support in the kernel to load userspace programs
- [ ] (later) write an ELF loader and store the binaries in ELF form
      rather than flat binaries
