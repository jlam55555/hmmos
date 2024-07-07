SECTIONS
{
	/* MBR */
	.text.boot (0x7C00) : {
		/* MBR entry code */
		*(.text.boot.entry);
		/* Other MBR code (e.g., basic print fcn */
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
		*(.text .text.*);
	}

	.rodata : {
		*(.rodata .rodata.*);
	}

	.data : {
		*(.data .data.*);
	}

	.bss : {
		*(COMMON);
		*(.bss);
	}

	/DISCARD/ : {
		*(.eh_frame);
		*(.comment);
		*(.note*);
	}
}
