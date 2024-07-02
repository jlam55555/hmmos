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
An annotated version of the `check_a20` function:

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
	- [X] Some basic printing functions
