#pragma once

/// Very simple ACPI shutdown code from OSDev.

#include "asm.h"
namespace acpi {

/// QEMU-specific shutdown code.
inline void shutdown() { outw(0x604, 0x2000); }

} // namespace acpi
