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

## Bootloader steps
Rough order, may need additional steps. Roughly based on the list from
[Rolling your own
bootloader](https://wiki.osdev.org/Rolling_Your_Own_Bootloader).

- [X] Set up MBR
- [ ] Set up basic printing
	- [X] BIOS 0x10 function
	- [ ] Serial port
- [X] Copy rest of bootloader (stage 1.5) to memory
- [ ] Enable A20 line
- [ ] Enter protected mode
	- [ ] Load GDTR
	- [ ] Enter unreal mode
		- [ ] Load kernel into memory
		- [ ] Setup BIOS graphics mode
	- [ ] Enter protected mode again (for real this time)
	- [ ] Jump to kernel
- [ ] Setup interrupts
- [ ] Bootloader utilities
	- [ ] ELF parsing (for kernel)
	- [ ] Some basic printing functions
