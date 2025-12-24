#pragma once

/// \file
/// \brief Interrupt (and exception) service routines.

#include <cstdint>

/// Interrupt vector, used to initialize the IDT.
extern void (**isrs)();
unsigned num_isrs();
