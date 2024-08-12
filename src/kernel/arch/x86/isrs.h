#pragma once

/// \file
/// \brief Interrupt (and exception) service routines.

#include <stdint.h>

/// Interrupt vector, used to initialize the IDT.
extern void (**isrs)();
unsigned num_isrs();
