#pragma once

/// Interrupt (and exception) service routines.

#include <stdint.h>

struct InterruptFrame {
  uint32_t eip;
  uint32_t cs;
  uint32_t eflags;
} __attribute__((packed));

struct RegisterFrame {
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t esp;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;
} __attribute__((packed));

/// Interrupt vector, used to initialize the IDT.
extern void (**isrs)();
unsigned num_isrs();
