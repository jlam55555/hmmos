#include "idt.h"
#include "asm.h"
#include "drivers/pic.h"
#include "isrs.h"
#include "util/assert.h"

#include <cstddef>

namespace arch::idt {

// We could probably use std::array here, but its layout is not
// strictly required to be the same as a plain array.
IDTEntry idt[256] = {};
IDTR idtr = {};

void init() {
  ASSERT(num_isrs() == 256);

  idtr.base = (size_t)idt;
  idtr.limit = sizeof idt - 1;

  for (unsigned i = 0; i < num_isrs(); ++i) {
    void (*isr)() = isrs[i];
    IDTEntry &entry = idt[i];
    entry.isr_lo = (size_t)isr & 0xFFFF;
    entry.isr_hi = (size_t)isr >> 16;
    entry.cs = 0x08; // ring 0 code descriptor at GDT index 1
    entry.rsv0 = 0;
    // Trap gate (0x0F) for exceptions, interrupt gate (0x0E) for
    // interrupts.
    entry.type = drivers::pic::is_hw_interrupt(i) ? 0x0E : 0x0F;
    entry.rsv1 = 0;
    entry.dpl = i == 0x80 ? 3 : 0;
    entry.p = 1;
  }

  // Map interrupts to [0x20, 0x30) for x86.
  drivers::pic::init(0x20, 0x28);

  // Load the new IDT and enable interrupts.
  __asm__ volatile("lidt %0\n\tsti" : : "m"(idtr));
}

} // namespace arch::idt
