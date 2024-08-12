#pragma once

/// \file
/// \brief C wrappers around common asm functions.
///
/// See asm.S for implementations.
///
/// Operand order is similar to Intel syntax (dest, src). This is
/// similar to the convention used by the C stdlib (think memcpy) and
/// OSDev.

#include <stdint.h>

extern "C" {

void outb(uint16_t port, uint8_t val);
void outw(uint16_t port, uint16_t val);
void outl(uint16_t port, uint32_t val);

uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);
}
