#pragma once

/// \file
/// \brief Programmable Interrupt Chip (PIC)
///
/// This is for the dual cascaded 8259 chip. This does NOT implement
/// the APIC (Advanced PIC) interface.

#include "asm.h"

namespace drivers::pic {

/// Mask all interrupts. Necessary for using the APIC.
///
void disable();

/// Called at the end of interrupts to unmask the interrupt in the PIC.
void eoi(uint8_t irq);

/// Initialize the PIC, and map from the BIOS defaults to the standard
/// interrupt vector range. Preserves the interrupt mask.
///
/// - PIC1: remap from [0x08, 0x10) -> [offset1, offset1+8)
/// - PIC2: remap from [0x70, 0x78) -> [offset2, offset2+8)
///
/// The sane default for x86 is offset1=0x20, offset2=0x28.
///
/// Copied from https://wiki.osdev.org/8259_PIC#Initialisation
void init(uint8_t offset1, uint8_t offset2);

/// Returns true if this (remapped) interrupt vector is expected to be
/// a HW interrupt using the standard x86 PIC remapping.
inline bool is_hw_interrupt(uint8_t ivec) {
  return ivec >= 0x20 && ivec < 0x30;
}

} // namespace drivers::pic
