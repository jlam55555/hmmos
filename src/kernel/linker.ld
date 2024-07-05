SECTIONS
{
	/* This must equal KERNEL_LOAD_ADDR. */
	.text.boot (0xFE000000) : {
		*(.text.entry)
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
