OUTPUT_FORMAT("elf32-i386");
ENTRY(_start);
SECTIONS
{
	/* MBR */
	.text.boot (0x7C00) : {
		*(.text.boot);
	}

	.rodata.boot : {
		*(.rodata.boot);
	}

	.sig (0x7DFE) : {
		SHORT(0xAA55);
	}

	/* Stage 1.5 bootloader */
	.text (0x7E00) : {
		*(.text);
	}

	.rodata : {
		*(.rodata);
	}

	.data : {
		*(.data);
	}

	/* Make sure the disk is large enough for the bootloader. */
	.end (0x7C00 + 63*0x200 - 1) : {
		BYTE(0x00);
	}

	/DISCARD/ : {
		*(.eh_frame);
		*(.comment);
		*(.note*);
	}
}
