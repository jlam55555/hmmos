#pragma once

/// \file
/// \brief Virtual memory definitions.

#include "boot_protocol.h"
#include "memdefs.h"
#include <cstddef>
#include <stddef.h>

namespace mem::virt {

constexpr size_t hhdm_len = 1 * GB - KERNEL_MAP_SZ;
constexpr size_t hhdm_start = HM_START;

template <typename T> inline T *direct_to_hhdm(size_t phys_addr) {
  return reinterpret_cast<T *>(phys_addr + hhdm_start);
}

} // namespace mem::virt
