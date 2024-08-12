#pragma once

/// \file
/// \brief Interrupt Descriptor Table (IDT).
///
/// Note that these structs are different than the 64-bit version.

#include <cstdint>

namespace arch::idt {

/// \brief A single interrupt vector descriptor.
///
struct IDTEntry {
  uint16_t isr_lo;
  uint16_t cs;
  uint8_t rsv0;
  uint8_t type : 4;
  uint8_t rsv1 : 1;
  uint8_t dpl : 2;
  uint8_t p : 1;
  uint16_t isr_hi;
} __attribute__((packed));

/// \brief IDT descriptor
///
struct IDTR {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed));

static_assert(sizeof(IDTEntry) == 8);
static_assert(sizeof(IDTR) == 6);

/// Initializes the IDT and PIC, and then enables interrupts.
/// See isrs.cc to see the interrupt vector -> ISR mapping.
void init();

} // namespace arch::idt
