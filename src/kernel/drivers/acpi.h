#pragma once

/// Very simple ACPI shutdown code from OSDev.

#include "asm.h"

namespace acpi {

/// QEMU-specific shutdown code.
__attribute__((noreturn)) inline void shutdown() {
  outw(0x604, 0x2000);
  __builtin_unreachable();
}

} // namespace acpi
