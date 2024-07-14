SECTIONS
{
	/* Note: test text sessions (and data, bss) will be mixed
	together with ordinary kernel code. I don't think this will
	cause issues but there's the possibility that it might.*/

	/* This must equal KERNEL_LOAD_ADDR. */
	.text (0xFE000000) : {
		/* `/?*` is a hack since /* is starts a comment. */
		*/?*/entry.o(.text.entry)
		*(.text .text.*)
	}

	.rodata : {
		*(.rodata .rodata.*)
	}

	.data : {
		*(.data .data.*)
	}

	.bss : {
		*(COMMON)
		*(.bss)
	}

	/DISCARD/ : {
		*(.eh_frame)
		*(.comment)
		*(.note*)
	}
}
