OUTPUT_FORMAT("elf32-i386");
ENTRY(_start);
SECTIONS
{
	.text (0x7C00) : {
		*(.text.boot);
		*(.text);
	}

	.rodata : {
		*(.rodata);
	}

	.data : {
		*(.data);
	}

	.sig (0x7DFE) : {
		SHORT(0xAA55);
	}

	/DISCARD/ : {
		*(.eh_frame);
		*(.comment);

		/* GNU ld adds an annoying .note.gnu.property
		section. I don't know how to remove this section
		without a linker conf. */
		*(.note*);
	}
}
