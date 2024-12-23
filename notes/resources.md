# Resources
Probably incomplete since it's impossible to link everything. I'm not
going to bother linking OSDev wikis since those are implied.

## Bootloader
- OSDev Rolling your own bootloader: https://wiki.osdev.org/Rolling_Your_Own_Bootloader
- GAS assembler directives: https://ftp.gnu.org/old-gnu/Manuals/gas-2.9.1/html_chapter/as_7.html
- GAS assembly instructions for bootloader: https://stackoverflow.com/a/47859400
- Use of .code16 directive: https://stackoverflow.com/a/60025790
  - also: https://ftp.gnu.org/old-gnu/Manuals/gas-2.9.1/html_node/as_203.html#SEC205
- Bootloader reset vector: https://stackoverflow.com/a/31296556
- Ralph Brown's Interrupt List (RBIL): http://www.cs.cmu.edu/~ralf/files.html
- Grub boot stages: https://www.system-rescue.org/disk-partitioning/Grub-boot-stages/

## Memory
- Paging strategy for x86: https://forum.osdev.org/viewtopic.php?p=312121#p312121
- _Understanding the Linux Virtual Memory Manager_ by Mel Gorman

## MBR
- CHS decoding: https://thestarman.pcministry.com/asm/mbr/PartTables.htm#Decoding
- fdisk CHS decoding: https://superuser.com/a/1733477

## Build system
- Advanced auto dependency generation: https://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
  - also: https://stackoverflow.com/a/39003791

## C/C++ runtime
- Mini FAQ about the misc libc/gcc crt files: https://dev.gentoo.org/~vapier/crt.txt

## QEMU
- QEMU internals blog: https://airbus-seclab.github.io/qemu_blog/
- Setting breakpoints in QEMU w/ KVM enabled: https://forum.osdev.org/viewtopic.php?p=315671&sid=fa474a99fd3a40ba5cf2c2fafd7e44a9#p315671

## filesystem
- `mtools` to manipulate filesystem image without mounting: https://stackoverflow.com/a/29798605
- design of the FAT filesystem: https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
