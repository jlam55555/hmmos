# Resources
Probably incomplete since it's impossible to link everything. I'm not
going to bother linking OSDev wikis since those are implied.

## General OS theory
- _Operating Systems: Three Easy Pieces_ (OSTEP): https://pages.cs.wisc.edu/~remzi/OSTEP/
- _Linux Device Drivers, third ed._ (LDD3): https://lwn.net/Kernel/LDD3/
- _Writing Device Drivers_ (Oracle): https://docs.oracle.com/cd/E18752_01/html/816-4854/index.html

## x86, x86-64 architectures
- Intel Software Developer Manual (SDM): https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html

## Bootloader
- OSDev Rolling your own bootloader: https://wiki.osdev.org/Rolling_Your_Own_Bootloader
- GAS assembler directives: https://ftp.gnu.org/old-gnu/Manuals/gas-2.9.1/html_chapter/as_7.html
- GAS assembly instructions for bootloader: https://stackoverflow.com/a/47859400
- Use of .code16 directive: https://stackoverflow.com/a/60025790
  - also: https://ftp.gnu.org/old-gnu/Manuals/gas-2.9.1/html_node/as_203.html#SEC205
- Bootloader reset vector: https://stackoverflow.com/a/31296556
- Ralph Brown's Interrupt List (RBIL): http://www.cs.cmu.edu/~ralf/files.html
- Grub boot stages: https://www.system-rescue.org/disk-partitioning/Grub-boot-stages/

## Memory mapping
- Paging strategy for x86: https://forum.osdev.org/viewtopic.php?p=312121#p312121
- _Understanding the Linux Virtual Memory Manager_ by Mel Gorman
- _A Primer on Memory Consistency and Cache Coherence_ by Nagarajan et al.
- ioremap()/memremap(): https://lwn.net/Articles/653585/
- Strategy for mapping segments from ELF file: https://f.osdev.org/viewtopic.php?p=260156&sid=19fe00f2a5ad12c8db036ef0668be8a1#p260156
- rmap: https://stackoverflow.com/questions/16581490/how-does-kernel-know-which-pages-in-the-virtual-address-space-correspond-to-a-s
- "private anonymous memory" vs. "shared anonymous memory": https://lore.kernel.org/linux-mm/af7f036d-a131-985f-edd7-2ab93353b373@redhat.com/
- "dirty cow attack": https://spectralops.io/blog/what-is-the-dirty-cow-exploit-and-how-to-prevent-it/

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
- Q35/ICH9 architecture: https://wiki.qemu.org/Features/Q35
- Attaching QEMU monitor to running VM instance: https://unix.stackexchange.com/a/476617

## filesystem
- `mtools` to manipulate filesystem image without mounting: https://stackoverflow.com/a/29798605
- design of the FAT filesystem: https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
- inode number assignment for FAT in Linux: https://unix.stackexchange.com/questions/642313/how-does-linux-assign-inode-numbers-on-filesystems-not-based-on-inodes
- in-memory vs. on-disk file tables: https://unix.stackexchange.com/questions/21325/is-the-file-table-in-the-filesystem-or-in-memory
- inode object: https://litux.nl/mirror/kerneldevelopment/0672327201/ch12lev1sec6.html
- overview of the linux virtual file system: https://www.kernel.org/doc/html/v6.3/filesystems/vfs.html
- file system drivers: https://linux-kernel-labs.github.io/refs/heads/master/labs/filesystems_part2.html
- readdir ordering: https://stackoverflow.com/a/8977490
- struct file: https://litux.nl/mirror/kerneldevelopment/0672327201/ch12lev1sec8.html
- path lookup: https://www.kernel.org/doc/html/v5.0/filesystems/path-lookup.html
- recovering an unlinked in-use file: https://unix.stackexchange.com/a/758186

## AHCI
- ATA/ATAPI command spec: https://people.freebsd.org/~imp/asiabsdcon2015/works/d2161r5-ATAATAPI_Command_Set_-_3.pdf
- SATA AHCI spec: https://www.intel.com/content/www/us/en/io/serial-ata/serial-ata-ahci-spec-rev1-3-1.html
- PIO, third-party DMA, and bus mastering (first-party DMA). A good first introduction to DMA: http://www.tweak3d.net/articles/howbusmaster/

## Process/timesharing abstraction
- The Evolution of the Unix Time-sharing System: https://www.read.seas.harvard.edu/~kohler/class/aosref/ritchie84evolution.pdf
