#pragma once

/// \file
/// \brief Architecture-specific timing functions.

#include <cstdint>

namespace arch::time {

/// Read the CPU's time-stamp counter.
__attribute__((naked)) inline uint64_t rdtsc() {
  __asm__ volatile("rdtsc\n\tret");
}

} // namespace arch::time
