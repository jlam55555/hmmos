	## Setting up the Master Boot Record (MBR).
	##
	## The MBR is the first sector (boot sector) of a device and
	## is identified by the "magic bytes" 0x55, 0xAA in byte
	## offsets 0x01FE and 0x01FF respectively (end of a 512-byte
	## sector). Additionally, the MBR contains the partition table
	## (four 16-byte entries at 0x01BE through 0x01FD), so we only
	## have 446 bytes for executable code at the start of the
	## sector.
	.global _start
	.code16
	.text
_start:
	jmp .			# while(1);

	.skip 510 - (. - _start), 0
	.byte 0x55, 0xAA
