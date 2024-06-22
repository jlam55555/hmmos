OUTPUT_FORMAT("elf32-i386");
ENTRY(_start);
SECTIONS
{
	. = 0x7C00;
	.text : SUBALIGN(0) {
		*(.text);
	}

	/* Cool party trick: can set .sig from linker script, though
	I'm not going to do that here. */

	/DISCARD/ : {
		*(.eh_frame);
		*(.comment);

		/* GNU ld adds an annoying .note.gnu.property
		section. I don't know how to remove this section
		without a linker conf. */		
		*(.note*);
	}
}
