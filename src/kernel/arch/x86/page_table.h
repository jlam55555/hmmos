#pragma once

/// \file arch/x86/page_table.h
/// \brief x86 page table implementation
///
/// \see mm/virt.h for full documentation

#include <cstdint>

namespace arch::page_table {

void enumerate_page_tables();
bool map(uint64_t phys, void *virt, bool uncacheable);
bool unmap(void *virt);
bool mark_uncacheable(void *virt);

} // namespace arch::page_table
