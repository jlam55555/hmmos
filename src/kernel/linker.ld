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

		/* Force .bss region in flat binary file for ld.
		https://f.osdev.org/viewtopic.php?p=101868 */
		LONG(0)
	}

	/* Force .bss region in flat binary file for lld.
	See https://stackoverflow.com/a/77339871 */
	.fake : {
		. = . + SIZEOF(.bss);
	}

	/DISCARD/ : {
		*(.eh_frame)
		*(.comment)
		*(.note*)
	}
}
