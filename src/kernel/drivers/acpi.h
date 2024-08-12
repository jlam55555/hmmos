#pragma once

/// \file
/// \brief Very simple ACPI shutdown code from OSDev.

#include "asm.h"

namespace acpi {

/// \brief QEMU-specific shutdown code.
///
__attribute__((noreturn)) inline void shutdown() {
  outw(0x604, 0x2000);

  while (1) {
    __asm__ volatile("hlt");
  }
}

} // namespace acpi
